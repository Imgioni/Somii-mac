// Phase 3: layers, keyboard modes, voice assign, performance controls, matrix [manual pp.68–91].

#include "TestHelpers.h"

#include <memory>

#include "core/Performance.h"

using namespace sg;

namespace
{
struct Rig
{
    PerformanceEngine e;
    LayerParams up, lo;
    PerformanceParams pp;
    std::vector<float> uL, uR, lL, lR;

    void init (int os = 2)
    {
        e.setAnalogTolerance (false);
        e.prepare (sgt::kFs, 512, os);
        apply();
    }
    void apply() { e.setParams (pp, up, lo); }

    // Renders `seconds` more audio, appending to the buffers; returns the start index.
    size_t run (double seconds)
    {
        const size_t start = uL.size();
        const int n = static_cast<int> (seconds * sgt::kFs);
        for (auto* v : { &uL, &uR, &lL, &lR }) v->resize (start + static_cast<size_t> (n));
        for (int pos = 0; pos < n; pos += 256)
        {
            const int len = std::min (256, n - pos);
            const size_t o = start + static_cast<size_t> (pos);
            e.process (uL.data() + o, uR.data() + o, lL.data() + o, lR.data() + o, len);
        }
        return start;
    }

    double upperHz (size_t from, size_t to) const { return sgt::zeroCrossingHz (uL, sgt::kFs, from, to); }
    double lowerHz (size_t from, size_t to) const { return sgt::zeroCrossingHz (lL, sgt::kFs, from, to); }
};

size_t at (double seconds) { return static_cast<size_t> (seconds * sgt::kFs); }
float portaFor (float secondsPerOctave) { return 1.0f + std::log10 (secondsPerOctave / 10.0f) / 4.0f; }
} // namespace

class KeyboardModeTests final : public juce::UnitTest
{
public:
    KeyboardModeTests() : juce::UnitTest ("Keyboard modes and MIDI routing", "perf") {}

    void runTest() override
    {
        beginTest ("SINGLE plays the upper layer by default [p.82]");
        {
            Rig r; r.init();
            r.e.noteOn (1, 69, 1.0f);
            const size_t s = r.run (0.3);
            expectGreaterThan (sgt::rms (r.uL, s + 2400), 0.05f);
            expectLessThan (sgt::rms (r.lL, s), 1.0e-6f);
            expectEquals (r.e.layer (0).getVoiceLimit(), 20);
        }

        beginTest ("SINGLE with LOWER selected");
        {
            Rig r; r.pp.singleLayer = 1; r.init();
            r.e.noteOn (1, 69, 1.0f);
            const size_t s = r.run (0.3);
            expectLessThan (sgt::rms (r.uL, s), 1.0e-6f);
            expectGreaterThan (sgt::rms (r.lL, s + 2400), 0.05f);
        }

        beginTest ("SINGLE: a held layer keeps sounding after switching away, until its HOLD goes off");
        {
            auto rig = std::make_unique<Rig>(); auto& r = *rig; r.init();   // heap: a Rig is too big for more stack frames here
            r.e.setHold (0, true);
            r.e.noteOn (1, 69, 1.0f);
            r.run (0.1);
            r.e.noteOff (1, 69);                               // held by HOLD
            r.pp.singleLayer = 1; r.apply();                   // switch to LOWER
            r.e.noteOn (1, 60, 1.0f);
            size_t s = r.run (0.3);
            expectGreaterThan (sgt::rms (r.uL, s + 2400), 0.05f, "upper keeps its held note");
            expectGreaterThan (sgt::rms (r.lL, s + 2400), 0.05f, "lower plays the new note");
            r.e.setHold (0, false);                            // HOLD off on the upper layer
            r.run (3.0);
            s = r.run (0.2);
            expectLessThan (sgt::rms (r.uL, s), 1.0e-4f, "upper stops once HOLD is off");
            expectEquals (r.e.layer (0).getVoiceLimit(), 0);
        }

        beginTest ("SINGLE: without HOLD, switching layer releases the old layer");
        {
            auto rig = std::make_unique<Rig>(); auto& r = *rig; r.init();   // heap: a Rig is too big for more stack frames here
            r.e.noteOn (1, 69, 1.0f);
            r.run (0.1);
            r.pp.singleLayer = 1; r.apply();
            r.run (3.0);
            const size_t s = r.run (0.2);
            expectLessThan (sgt::rms (r.uL, s), 1.0e-4f);
        }

        beginTest ("SINGLE: the sustain pedal moves with the layer switch");
        {
            auto rig = std::make_unique<Rig>(); auto& r = *rig; r.init();   // heap: a Rig is too big for more stack frames here
            r.e.controller (1, 64, 127);
            r.e.noteOn (1, 69, 1.0f);
            r.run (0.1);
            r.e.noteOff (1, 69);
            r.pp.singleLayer = 1; r.apply();
            r.e.controller (1, 64, 0);
            r.run (3.0);
            const size_t s = r.run (0.2);
            expectLessThan (sgt::rms (r.uL, s), 1.0e-4f, "upper not left sustained");
        }

        beginTest ("DUAL stacks both layers, 10 voices each [p.82]");
        {
            Rig r; r.pp.mode = KeyboardMode::Dual; r.init();
            r.e.noteOn (1, 69, 1.0f);
            const size_t s = r.run (0.3);
            expectGreaterThan (sgt::rms (r.uL, s + 2400), 0.05f);
            expectGreaterThan (sgt::rms (r.lL, s + 2400), 0.05f);
            for (int n = 0; n < 7; ++n) r.e.noteOn (1, 40 + n, 1.0f);   // 8 notes > 5 binaural super voices
            r.run (0.05);
            expectEquals (r.e.layer (0).getActiveVoiceCount(), 10);
            expectEquals (r.e.layer (1).getActiveVoiceCount(), 10);
        }

        beginTest ("SPLIT: upper from the split point up (default C4) [p.82]");
        {
            Rig r; r.pp.mode = KeyboardMode::Split; r.init();
            r.e.noteOn (1, 59, 1.0f);
            size_t s = r.run (0.3);
            expectLessThan (sgt::rms (r.uL, s), 1.0e-6f, "B3 → lower");
            expectGreaterThan (sgt::rms (r.lL, s + 2400), 0.05f);
            r.e.noteOff (1, 59);
            r.run (1.0);
            r.e.noteOn (1, 60, 1.0f);
            s = r.run (0.3);
            expectGreaterThan (sgt::rms (r.uL, s + 2400), 0.05f, "C4 → upper");
            expectLessThan (sgt::rms (r.lL, s + 2400), 1.0e-4f);
        }

        beginTest ("MIDI channel + 1 plays the lower layer [p.99, p.110]");
        {
            Rig r; r.pp.mode = KeyboardMode::Dual; r.init();
            r.e.noteOn (2, 69, 1.0f);
            const size_t s = r.run (0.3);
            expectLessThan (sgt::rms (r.uL, s), 1.0e-6f);
            expectGreaterThan (sgt::rms (r.lL, s + 2400), 0.05f);
        }

        beginTest ("A note-off follows its note even after a mode change");
        {
            Rig r; r.pp.mode = KeyboardMode::Split; r.init();
            r.e.noteOn (1, 72, 1.0f);
            r.run (0.1);
            r.pp.mode = KeyboardMode::Single; r.pp.singleLayer = 1; r.apply();
            r.e.noteOff (1, 72);
            r.run (1.0);
            expectEquals (r.e.layer (0).countVoicesOnNote (72), 0);
        }
    }
};

class VoiceAssignTests final : public juce::UnitTest
{
public:
    VoiceAssignTests() : juce::UnitTest ("Voice assign modes and unison", "perf") {}

    void runTest() override
    {
        beginTest ("SOLO: one voice, last-note priority [p.90]");
        {
            Rig r; r.up.voiceMode = VoiceMode::Solo; r.init();
            r.e.noteOn (1, 57, 1.0f); r.run (0.2);
            r.e.noteOn (1, 69, 1.0f);
            size_t s = r.run (0.3);
            expectEquals (r.e.layer (0).getActiveVoiceCount(), 2, "one binaural pair");
            expectWithinAbsoluteError (r.upperHz (s + 2400, s + 14400), 440.0, 1.0);
            r.e.noteOff (1, 69);
            s = r.run (0.3);
            expectWithinAbsoluteError (r.upperHz (s + 2400, s + 14400), 220.0, 0.6, "back to the held note");
        }

        auto legatoTest = [] (VoiceMode mode)
        {
            Rig r;
            r.up.voiceMode = mode;
            r.up.e2Decay = 0.45f; r.up.e2Sustain = 0.0f;  // 63 ms decay: silent long before the 2nd note
            r.init();
            r.e.noteOn (1, 57, 1.0f); r.run (0.8);
            r.e.noteOn (1, 60, 1.0f);                     // played legato
            const size_t s = r.run (0.1);
            return sgt::rms (r.uL, s, s + 960);           // first 20 ms after the second note
        };
        beginTest ("SOLO retriggers the envelopes, LEGATO doesn't [p.90]");
        {
            const float solo = legatoTest (VoiceMode::Solo), legato = legatoTest (VoiceMode::Legato);
            logMessage ("  level after legato note: SOLO " + juce::String (solo, 4) + ", LEGATO " + juce::String (legato, 5));
            expectGreaterThan (solo, 0.05f);
            expectLessThan (legato, 0.005f);
        }

        beginTest ("POLY 1 overlaps releases, POLY 2 curtails them [p.90]");
        {
            auto voicesAfterRepress = [] (VoiceMode mode)
            {
                Rig r; r.up.voiceMode = mode; r.up.e2Release = 0.8f; r.init();
                r.e.noteOn (1, 60, 1.0f); r.run (0.1);
                r.e.noteOff (1, 60); r.run (0.05);
                r.e.noteOn (1, 60, 1.0f); r.run (0.05);
                return r.e.layer (0).getActiveVoiceCount();
            };
            expectEquals (voicesAfterRepress (VoiceMode::Poly1), 4, "old release keeps sounding");
            expectEquals (voicesAfterRepress (VoiceMode::Poly2), 2, "release cut, voice reused");
        }

        beginTest ("UNISON in SOLO stacks all (size 2) or half (size 1) the voices [pp.90–91]");
        {
            for (int size : { 1, 2 })
            {
                Rig r; r.up.voiceMode = VoiceMode::Solo; r.up.unison = true; r.up.unisonSize = size; r.init();
                r.e.noteOn (1, 60, 1.0f); r.run (0.05);
                expectEquals (r.e.layer (0).countVoicesOnNote (60), size == 2 ? 20 : 10);
            }
        }

        beginTest ("UNISON in POLY divides the voices by the notes held [p.90]");
        {
            Rig r; r.up.unison = true; r.init();
            r.e.noteOn (1, 60, 1.0f); r.run (0.05);
            expectEquals (r.e.layer (0).countVoicesOnNote (60), 20);
            r.e.noteOn (1, 64, 1.0f); r.run (0.05);
            expectEquals (r.e.layer (0).countVoicesOnNote (60), 10);
            expectEquals (r.e.layer (0).countVoicesOnNote (64), 10);
            r.e.noteOn (1, 67, 1.0f); r.run (0.05);
            const int a = r.e.layer (0).countVoicesOnNote (60), b = r.e.layer (0).countVoicesOnNote (64), c = r.e.layer (0).countVoicesOnNote (67);
            logMessage ("  three notes: " + juce::String (a) + " / " + juce::String (b) + " / " + juce::String (c) + " voices");
            expect (a + b + c == 20 && std::max ({ a, b, c }) - std::min ({ a, b, c }) <= 2, "balanced stacks");
        }

        beginTest ("UNISON size 3 stacks an octave, size 4 octave + fifth [p.91]");
        {
            auto ratioDb = [] (int size, double ratio)
            {
                Rig r; r.up.voiceMode = VoiceMode::Solo; r.up.unison = true; r.up.unisonSize = size;
                r.up.dds1Wave = Dds1Wave::Sine; r.init();
                r.e.noteOn (1, 57, 1.0f);
                const size_t s = r.run (1.6);
                auto spec = sgt::spectrum (r.uL, s + 9600, 16, sgt::kFs);
                return sgt::Spectrum::db (spec.magAt (220.0 * ratio, 5.0)) - sgt::Spectrum::db (spec.magAt (220.0, 5.0));
            };
            expectGreaterThan (ratioDb (3, 2.0), -6.0f, "octave present");
            expectGreaterThan (ratioDb (4, std::pow (2.0, 7.0 / 12.0)), -9.0f, "fifth present");
            expectLessThan (ratioDb (2, 2.0), -40.0f, "size 2 has no octave");
        }
    }
};

class PitchControlTests final : public juce::UnitTest
{
public:
    PitchControlTests() : juce::UnitTest ("Octave, transpose, tuning, bender, ribbon, portamento", "perf") {}

    void runTest() override
    {
        auto hzWith = [] (auto configure)
        {
            Rig r; configure (r); r.init();
            r.e.noteOn (1, 69, 1.0f);
            const size_t s = r.run (0.6);
            return r.upperHz (s + 4800, s + 28000);
        };

        beginTest ("OCTAVE, global TRANSPOSE and FINE TUNE [pp.75–76]");
        expectWithinAbsoluteError (hzWith ([] (Rig& r) { r.up.octave = 1; }), 880.0, 1.0);
        expectWithinAbsoluteError (hzWith ([] (Rig& r) { r.pp.transpose = 12.0f; }), 880.0, 1.0);
        expectWithinAbsoluteError (hzWith ([] (Rig& r) { r.pp.fineTuneCents = 100.0f; }), 466.16, 0.6);

        beginTest ("LOWER DETUNE only moves the lower layer, PERF DETUNE moves both [p.80]");
        {
            Rig r; r.pp.mode = KeyboardMode::Dual; r.pp.lowerDetune = 7.0f; r.init();
            r.e.noteOn (1, 69, 1.0f);
            const size_t s = r.run (0.6);
            expectWithinAbsoluteError (r.upperHz (s + 4800, s + 28000), 440.0, 0.6);
            expectWithinAbsoluteError (r.lowerHz (s + 4800, s + 28000), 659.26, 0.8);
        }
        {
            Rig r; r.pp.mode = KeyboardMode::Dual; r.pp.perfDetune = -7.0f; r.init();
            r.e.noteOn (1, 69, 1.0f);
            const size_t s = r.run (0.6);
            expectWithinAbsoluteError (r.upperHz (s + 4800, s + 28000), 293.66, 0.5);
            expectWithinAbsoluteError (r.lowerHz (s + 4800, s + 28000), 293.66, 0.5);
        }

        beginTest ("Bender: full bend = DDS fader range, max one octave; DEST osc [pp.69, 72]");
        {
            Rig r; r.up.benderDds = 1.0f; r.init();
            r.e.pitchBend (1, 1.0f);
            r.e.noteOn (1, 69, 1.0f);
            const size_t s = r.run (0.6);
            expectWithinAbsoluteError (r.upperHz (s + 4800, s + 28000), 880.0, 1.0);
        }
        {
            Rig r; r.up.benderDds = 1.0f; r.up.destOsc = OscDest::Dds2; r.init();   // MIX = DDS 1 → unaffected
            r.e.pitchBend (1, 1.0f);
            r.e.noteOn (1, 69, 1.0f);
            const size_t s = r.run (0.6);
            expectWithinAbsoluteError (r.upperHz (s + 4800, s + 28000), 440.0, 0.6);
        }

        beginTest ("Ribbon: relative bend, a full slide = one octave; released notes keep pitch [p.77]");
        {
            Rig r; r.init();
            r.e.noteOn (1, 69, 1.0f);
            r.e.ribbonTouch (0.2f);
            r.e.ribbonMove (0.7f);                       // half the ribbon = +6 st
            const size_t s = r.run (0.6);
            expectWithinAbsoluteError (r.upperHz (s + 4800, s + 28000), 622.25, 1.0);
        }

        beginTest ("Ribbon from MIDI / host is absolute: centre = no bend, 3/4 = +6 st (deviation)");
        {
            auto rig = std::make_unique<Rig>(); auto& r = *rig;   // heap: one more stack Rig overflows this frame
            r.init();
            r.e.noteOn (1, 69, 1.0f);
            r.e.ribbonAbsolute (0.75f);
            const size_t s = r.run (0.6);
            expectWithinAbsoluteError (r.upperHz (s + 4800, s + 28000), 622.25, 1.0);
        }

        beginTest ("Assigning RIBN in the matrix disables ribbon pitch [p.77]");
        {
            Rig r;
            r.up.matrix[static_cast<int> (MSrc::Ribbon)][static_cast<int> (MDest::Hpf)] = 0.5f;
            r.init();
            r.e.noteOn (1, 69, 1.0f);
            r.e.ribbonTouch (0.2f);
            r.e.ribbonMove (0.7f);
            const size_t s = r.run (0.6);
            expectWithinAbsoluteError (r.upperHz (s + 4800, s + 28000), 440.0, 0.8);
        }

        beginTest ("Portamento: 0.5 s per octave, bigger intervals take longer [pp.73–74]");
        {
            for (int from : { 57, 45 })
            {
                Rig r; r.up.voiceMode = VoiceMode::Solo; r.up.portaTime = portaFor (0.5f); r.init();
                r.e.noteOn (1, from, 1.0f); r.run (0.2);
                r.e.noteOn (1, 69, 1.0f);
                r.run (0.25);
                const float mid = r.e.layer (0).getSoundingPitch();
                const float expected = static_cast<float> (from) + 12.0f * 0.25f / 0.5f;
                logMessage ("  from " + juce::String (from) + ": pitch after 0.25 s = " + juce::String (mid, 2));
                expectWithinAbsoluteError (mid, expected, 0.3f);
            }
        }
    }
};

class Lfo2Tests final : public juce::UnitTest
{
public:
    Lfo2Tests() : juce::UnitTest ("LFO 2 triggers and aftertouch", "perf") {}

    void runTest() override
    {
        // Square LFO 2 at 1 Hz, full DDS depth (+12 st while high): the first half-second reads 880 Hz
        // when LFO 2 is active, 440 Hz when it isn't.
        auto firstHalfHz = [] (Lfo2Trigger trig, float push, float at)
        {
            Rig r;
            r.up.lfo2Wave = Lfo2Wave::Square;
            r.up.lfo2Rate = std::log (20.0f) / std::log (1000.0f);
            r.up.lfo2Dds = 1.0f;
            r.up.lfo2Trigger = trig;
            r.init();
            r.e.controller (1, 1, static_cast<int> (push * 127.0f));
            r.e.channelPressure (1, at);
            r.e.noteOn (1, 69, 1.0f);
            const size_t s = r.run (0.45);
            return r.upperHz (s + 4800, s + 19200);
        };

        beginTest ("TRIG: the bender push (mod wheel) sets the depth [p.71]");
        {
            const double off = firstHalfHz (Lfo2Trigger::Trig, 0.0f, 0.0f), on = firstHalfHz (Lfo2Trigger::Trig, 1.0f, 0.0f);
            const double lfoPhaseCheck = on;
            logMessage ("  push 0: " + juce::String (off, 1) + " Hz, push max: " + juce::String (lfoPhaseCheck, 1) + " Hz");
            expectWithinAbsoluteError (off, 440.0, 1.0);
            expect (std::abs (on - 880.0) < 3.0 || std::abs (on - 440.0) > 50.0, "LFO 2 moves the pitch");
        }

        beginTest ("AT + TRIG responds to aftertouch, TRIG ignores it [p.71]");
        {
            expectWithinAbsoluteError (firstHalfHz (Lfo2Trigger::Trig, 0.0f, 1.0f), 440.0, 1.0);
            expect (std::abs (firstHalfHz (Lfo2Trigger::AtTrig, 0.0f, 1.0f) - 440.0) > 50.0);
        }

        beginTest ("ON: LFO 2 always runs [p.71]");
        expect (std::abs (firstHalfHz (Lfo2Trigger::On, 0.0f, 0.0f) - 440.0) > 50.0);

        beginTest ("ON (AT→BEND): aftertouch bends like the horizontal bender [p.71]");
        {
            Rig r;
            r.up.lfo2Trigger = Lfo2Trigger::On;
            r.up.lfo2Dds = 0.0f;
            r.up.benderDds = 1.0f;
            r.init();
            r.e.channelPressure (1, 1.0f);
            r.e.noteOn (1, 69, 1.0f);
            const size_t s = r.run (0.6);
            expectWithinAbsoluteError (r.upperHz (s + 4800, s + 28000), 880.0, 1.0);
        }

        beginTest ("Polyphonic aftertouch reaches only its own note");
        {
            Rig r;
            r.up.lfo2Trigger = Lfo2Trigger::On;
            r.up.lfo2Dds = 0.0f;
            r.up.benderDds = 1.0f;
            r.pp.mode = KeyboardMode::Split;
            r.init();
            r.e.noteOn (1, 69, 1.0f);
            r.e.polyPressure (1, 69, 1.0f);
            r.e.noteOn (1, 45, 1.0f);            // lower layer, no pressure
            const size_t s = r.run (0.6);
            expectWithinAbsoluteError (r.upperHz (s + 4800, s + 28000), 880.0, 1.0);
            expectWithinAbsoluteError (r.lowerHz (s + 4800, s + 28000), 110.0, 0.5);
        }
    }
};

class HoldMatrixTests final : public juce::UnitTest
{
public:
    HoldMatrixTests() : juce::UnitTest ("Hold, sustain and the modulation matrix", "perf") {}

    void runTest() override
    {
        beginTest ("HOLD keeps notes after the keys are released [p.81]");
        {
            Rig r; r.init();
            r.e.setHold (0, true);
            r.e.noteOn (1, 60, 1.0f); r.run (0.1);
            r.e.noteOff (1, 60); r.run (0.5);
            expectEquals (r.e.layer (0).countVoicesOnNote (60), 2);
            r.e.setHold (0, false); r.run (1.0);
            expectEquals (r.e.layer (0).getActiveVoiceCount(), 0);
        }

        beginTest ("Sustain pedal (CC 64) [p.29]");
        {
            Rig r; r.init();
            r.e.controller (1, 64, 127);
            r.e.noteOn (1, 60, 1.0f); r.run (0.1);
            r.e.noteOff (1, 60); r.run (0.5);
            expectEquals (r.e.layer (0).countVoicesOnNote (60), 2);
            r.e.controller (1, 64, 0); r.run (1.0);
            expectEquals (r.e.layer (0).getActiveVoiceCount(), 0);
        }

        beginTest ("Matrix: VEL → HPF (destination E) [p.84]");
        {
            auto fundamental = [] (float vel)
            {
                Rig r;
                r.up.matrix[static_cast<int> (MSrc::Velocity)][static_cast<int> (MDest::Hpf)] = 1.0f;
                r.init();
                r.e.noteOn (1, 45, vel);
                const size_t s = r.run (0.5);
                return sgt::toneAmplitude (r.uL, s + 4800, s + 24000, 110.0, sgt::kFs);
            };
            const double soft = fundamental (0.05f), hard = fundamental (1.0f);
            logMessage ("  110 Hz level: soft " + juce::String (soft, 4) + ", hard " + juce::String (hard, 5));
            expectLessThan (hard, soft * 0.1, "hard notes lose their bass");
        }

        beginTest ("Matrix: EXPR → LPF cutoff (direct 2) [p.88]");
        {
            auto bright = [] (float expr)
            {
                Rig r;
                r.up.lpf = 0.3f;
                r.up.matrix[static_cast<int> (MSrc::Expression)][static_cast<int> (MDest::LpfCutoff)] = 0.6f;
                r.init();
                r.e.controller (1, 11, static_cast<int> (expr * 127.0f));
                r.e.noteOn (1, 57, 1.0f);
                const size_t s = r.run (0.5);
                return sgt::toneAmplitude (r.uL, s + 4800, s + 24000, 220.0 * 8, sgt::kFs);
            };
            expectGreaterThan (bright (1.0f), bright (0.0f) * 10.0);
        }

        beginTest ("Matrix: ENV 1 → WAVE MOD morphs the waveform [p.84]");
        {
            Rig r;
            r.up.dds1Wave = Dds1Wave::Sine;
            r.up.e1Sustain = 1.0f;
            r.up.matrix[static_cast<int> (MSrc::Env1)][static_cast<int> (MDest::WaveMod)] = 1.0f;
            r.init();
            r.e.noteOn (1, 57, 1.0f);
            const size_t s = r.run (1.0);
            auto spec = sgt::spectrum (r.uL, s + 4800, 15, sgt::kFs);
            expectGreaterThan (sgt::Spectrum::db (spec.magAt (440.0)) - sgt::Spectrum::db (spec.magAt (220.0)), -8.0f, "saw harmonics appear");
        }

        beginTest ("Duplicate routings are excluded [p.87]");
        {
            LayerParams p;
            p.matrix[static_cast<int> (MSrc::Lfo2)][static_cast<int> (MDest::LpfCutoff)] = 1.0f;
            p.matrix[static_cast<int> (MSrc::Aftertouch)][static_cast<int> (MDest::Lfo2Rate)] = 1.0f;
            MatrixRoutes routes;
            routes.build (p.matrix);
            expectEquals (routes.count, 0);
        }
    }
};

class StealTests final : public juce::UnitTest
{
public:
    StealTests() : juce::UnitTest ("Click-free voice stealing", "perf") {}

    void runTest() override
    {
        beginTest ("A stolen voice fades out over 3 ms before the new note starts");
        LayerParams p;
        LayerControl c;
        c.update (p, AltWaveBank::factory());
        LayerMods m;
        Voice v;
        v.prepare (48000.0f, 2, 1, false);
        v.start (60, 1.0f, 0.0f, true, false);
        v.setupBlock (c, 32.0f / 48000.0f);
        float out[2];
        const float zero = 0.0f;
        for (int i = 0; i < 4800; ++i) v.process (c, m, i % 32, 1, &zero, &zero, out, 2);
        v.start (72, 1.0f, 0.0f, true, false);
        expect (v.isStealing());
        int samples = 0;
        while (v.isStealing() && samples < 1000) { v.process (c, m, samples % 32, 1, &zero, &zero, out, 2); ++samples; }
        expectWithinAbsoluteError (samples, 144, 2);
        expectEquals (v.getNote(), 72);
    }
};

class ArpHoldHandoverTests final : public juce::UnitTest
{
public:
    ArpHoldHandoverTests() : juce::UnitTest ("HOLD across arp on/off, arp speed changes, free arp rate", "perf") {}

    void runTest() override
    {
        beginTest ("HOLD: held notes keep playing when the arp goes on, and as plain notes when it goes off");
        {
            auto rig = std::make_unique<Rig>(); auto& r = *rig; r.init();
            r.e.setHold (0, true);
            for (int n : { 60, 64, 67 }) r.e.noteOn (1, n, 1.0f);
            r.run (0.1);
            for (int n : { 60, 64, 67 }) r.e.noteOff (1, n);
            size_t s = r.run (0.3);
            expectGreaterThan (sgt::rms (r.uL, s), 0.05f, "held chord");

            r.up.arpOn = true; r.apply();
            r.run (0.05);
            expect (r.e.layer (0).getArpSeq().isRunning(), "arp picked up the held chord");
            s = r.run (1.0);
            expectGreaterThan (sgt::rms (r.uL, s), 0.05f, "arpeggio of the held chord");

            r.up.arpOn = false; r.apply();
            s = r.run (0.5);
            expectGreaterThan (sgt::rms (r.uL, s + 4800), 0.05f, "held chord plays again with the arp off");
            for (int n : { 60, 64, 67 }) expectGreaterThan (r.e.layer (0).countVoicesOnNote (n), 0);

            r.e.setHold (0, false);
            r.run (3.0);
            s = r.run (0.2);
            expectLessThan (sgt::rms (r.uL, s), 1.0e-4f, "HOLD off stops everything");
        }

        beginTest ("Without HOLD, notes still down carry across the arp switch; released notes do not");
        {
            auto rig = std::make_unique<Rig>(); auto& r = *rig; r.init();
            r.up.arpOn = true; r.apply();
            r.e.noteOn (1, 60, 1.0f);
            r.run (0.3);
            r.up.arpOn = false; r.apply();
            r.run (0.1);
            expectGreaterThan (r.e.layer (0).countVoicesOnNote (60), 0, "key still down plays");
            r.e.noteOff (1, 60);
            r.run (3.0);
            const size_t s = r.run (0.2);
            expectLessThan (sgt::rms (r.uL, s), 1.0e-4f);
        }

        beginTest ("Changing CLK DIV mid-run keeps the arpeggio going");
        {
            auto rig = std::make_unique<Rig>(); auto& r = *rig;
            r.up.arpOn = true; r.up.arpSync = true; r.up.arpClockDiv = 4; r.init();   // 1/16 at 120 BPM
            r.e.setHold (0, true);
            r.e.noteOn (1, 60, 1.0f); r.e.noteOff (1, 60);
            r.run (10.0);
            const int before = r.e.layer (0).getArpSeq().getStepIndex();
            r.up.arpClockDiv = 2; r.apply();                                           // 1/4
            r.run (1.3);
            expectGreaterOrEqual (r.e.layer (0).getArpSeq().getStepIndex() - before, 2, "no long gap after the change");
        }

        beginTest ("SYNC off: the arp runs at the free RATE (20 ms … 2000 ms)");
        {
            auto fast = std::make_unique<Rig>(); auto& r = *fast;
            r.up.arpOn = true; r.up.arpSync = false; r.up.arpRate = 0.0f; r.init();    // 20 ms steps
            r.e.setHold (0, true);
            r.e.noteOn (1, 60, 1.0f); r.e.noteOff (1, 60);
            r.run (1.0);
            expectWithinAbsoluteError (r.e.layer (0).getArpSeq().getStepIndex(), 50, 3);

            auto slow = std::make_unique<Rig>(); auto& q = *slow;
            q.up.arpOn = true; q.up.arpSync = false; q.up.arpRate = 1.0f; q.init();    // 2000 ms steps
            q.e.setHold (0, true);
            q.e.noteOn (1, 60, 1.0f); q.e.noteOff (1, 60);
            q.run (3.0);
            expectEquals (q.e.layer (0).getArpSeq().getStepIndex(), 2);
        }
    }
};

static ArpHoldHandoverTests arpHoldHandoverTests;
static KeyboardModeTests keyboardModeTests;
static VoiceAssignTests voiceAssignTests;
static PitchControlTests pitchControlTests;
static Lfo2Tests lfo2Tests;
static HoldMatrixTests holdMatrixTests;
static StealTests stealTests;
