// Phase 4: effects, clock sync, arpeggiator, sequencer, MIDI implementation [manual pp.64–67, 92–128].

#include "TestHelpers.h"

#include "core/ArpSeq.h"
#include "core/Effects.h"
#include "core/MidiDecoder.h"
#include "core/Performance.h"

using namespace sg;

namespace
{
constexpr double kFs = 48000.0;

struct Event { int sample; int note; bool on; float velocity; };

struct Recorder final : ArpSeq::Sink
{
    std::vector<Event> events;
    int now = 0;
    void arpNoteOn (int n, float v) override { events.push_back ({ now, n, true, v }); }
    void arpNoteOff (int n) override         { events.push_back ({ now, n, false, 0.0f }); }
    void arpBend (float) override {}

    std::vector<int> onNotes() const
    {
        std::vector<int> r;
        for (const auto& e : events) if (e.on) r.push_back (e.note);
        return r;
    }
    std::vector<int> onTimes() const
    {
        std::vector<int> r;
        for (const auto& e : events) if (e.on) r.push_back (e.sample);
        return r;
    }
};

// Runs the arpeggiator's clock for `samples` at `bpm` from beat position `beat`.
void runArp (ArpSeq& a, Recorder& rec, double& beat, int samples, double bpm = 120.0)
{
    const double bps = bpm / 60.0 / kFs;
    int left = samples;
    while (left > 0)
    {
        const int until = a.samplesUntilNext (beat, bps, left);
        if (until == 0) { a.fire (beat, rec); continue; }
        beat += until * bps;
        rec.now += until;
        left -= until;
    }
}

ArpSeqSettings arpSettings (ArpMode mode, int range = 1, int swing = 0)
{
    ArpSeqSettings s;
    s.on = true; s.mode = mode; s.range = range; s.swing = swing; s.clockDiv = 4;   // 1/16
    return s;
}
} // namespace

class DelayTests final : public juce::UnitTest
{
public:
    DelayTests() : juce::UnitTest ("Delay: time, infinite feedback, freeze, sync", "fx") {}

    void runTest() override
    {
        beginTest ("TIME is sample-accurate [p.65]");
        {
            StereoDelay d; d.prepare (kFs, 6.0);
            d.setParams (0.25f, 0.0f, 1.0f, false); d.reset();
            std::vector<float> l (24000, 0.0f), r (24000, 0.0f);
            l[0] = r[0] = 1.0f;
            d.process (l.data(), r.data(), 24000);
            int peak = 1;
            for (int i = 1; i < 24000; ++i) if (std::abs (l[static_cast<size_t> (i)]) > std::abs (l[static_cast<size_t> (peak)])) peak = i;
            expectEquals (peak, 12000);
        }

        beginTest ("FEEDBACK at max repeats with no decay or degradation [p.66]");
        {
            StereoDelay d; d.prepare (kFs, 6.0);
            d.setParams (0.05f, 1.0f, 1.0f, false); d.reset();
            std::vector<float> l (2400 * 41, 0.0f), r (l.size(), 0.0f);
            l[0] = r[0] = 0.5f;
            d.process (l.data(), r.data(), static_cast<int> (l.size()));
            const float first = l[2400];
            float worst = 0.0f;
            for (int k = 2; k <= 40; ++k) worst = std::max (worst, std::abs (l[static_cast<size_t> (2400 * k)] - first));
            logMessage ("  echo 1 = " + juce::String (first, 4) + ", max drift over 40 repeats = " + juce::String (worst, 7));
            expectWithinAbsoluteError (first, 0.5f, 1.0e-4f);
            expectLessThan (worst, 1.0e-4f);
        }

        beginTest ("FREEZE: the loop keeps playing and new notes stay out [p.67]");
        {
            StereoDelay d; d.prepare (kFs, 6.0);
            d.setParams (0.1f, 0.5f, 1.0f, false); d.reset();
            const int n = static_cast<int> (0.8 * kFs);
            std::vector<float> l (static_cast<size_t> (n), 0.0f), r (static_cast<size_t> (n), 0.0f);
            l[0] = 1.0f;                                         // enters the loop
            l[static_cast<size_t> (0.25 * kFs)] = 1.0f;          // played while frozen: must not enter
            const int freezeAt = static_cast<int> (0.15 * kFs);
            d.process (l.data(), r.data(), freezeAt);
            d.setParams (0.1f, 0.5f, 1.0f, true);
            d.process (l.data() + freezeAt, r.data() + freezeAt, n - freezeAt);
            auto at = [&] (double s) { return l[static_cast<size_t> (s * kFs)]; };
            logMessage ("  echoes at 0.2 / 0.4 / 0.6 s: " + juce::String (at (0.2), 3) + " / " + juce::String (at (0.4), 3) + " / " + juce::String (at (0.6), 3)
                        + ", frozen-out note echo at 0.35 s: " + juce::String (at (0.35), 4));
            expectWithinAbsoluteError (at (0.4), at (0.2), 0.01f, "loop level constant");
            expectWithinAbsoluteError (at (0.6), at (0.2), 0.01f, "loop level constant");
            expectLessThan (std::abs (at (0.35)), 0.01f, "new input doesn't enter the frozen loop");
        }

        beginTest ("SYNC: delay time and LFO 1 rate in note values [pp.55–56, 65–66]");
        {
            LayerParams p;
            p.arpSync = true;
            LayerControl c;
            c.bpm = 120.0f;
            c.update (p, AltWaveBank::factory());
            expectWithinAbsoluteError (c.delaySecondsFor (11.5f / 16.0f), 0.5f, 1.0e-5f, "1/4 note at 120 BPM");
            expectWithinAbsoluteError (c.delaySecondsFor (15.5f / 16.0f), 4.0f / 3.0f, 1.0e-4f, "whole-note triplet");
            expectWithinAbsoluteError (c.lfo1LowHzFor (7.5f / 16.0f), 2.0f, 1.0e-5f, "LFO 1 at 1/4 note = 2 Hz");
            expectWithinAbsoluteError (c.lfo1LowHzFor (0.0f), 120.0f / 60.0f / 32.0f, 1.0e-6f, "8 whole notes");
        }
    }
};

class ChorusTests final : public juce::UnitTest
{
public:
    ChorusTests() : juce::UnitTest ("Chorus I / II / I+II", "fx") {}

    void runTest() override
    {
        auto run = [] (int mode, std::vector<float>& l, std::vector<float>& r)
        {
            Chorus c; c.setMode (mode); c.prepare (kFs);
            const size_t n = static_cast<size_t> (kFs * 2);
            l.assign (n, 0.0f); r.assign (n, 0.0f);
            for (size_t i = 0; i < n; ++i) l[i] = r[i] = 0.5f * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * i / kFs));
            c.process (l.data(), r.data(), static_cast<int> (n));
        };

        beginTest ("Off passes the signal through unchanged");
        {
            std::vector<float> l, r;
            run (Chorus::Off, l, r);
            expectWithinAbsoluteError (l[50000], 0.5f * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * 440.0 * 50000 / kFs)), 1.0e-6f);
        }

        beginTest ("Each mode widens a mono input into stereo [p.64]");
        for (int mode : { Chorus::I, Chorus::II, Chorus::Both })
        {
            std::vector<float> l, r;
            run (mode, l, r);
            double lr = 0, ll = 0, rr = 0;
            for (size_t i = 24000; i < l.size(); ++i) { lr += l[i] * r[i]; ll += l[i] * l[i]; rr += r[i] * r[i]; }
            const double corr = lr / std::sqrt (ll * rr);
            const float level = sgt::rms (l, 24000) / 0.3536f;
            logMessage ("  mode " + juce::String (mode) + ": L/R correlation " + juce::String (corr, 3) + ", level " + juce::String (level, 2));
            expectLessThan (corr, 0.995, "stereo");
            expect (level > 0.6f && level < 1.6f, "level stays sensible");
        }
    }
};

class ArpTests final : public juce::UnitTest
{
public:
    ArpTests() : juce::UnitTest ("Arpeggiator modes, timing, swing, hold", "arp") {}

    void runTest() override
    {
        auto play = [] (ArpSeqSettings s, std::initializer_list<int> keys, int samples)
        {
            ArpSeq a; Recorder rec; double beat = 0.0;
            a.setSettings (s, rec);
            for (int k : keys) a.keyDown (k, 0.9f, false, beat, rec);
            runArp (a, rec, beat, samples);
            return rec;
        };
        const int step = 6000;   // 1/16 at 120 BPM

        beginTest ("UP / DOWN / U&D / RANGE [p.94]");
        {
            auto up = play (arpSettings (ArpMode::Up), { 64, 60, 67 }, step * 6).onNotes();
            expect (up == std::vector<int> ({ 60, 64, 67, 60, 64, 67 }), "UP low → high");
            auto down = play (arpSettings (ArpMode::Down), { 60, 64, 67 }, step * 4).onNotes();
            expect (down == std::vector<int> ({ 67, 64, 60, 67 }), "DOWN high → low");
            auto ud = play (arpSettings (ArpMode::UpDown), { 60, 64, 67 }, step * 7).onNotes();
            expect (ud == std::vector<int> ({ 60, 64, 67, 64, 60, 64, 67 }), "U&D");
            auto r2 = play (arpSettings (ArpMode::Up, 2), { 60, 64 }, step * 5).onNotes();
            expect (r2 == std::vector<int> ({ 60, 64, 72, 76, 60 }), "RANGE 2 adds the octave above");
        }

        beginTest ("RANDOM stays inside the chord");
        {
            auto rnd = play (arpSettings (ArpMode::Random), { 60, 64, 67 }, step * 24).onNotes();
            bool inside = true, varied = false;
            for (size_t i = 0; i < rnd.size(); ++i)
            {
                inside = inside && (rnd[i] == 60 || rnd[i] == 64 || rnd[i] == 67);
                if (i > 0 && rnd[i] != rnd[i - 1]) varied = true;
            }
            expect (inside && varied);
        }

        beginTest ("Steps land on the 1/16 grid with a 50 % gate");
        {
            auto rec = play (arpSettings (ArpMode::Up), { 60 }, step * 3 - 10);
            const auto times = rec.onTimes();
            expectEquals (static_cast<int> (times.size()), 3);
            expectEquals (times[0], 0);
            expectWithinAbsoluteError (times[1], step, 1);
            expectWithinAbsoluteError (times[2], 2 * step, 1);
            int firstOff = -1;
            for (const auto& e : rec.events) if (! e.on) { firstOff = e.sample; break; }
            expectWithinAbsoluteError (firstOff, step / 2, 1);
        }

        beginTest ("SWING 4 delays every second step [p.93]");
        {
            const auto times = play (arpSettings (ArpMode::Up, 1, 4), { 60 }, step * 4).onTimes();
            expect (times.size() >= 3);
            expectWithinAbsoluteError (times[1], static_cast<int> (std::lround (2.0 * step * 0.67)), 1);
            expectWithinAbsoluteError (times[2], 2 * step, 1);
        }

        beginTest ("Without HOLD the arpeggio stops when the keys are released");
        {
            ArpSeq a; Recorder rec; double beat = 0.0;
            a.setSettings (arpSettings (ArpMode::Up), rec);
            a.keyDown (60, 0.9f, false, beat, rec);
            runArp (a, rec, beat, step * 2);
            a.keyUp (60, rec);
            runArp (a, rec, beat, step * 4);
            expect (! a.isRunning());
            expectEquals (static_cast<int> (rec.onNotes().size()), 2);
        }

        beginTest ("HOLD keeps playing; a new chord after release replaces it [pp.81, 94]");
        {
            ArpSeq a; Recorder rec; double beat = 0.0;
            a.setSettings (arpSettings (ArpMode::Up), rec);
            a.setHold (true, rec);
            a.keyDown (60, 0.9f, false, beat, rec); a.keyDown (64, 0.9f, false, beat, rec);
            a.keyUp (60, rec); a.keyUp (64, rec);
            runArp (a, rec, beat, step * 4);
            expect (a.isRunning(), "still running after release");
            a.keyDown (70, 0.9f, false, beat, rec);
            rec.events.clear();
            runArp (a, rec, beat, step * 3);
            const auto notes = rec.onNotes();
            bool onlyNew = ! notes.empty();
            for (int n : notes) onlyNew = onlyNew && n == 70;
            expect (onlyNew, "the new chord replaced the latched one");
        }

        beginTest ("Host transport: the first step waits for the next grid line");
        {
            ArpSeq a; Recorder rec; double beat = 0.1;   // 0.1 beat past the grid
            a.setSettings (arpSettings (ArpMode::Up), rec);
            a.keyDown (60, 0.9f, true, beat, rec);
            runArp (a, rec, beat, step * 2);
            expectWithinAbsoluteError (rec.onTimes()[0], static_cast<int> (std::lround ((0.25 - 0.1) * 24000.0)), 1);
        }
    }
};

class SequencerTests final : public juce::UnitTest
{
public:
    SequencerTests() : juce::UnitTest ("Sequencer recording and playback", "arp") {}

    void runTest() override
    {
        const int step = 6000;

        auto recordDemo = [] (ArpSeq& a, Recorder& rec)
        {
            a.setRecording (true, 0);
            double beat = 0.0;
            a.keyDown (60, 0.9f, false, beat, rec); a.keyUp (60, rec);                       // step 1: C4
            a.keyDown (64, 0.7f, false, beat, rec); a.keyDown (67, 0.7f, false, beat, rec);  // step 2: chord
            a.keyUp (64, rec); a.keyUp (67, rec);
            a.keyDown (62, 0.5f, false, beat, rec); a.keyUp (62, rec);                       // step 3: D4
            a.setRecording (false);
            rec.events.clear();
        };

        beginTest ("STEP recording stores notes and chords on key release [p.96]");
        {
            ArpSeq a; Recorder rec;
            recordDemo (a, rec);
            const auto& s = a.working();
            expectEquals (s.length, 16, "length unchanged when recording within it");
            expectEquals (static_cast<int> (s.steps[0].count), 1);
            expectEquals (static_cast<int> (s.steps[1].count), 2);
            expectEquals (static_cast<int> (s.steps[1].notes[0]), 64);
            expectEquals (static_cast<int> (s.steps[1].notes[1]), 67);
            expectEquals (static_cast<int> (s.steps[2].notes[0]), 62);
            expectEquals (a.getRecordStep(), 3);
        }

        beginTest ("Playback, LENGTH, REST and transpose from C4 [pp.96–97]");
        {
            ArpSeq a; Recorder rec;
            recordDemo (a, rec);
            a.working().length = 4;                  // step 4 is empty = silent
            a.working().steps[2].rest = true;        // REST on step 3
            ArpSeqSettings s = arpSettings (ArpMode::Seq);
            a.setSettings (s, rec);
            double beat = 0.0;
            a.keyDown (62, 1.0f, false, beat, rec);  // D4: transpose +2
            runArp (a, rec, beat, step * 8);
            const auto notes = rec.onNotes();
            const std::vector<int> expected { 62, 66, 69, 62, 66, 69 };
            expect (notes == expected, "C4→D4 transposes every step by +2; rests and empty steps are silent");
        }

        beginTest ("ACCENT plays at full velocity [p.97]");
        {
            ArpSeq a; Recorder rec;
            recordDemo (a, rec);
            a.working().steps[0].accent = true;
            a.setSettings (arpSettings (ArpMode::Seq), rec);
            double beat = 0.0;
            a.keyDown (60, 1.0f, false, beat, rec);
            runArp (a, rec, beat, step * 2);
            expectEquals (rec.events[0].velocity, 1.0f);
            float second = 0.0f;
            for (const auto& e : rec.events) if (e.on && e.note == 64) { second = e.velocity; break; }
            expectLessThan (second, 1.0f);
        }

        beginTest ("SLIDE ties steps: the next note starts before the old one ends [p.97]");
        {
            ArpSeq a; Recorder rec;
            recordDemo (a, rec);
            a.working().steps[0].tie = true;
            a.setSettings (arpSettings (ArpMode::Seq), rec);
            double beat = 0.0;
            a.keyDown (60, 1.0f, false, beat, rec);
            runArp (a, rec, beat, step + 100);
            // Expected order: on 60 @0, then at step 1: on 64, on 67, off 60 (no gate-off at step/2).
            bool gateOffBeforeStep = false;
            for (const auto& e : rec.events) if (! e.on && e.sample < step) gateOffBeforeStep = true;
            expect (! gateOffBeforeStep, "tied note isn't released mid-step");
            const auto& last = rec.events.back();
            expect (! last.on && last.note == 60 && std::abs (last.sample - step) <= 1, "old note released after the new ones start");
        }

        beginTest ("LOAD / STORE between the 16 memories [p.98]");
        {
            SequenceBank bank {};
            ArpSeq a; Recorder rec;
            a.setBank (&bank);
            recordDemo (a, rec);
            a.store (5);
            a.clearSequence();
            expectEquals (static_cast<int> (a.working().steps[0].count), 0);
            a.load (5);
            expectEquals (static_cast<int> (a.working().steps[1].count), 2);
        }

        beginTest ("Arpeggiator drives the voices through the layer engine");
        {
            PerformanceEngine e;
            e.setAnalogTolerance (false);
            e.prepare (kFs, 512, 2);
            LayerParams up, lo;
            up.arpOn = true;
            up.arpSync = true;   // on the clock: 1/16 at 120 BPM (SYNC off would run at the free RATE)
            PerformanceParams pp;
            e.setParams (pp, up, lo);
            ClockInfo ci; ci.bpm = 120.0;
            e.setClock (ci);
            e.noteOn (1, 60, 1.0f);
            e.noteOn (1, 67, 1.0f);
            std::vector<float> a (512), b (512), c (512), d (512);
            std::vector<int> gated;
            for (int stepNo = 0; stepNo < 6; ++stepNo)
            {
                for (int i = 0; i < 6; ++i) e.process (a.data(), b.data(), c.data(), d.data(), 500);   // first half of the step
                const auto& L = e.layer (0);
                gated.push_back (L.countVoicesOnNote (60) > 0 ? 60 : (L.countVoicesOnNote (67) > 0 ? 67 : -1));
                for (int i = 0; i < 6; ++i) e.process (a.data(), b.data(), c.data(), d.data(), 500);   // second half
            }
            expect (gated == std::vector<int> ({ 60, 67, 60, 67, 60, 67 }), "UP arpeggio of the held chord, one note per step");
        }

        beginTest ("The bender position is recorded with the step [p.97]");
        {
            PerformanceEngine e;
            e.setAnalogTolerance (false);
            e.prepare (kFs, 512, 2);
            LayerParams up, lo;
            PerformanceParams pp;
            e.setParams (pp, up, lo);
            auto& seq = e.layer (0).getArpSeq();
            seq.setRecording (true, 0);
            e.noteOn (1, 60, 1.0f); e.noteOff (1, 60);             // step 1: no bend
            e.pitchBend (1, 0.5f);
            e.noteOn (1, 62, 1.0f); e.noteOff (1, 62);             // step 2: bent
            seq.setRecording (false);
            e.pitchBend (1, 0.0f);
            const auto& s = seq.working();
            expect (! s.steps[0].hasBend, "untouched bender: nothing stored");
            expect (s.steps[1].hasBend, "moved bender: position stored");
            expectWithinAbsoluteError (s.steps[1].bend, 0.5f, 1.0e-6f);
            expect (! s.steps[2].hasBend, "the stored bend belongs to one step only");
        }
    }
};

class MidiMapTests final : public juce::UnitTest
{
public:
    MidiMapTests() : juce::UnitTest ("MIDI CC / NRPN / RPN implementation", "midi") {}

    void runTest() override
    {
        using namespace midimap;

        beginTest ("CC table matches pp.116–122");
        expect (std::string (kCc[74].target) == "vcf.lpf" && kCc[74].kind == CcKind::Continuous);
        expect (std::string (kCc[71].target) == "vcf.res");
        expect (std::string (kCc[95].target) == "vcf.hpf");
        expect (std::string (kCc[7].target) == "vca.envLevel");
        expect (std::string (kCc[69].target) == "fx.freeze" && kCc[69].kind == CcKind::Switch);
        expect (kCc[14].kind == CcKind::Index && std::string (kCc[14].target) == "seq.slot");
        expect (kCc[29].valueCount == 6 && kCc[29].values[5] == 107);
        expect (kCc[107].valueCount == 8 && kCc[107].values[5] == 70, "CC 107 transcribed as printed");
        expect (kCc[78].values[3] == 96 && kCc[67].values[4] == 102);
        for (int cc : { 1, 2, 4, 11, 34, 64, 120, 121, 123 }) expect (kCc[static_cast<size_t> (cc)].kind == CcKind::Perf, "CC " + juce::String (cc) + " is a performance controller");
        for (int cc : { 8, 15, 39, 40, 66, 68, 88, 89, 90, 102, 103, 112, 119 }) expect (kCc[static_cast<size_t> (cc)].kind == CcKind::None, "CC " + juce::String (cc) + " unused");
        int params = 0;
        for (const auto& e : kCc) if (e.target != nullptr) ++params;
        logMessage ("  " + juce::String (params) + " CCs mapped to parameters");
        expectEquals (params, 86);

        beginTest ("Stepped values: each received value selects its band (DD-30)");
        expectEquals (stepIndex (kCc[29], 30), 1);
        expectEquals (stepIndex (kCc[29], 127), 5);
        expectEquals (stepIndex (kCc[107], 75), 5);
        expectEquals (stepIndex (kCc[107], 90), 5);
        expectEquals (stepIndex (kCc[79], 0), 0);
        expectEquals (stepIndex (kCc[79], 4), 3);

        beginTest ("Channel N = upper, N + 1 = lower; performance CCs pass through [p.99]");
        {
            MidiDecoder d;
            d.setBaseChannel (3);
            std::vector<ParamChange> got;
            auto emit = [&got] (const ParamChange& c) { got.push_back (c); };
            d.controller (3, 74, 127, emit);
            d.controller (4, 74, 0, emit);
            d.controller (5, 74, 64, emit);    // not ours
            expectEquals (static_cast<int> (got.size()), 2);
            expect (got[0].slot == 74 && got[0].layer == 0 && got[0].value == 1.0f);
            expect (got[1].layer == 1 && got[1].value == 0.0f);
            expect (! d.controller (3, 64, 127, emit), "sustain goes to the engine");
            d.setReceiveEnabled (false);
            got.clear();
            d.controller (3, 74, 10, emit);
            expect (got.empty(), "CC receive off");
        }

        beginTest ("NRPN 1024 + CC carries 14-bit values [pp.124–128]");
        {
            MidiDecoder d;
            std::vector<ParamChange> got;
            auto emit = [&got] (const ParamChange& c) { got.push_back (c); };
            d.controller (1, 99, 8, emit);      // 08H
            d.controller (1, 98, 74, emit);     // 4AH → NRPN 1098 = VCF cutoff
            d.controller (1, 6, 64, emit);
            d.controller (1, 38, 0, emit);
            expect (! got.empty() && got.back().slot == 74 && got.back().layer == 0);
            expectWithinAbsoluteError (got.back().value, 8192.0f / 16383.0f, 1.0e-6f);
        }

        beginTest ("RPN 0 / 1 / 2 and global NRPNs [p.123]");
        {
            MidiDecoder d;
            std::vector<ParamChange> got;
            auto emit = [&got] (const ParamChange& c) { got.push_back (c); };
            d.controller (1, 101, 0, emit); d.controller (1, 100, 0, emit); d.controller (1, 6, 7, emit);
            expect (got.back().slot == midislot::kBendRange && std::abs (got.back().value - 7.0f / 12.0f) < 1.0e-6f);
            d.controller (1, 101, 0, emit); d.controller (1, 100, 2, emit); d.controller (1, 6, 0x40 + 5, emit);
            expect (got.back().slot == midislot::kTranspose && got.back().value == 5.0f);
            d.controller (1, 101, 0, emit); d.controller (1, 100, 1, emit); d.controller (1, 6, 0x7F, emit); d.controller (1, 38, 0x7F, emit);
            expect (got.back().slot == midislot::kFineTune && got.back().value > 0.999f, "7F 7F = +100 cents");
            d.controller (1, 99, 0x10, emit); d.controller (1, 98, 0x03, emit); d.controller (1, 6, 4, emit);
            expect (got.back().slot == midislot::kMidiChannel && got.back().value == 5.0f, "NRPN 2051 = MIDI channel");
        }
    }
};

static DelayTests delayTests;
static ChorusTests chorusTests;
static ArpTests arpTests;
static SequencerTests sequencerTests;
static MidiMapTests midiMapTests;
