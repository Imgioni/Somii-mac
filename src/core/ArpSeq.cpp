#include "ArpSeq.h"

#include <algorithm>
#include <cmath>

namespace sg
{

namespace
{
template <typename List>
void erase (List& v, int note) noexcept
{
    v.eraseIf ([note] (const auto& e) { return e.first == note; });
}
template <typename List>
bool contains (const List& v, int n) noexcept { return std::find (v.begin(), v.end(), n) != v.end(); }
} // namespace

void ArpSeq::reset() noexcept
{
    physical.clear();
    chord.clear();
    sounding.clear();
    running = false;
    gateOffBeat = -1.0;
    stepCounter = 0;
    chordReleased = true;
    tieIntoNext = false;
    recording = false;
    recChord.clear();
}

void ArpSeq::setSettings (const ArpSeqSettings& s, Sink& sink)
{
    ArpSeqSettings n = s;
    n.clockDiv = std::clamp (n.clockDiv, 0, 7);
    n.range = std::clamp (n.range, 1, 4);
    n.swing = std::clamp (n.swing, 0, 4);
    n.seqSlot = std::clamp (n.seqSlot, 0, 15);
    n.freeBeats = std::clamp (n.freeBeats, 1.0e-3, 64.0);

    const bool wasOn = settings.on;
    const double oldLen = stepBeats();
    const int oldSwing = settings.swing;
    const bool seqChanged = (settings.mode == ArpMode::Seq) != (n.mode == ArpMode::Seq);
    const bool slotChanged = n.seqSlot != settings.seqSlot;
    settings = n;

    // A new step length (CLK DIV, free rate, tempo) keeps the next step where it is and spaces the
    // following ones by the new length, instead of recomputing the whole grid from its origin -
    // that could put the next step many beats away and silence the arpeggio.
    if (running && (std::abs (stepBeats() - oldLen) > 1.0e-9 || settings.swing != oldSwing))
        gridOrigin = nextStepBeat - stepOffset (stepCounter);

    if (slotChanged) load (n.seqSlot);                     // LOAD sequence [p.98]
    if (wasOn && ! n.on) stop (sink);
    else if (seqChanged && running) releaseSounding (sink);
}

double ArpSeq::stepOffset (int64_t k) const noexcept
{
    // SWING delays the second step of every pair [p.93].
    const double len = stepBeats();
    const double swing = clockdiv::kSwingFraction[static_cast<size_t> (settings.swing)];
    double t = static_cast<double> (k / 2) * 2.0 * len;
    if (k % 2 != 0) t += 2.0 * len * swing;
    return t;
}

void ArpSeq::startRunning (bool hostSynced, double beatPos)
{
    running = true;
    stepCounter = 0;
    sounding.clear();
    gateOffBeat = -1.0;
    tieIntoNext = false;
    const double len = stepBeats();
    // Following the host transport: start on the next grid line. Free-running: start immediately.
    gridOrigin = hostSynced && settings.sync ? std::ceil (beatPos / len - 1.0e-9) * len : beatPos;
    nextStepBeat = stepStart (0);
}

void ArpSeq::keyDown (int note, float velocity, bool hostSynced, double beatPos, Sink& sink)
{
    if (recording)
    {
        // SEQ REC: keys sound directly and are stored when all of them are released [p.96].
        sink.arpNoteOn (note, velocity);
        erase (physical, note);
        physical.push_back ({ note, velocity });
        erase (recChord, note);
        recChord.push_back ({ note, velocity });
        return;
    }

    erase (physical, note);
    physical.push_back ({ note, velocity });

    // A new chord after all keys were released replaces the latched one [pp.81, 94].
    if (chordReleased) chord.clear();
    chordReleased = false;
    erase (chord, note);
    chord.push_back ({ note, velocity });

    if (! running) startRunning (hostSynced, beatPos);
}

void ArpSeq::keyUp (int note, Sink& sink)
{
    const bool wasHeld = std::any_of (physical.begin(), physical.end(), [note] (const auto& e) { return e.first == note; });
    erase (physical, note);

    if (recording)
    {
        if (wasHeld) sink.arpNoteOff (note);
        if (physical.empty() && ! recChord.empty())
        {
            std::sort (recChord.begin(), recChord.end());
            SeqStep st;
            st.count = static_cast<uint8_t> (std::min<size_t> (recChord.size(), st.notes.size()));
            float vel = 0.0f;
            for (int i = 0; i < st.count; ++i)
            {
                st.notes[static_cast<size_t> (i)] = static_cast<int8_t> (recChord[static_cast<size_t> (i)].first);
                vel = std::max (vel, recChord[static_cast<size_t> (i)].second);
            }
            st.velocity = vel;
            st.hasBend = recHasBend;
            st.bend = recBend;
            seq.steps[static_cast<size_t> (recStep)] = st;
            seq.length = std::max (seq.length, recStep + 1);
            recStep = (recStep + 1) % 64;
            ++revision;
            recChord.clear();
            recHasBend = false;
        }
        return;
    }

    if (! hold) erase (chord, note);
    if (physical.empty())
    {
        chordReleased = true;
        if (! hold) stop (sink);
    }
}

void ArpSeq::adoptHeld (int note, float velocity, bool keyIsDown, bool hostSynced, double beatPos)
{
    if (keyIsDown)
    {
        erase (physical, note);
        physical.push_back ({ note, velocity });
    }
    erase (chord, note);
    chord.push_back ({ note, velocity });
    chordReleased = physical.empty();   // a fresh chord replaces a purely latched one [pp.81, 94]
    if (! running) startRunning (hostSynced, beatPos);
}

void ArpSeq::setHold (bool on, Sink& sink)
{
    if (on == hold) return;
    hold = on;
    if (! on && physical.empty())
    {
        chord.clear();
        chordReleased = true;
        stop (sink);
    }
}

void ArpSeq::stop (Sink& sink)
{
    releaseSounding (sink);
    running = false;
    gateOffBeat = -1.0;
    tieIntoNext = false;
}

void ArpSeq::releaseSounding (Sink& sink)
{
    for (int n : sounding) sink.arpNoteOff (n);
    sounding.clear();
}

int ArpSeq::samplesUntilNext (double beatPos, double beatsPerSample, int maxSamples) const noexcept
{
    if (! running || beatsPerSample <= 0.0) return maxSamples;
    double next = nextStepBeat;
    if (gateOffBeat >= 0.0) next = std::min (next, gateOffBeat);
    const double d = next - beatPos;
    if (d <= 1.0e-9) return 0;
    const double s = std::ceil (d / beatsPerSample - 1.0e-9);
    return s >= static_cast<double> (maxSamples) ? maxSamples : std::max (1, static_cast<int> (s));
}

void ArpSeq::fire (double beatPos, Sink& sink)
{
    if (! running) return;
    if (gateOffBeat >= 0.0 && gateOffBeat <= beatPos + 1.0e-9)
    {
        releaseSounding (sink);
        gateOffBeat = -1.0;
    }
    if (nextStepBeat <= beatPos + 1.0e-9)
    {
        if (settings.mode == ArpMode::Seq) playSeqStep (sink);
        else                               playArpStep (sink);
        ++stepCounter;
        nextStepBeat = stepStart (stepCounter);
        while (nextStepBeat <= beatPos + 1.0e-9) { ++stepCounter; nextStepBeat = stepStart (stepCounter); }   // host jumped ahead
    }
}

void ArpSeq::playArpStep (Sink& sink)
{
    releaseSounding (sink);
    if (chord.empty()) return;

    // Held notes, low to high, repeated over RANGE octaves [p.94].
    auto& notes = arpNotes;
    notes.assign (chord);
    std::sort (notes.begin(), notes.end());
    const size_t base = notes.size();
    for (int o = 1; o < settings.range; ++o)
        for (size_t i = 0; i < base; ++i)
            if (notes[i].first + 12 * o <= 127) notes.push_back ({ notes[i].first + 12 * o, notes[i].second });

    const int n = static_cast<int> (notes.size());
    const int64_t k = stepCounter;
    int idx = 0;
    switch (settings.mode)
    {
        case ArpMode::Up:     idx = static_cast<int> (k % n); break;
        case ArpMode::Down:   idx = n - 1 - static_cast<int> (k % n); break;
        case ArpMode::UpDown:
            if (n > 1) { const int period = 2 * n - 2; const int j = static_cast<int> (k % period); idx = j < n ? j : period - j; }
            break;
        case ArpMode::Random:
            rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
            idx = static_cast<int> (rng % static_cast<uint32_t> (n));
            break;
        case ArpMode::Seq: break;
    }

    const auto& note = notes[static_cast<size_t> (idx)];
    sink.arpNoteOn (note.first, note.second);
    sounding.push_back (note.first);
    gateOffBeat = stepStart (k) + kGate * stepBeats();
}

int ArpSeq::transposeKey() const noexcept
{
    if (! physical.empty()) return physical.back().first;
    if (! chord.empty())    return chord.back().first;
    return 60;
}

void ArpSeq::playSeqStep (Sink& sink)
{
    const int len = std::clamp (seq.length, 1, 64);
    const SeqStep& st = seq.steps[static_cast<size_t> (stepCounter % len)];
    const int transpose = transposeKey() - 60;   // relative to middle C (C4) [p.97]

    if (st.rest || st.count == 0)
    {
        releaseSounding (sink);
        tieIntoNext = false;
        gateOffBeat = -1.0;
        return;
    }

    FixedList<int, 8> notes;
    for (int i = 0; i < st.count; ++i)
        notes.push_back (std::clamp (st.notes[static_cast<size_t> (i)] + transpose, 0, 127));
    // ACCENT: full level / brightness (heard when DYNAMICS is ½ or ON) [p.97, DD-55]
    const float vel = st.accent ? 1.0f : st.velocity * 0.8f;
    if (st.hasBend) sink.arpBend (st.bend);

    if (tieIntoNext)
    {
        // SLIDE: the new notes start before the old ones end (legato / portamento glide) [p.97].
        for (int n : notes)    if (! contains (sounding, n)) sink.arpNoteOn (n, vel);
        for (int n : sounding) if (! contains (notes, n))    sink.arpNoteOff (n);
    }
    else
    {
        releaseSounding (sink);
        for (int n : notes) sink.arpNoteOn (n, vel);
    }
    sounding.assign (notes);
    tieIntoNext = st.tie;
    gateOffBeat = st.tie ? -1.0 : stepStart (stepCounter) + kGate * stepBeats();
}

void ArpSeq::load (int slot) noexcept
{
    if (bank != nullptr) seq = (*bank)[static_cast<size_t> (std::clamp (slot, 0, 15))];
    ++revision;
}

void ArpSeq::setStep (int index, const SeqStep& s) noexcept
{
    if (index < 0 || index >= 64) return;
    seq.steps[static_cast<size_t> (index)] = s;
    ++revision;
}

void ArpSeq::setLength (int length) noexcept
{
    seq.length = std::clamp (length, 1, 64);
    ++revision;
}

void ArpSeq::store (int slot) noexcept
{
    if (bank != nullptr) (*bank)[static_cast<size_t> (std::clamp (slot, 0, 15))] = seq;
}

void ArpSeq::setRecording (bool on, int startStep) noexcept
{
    recording = on;
    if (on && startStep >= 0) recStep = std::clamp (startStep, 0, 63);   // otherwise keep the record position
    recChord.clear();
    recHasBend = false;
}

} // namespace sg
