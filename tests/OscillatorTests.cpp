// DDS 1 / DDS 2 behaviour through the full layer path (oscillators → filter open → VCA → decimator).

#include "TestHelpers.h"

using namespace sg;

namespace
{
LayerParams openPatch()
{
    LayerParams p;             // init: DDS 1 SAW 8', MIX = DDS 1, LPF open, ENV 2 sustain 10
    p.e2Release = 0.0398f;   // 16 ms
    return p;
}
} // namespace

class PitchTests final : public juce::UnitTest
{
public:
    PitchTests() : juce::UnitTest ("DDS pitch and range", "osc") {}

    void runTest() override
    {
        beginTest ("A4 at 8' is 440 Hz");
        {
            LayerEngine e; sgt::prepareEngine (e, openPatch());
            auto s = sgt::renderNote (e, 69, 1.0f, 1.0);
            const double hz = sgt::zeroCrossingHz (s.l, sgt::kFs, 4800, s.l.size());
            logMessage ("  measured " + juce::String (hz, 3) + " Hz");
            expectWithinAbsoluteError (hz, 440.0, 0.2);
        }

        beginTest ("RANGE 64' … 2' [p.33]");
        const double expected[] = { 55.0, 110.0, 220.0, 440.0, 880.0, 1760.0 };
        for (int r = 0; r < 6; ++r)
        {
            auto p = openPatch();
            p.dds1Range = r;
            LayerEngine e; sgt::prepareEngine (e, p);
            auto s = sgt::renderNote (e, 69, 1.0f, 1.0);
            const double hz = sgt::zeroCrossingHz (s.l, sgt::kFs, 4800, s.l.size());
            expectWithinAbsoluteError (hz, expected[r], expected[r] * 0.001, "range index " + juce::String (r));
        }

        beginTest ("DDS 2 TUNE +7 st and RANGE [p.36]");
        {
            auto p = openPatch();
            p.mix = 1.0f;
            p.dds2Wave = Dds2Wave::Saw;
            p.dds2Tune = 7.0f;
            p.dds2Range = 2;   // 16'
            LayerEngine e; sgt::prepareEngine (e, p);
            auto s = sgt::renderNote (e, 69, 1.0f, 1.0);
            const double hz = sgt::zeroCrossingHz (s.l, sgt::kFs, 4800, s.l.size());
            const double want = 220.0 * std::pow (2.0, 7.0 / 12.0);
            expectWithinAbsoluteError (hz, want, want * 0.001);
        }
    }
};

class AliasTests final : public juce::UnitTest
{
public:
    AliasTests() : juce::UnitTest ("Open top end without audible aliasing", "osc") {}

    void runTest() override
    {
        for (int os : { 2, 4, 8 })
        {
            beginTest ("SAW at A7 (3520 Hz), " + juce::String (os) + "x");
            auto p = openPatch();
            LayerEngine e; sgt::prepareEngine (e, p, os);
            auto s = sgt::renderNote (e, 105, 1.0f, 1.6);
            auto spec = sgt::spectrum (s.l, 9600, 16, sgt::kFs);
            const float fund = spec.magAt (3520.0);
            const float worst = spec.worstNonHarmonicDb (3520.0, 20000.0, fund);
            const float h3 = sgt::Spectrum::db (spec.magAt (3 * 3520.0)) - sgt::Spectrum::db (fund);
            logMessage ("  worst alias " + juce::String (worst, 1) + " dB, 3rd harmonic " + juce::String (h3, 2)
                        + " dB (ideal saw -9.54)");
            // Top end: the 3rd harmonic (10.6 kHz) must stay within 2 dB of an ideal sawtooth.
            expectWithinAbsoluteError (h3, -9.54f, 2.0f, "top-end flatness");
            // Measured after the 4-point BLEP: −67 / −80 / −90 dB. Limits keep ~2 dB margin.
            const float limit = os >= 8 ? -87.0f : (os >= 4 ? -78.0f : -65.0f);
            expectLessThan (worst, limit, "aliasing");
        }
    }
};

class Dds2ModeTests final : public juce::UnitTest
{
public:
    Dds2ModeTests() : juce::UnitTest ("DDS 2 modes, sub-oscillator, LFO range", "osc") {}

    void runTest() override
    {
        beginTest ("Phase resets to zero on every note [p.35]");
        {
            Dds2 d; d.prepare (1);
            d.setup (Dds2Wave::Saw, 0.01f, 0.5f);
            for (int i = 0; i < 1234; ++i) d.tick (1.0f, -1.0f);
            d.noteOn();
            d.tick (1.0f, -1.0f);
            expectWithinAbsoluteError (d.getPhase(), 0.01f, 1.0e-5f);
        }

        beginTest ("SYNC locks DDS 2 to DDS 1's period [p.37]");
        {
            auto p = openPatch();
            p.mix = 1.0f;
            p.dds2Wave = Dds2Wave::Saw;
            p.dds2Tune = 5.3f;
            p.dds2Mode = Dds2Mode::Sync;
            LayerEngine e; sgt::prepareEngine (e, p);
            auto s = sgt::renderNote (e, 57, 1.0f, 1.6);   // A3 = 220 Hz
            auto spec = sgt::spectrum (s.l, 9600, 16, sgt::kFs);
            float strongest = 0.0f;
            for (int k = 1; k <= 20; ++k) strongest = std::max (strongest, spec.magAt (220.0 * k));
            const float worst = spec.worstNonHarmonicDb (220.0, 20000.0, strongest);
            logMessage ("  non-harmonic content " + juce::String (worst, 1) + " dB");
            expectLessThan (worst, -45.0f, "synced output only contains harmonics of DDS 1");
        }

        beginTest ("RING: DDS 1 × DDS 2 in DDS 2's channel [p.36]");
        {
            auto p = openPatch();
            p.dds1Wave = Dds1Wave::Sine;
            p.dds2Wave = Dds2Wave::Sine;
            p.dds2Tune = 7.0f;
            p.dds2Mode = Dds2Mode::Ring;
            p.mix = 1.0f;
            LayerEngine e; sgt::prepareEngine (e, p);
            auto s = sgt::renderNote (e, 69, 1.0f, 1.6);
            auto spec = sgt::spectrum (s.l, 9600, 16, sgt::kFs);
            const double f1 = 440.0, f2 = 440.0 * std::pow (2.0, 7.0 / 12.0);
            const float sum = spec.magAt (f2 + f1), diff = spec.magAt (f2 - f1);
            const float c1 = spec.magAt (f1), c2 = spec.magAt (f2);
            expectGreaterThan (sgt::Spectrum::db (sum), -24.0f, "sum sideband present");
            expectGreaterThan (sgt::Spectrum::db (diff), -24.0f, "difference sideband present");
            expectLessThan (sgt::Spectrum::db (c1) - sgt::Spectrum::db (sum), -40.0f, "carrier suppressed");
            expectLessThan (sgt::Spectrum::db (c2) - sgt::Spectrum::db (sum), -40.0f, "modulator suppressed");
        }

        beginTest ("Sub-oscillator one octave below DDS 1 [p.39]");
        for (auto mode : { Dds2Mode::Ring, Dds2Mode::Sync })   // middle = square, top = sine
        {
            auto p = openPatch();
            p.dds2Range = kDds2RangeLfo;
            p.dds2Mode = mode;
            p.mix = 1.0f;
            LayerEngine e; sgt::prepareEngine (e, p);
            auto s = sgt::renderNote (e, 69, 1.0f, 1.0);
            const double hz = sgt::zeroCrossingHz (s.l, sgt::kFs, 4800, s.l.size());
            expectWithinAbsoluteError (hz, 220.0, 0.3, mode == Dds2Mode::Ring ? "square sub" : "sine sub");
        }

        beginTest ("LFO range removes DDS 2 from the audio path [p.38]");
        {
            auto p = openPatch();
            p.dds2Range = kDds2RangeLfo;
            p.dds2Mode = Dds2Mode::Norm;   // sub off
            p.mix = 1.0f;
            LayerEngine e; sgt::prepareEngine (e, p);
            auto s = sgt::renderNote (e, 69, 1.0f, 0.5);
            expectLessThan (sgt::rms (s.l, 2400), 1.0e-5f);
        }
    }
};

class Dds1FeatureTests final : public juce::UnitTest
{
public:
    Dds1FeatureTests() : juce::UnitTest ("DDS 1 super mode, morph, alt waves", "osc") {}

    void runTest() override
    {
        beginTest ("SUPER ON spreads six sisters around the centroid [pp.61–62]");
        {
            auto p = openPatch();
            p.dds1Wave = Dds1Wave::Sine;
            p.superMode = Tri::On;
            p.pwDetune = 1.0f;
            LayerEngine e; sgt::prepareEngine (e, p);
            auto s = sgt::renderNote (e, 69, 1.0f, 3.0);
            auto spec = sgt::spectrum (s.l, 4800, 17, sgt::kFs);   // 0.37 Hz bins
            const float c = spec.magAt (440.0, 0.8);
            const double lo = 440.0 * std::pow (2.0, -50.0 / 1200.0);
            const double hi = 440.0 * std::pow (2.0, 0.98 * 50.0 / 1200.0);
            const float dLo = sgt::Spectrum::db (spec.magAt (lo, 0.8)) - sgt::Spectrum::db (c);
            const float dHi = sgt::Spectrum::db (spec.magAt (hi, 0.8)) - sgt::Spectrum::db (c);
            logMessage ("  outer sisters " + juce::String (dLo, 1) + " / " + juce::String (dHi, 1) + " dB vs centroid");
            expectWithinAbsoluteError (dLo, 0.0f, 3.0f, "lowest sister at full level");
            expectWithinAbsoluteError (dHi, 0.0f, 3.0f, "highest sister at full level");
        }

        beginTest ("SUPER OFF: DETUNE has no effect [p.61]");
        {
            auto p = openPatch();
            p.dds1Wave = Dds1Wave::Sine;
            p.superMode = Tri::Off;
            p.pwDetune = 1.0f;
            LayerEngine e; sgt::prepareEngine (e, p);
            auto s = sgt::renderNote (e, 69, 1.0f, 3.0);
            auto spec = sgt::spectrum (s.l, 4800, 17, sgt::kFs);
            const float c = spec.magAt (440.0, 0.8);
            const double lo = 440.0 * std::pow (2.0, -50.0 / 1200.0);
            expectLessThan (sgt::Spectrum::db (spec.magAt (lo, 0.8)) - sgt::Spectrum::db (c), -60.0f);
        }

        beginTest ("PWM/WAVE morphs SINE → SAW [pp.32, 62]");
        {
            auto h2of = [] (float morph)
            {
                auto p = openPatch();
                p.dds1Wave = Dds1Wave::Sine;
                p.pwmWave = morph;
                LayerEngine e; sgt::prepareEngine (e, p);
                auto s = sgt::renderNote (e, 57, 1.0f, 1.0);
                auto spec = sgt::spectrum (s.l, 4800, 15, sgt::kFs);
                return sgt::Spectrum::db (spec.magAt (440.0)) - sgt::Spectrum::db (spec.magAt (220.0));
            };
            const float pure = h2of (0.0f), half = h2of (0.5f), full = h2of (1.0f);
            logMessage ("  2nd harmonic: morph 0 " + juce::String (pure, 1) + " dB, 0.5 " + juce::String (half, 1)
                        + " dB, 1.0 " + juce::String (full, 1) + " dB (saw = -6.0)");
            expectLessThan (pure, -60.0f, "pure sine at A");
            expectWithinAbsoluteError (full, -6.02f, 1.0f, "sawtooth at B");
            expect (half > pure && half < full + 0.5f, "in between");
        }

        beginTest ("ALT plays the channel-A table [p.33]");
        {
            auto p = openPatch();
            p.dds1Wave = Dds1Wave::Alt;
            p.altA = 0;   // "Organ 8+4": harmonics 1, 2, 4 at 1, 0.6, 0.3
            LayerEngine e; sgt::prepareEngine (e, p);
            auto s = sgt::renderNote (e, 57, 1.0f, 1.0);
            auto spec = sgt::spectrum (s.l, 4800, 15, sgt::kFs);
            const float h1 = spec.magAt (220.0), h2 = spec.magAt (440.0), h3 = spec.magAt (660.0), h4 = spec.magAt (880.0);
            expectWithinAbsoluteError (h2 / h1, 0.6f, 0.03f);
            expectWithinAbsoluteError (h4 / h1, 0.3f, 0.03f);
            expectLessThan (h3 / h1, 0.01f);
        }

        beginTest ("ALT A → B morph with PWM/WAVE at max [p.62]");
        {
            auto p = openPatch();
            p.dds1Wave = Dds1Wave::Alt;
            p.altA = 0;       // organ: no 3rd harmonic
            p.altB = 2;       // hollow: odd harmonics only
            p.pwmWave = 1.0f;
            LayerEngine e; sgt::prepareEngine (e, p);
            auto s = sgt::renderNote (e, 57, 1.0f, 1.0);
            auto spec = sgt::spectrum (s.l, 4800, 15, sgt::kFs);
            const float h1 = spec.magAt (220.0), h2 = spec.magAt (440.0), h3 = spec.magAt (660.0);
            expectLessThan (h2 / h1, 0.01f, "even harmonics gone at B");
            expectWithinAbsoluteError (h3 / h1, 1.0f / std::pow (3.0f, 1.5f), 0.02f, "hollow wave at B");
        }

        beginTest ("CUSTOM sample: ROOT KEY, FINE, BINAURAL channels, ONE SHOT");
        {
            // an A4 sine recorded at 44.1 kHz, left channel only
            Sample smp;
            smp.rate = 44100.0f;
            smp.frames = 44100;
            smp.l.resize (44101); smp.r.assign (44101, 0.0f);
            for (int i = 0; i <= smp.frames; ++i) smp.l[static_cast<size_t> (i)] = std::sin (kTwoPi * 440.0f * static_cast<float> (i) / smp.rate);
            auto rms = [] (const std::vector<float>& v, size_t a, size_t b)
            { double s = 0.0; for (size_t i = a; i < b; ++i) s += v[i] * v[i]; return std::sqrt (s / static_cast<double> (b - a)); };

            auto p = openPatch();
            p.smpOn = true; p.smpLoop = true; p.smpRoot = 69;
            {
                LayerEngine e; e.setCustomSample (&smp); sgt::prepareEngine (e, p);
                auto s = sgt::renderNote (e, 72, 1.0f, 0.5);    // C5 from an A4 sample
                expectWithinAbsoluteError (sgt::zeroCrossingHz (s.l, sgt::kFs, 4800, s.l.size()), 523.25, 0.5, "C5");
                expectLessThan (rms (s.r, 4800, s.r.size()), 0.01 * rms (s.l, 4800, s.l.size()), "BINAURAL: right voice plays the silent right channel");
            }
            p.smpFine = 100.0f;
            {
                LayerEngine e; e.setCustomSample (&smp); sgt::prepareEngine (e, p);
                auto s = sgt::renderNote (e, 69, 1.0f, 0.5);
                expectWithinAbsoluteError (sgt::zeroCrossingHz (s.l, sgt::kFs, 4800, s.l.size()), 466.16, 0.5, "FINE +100 ct");
            }
            p.smpFine = 0.0f; p.smpLoop = false; p.smpEnd = 0.1f;   // ONE SHOT of the first 0.1 s
            {
                LayerEngine e; e.setCustomSample (&smp); sgt::prepareEngine (e, p);
                auto s = sgt::renderNote (e, 69, 1.0f, 0.5);
                expectGreaterThan (rms (s.l, 960, 3840), 0.05, "sounding before END");
                expectLessThan (rms (s.l, 9600, s.l.size()), 1.0e-4, "silent after END");
            }
            // De-click: a rising ramp jumps from 1 back to 0 at every wrap. Played an octave up
            // (0.5 s per pass) it loops 3 times in 1.5 s; no output step may stand out.
            Sample ramp;
            ramp.rate = 44100.0f; ramp.frames = 44100;
            ramp.l.resize (44101); ramp.r.resize (44101);
            for (int i = 0; i <= ramp.frames; ++i) ramp.l[static_cast<size_t> (i)] = ramp.r[static_cast<size_t> (i)] = 2.0f * static_cast<float> (i) / 44100.0f - 1.0f;
            auto q = openPatch();
            q.smpOn = true; q.smpLoop = true; q.smpRoot = 57;
            for (float end : { 1.0f, 0.37f })
            {
                q.smpEnd = end;
                LayerEngine e; e.setCustomSample (&ramp); sgt::prepareEngine (e, q);
                auto s = sgt::renderNote (e, 69, 1.0f, 1.5);
                float jump = 0.0f;
                for (size_t i = 4800; i < s.l.size(); ++i) jump = std::max (jump, std::abs (s.l[i] - s.l[i - 1]));
                logMessage ("  largest step, END " + juce::String (end) + ": " + juce::String (jump, 5));
                expectLessThan (jump, 0.003f, "LOOP seam is click-free");
            }

            // LOOP START: an "attack" (220 Hz, first half) plays once, then only the 880 Hz tail loops.
            Sample two;
            two.rate = 44100.0f; two.frames = 44100;
            two.l.resize (44101); two.r.resize (44101);
            for (int i = 0; i <= two.frames; ++i)
                two.l[static_cast<size_t> (i)] = two.r[static_cast<size_t> (i)]
                    = std::sin (kTwoPi * (i < 22050 ? 220.0f : 880.0f) * static_cast<float> (i) / two.rate);
            auto t = openPatch();
            t.smpOn = true; t.smpLoop = true; t.smpRoot = 69; t.smpLoopStart = 0.55f;
            {
                LayerEngine e; e.setCustomSample (&two); sgt::prepareEngine (e, t);
                auto s = sgt::renderNote (e, 69, 1.0f, 3.0);
                expectWithinAbsoluteError (sgt::zeroCrossingHz (s.l, sgt::kFs, 2400, 19200), 220.0, 2.0, "attack plays first");
                expectWithinAbsoluteError (sgt::zeroCrossingHz (s.l, sgt::kFs, 52800, s.l.size()), 880.0, 3.0, "then only the tail loops");
            }
            // LEVEL: 0.4 on the T-dB taper = (0.4 / 0.8)² = -12 dB
            auto levelRms = [&] (float level)
            {
                auto u = t; u.smpLoopStart = 0.0f; u.smpLevel = level;
                LayerEngine e; e.setCustomSample (&smp); sgt::prepareEngine (e, u);
                auto s = sgt::renderNote (e, 69, 1.0f, 0.5);
                return rms (s.l, 4800, s.l.size());
            };
            expectWithinAbsoluteError (levelRms (0.4f) / levelRms (0.8f), 0.25, 0.01, "LEVEL");

            // Unity level, like any sampler: a full-scale sine in the file comes out at full scale (one note, VCA
            // LEVEL 0 dB, BINAURAL: the left voice alone feeds L). The voice's -12 dB is given back.
            {
                auto u = t; u.smpLoopStart = 0.0f; u.smpLevel = 0.8f;
                LayerEngine e; e.setCustomSample (&smp); sgt::prepareEngine (e, u);
                auto s = sgt::renderNote (e, 69, 1.0f, 0.5);
                const double db = 20.0 * std::log10 (rms (s.l, 4800, s.l.size()) / (1.0 / std::sqrt (2.0)));
                logMessage ("  sample level through the voice: " + juce::String (db, 2) + " dB");
                expectWithinAbsoluteError (db, 0.0, 0.5, "a sample plays at its own level");
            }
        }

        beginTest ("CUSTOM SLICE: transients found, one slice per key at its own speed (user, 2026-10-05)");
        {
            // 1 s of silence with four 1 kHz bursts at 0.10, 0.35, 0.60 and 0.85 s, each 0.1 s long
            Sample hits;
            hits.rate = 44100.0f; hits.frames = 44100;
            hits.l.assign (44101, 0.0f); hits.r.assign (44101, 0.0f);
            const float at[4] = { 0.10f, 0.35f, 0.60f, 0.85f };
            for (float t0 : at)
                for (int i = 0; i < 4410; ++i)
                    hits.l[static_cast<size_t> (t0 * 44100.0f) + static_cast<size_t> (i)] = hits.r[static_cast<size_t> (t0 * 44100.0f) + static_cast<size_t> (i)]
                        = 0.8f * std::sin (kTwoPi * 1000.0f * static_cast<float> (i) / 44100.0f);
            hits.findOnsets();
            expectEquals (static_cast<int> (hits.onsetPos.size()), 4, "four transients");
            for (size_t j = 0; j < std::min<size_t> (4, hits.onsetPos.size()); ++j)
                expectWithinAbsoluteError (hits.onsetPos[j] / 44100.0, static_cast<double> (at[j]), 0.012, "transient " + juce::String (static_cast<int> (j)));

            auto rms = [] (const std::vector<float>& v, size_t a, size_t b)
            { double s = 0.0; for (size_t i = a; i < b; ++i) s += v[i] * v[i]; return std::sqrt (s / static_cast<double> (b - a)); };
            auto p = openPatch();
            p.smpOn = true; p.smpSlice = true; p.smpRoot = 60; p.smpSense = 0.5f;
            auto play = [&] (int note) { LayerEngine e; e.setCustomSample (&hits); sgt::prepareEngine (e, p); return sgt::renderNote (e, note, 1.0f, 0.3); };
            const size_t ms = static_cast<size_t> (sgt::kFs / 1000.0);
            // AUTO: C4 = the lead-in before the first hit (silence), C#4 = the first hit, then one key per hit
            expectLessThan (rms (play (60).l, 0, 90 * ms), 1.0e-3, "C4: the silent lead-in");
            const auto s1 = play (61);
            expectGreaterThan (rms (s1.l, 10 * ms, 90 * ms), 0.05, "C#4: the first hit, at once");
            expectLessThan (rms (s1.l, 160 * ms, 240 * ms), 1.0e-3, "C#4 ends where the next hit starts");
            expectWithinAbsoluteError (sgt::zeroCrossingHz (s1.l, sgt::kFs, 10 * ms, 90 * ms), 1000.0, 15.0, "a slice plays at its own speed");
            expectGreaterThan (rms (play (64).l, 10 * ms, 90 * ms), 0.05, "E4: the fourth hit");
            expectLessThan (rms (play (66).l, 0, 250 * ms), 1.0e-3, "past the last slice: silent");
            expectLessThan (rms (play (59).l, 0, 250 * ms), 1.0e-3, "below ROOT KEY: silent");
            // 4 equal slices: D4 is 0.50 - 0.75 s, whose hit starts 0.1 s in
            p.smpSlices = 4;
            const auto q = play (62);
            expectLessThan (rms (q.l, 0, 90 * ms), 1.0e-3, "equal slice: silent before its hit");
            expectGreaterThan (rms (q.l, 110 * ms, 190 * ms), 0.05, "equal slice: then the hit");
        }

        beginTest ("CUSTOM overview of a long file (the 2026-10-05 crash: points * frames overflowed int)");
        {
            // 95 s at 48 kHz: 60 Hz for the first half, 8 kHz for the second
            Sample song;
            song.rate = 48000.0f; song.frames = 95 * 48000;
            song.l.resize (static_cast<size_t> (song.frames) + 1); song.r.resize (static_cast<size_t> (song.frames) + 1);
            for (int i = 0; i <= song.frames; ++i)
                song.l[static_cast<size_t> (i)] = song.r[static_cast<size_t> (i)] = 0.5f * std::sin (kTwoPi * (i < song.frames / 2 ? 60.0f : 8000.0f) * static_cast<float> (i) / 48000.0f);
            const auto ov = song.overview (1024);
            float top = 0.0f; bool finite = true;
            for (size_t p = 0; p < 1024; ++p) { top = std::max (top, ov.hi[p]); finite = finite && std::isfinite (ov.lo[p]) && std::isfinite (ov.rms[p]); }
            expect (finite, "every column is a number");
            expectWithinAbsoluteError (top, 1.0f, 0.01f, "peaks scaled to the loudest");
            expectGreaterThan (ov.low[100], 0.8f, "the 60 Hz half reads as lows");
            expectGreaterThan (ov.high[900], 0.8f, "the 8 kHz half reads as highs");
            expectWithinAbsoluteError (ov.rms[900], 0.707f, 0.03f, "RMS of a sine against its peak");
        }

        beginTest ("DDS 2 pulse width follows PW and PWM [p.62]");
        {
            auto duty = [] (float pwBase, float m)
            {
                Dds2 d; d.prepare (3);
                d.setup (Dds2Wave::Pulse, 0.001f, 0.5f - 0.45f * clampf (pwBase + m, 0.0f, 1.0f));
                int pos = 0;
                for (int i = 0; i < 100000; ++i) if (d.tick (1.0f, -1.0f) > 0.0f) ++pos;
                return pos / 100000.0f;
            };
            expectWithinAbsoluteError (duty (0.0f, 0.0f), 0.5f, 0.01f);
            expectWithinAbsoluteError (duty (0.5f, 0.0f), 0.275f, 0.01f);
            expectWithinAbsoluteError (duty (0.2f, 0.3f), 0.275f, 0.01f);
        }
    }
};

static PitchTests pitchTests;
static AliasTests aliasTests;
static Dds2ModeTests dds2ModeTests;
static Dds1FeatureTests dds1FeatureTests;
