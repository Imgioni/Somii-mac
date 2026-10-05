#include "Voice.h"
#include "Lfo2.h"

namespace sg
{

void Voice::prepare (float hostRate, int oversampling, uint32_t seed, bool analogTolerance)
{
    hostFs = hostRate;
    osFactor = oversampling < 1 ? 1 : oversampling;
    osFs = hostFs * static_cast<float> (osFactor);
    analog = analogTolerance;

    rng.seed (seed * 2654435761u + 12345u);
    dds1.prepare (osFs, hostFs, seed * 3u + 1u);
    dds2.prepare (seed * 5u + 7u);
    sub.reset();
    env1.setSampleRate (hostFs);
    env2.setSampleRate (hostFs);
    envFixed.setSampleRate (hostFs);

    // DD-35: small fixed component tolerances, like a real analog voice card.
    tolPitchCents = analog ? 1.5f * rng.bipolar() : 0.0f;
    tolCutOct     = analog ? 0.03f * rng.bipolar() : 0.0f;
    tolEnv        = analog ? 1.0f + 0.03f * rng.bipolar() : 1.0f;
    // Each voice card carries its own LFO 2 [p.72]: its own starting phase, and its own rate.
    tolLfo2       = analog ? 1.0f + 0.025f * rng.bipolar() : 1.0f;
    lfo2Seed      = seed * 2246822519u + 374761393u;
    lfo2Phase     = rng.unipolar();
    reset();
}

void Voice::reset()
{
    active = gate = stealing = false;
    stealGain = 1.0f;
    env1.kill(); env2.kill(); envFixed.kill();
    dds1.reset();
    hpf.reset();
    lpf.reset();
    lpf3w.reset();
    svf.reset();
    dds2Prev = 0.0f;
    lastVcaEnv = 0.0f;
    hasPlayed = false;
    modSum.fill (0.0f);
}

void Voice::start (int n, float vel, float uni, bool declick, float glide)
{
    if (stealing)
    {
        pending = { n, vel, uni, glide };   // still fading: just replace the note to start
        return;
    }
    if (declick && active && lastVcaEnv > 1.0e-3f)
    {
        // Voice steal: fade the old note out over 3 ms, then start from silence (no click).
        pending = { n, vel, uni, glide };
        stealing = true;
        stealGain = 1.0f;
        stealStep = 1.0f / (kStealFadeSeconds * hostFs);
        return;
    }
    begin (n, vel, uni, glide, false);
}

void Voice::begin (int n, float vel, float uni, float glide, bool resetEnvelopes)
{
    if (resetEnvelopes) { env1.kill(); env2.kill(); envFixed.kill(); }

    note = n;
    velocity = clampf (vel, 0.0f, 1.0f);
    unisonSemis = uni;
    gate = active = true;
    samplesSinceOn = 0.0f;
    controlCountdown = 0;       // full control update on the first sample
    cutoffPrimed = false;       // jump (don't glide) to the new note's cutoff
    polyAT = 0.0f;

    pitchTarget = static_cast<float> (n) + uni;
    // portamento: from the layer's last played note (poly), from this voice's own pitch (mono), or not at all
    if (glide >= 0.0f) pitchNow = glide + uni;
    else if (! (glide == kGlideOwn && hasPlayed)) pitchNow = pitchTarget;
    hasPlayed = true;

    dds1.noteOn (superHalf);    // SUPER ½ resets DDS 1 phase [p.61]
    dds2.noteOn();              // DDS 2 always resets to zero [p.35]
    env1.gateOn();
    env2.gateOn();
    envFixed.gateOn();
}

void Voice::changeNote (int n, float uni) noexcept
{
    if (stealing) { pending.note = n; pending.unison = uni; return; }
    note = n;
    unisonSemis = uni;
    pitchTarget = static_cast<float> (n) + uni;
    controlCountdown = 0;
}

void Voice::release() noexcept
{
    if (stealing) { stealing = false; stealGain = 1.0f; }
    gate = false;
    env1.gateOff();
    env2.gateOff();
    envFixed.gateOff();
}

void Voice::kill() noexcept { reset(); }

void Voice::setupBlock (const LayerControl& c, float blockSeconds) noexcept
{
    const LayerParams& P = *c.norm;

    // DRIFT: slow Ornstein–Uhlenbeck walks with unit variance, ~1.5 s correlation time.
    const float a = std::exp (-blockSeconds / 1.5f);
    const float s = std::sqrt (1.0f - a * a) * 1.7320508f;
    driftPitch = a * driftPitch + s * rng.bipolar();
    driftCut   = a * driftCut   + s * rng.bipolar();
    driftEnv   = a * driftEnv   + s * rng.bipolar();

    const float tEnv = tolEnv * (1.0f + 0.1f * c.drift * clampf (driftEnv, -2.0f, 2.0f));
    const float kt1 = envKeytrackScale (c.e1KeytrackK, note);

    // Envelope times (matrix destinations G and direct 9–15 act here, once per block).
    const auto& r = c.routes;
    const float e1A = r.has (MDest::Env1Attack)  ? taper::envTime (eff (c, MDest::Env1Attack, P.e1Attack))   : c.e1A;
    const float e1D = r.has (MDest::Env1Decay)   ? taper::envTime (eff (c, MDest::Env1Decay, P.e1Decay))     : c.e1D;
    const float e1S = r.has (MDest::Env1Sustain) ? eff (c, MDest::Env1Sustain, P.e1Sustain)                  : c.e1S;
    const float e1R = r.has (MDest::Env1Release) ? taper::envTime (eff (c, MDest::Env1Release, P.e1Release)) : c.e1R;
    const float e2A = r.has (MDest::Env2Attack)  ? taper::envTime (eff (c, MDest::Env2Attack, P.e2Attack))   : c.e2A;
    const float e2D = r.has (MDest::Env2Decay)   ? taper::envTime (eff (c, MDest::Env2Decay, P.e2Decay))     : c.e2D;
    const float e2S = r.has (MDest::Env2Sustain) ? eff (c, MDest::Env2Sustain, P.e2Sustain)                  : c.e2S;
    const float e2R = r.has (MDest::Env2Release) ? taper::envTime (eff (c, MDest::Env2Release, P.e2Release)) : c.e2R;

    env1.setParams (c.e1AH * tEnv, e1A * tEnv, c.e1DH * tEnv, e1D * tEnv * kt1, e1S, e1R * tEnv * kt1,
                    c.e1Mode == Env1Mode::Loop);
    env2.setParams (0.0f, e2A * tEnv, c.e2DH * tEnv, e2D * tEnv, e2S, e2R * tEnv, false);
    // Fixed VCA envelopes [p.45]: gate (no A/D/R) and gate + release (release per ENV 2 R, DD-24).
    envFixed.setParams (0.0f, 0.0005f, 0.0f, 0.001f, 1.0f,
                        c.vcaEnv == VcaEnv::GateRelease ? e2R * tEnv : 0.001f, false);

    const float resNow = r.has (MDest::Res) ? eff (c, MDest::Res, c.res) : c.res;
    lpf.setResonance (resNow, c.drive);
    lpf3w.setResonance (resNow, c.twComp);
    lpf3w.setSaturation (c.vcfSat);
    svf.setResonance (c.svfRes);
    svf.setMode (c.svfModeMix, c.svfBand);
    svf.setCutoff (c.svfHz, osFs);
    hpf.setCutoff (r.has (MDest::Hpf) ? taper::hpfHz (eff (c, MDest::Hpf, P.hpf)) : c.hpfHz, osFs);

    superHalf = c.superMode == Tri::Half;
    const bool wasThirdWave = thirdWave;
    thirdWave = c.vcfStyle == VcfStyle::ThirdWave;
    if (thirdWave != wasThirdWave)
    {
        // the engine that is now idle keeps nothing: no stale states, no tail from the old filter
        if (thirdWave) { lpf.reset(); hpf.reset(); }
        else           { lpf3w.reset(); svf.reset(); }
        cutoffPrimed = false;
    }
    svfOn = c.svfOn && thirdWave;      // the state-variable filter belongs to the 3rd Wave engine
    dds1.setShape (c.dds1Wave, c.altA, c.altB);
    // A voice is scaled by kOutputGain (-12 dB) for polyphonic headroom; a CUSTOM sample gets that
    // back after the filter, like any sampler: a 0 dBFS file plays at 0 dBFS (VCA LEVEL 0 dB, one note).
    sampleBoost = c.sample != nullptr ? 1.0f / kOutputGain : 1.0f;
    dds1.setSample (c.sample, P.smpLoop, P.smpStart, P.smpEnd, P.smpLoopStart, P.smpLevel, P.smpRoot, P.smpFine, sampleCh);
    dds1.setSlice (P.smpSlice, P.smpSlices, P.smpSense, note);
    dds1.setSuper (c.superMode, r.has (MDest::PwDetune) ? eff (c, MDest::PwDetune, P.pwDetune) : c.detune, mirrored);

    dds2Audio = ! c.dds2IsLfo;
    sync = dds2Audio && c.dds2Mode == Dds2Mode::Sync;
    ring = dds2Audio && c.dds2Mode == Dds2Mode::Ring;
    subOn = c.dds2IsLfo && c.dds2Mode != Dds2Mode::Norm;          // mid = square, top = sine [p.39]
    subSquare = c.dds2Mode == Dds2Mode::Ring;
    // DDS 2 only needs to run when it is heard or modulates something.
    needDds2 = c.mix > 0.0f || c.xmodOct > 0.0f || c.vcfDds2Oct > 0.0f || c.vcaDds2Depth > 0.0f
            || r.uses (MSrc::Dds2) || r.has (MDest::OscMix) || r.has (MDest::CrossMod)
            || r.has (MDest::VcfDds2Amt) || r.has (MDest::VcaDds2Amt);
    velGain = (1.0f - c.dynamicsK) + c.dynamicsK * velocity * std::sqrt (velocity);   // DYNAMICS [p.45]

    // Unmodulated per-sample amounts (the control update overrides them when the matrix is active).
    if (! r.has (MDest::VcaEnvLevel)) vcaGainEff = c.vcaGain;
    if (! r.has (MDest::VcaLfo1Amt))  vcaLfoDepthEff = c.vcaLfoDepth;
    if (! r.has (MDest::Lfo1Delay))   lfo1DelayEff = c.lfo1DelayS;
}

void Voice::process (const LayerControl& c, const LayerMods& m, int sampleInBlock, int n,
                     const float* lfoValue, const float* lfoUni, float* out, int os) noexcept
{
    float gains[kMaxGroup];
    for (int j = 0; j < n; ++j)
        gains[j] = control (c, m, sampleInBlock + j, lfoValue[j], lfoUni[j]);
    render (out, n, os, gains);
}

float Voice::control (const LayerControl& c, const LayerMods& m, int sampleInBlock, float lfoValue, float lfoUni) noexcept
{
    samplesSinceOn += 1.0f;

    if (stealing)
    {
        stealGain -= stealStep;
        if (stealGain <= 0.0f)
        {
            stealing = false;
            stealGain = 1.0f;
            begin (pending.note, pending.velocity, pending.unison, pending.glideFrom, true);
        }
    }

    const float e1 = env1.tick();
    const float e2 = env2.tick();
    const float ef = envFixed.tick();
    const float vcaEnv = c.vcaEnv == VcaEnv::Env2 ? e2 : ef;
    lastVcaEnv = vcaEnv;
    if (! gate && ! stealing && (c.vcaEnv == VcaEnv::Env2 ? env2.isIdle() : envFixed.isIdle()))
        active = false;

    const bool inverted = c.e1Mode == Env1Mode::Inverted;
    const float e1s = inverted ? -e1 : e1;           // polarity-inverted envelope [p.48, DD-27]
    const float e1u = inverted ? 1.0f - e1 : e1;     // unipolar view for PWM/WAVE

    // LFO 1 DELAY: fade-in after note-on [p.56]
    float lg = 1.0f;
    if (lfo1DelayEff > 0.0f) lg = std::min (1.0f, samplesSinceOn / (lfo1DelayEff * hostFs));
    const bool hf = c.lfo1Wave == Lfo1Wave::HF || c.lfo1Wave == Lfo1Wave::HFTrk;
    const float lv = hf ? 0.0f : lfoValue * lg;
    const float lu = hf ? 0.0f : lfoUni;

    // ── VCA (every sample) ──
    const float trem1 = hf ? 1.0f : 1.0f - vcaLfoDepthEff * lg * (1.0f - lu);
    const float trem2 = 1.0f - lfo2TremDepth * (1.0f - lfo2Uni);                   // LFO 2 VCA [p.72]
    const float boost = 1.0f + (sampleBoost - 1.0f) * (1.0f - mix);   // only the DDS 1 share of MIX
    const float gain = kOutputGain * boost * vcaGainEff * vcaEnv * velGain * trem1 * trem2 * (stealing ? stealGain : 1.0f);

    if (--controlCountdown <= 0)
    {
        controlCountdown = kControlDivider;
        fullUpdate (c, m, sampleInBlock, e1s, e1u, e2, lv, lu, lg, hf);
    }
    return gain;
}

void Voice::fullUpdate (const LayerControl& c, const LayerMods& m, int sampleInBlock,
                        float e1s, float e1u, float e2, float lv, float lu, float lg, bool hf) noexcept
{
    const LayerParams& P = *c.norm;
    const auto& r = c.routes;
    const float dt = static_cast<float> (kControlDivider) / hostFs;

    // ── LFO 2, per voice [pp.70–72] ──
    const float atVoice = std::max (m.channelAT, polyAT);
    const float driver = std::max (m.push, atVoice);
    float lfo2Hz = r.has (MDest::Lfo2Rate) ? taper::lfoLowHz (eff (c, MDest::Lfo2Rate, P.lfo2Rate)) : c.lfo2Hz;
    lfo2Hz *= fastExp2 (c.lfo2RateModOct * driver);          // LFO2 RATE fader: push / AT speed it up
    lfo2Phase += static_cast<double> (lfo2Hz * tolLfo2 * dt);   // this voice's own LFO, free-running
    const float lfo2Raw = lfo2Shape (c.lfo2Wave, lfo2Phase, rng, lfo2Seed);
    float depth2 = m.push;                                                   // TRIG: bender push
    if (c.lfo2Trigger == Lfo2Trigger::AtTrig) depth2 = driver;              // AT + TRIG: whichever is greater
    else if (c.lfo2Trigger == Lfo2Trigger::On) depth2 = 1.0f;              // permanently on
    const float lfo2DelayS = r.has (MDest::Lfo2Delay) ? taper::lfo2Delay (eff (c, MDest::Lfo2Delay, P.lfo2Delay)) : c.lfo2DelayS;
    const float lg2 = lfo2DelayS > 0.0f ? std::min (1.0f, samplesSinceOn / (lfo2DelayS * hostFs)) : 1.0f;
    lfo2Out = lfo2Raw * depth2 * lg2;
    lfo2Uni = lfo2IsBipolar (c.lfo2Wave) ? 0.5f * (lfo2Raw + 1.0f) : lfo2Raw;
    lfo2TremDepth = c.lfo2VcaDepth * depth2 * lg2;

    // ── matrix [pp.84–89] ──
    if (r.count > 0)
    {
        float src[kMatrixSources];
        src[static_cast<int> (MSrc::Dds2)]       = dds2Prev;
        src[static_cast<int> (MSrc::Lfo2)]       = lfo2Out;
        src[static_cast<int> (MSrc::Env1)]       = e1s;
        src[static_cast<int> (MSrc::Velocity)]   = velocity;
        src[static_cast<int> (MSrc::Aftertouch)] = atVoice;
        src[static_cast<int> (MSrc::Expression)] = m.expression;
        src[static_cast<int> (MSrc::Ribbon)]     = m.ribbonPos;
        src[static_cast<int> (MSrc::Note)]       = clampf ((static_cast<float> (note) - 60.0f) / 60.0f, -1.0f, 1.0f);
        modSum.fill (0.0f);
        for (int i = 0; i < r.count; ++i)
        {
            const auto& route = r.routes[static_cast<size_t> (i)];
            modSum[route.dest] += route.amount * src[route.src];
        }
    }

    // ── portamento: glide time grows with the interval [pp.73–74] ──
    const float porta = r.has (MDest::PortaTime) ? taper::portaSecondsPerOctave (eff (c, MDest::PortaTime, P.portaTime))
                                                 : c.portaSecPerOct;
    if (porta <= 0.0f)
        pitchNow = pitchTarget;
    else
    {
        const float step = 12.0f * dt / porta;
        if (pitchNow < pitchTarget)      pitchNow = std::min (pitchTarget, pitchNow + step);
        else if (pitchNow > pitchTarget) pitchNow = std::max (pitchTarget, pitchNow - step);
    }

    // ── pitch ──
    const float cents = tolPitchCents + taper::driftCents (c.drift) * clampf (driftPitch, -2.0f, 2.0f);
    const float base = pitchNow + m.pitchOffset + cents * 0.01f;
    if (gate) ribbonHeld = m.ribbonToPitch ? m.ribbonSemis : 0.0f;          // released notes keep their bend
    const float ribbon = ribbonHeld;

    // Bender / aftertouch [pp.69, 71]: ON (AT→BEND) makes aftertouch act like horizontal bend.
    const float bendAmt = m.bend + (c.lfo2Trigger == Lfo2Trigger::On ? atVoice : 0.0f);
    const float perfPitch = bendAmt * c.benderSemis + lfo2Out * c.lfo2PitchSemis;
    const bool b1 = c.destOsc != OscDest::Dds2, b2 = c.destOsc != OscDest::Dds1;    // DEST osc selector [p.72]

    const float pitchLfoSemis = r.has (MDest::DdsModLfo1Amt) ? taper::pitchLfoSemis (eff (c, MDest::DdsModLfo1Amt, P.pitchLfo1Amt))
                                                             : c.pitchLfoSemis;
    const float pmod = pitchLfoSemis * lv + c.pitchEnvSemis * e1s;
    const bool to1 = c.pitchDest != OscDest::Dds2;
    const bool to2 = c.pitchDest != OscDest::Dds1;

    // PWM/WAVE: manual position, or depth of LFO 1 / ENV 1 [p.62]
    const float pwmAmount = r.has (MDest::WaveMod) ? eff (c, MDest::WaveMod, P.pwmWave) : c.pwmAmount;
    float morph = pwmAmount;
    if (c.pwmSource == PwmSource::Lfo1)      morph = pwmAmount * lu * lg;
    else if (c.pwmSource == PwmSource::Env1) morph = pwmAmount * e1u;

    const float f1 = noteToHz (base + ribbon + 12.0f * static_cast<float> (c.dds1Octave)
                               + (to1 ? pmod : 0.0f) + (b1 ? perfPitch : 0.0f));
    dds1.setFrequency (f1 / osFs, morph);

    const float pwBase = r.has (MDest::PwDetune) ? eff (c, MDest::PwDetune, P.pwDetune) : c.pwBase;
    const float duty = 0.5f - 0.45f * clampf (pwBase + morph, 0.0f, 1.0f);   // DD-25
    const float tune = r.has (MDest::Dds2Tune) ? eff (c, MDest::Dds2Tune, (P.dds2Tune + 7.0f) / 14.0f) * 14.0f - 7.0f
                                               : c.dds2Tune;
    const float f2 = c.dds2IsLfo ? taper::dds2LfoHz (tune)
                                 : noteToHz (base + ribbon + 12.0f * static_cast<float> (c.dds2Octave) + tune
                                             + (to2 ? pmod : 0.0f) + (b2 ? perfPitch : 0.0f));
    dds2.setup (c.dds2Wave, f2 / osFs, duty);
    xmodOct = r.has (MDest::CrossMod) ? taper::crossModOctaves (eff (c, MDest::CrossMod, P.crossMod)) : c.xmodOct;
    mix = r.has (MDest::OscMix) ? eff (c, MDest::OscMix, P.mix) : c.mix;

    // ── LFO 1 in HF / HF TRK: audio-rate sine [p.59] ──
    const float keyPitch = pitchNow + m.keyOffset;
    hfOn = hf;
    const bool hfMod = hf && c.lfo1Mode == Lfo1Mode::FreeNorm;
    if (hf)
    {
        const float rate = r.has (MDest::Lfo1Rate) ? taper::lfoHighHz (eff (c, MDest::Lfo1Rate, P.lfo1Rate)) : c.lfo1Hz;
        const float track = c.lfo1Wave == Lfo1Wave::HFTrk ? fastExp2 ((keyPitch - 60.0f) / 12.0f) : 1.0f;
        hfInc = clampf (rate * track / osFs, 0.0f, 0.45f);
    }
    vcaLfoDepthEff = r.has (MDest::VcaLfo1Amt) ? eff (c, MDest::VcaLfo1Amt, P.vcaLfo1Amt) : c.vcaLfoDepth;
    const float vcfLfoOct = r.has (MDest::VcfLfo1Amt) ? taper::vcfLfoOctaves (eff (c, MDest::VcfLfo1Amt, thirdWave ? 0.0f : P.vcfLfo1Amt)) : c.vcfLfoOct;
    hfInject1 = (hf && c.lfo1Mode == Lfo1Mode::OnceDds1) ? 1.0f : 0.0f;
    hfInject2 = (hf && c.lfo1Mode == Lfo1Mode::ResetDds2) ? 1.0f : 0.0f;
    hfPitch1Oct = (hfMod && to1) ? pitchLfoSemis / 12.0f * lg : 0.0f;
    hfPitch2Oct = (hfMod && to2) ? pitchLfoSemis / 12.0f * lg : 0.0f;
    hfCutOct    = hfMod ? vcfLfoOct * lg : 0.0f;
    hfTremDepth = hfMod ? vcaLfoDepthEff * lg : 0.0f;
    lfo1DelayEff = r.has (MDest::Lfo1Delay) ? taper::lfo1Delay (eff (c, MDest::Lfo1Delay, P.lfo1Delay)) : c.lfo1DelayS;

    // ── VCF cutoff in octaves above 20 Hz ──
    float envSel = e1s;
    if (c.envSource == EnvSource::Env2)      envSel = e2;
    else if (c.envSource == EnvSource::Both) envSel = 0.5f * (e1s + e2);

    const float lpfFader = r.has (MDest::LpfCutoff) ? eff (c, MDest::LpfCutoff, c.lpfFader) : c.lpfFader;
    const float envOct = r.has (MDest::VcfEnvAmt)
        ? (thirdWave ? 10.0f * (2.0f * eff (c, MDest::VcfEnvAmt, P.twEnv) - 1.0f)
                     : taper::vcfEnvOctaves (eff (c, MDest::VcfEnvAmt, P.vcfEnvAmt))) : c.vcfEnvOct;
    if (thirdWave && svfOn)
        svf.setCutoff (c.svfHz * fastExp2 (c.svfEnvOct * e1s + c.svfVelOct * (velocity - 1.0f)
                                       + c.svfKeyK * (keyPitch - 60.0f) / 12.0f), osFs);
    cutOctBase = lpfFader * c.lpfOctaves
               + c.keytrackK * (keyPitch - 60.0f) / 12.0f              // [p.43]
               + envOct * envSel
               + vcfLfoOct * lv
               + bendAmt * c.benderVcfOct                              // bender VCF [p.69]
               + lfo2Out * c.lfo2VcfOct                                // LFO 2 VCF [p.71]
               + (thirdWave ? 0.0f : c.dynamicsK * (velocity - 1.0f) * 3.0f)                // DYNAMICS → brightness [p.45]
               + c.vcfVelOct * (velocity - 1.0f)                       // 3rd Wave VELOCITY knob
               + tolCutOct + taper::driftOctaves (c.drift) * clampf (driftCut, -2.0f, 2.0f);
    dds2CutOct = r.has (MDest::VcfDds2Amt) ? taper::vcfDds2Octaves (eff (c, MDest::VcfDds2Amt, thirdWave ? 0.0f : P.vcfDds2Amt)) : c.vcfDds2Oct;
    cutAudioRate = dds2CutOct > 0.0f || hfCutOct > 0.0f;
    if (! cutAudioRate)
    {
        const float hz = 20.0f * fastExp2 (cutOctBase);
        if (cutoffPrimed) { lpf.rampCutoff (hz, osFs, kControlDivider); lpf3w.rampCutoff (hz, osFs, kControlDivider); }
        else            { lpf.setCutoff (hz, osFs); lpf3w.setCutoff (hz, osFs); cutoffPrimed = true; }
    }
    if (r.has (MDest::Hpf)) hpf.setCutoff (taper::hpfHz (eff (c, MDest::Hpf, P.hpf)), osFs);
    if (r.has (MDest::Res)) { const float rr = eff (c, MDest::Res, c.res); lpf.setResonance (rr, c.drive); lpf3w.setResonance (rr, c.twComp); }

    // ── VCA amounts used every sample ──
    vcaGainEff = r.has (MDest::VcaEnvLevel) ? taper::levelGain (eff (c, MDest::VcaEnvLevel, P.vcaLevel)) : c.vcaGain;
    amDepth = r.has (MDest::VcaDds2Amt) ? eff (c, MDest::VcaDds2Amt, P.vcaDds2Amt) : c.vcaDds2Depth;
}

#if defined(_MSC_VER)
 #define SG_INLINE __forceinline
#else
 #define SG_INLINE inline __attribute__((always_inline))
#endif

SG_INLINE float Voice::renderSample (float gainBase) noexcept
{
    float hf = 0.0f;
    if (hfOn)
    {
        hf = sin2pi (hfPhase + hfOffset);
        hfPhase += hfInc;
        if (hfPhase >= 1.0f) hfPhase -= 1.0f;
    }

    // CROSS MOD: DDS 2 → DDS 1 exponential FM; reversed (DDS 1 → DDS 2) in SYNC [p.63].
    const float fmOct1 = (sync ? 0.0f : xmodOct * dds2Prev) + hfPitch1Oct * hf;
    const float fm1 = fmOct1 != 0.0f ? fastExp2 (fmOct1) : 1.0f;
    float wrap;
    const float d1 = dds1.tick (fm1, wrap);

    const float fmOct2 = (sync ? xmodOct * d1 : 0.0f) + hfPitch2Oct * hf;
    const float fm2 = fmOct2 != 0.0f ? fastExp2 (fmOct2) : 1.0f;
    const float d2 = needDds2 ? dds2.tick (fm2, sync ? wrap : -1.0f) : 0.0f;
    dds2Prev = d2;

    const float ch1 = d1 + hfInject1 * hf;
    float ch2;
    if (dds2Audio) ch2 = ring ? d1 * d2 : d2;   // RING: DDS 1 carrier × DDS 2, in DDS 2's channel [p.36]
    else           ch2 = subOn ? sub.tick (subSquare, dds1.getCentroidInc() * fm1, wrap) : 0.0f;
    ch2 += hfInject2 * hf;

    float x = ch1 + mix * (ch2 - ch1);            // MIX [p.40]
    if (cutAudioRate)
    {
        const float hz = 20.0f * fastExp2 (cutOctBase + dds2CutOct * d2 + hfCutOct * hf);
        if (thirdWave) lpf3w.setCutoff (hz, osFs); else lpf.setCutoff (hz, osFs);
    }
    if (thirdWave)
    {
        // 3rd Wave: the state-variable filter feeds the Curtis low-pass [its manual p.53].
        // The Super Gemini's HPF and DRIVE are not part of this engine and stay out of the path.
        if (svfOn) x = svf.process (x);
        x = lpf3w.process (x);
    }
    else
    {
        x = hpf.process (x);
        x = lpf.process (x);
    }

    float g = gainBase;
    if (amDepth > 0.0f)     g *= 1.0f - amDepth * 0.5f * (1.0f - d2);      // VCA DDS 2 [p.45]
    if (hfTremDepth > 0.0f) g *= 1.0f - hfTremDepth * 0.5f * (1.0f - hf);
    return x * g;
}

void Voice::render (float* out, int n, int os, const float* gains) noexcept
{
    for (int j = 0; j < n; ++j)
    {
        if (thirdWave) lpf3w.advanceRamp(); else lpf.advanceRamp();   // only the live engine
        for (int k = 0; k < os; ++k)
            out[j * os + k] = renderSample (gains[j]);
    }
}

void Voice::processPair (Voice& a, Voice& b, const LayerControl& c, const LayerMods& m, int sampleInBlock, int n,
                         const float* lvA, const float* luA, const float* lvB, const float* luB,
                         float* outA, float* outB, int os) noexcept
{
    float gA[kMaxGroup], gB[kMaxGroup];
    for (int j = 0; j < n; ++j)
    {
        gA[j] = a.control (c, m, sampleInBlock + j, lvA[j], luA[j]);
        gB[j] = b.control (c, m, sampleInBlock + j, lvB[j], luB[j]);
    }
    for (int j = 0; j < n; ++j)
    {
        if (a.thirdWave) a.lpf3w.advanceRamp(); else a.lpf.advanceRamp();
        if (b.thirdWave) b.lpf3w.advanceRamp(); else b.lpf.advanceRamp();
        for (int k = 0; k < os; ++k)
        {
            outA[j * os + k] = a.renderSample (gA[j]);
            outB[j * os + k] = b.renderSample (gB[j]);
        }
    }
}

} // namespace sg
