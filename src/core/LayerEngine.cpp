#include "LayerEngine.h"

#include <algorithm>
#include <cstring>

namespace sg
{

namespace
{
// Continuous parameters are smoothed (10 ms) so knob moves and automation never zipper.
void smoothTowards (LayerParams& s, const LayerParams& t, float a) noexcept
{
    const LayerParams prev = s;
    s = t;   // discrete fields (and matrix amounts) switch immediately
#define SG_SMOOTH(f) s.f = prev.f + (t.f - prev.f) * a
    SG_SMOOTH (twCutoff); SG_SMOOTH (twRes); SG_SMOOTH (twEnv); SG_SMOOTH (twKey);
    SG_SMOOTH (svfEnv); SG_SMOOTH (svfVelocity); SG_SMOOTH (svfKey);
    SG_SMOOTH (vcfSat); SG_SMOOTH (vcfVelocity); SG_SMOOTH (svfCutoff); SG_SMOOTH (svfRes); SG_SMOOTH (svfModeMix);
    SG_SMOOTH (smpLevel); SG_SMOOTH (dds2Tune); SG_SMOOTH (mix); SG_SMOOTH (pan);
    SG_SMOOTH (hpf); SG_SMOOTH (lpf); SG_SMOOTH (res);
    SG_SMOOTH (vcfEnvAmt); SG_SMOOTH (vcfLfo1Amt); SG_SMOOTH (vcfDds2Amt);
    SG_SMOOTH (vcaLevel); SG_SMOOTH (vcaLfo1Amt); SG_SMOOTH (vcaDds2Amt);
    SG_SMOOTH (e1AttackHold); SG_SMOOTH (e1Attack); SG_SMOOTH (e1DecayHold); SG_SMOOTH (e1Decay);
    SG_SMOOTH (e1Sustain); SG_SMOOTH (e1Release);
    SG_SMOOTH (e2Attack); SG_SMOOTH (e2DecayHold); SG_SMOOTH (e2Decay); SG_SMOOTH (e2Sustain); SG_SMOOTH (e2Release);
    SG_SMOOTH (lfo1Rate); SG_SMOOTH (lfo1Delay); SG_SMOOTH (lfo1LrPhase);
    SG_SMOOTH (pitchLfo1Amt); SG_SMOOTH (pitchEnv1Amt); SG_SMOOTH (pwDetune); SG_SMOOTH (drift);
    SG_SMOOTH (pwmWave); SG_SMOOTH (crossMod);
    SG_SMOOTH (benderDds); SG_SMOOTH (benderVcf);
    SG_SMOOTH (lfo2Rate); SG_SMOOTH (lfo2Delay); SG_SMOOTH (lfo2RateMod);
    SG_SMOOTH (lfo2Dds); SG_SMOOTH (lfo2Vcf); SG_SMOOTH (lfo2Vca); SG_SMOOTH (portaTime);
    SG_SMOOTH (delayFeedback); SG_SMOOTH (delaySend);
#undef SG_SMOOTH
}
} // namespace

void LayerEngine::prepare (double hostRate, int maxBlockSize, int oversampling)
{
    fs = hostRate;
    os = oversampling < 1 ? 1 : (oversampling > 16 ? 16 : oversampling);

    for (int i = 0; i < kSuperVoices; ++i)
    {
        voices[static_cast<size_t> (i)].prepare (static_cast<float> (fs), os, 1000u + static_cast<uint32_t> (i) * 97u, analog);
        voices[static_cast<size_t> (i)].setBinaural (binaural);
    }

    decL.prepare (os, kChunk);
    decR.prepare (os, kChunk);
    osL.assign (static_cast<size_t> (kChunk * os), 0.0f);
    osR.assign (static_cast<size_t> (kChunk * os), 0.0f);
    keyStack.reserve (128);
    chorus.prepare (fs);
    delay.prepare (fs, 6.0);   // synced times at slow tempos can exceed 1 s (1/2 note at 30 BPM = 4 s)
    (void) maxBlockSize;
    reset();
}

void LayerEngine::reset()
{
    for (auto& v : voices) v.reset();
    decL.reset();
    decR.reset();
    arp.reset();
    chorus.reset();
    delay.reset();
    beatPos = 0.0;
    firstBlock = true;
    keyDown.fill (false);
    latched.fill (false);
    keyStack.clear();
    unitNote.fill (-1);
    unitAge.fill (0);
    monoNote = -1;
    sustain = hold = false;
}

// ── voice units ─────────────────────────────────────────────────────────────────────────────

bool LayerEngine::unitActive (int u) const noexcept
{
    return binaural ? voices[static_cast<size_t> (u)].isActive()
                    : voices[static_cast<size_t> (u / 2)].isSlotActive (u % 2);
}

bool LayerEngine::unitGateOn (int u) const noexcept
{
    if (binaural)
    {
        const auto& sv = voices[static_cast<size_t> (u)];
        return (sv.getVoice (0).isActive() && sv.getVoice (0).isGateOn()) || (sv.getVoice (1).isActive() && sv.getVoice (1).isGateOn());
    }
    const auto& v = voices[static_cast<size_t> (u / 2)].getVoice (u % 2);
    return v.isActive() && v.isGateOn();
}

float LayerEngine::unitLevel (int u) const noexcept
{
    if (binaural)
    {
        const auto& sv = voices[static_cast<size_t> (u)];
        return std::max (sv.getVoice (0).getLevel(), sv.getVoice (1).getLevel());
    }
    return voices[static_cast<size_t> (u / 2)].getVoice (u % 2).getLevel();
}

void LayerEngine::startUnit (int u, int note, float velocity, float unisonSemis, bool declick, float glide)
{
    auto& sv = voices[static_cast<size_t> (binaural ? u : u / 2)];
    sv.startNote (binaural ? 0 : u % 2, note, velocity, unisonSemis, control, declick, glide);
    sv.age = ++ageCounter;
    unitAge[static_cast<size_t> (u)] = ageCounter;
    sv.setupBlock (control, static_cast<float> (kChunk / fs));
    unitNote[static_cast<size_t> (u)] = note;
}

void LayerEngine::releaseUnit (int u) noexcept
{
    voices[static_cast<size_t> (binaural ? u : u / 2)].releaseSlot (binaural ? 0 : u % 2);
}

// UNISON stack layout [p.91]: sizes 1 and 2 stack on the note, 3 alternates root / octave, 4 cycles
// root / fifth / octave. A ±5 cent spread keeps stacked voices from phase-locking (DD-43).
float LayerEngine::unisonOffset (int index, int count) const noexcept
{
    float s = 0.0f;
    if (target.unisonSize == 3)      s = (index % 2) ? 12.0f : 0.0f;
    else if (target.unisonSize == 4) s = (index % 3 == 1) ? 7.0f : ((index % 3 == 2) ? 12.0f : 0.0f);
    if (count > 1) s += 0.05f * (2.0f * static_cast<float> (index) / static_cast<float> (count - 1) - 1.0f);
    return s;
}

// ── note handling ───────────────────────────────────────────────────────────────────────────

void LayerEngine::noteOn (int note, float velocity)
{
    if (note < 0 || note > 127) return;
    if (firstBlock) beginBlock (0);
    if (arpTakesKeys()) { arp.keyDown (note, velocity, hostSynced, beatPos, *this); return; }
    keyOn (note, velocity);
}

void LayerEngine::noteOff (int note)
{
    if (note < 0 || note > 127) return;
    if (arpTakesKeys()) { arp.keyUp (note, *this); return; }
    keyOff (note, false);
}

void LayerEngine::keyOn (int note, float velocity)
{
    if (note < 0 || note > 127) return;
    keyDown[static_cast<size_t> (note)] = true;
    latched[static_cast<size_t> (note)] = false;
    keyVelocity[static_cast<size_t> (note)] = velocity;
    keyStack.erase (std::remove (keyStack.begin(), keyStack.end(), note), keyStack.end());
    keyStack.push_back (note);
    if (lastKey < 0 || clockSamples - lastKeyAt > static_cast<uint64_t> (0.03 * fs)) glideFrom = lastKey;
    lastKey = note; lastKeyAt = clockSamples;

    if (poolUnits() <= 0) return;
    if (isMono()) monoNoteOn (note, velocity);
    else          polyNoteOn (note, velocity);
}

void LayerEngine::keyOff (int note, bool fromArp)
{
    if (note < 0 || note > 127) return;
    keyDown[static_cast<size_t> (note)] = false;
    keyStack.erase (std::remove (keyStack.begin(), keyStack.end(), note), keyStack.end());

    // HOLD [p.81] / sustain latch the keys; with the arpeggiator on they latch its chord instead.
    if (! fromArp && (sustain || hold)) { latched[static_cast<size_t> (note)] = true; return; }
    keyReleased (note);
}

void LayerEngine::polyNoteOn (int note, float velocity)
{
    const int pool = poolUnits();

    // UNISON in POLY: the available voices are divided by the number of notes held [p.90].
    int groupSize[128] {};
    int held = 1;
    for (int u = 0; u < pool; ++u)
        if (unitGateOn (u))
        {
            const int n = unitNote[static_cast<size_t> (u)];
            if (n >= 0 && n != note && groupSize[n]++ == 0) ++held;
        }

    int count = 1;
    if (target.unison)
    {
        const int avail = target.unisonSize == 1 ? std::max (1, pool / 2) : pool;
        count = std::max (1, avail / held);
    }
    count = std::min (count, pool);

    std::array<bool, kMaxUnits> taken {};
    std::array<int, kMaxUnits> chosen {};
    for (int k = 0; k < count; ++k)
    {
        int best = -1;
        double bestScore = 1.0e30;
        for (int u = 0; u < pool; ++u)
        {
            if (taken[static_cast<size_t> (u)]) continue;
            const bool act = unitActive (u);
            const bool gated = act && unitGateOn (u);
            const int un = unitNote[static_cast<size_t> (u)];
            const double age = static_cast<double> (unitAge[static_cast<size_t> (u)]) * 1.0e-12;
            const double stealScore = 3.0 + (un >= 0 && groupSize[un] > 0 ? 1.0 / groupSize[un] : 1.0) + age;

            double score;
            if (voiceMode == VoiceMode::Poly2)
            {
                // POLY 2: overlapping releases are curtailed — a re-pressed note reuses its voice and
                // releasing voices are taken before idle ones [p.90, DD-44].
                if (act && un == note)  score = -1.0;
                else if (act && ! gated) score = unitLevel (u);
                else if (! act)          score = 1.0 + age;
                else                     score = stealScore;
            }
            else
            {
                // POLY 1: releases overlap — idle voices first, then the quietest releasing voice.
                if (! act)          score = age;
                else if (! gated)   score = 1.0 + unitLevel (u);
                else                score = stealScore;
            }
            if (score < bestScore) { bestScore = score; best = u; }
        }
        if (best < 0) break;
        taken[static_cast<size_t> (best)] = true;
        chosen[static_cast<size_t> (k)] = best;
        const int stolen = unitNote[static_cast<size_t> (best)];
        if (stolen >= 0 && groupSize[stolen] > 0 && unitGateOn (best)) --groupSize[stolen];   // keep stacks balanced
    }

    for (int k = 0; k < count; ++k)
        startUnit (chosen[static_cast<size_t> (k)], note, velocity, unisonOffset (k, count), true, glideFrom >= 0 ? static_cast<float> (glideFrom) : Voice::kNoGlide);
}

void LayerEngine::monoNoteOn (int note, float velocity)
{
    const int pool = poolUnits();
    int count = 1;
    if (target.unison) count = target.unisonSize == 1 ? std::max (1, pool / 2) : pool;   // [p.90]
    count = std::min (count, pool);

    bool anyGate = false;
    for (int u = 0; u < count; ++u) anyGate = anyGate || unitGateOn (u);
    const bool legatoChange = voiceMode == VoiceMode::Legato && monoNote >= 0 && anyGate;

    for (int u = 0; u < count; ++u)
    {
        if (legatoChange)
        {
            // LEGATO: no retrigger while playing legato [p.90]
            voices[static_cast<size_t> (binaural ? u : u / 2)].changeNote (binaural ? 0 : u % 2, note, unisonOffset (u, count));
            unitNote[static_cast<size_t> (u)] = note;
        }
        else
        {
            // SOLO: every note retriggers the envelopes [p.90]
            startUnit (u, note, velocity, unisonOffset (u, count), false, Voice::kGlideOwn);
        }
    }
    for (int u = count; u < pool; ++u)
        if (unitGateOn (u)) releaseUnit (u);
    monoNote = note;
}

void LayerEngine::keyReleased (int note)
{
    if (isMono())
    {
        if (note != monoNote) return;
        if (! keyStack.empty())
        {
            // Last-note priority: fall back to the most recent key still held (DD-45).
            const int next = keyStack.back();
            monoNoteOn (next, keyVelocity[static_cast<size_t> (next)]);
        }
        else
        {
            for (int u = 0; u < poolUnits(); ++u) if (unitGateOn (u)) releaseUnit (u);
            monoNote = -1;
        }
        return;
    }

    for (int u = 0; u < kMaxUnits; ++u)
        if (unitNote[static_cast<size_t> (u)] == note && u < (binaural ? kSuperVoices : kMaxUnits) && unitGateOn (u))
            releaseUnit (u);
}

void LayerEngine::releaseLatched()
{
    for (int n = 0; n < 128; ++n)
    {
        if (! latched[static_cast<size_t> (n)]) continue;
        latched[static_cast<size_t> (n)] = false;
        if (keyDown[static_cast<size_t> (n)]) continue;
        if (isMono())
        {
            if (keyStack.empty())
            {
                for (int u = 0; u < poolUnits(); ++u) if (unitGateOn (u)) releaseUnit (u);
                monoNote = -1;
            }
        }
        else
        {
            keyReleased (n);
        }
    }
}

void LayerEngine::handHeldToArp()
{
    bool any = false;
    for (int n = 0; n < 128; ++n)
    {
        const auto i = static_cast<size_t> (n);
        const bool down = keyDown[i], lat = latched[i];
        if (! down && ! lat) continue;
        any = true;
        keyDown[i] = latched[i] = false;
        keyStack.erase (std::remove (keyStack.begin(), keyStack.end(), n), keyStack.end());
        if (! isMono()) keyReleased (n);
        arp.adoptHeld (n, keyVelocity[i], down, hostSynced, beatPos);
    }
    if (any && isMono())
    {
        for (int u = 0; u < poolUnits(); ++u) if (unitGateOn (u)) releaseUnit (u);
        monoNote = -1;
    }
}

void LayerEngine::takeHeldFromArp()
{
    const bool latch = hold || sustain;
    arp.forEachHeld ([this, latch] (int n, float v, bool down)
    {
        if (! down && ! latch) return;
        keyOn (n, v);
        if (! down)
        {
            const auto i = static_cast<size_t> (n);
            keyDown[i] = false;
            keyStack.erase (std::remove (keyStack.begin(), keyStack.end(), n), keyStack.end());
            latched[i] = true;
        }
    });
    arp.clearKeys();
}

void LayerEngine::setSustain (bool on) noexcept
{
    sustain = on;
    arp.setHold (hold || sustain, *this);
    if (! on && ! hold) releaseLatched();
}

void LayerEngine::setHold (bool on) noexcept
{
    hold = on;
    arp.setHold (hold || sustain, *this);   // HOLD keeps an arpeggio / sequence running [p.81]
    if (! on && ! sustain) releaseLatched();
}

void LayerEngine::setClock (const ClockInfo& info) noexcept
{
    bpm = std::clamp (info.bpm, 20.0, 400.0);
    hostSynced = info.hostPlaying;
    if (hostSynced) beatPos = info.ppq;   // follow the host transport; otherwise free-run
}

void LayerEngine::setPolyAftertouch (int note, float v) noexcept
{
    for (auto& sv : voices) sv.setPolyAftertouch (note, v);
}

void LayerEngine::setVoiceLimit (int v) noexcept
{
    v = std::clamp (v, 0, kMaxUnits);
    if (v == voiceLimit) return;
    voiceLimit = v;
    const int units = binaural ? kSuperVoices : kMaxUnits;
    for (int u = poolUnits(); u < units; ++u)
        if (unitGateOn (u)) releaseUnit (u);
}

void LayerEngine::allNotesOff (bool immediate)
{
    arp.stop (*this);
    keyDown.fill (false);
    latched.fill (false);
    keyStack.clear();
    monoNote = -1;
    for (auto& v : voices)
    {
        if (immediate) v.killAll();
        else           v.releaseAll();
    }
}

// ── rendering ───────────────────────────────────────────────────────────────────────────────

void LayerEngine::applyStructuralChanges()
{
    if (target.binaural != binaural)
    {
        binaural = target.binaural;                  // BINAURAL on/off [p.91]
        for (auto& v : voices) { v.killAll(); v.setBinaural (binaural); }
        unitNote.fill (-1);
        monoNote = -1;
    }
    if (target.voiceMode != voiceMode)
    {
        const bool wasMono = isMono();
        voiceMode = target.voiceMode;
        if (wasMono != isMono())
        {
            for (auto& v : voices) v.releaseAll();
            monoNote = -1;
        }
    }
    if (target.lfo2Trigger != lastTrigger)
    {
        lastTrigger = target.lfo2Trigger;            // flipping the trigger switch resyncs LFO 2 [p.72]
        for (auto& v : voices) v.resyncLfo2();
    }
}

void LayerEngine::beginBlock (int len)
{
    applyStructuralChanges();
    if (firstBlock)
    {
        smoothed = target;
        firstBlock = false;
    }
    else
    {
        const float a = 1.0f - std::exp (-static_cast<float> (len) / (0.010f * static_cast<float> (fs)));
        smoothTowards (smoothed, target, a);
    }
    control.bpm = static_cast<float> (bpm);
    control.update (smoothed, AltWaveBank::factory());
    control.sample = smoothed.smpOn ? customSample.load (std::memory_order_acquire) : nullptr;
    mods.ribbonToPitch = ! control.routes.uses (MSrc::Ribbon);

    ArpSeqSettings as;
    as.on = smoothed.arpOn;
    as.clockDiv = smoothed.arpClockDiv;
    as.range = smoothed.arpRange;
    as.swing = smoothed.arpSwing;
    as.mode = smoothed.arpMode;
    as.seqSlot = smoothed.seqSlot;
    as.sync = smoothed.arpSync;
    as.freeBeats = static_cast<double> (taper::arpStepMs (smoothed.arpRate)) * bpm / 60000.0;
    const bool arpWasOn = arp.isOn();
    arp.setSettings (as, *this);
    // HOLD / sustain / held keys survive switching the arp: ON arpeggiates what is held, OFF plays it.
    if (! arp.isRecording())
    {
        if (as.on && ! arpWasOn) handHeldToArp();
        else if (! as.on && arpWasOn) takeHeldFromArp();
    }

    // Layer-wide LFO 2 clock: push / aftertouch speed it up via the LFO2 RATE fader [p.71].
    const float driver = std::max (mods.push, mods.channelAT);
    mods.lfo2Inc = control.lfo2Hz * fastExp2 (control.lfo2RateModOct * driver) / static_cast<float> (fs);

    panL = std::min (1.0f, 1.0f - smoothed.pan);    // PAN [p.40]
    panR = std::min (1.0f, 1.0f + smoothed.pan);

    const bool monoSpread = ! binaural && isMono();
    const float spread = smoothed.lfo1LrPhase;
    const float denom = static_cast<float> (std::max (1, poolUnits()));
    for (int i = 0; i < kSuperVoices; ++i)
        voices[static_cast<size_t> (i)].setMonoSpread (monoSpread, spread * (2.0f * i) / denom, spread * (2.0f * i + 1.0f) / denom);

    const float blockSeconds = static_cast<float> (len / fs);
    for (auto& v : voices)
        if (v.isActive()) v.setupBlock (control, blockSeconds);

    updateEffects();
}

// Effects settings; DELAY TIME / SEND / FEEDBACK can be matrix destinations (H, 23, 24) and follow
// the most recently played voice (DD-56).
void LayerEngine::updateEffects() noexcept
{
    float time = control.delaySeconds, feedback = control.delayFeedback, send = control.delaySend;
    const auto& r = control.routes;
    if (r.has (MDest::DelayTime) || r.has (MDest::DelaySend) || r.has (MDest::DelayFeedback))
    {
        const Voice* newest = nullptr;
        uint64_t age = 0;
        for (const auto& sv : voices)
            for (int s = 0; s < 2; ++s)
                if (sv.getVoice (s).isActive() && sv.age >= age) { age = sv.age; newest = &sv.getVoice (s); }
        if (newest != nullptr)
        {
            auto eff = [&] (MDest d, float base) { return clampf (base + newest->getMod (d), 0.0f, 1.0f); };
            if (r.has (MDest::DelayTime))     time = control.delaySecondsFor (eff (MDest::DelayTime, smoothed.delayTime));
            if (r.has (MDest::DelaySend))     send = eff (MDest::DelaySend, smoothed.delaySend);
            if (r.has (MDest::DelayFeedback)) feedback = eff (MDest::DelayFeedback, smoothed.delayFeedback);
        }
    }
    chorus.setMode (control.chorus);
    delay.setParams (time, feedback, send, control.freeze);
}

void LayerEngine::process (float* outL, float* outR, int n)
{
    const double beatsPerSample = bpm / 60.0 / fs;
    int done = 0;
    while (done < n)
    {
        int len = std::min (kChunk, n - done);
        if (arp.isRunning())
        {
            // Arpeggiator / sequencer events land on the exact sample of their clock position.
            const int until = arp.samplesUntilNext (beatPos, beatsPerSample, len);
            if (until == 0) { arp.fire (beatPos, *this); continue; }
            len = until;
        }
        beginBlock (len);

        const size_t osLen = static_cast<size_t> (len * os);
        std::memset (osL.data(), 0, sizeof (float) * osLen);
        std::memset (osR.data(), 0, sizeof (float) * osLen);

        const float spread = binaural ? 0.0f : smoothed.lfo1LrPhase;   // LR PHASE becomes SPREAD [p.58]
        for (auto& v : voices)
            if (v.isActive()) v.render (control, mods, osL.data(), osR.data(), len, os, spread);

        decL.process (osL.data(), outL + done, len);
        decR.process (osR.data(), outR + done, len);
        for (int i = 0; i < len; ++i) { outL[done + i] *= panL; outR[done + i] *= panR; }

        // PAN → CHORUS → dry + SEND → DELAY (DD-17)
        chorus.process (outL + done, outR + done, len);
        delay.process (outL + done, outR + done, len);

        mods.lfo2Phase += static_cast<double> (mods.lfo2Inc) * len;
        if (mods.lfo2Phase > 1.0e6) mods.lfo2Phase -= 1.0e6;
        beatPos += beatsPerSample * len;
        clockSamples += static_cast<uint64_t> (len);
        done += len;
    }
    for (size_t i = 0; i < voices.size(); ++i)
        for (int s = 0; s < 2; ++s)
            playheads[i * 2 + static_cast<size_t> (s)].store (voices[i].getVoice (s).samplePlayhead(), std::memory_order_relaxed);
}

int LayerEngine::getActiveVoiceCount() const noexcept
{
    int count = 0;
    for (const auto& v : voices)
        count += (v.isSlotActive (0) ? 1 : 0) + (v.isSlotActive (1) ? 1 : 0);
    return count;
}

int LayerEngine::countVoicesOnNote (int note) const noexcept
{
    int count = 0;
    for (const auto& v : voices)
        for (int s = 0; s < 2; ++s)
        {
            const auto& voice = v.getVoice (s);
            if (voice.isActive() && voice.isGateOn() && voice.getNote() == note) ++count;
        }
    return count;
}

const Voice* LayerEngine::getNewestVoice() const noexcept
{
    const Voice* newest = nullptr;
    uint64_t age = 0;
    for (const auto& sv : voices)
        for (int s = 0; s < 2; ++s)
            if (sv.getVoice (s).isActive() && sv.age >= age) { age = sv.age; newest = &sv.getVoice (s); }
    return newest;
}

unsigned LayerEngine::getLoopCycles() const noexcept
{
    const Voice* newest = getNewestVoice();
    return newest != nullptr ? newest->getEnv1Cycles() : 0u;
}

float LayerEngine::getSoundingPitch() const noexcept
{
    for (const auto& v : voices)
        for (int s = 0; s < 2; ++s)
            if (v.getVoice (s).isActive()) return v.getVoice (s).getPitch();
    return -1.0f;
}

} // namespace sg
