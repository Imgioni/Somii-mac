// VCF: SSI-style 4-pole ladder, 1-pole HPF, keytracking, drive, stability [manual pp.41–43].

#include "TestHelpers.h"

#include "core/Filters.h"

#include <functional>

using namespace sg;

namespace
{
constexpr double kOs = 96000.0;

std::vector<float> runLadder (SsiLadder& f, int n, const std::function<float (int)>& input)
{
    std::vector<float> y (static_cast<size_t> (n));
    for (int i = 0; i < n; ++i) y[static_cast<size_t> (i)] = f.process (input (i));
    return y;
}

std::vector<float> sine (double hz, double fs, int n, float amp)
{
    std::vector<float> x (static_cast<size_t> (n));
    for (int i = 0; i < n; ++i) x[static_cast<size_t> (i)] = amp * static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * hz * i / fs));
    return x;
}
} // namespace

class LadderTests final : public juce::UnitTest
{
public:
    LadderTests() : juce::UnitTest ("LPF ladder", "filter") {}

    void runTest() override
    {
        beginTest ("Self-oscillates at the cutoff frequency at max RES [p.42]");
        for (double fc : { 440.0, 1000.0, 4000.0 })
        {
            SsiLadder f;
            f.setResonance (1.0f, Drive::Off);
            f.setCutoff (static_cast<float> (fc), static_cast<float> (kOs));
            auto y = runLadder (f, static_cast<int> (kOs * 1.5), [] (int i) { return i == 0 ? 1.0f : 0.0f; });
            const double hz = sgt::zeroCrossingHz (y, kOs, static_cast<size_t> (kOs), y.size());
            const float amp = sgt::peakAbs (y, static_cast<size_t> (kOs), y.size());
            logMessage ("  fc " + juce::String (fc) + " → " + juce::String (hz, 1) + " Hz, amplitude " + juce::String (amp, 2));
            expectWithinAbsoluteError (hz, fc, fc * 0.03, "self-oscillation pitch");
            expectGreaterThan (amp, 0.05f, "oscillation is sustained");
            expectLessThan (amp, 4.0f, "oscillation is bounded");
        }

        beginTest ("Self-oscillation is a clean sine");
        {
            SsiLadder f;
            f.setResonance (1.0f, Drive::Off);
            f.setCutoff (1000.0f, static_cast<float> (kOs));
            auto y = runLadder (f, static_cast<int> (kOs * 2), [] (int i) { return i == 0 ? 1.0f : 0.0f; });
            auto spec = sgt::spectrum (y, static_cast<size_t> (kOs), 16, kOs);
            const double f0 = spec.peakHz (500.0, 2000.0);
            const float h1 = spec.magAt (f0), h3 = spec.magAt (3.0 * f0);
            logMessage ("  3rd harmonic " + juce::String (sgt::Spectrum::db (h3) - sgt::Spectrum::db (h1), 1) + " dB");
            expectLessThan (sgt::Spectrum::db (h3) - sgt::Spectrum::db (h1), -30.0f);
        }

        beginTest ("24 dB/oct slope, unity passband at RES 0 [p.41]");
        {
            SsiLadder f;
            f.setResonance (0.0f, Drive::Off);
            f.setCutoff (500.0f, static_cast<float> (kOs));
            const int n = static_cast<int> (kOs);
            auto lo = sine (50.0, kOs, n, 0.5f);
            auto yLo = runLadder (f, n, [&] (int i) { return lo[static_cast<size_t> (i)]; });
            const double gLo = sgt::toneAmplitude (yLo, static_cast<size_t> (n / 4), static_cast<size_t> (n), 50.0, kOs) / 0.5;
            f.reset();
            auto hi = sine (4000.0, kOs, n, 0.5f);
            auto yHi = runLadder (f, n, [&] (int i) { return hi[static_cast<size_t> (i)]; });
            const double gHi = sgt::toneAmplitude (yHi, static_cast<size_t> (n / 4), static_cast<size_t> (n), 4000.0, kOs) / 0.5;
            logMessage ("  50 Hz " + juce::String (20 * std::log10 (gLo), 2) + " dB, 4 kHz (3 oct above) "
                        + juce::String (20 * std::log10 (gHi), 1) + " dB");
            expectWithinAbsoluteError (gLo, 1.0, 0.06);
            expectLessThan (20.0 * std::log10 (gHi), -60.0);
        }

        beginTest ("DRIVE 1 compensates the resonance level loss [p.42]");
        {
            auto passband = [&] (Drive d, float res)
            {
                SsiLadder f;
                f.setResonance (res, d);
                f.setCutoff (5000.0f, static_cast<float> (kOs));
                const int n = static_cast<int> (kOs);
                auto x = sine (100.0, kOs, n, 0.3f);
                auto y = runLadder (f, n, [&] (int i) { return x[static_cast<size_t> (i)]; });
                return 20.0 * std::log10 (sgt::toneAmplitude (y, static_cast<size_t> (n / 4), static_cast<size_t> (n), 100.0, kOs) / 0.3);
            };
            const double offLoss = passband (Drive::Off, 0.0f) - passband (Drive::Off, 0.9f);
            const double oneLoss = passband (Drive::One, 0.0f) - passband (Drive::One, 0.9f);
            logMessage ("  bass loss at RES 9: DRIVE OFF " + juce::String (offLoss, 1) + " dB, DRIVE 1 " + juce::String (oneLoss, 1) + " dB");
            expectGreaterThan (offLoss, 8.0, "clean mode loses bass with resonance (classic ladder behaviour)");
            expectLessThan (std::abs (oneLoss), 3.0, "DRIVE 1 compensates");
        }

        beginTest ("DRIVE 2 adds harmonic distortion [p.42]");
        {
            auto thd = [&] (Drive d)
            {
                SsiLadder f;
                f.setResonance (0.0f, d);
                f.setCutoff (20000.0f, static_cast<float> (kOs));
                const int n = static_cast<int> (kOs);
                auto x = sine (200.0, kOs, n, 0.8f);
                auto y = runLadder (f, n, [&] (int i) { return x[static_cast<size_t> (i)]; });
                const double h1 = sgt::toneAmplitude (y, static_cast<size_t> (n / 4), static_cast<size_t> (n), 200.0, kOs);
                const double h3 = sgt::toneAmplitude (y, static_cast<size_t> (n / 4), static_cast<size_t> (n), 600.0, kOs);
                return 20.0 * std::log10 (h3 / h1);
            };
            const double clean = thd (Drive::Off), heavy = thd (Drive::Two);
            logMessage ("  3rd harmonic: OFF " + juce::String (clean, 1) + " dB, DRIVE 2 " + juce::String (heavy, 1) + " dB");
            expectLessThan (clean, -35.0);
            expectGreaterThan (heavy, -20.0);
        }
    }
};

class HpfTests final : public juce::UnitTest
{
public:
    HpfTests() : juce::UnitTest ("HPF 6 dB/oct", "filter") {}

    void runTest() override
    {
        beginTest ("−3 dB at cutoff, 6 dB/oct below [p.41]");
        auto gainAt = [] (double hz)
        {
            OnePoleHpf h;
            h.setCutoff (1000.0f, static_cast<float> (kOs));
            const int n = static_cast<int> (kOs);
            auto x = sine (hz, kOs, n, 1.0f);
            std::vector<float> y (static_cast<size_t> (n));
            for (int i = 0; i < n; ++i) y[static_cast<size_t> (i)] = h.process (x[static_cast<size_t> (i)]);
            return 20.0 * std::log10 (sgt::toneAmplitude (y, static_cast<size_t> (n / 4), static_cast<size_t> (n), hz, kOs));
        };
        expectWithinAbsoluteError (gainAt (1000.0), -3.01, 0.2);
        expectWithinAbsoluteError (gainAt (125.0), -18.2, 0.6);
        expectWithinAbsoluteError (gainAt (10000.0), 0.0, 0.1);
    }
};

class KeytrackTests final : public juce::UnitTest
{
public:
    KeytrackTests() : juce::UnitTest ("VCF keytrack through the voice", "filter") {}

    void runTest() override
    {
        // Oscillators silenced (DDS 2 in LFO range, sub off, MIX fully right) apart from a whisper of
        // DDS 1 noise to excite the self-oscillating filter; the filter becomes the sound source.
        auto selfOscHz = [] (Tri kt, int note)
        {
            LayerParams p;
            p.dds1Wave = Dds1Wave::Noise;
            p.dds2Range = kDds2RangeLfo;
            p.dds2Mode = Dds2Mode::Norm;
            p.mix = 0.995f;
            p.res = 1.0f;
            p.lpf = 0.6f;
            p.vcfKeytrack = kt;
            LayerEngine e;
            sgt::prepareEngine (e, p);
            auto s = sgt::renderNote (e, note, 1.0f, 1.6);
            auto spec = sgt::spectrum (s.l, 9600, 16, sgt::kFs);
            return spec.peakHz (60.0, 20000.0);
        };

        beginTest ("ON: filter follows in semitones [p.43]");
        {
            const double a = selfOscHz (Tri::On, 60), b = selfOscHz (Tri::On, 72);
            logMessage ("  C4 " + juce::String (a, 1) + " Hz, C5 " + juce::String (b, 1) + " Hz");
            expectWithinAbsoluteError (b / a, 2.0, 0.06);
        }
        beginTest ("½: quarter-tone steps (half tracking) [p.43]");
        {
            const double a = selfOscHz (Tri::Half, 60), b = selfOscHz (Tri::Half, 72);
            expectWithinAbsoluteError (b / a, std::sqrt (2.0), 0.05);
        }
        beginTest ("OFF: no tracking");
        {
            const double a = selfOscHz (Tri::Off, 60), b = selfOscHz (Tri::Off, 72);
            expectWithinAbsoluteError (b / a, 1.0, 0.03);
        }
    }
};

class StabilityTests final : public juce::UnitTest
{
public:
    StabilityTests() : juce::UnitTest ("Random patches stay finite and bounded", "filter") {}

    void runTest() override
    {
        beginTest ("150 random patches × chords");
        Rng rng;
        rng.seed (4242);
        auto u = [&] { return rng.unipolar(); };
        auto pick = [&] (int n) { return static_cast<int> (rng.next() % static_cast<uint32_t> (n)); };

        bool allOk = true;
        float worstPeak = 0.0f;
        for (int trial = 0; trial < 150; ++trial)
        {
            LayerParams p;
            p.dds1Wave = static_cast<Dds1Wave> (pick (6));
            p.dds1Range = pick (6);
            p.altA = pick (32); p.altB = pick (32);
            p.dds2Wave = static_cast<Dds2Wave> (pick (6));
            p.dds2Range = pick (6);
            p.dds2Tune = 14.0f * u() - 7.0f;
            p.dds2Mode = static_cast<Dds2Mode> (pick (3));
            p.mix = u();
            p.drive = static_cast<Drive> (pick (3));
            p.hpf = u() * u(); p.lpf = u(); p.res = u();
            p.envSource = static_cast<EnvSource> (pick (3));
            p.vcfKeytrack = static_cast<Tri> (pick (3));
            p.vcfEnvAmt = u(); p.vcfLfo1Amt = u(); p.vcfDds2Amt = u();
            p.vcaLevel = u(); p.vcaLfo1Amt = u(); p.vcaDds2Amt = u();
            p.vcaEnv = static_cast<VcaEnv> (pick (3));
            p.dynamics = static_cast<Tri> (pick (3));
            p.e1AttackHold = u() * 0.3f; p.e1Attack = u() * 0.6f; p.e1DecayHold = u() * 0.3f;
            p.e1Decay = u() * 0.7f; p.e1Sustain = u(); p.e1Release = u() * 0.6f;
            p.e1Mode = static_cast<Env1Mode> (pick (3));
            p.e1Keytrack = static_cast<Tri> (pick (3));
            p.e2Attack = u() * 0.6f; p.e2Decay = u() * 0.7f; p.e2Sustain = u(); p.e2Release = u() * 0.6f;
            p.lfo1Wave = static_cast<Lfo1Wave> (pick (6));
            p.lfo1Rate = u(); p.lfo1Delay = u() * 0.3f; p.lfo1LrPhase = u();
            p.lfo1Mode = static_cast<Lfo1Mode> (pick (3));
            p.pitchLfo1Amt = u(); p.pitchEnv1Amt = u();
            p.pitchDest = static_cast<OscDest> (pick (3));
            p.superMode = static_cast<Tri> (pick (3));
            p.pwDetune = u(); p.drift = u(); p.pwmWave = u();
            p.pwmSource = static_cast<PwmSource> (pick (3));
            p.crossMod = u();

            LayerEngine e;
            e.setAnalogTolerance (true);
            e.prepare (sgt::kFs, 512, 2);
            e.setParams (p);
            std::vector<float> l (4800), r (4800);
            for (int note : { 36, 60, 64, 67, 96 }) e.noteOn (note, u());
            for (int block = 0; block < 6; ++block)
            {
                e.process (l.data(), r.data(), 4800);
                if (block == 3) e.allNotesOff (false);
                allOk = allOk && sgt::allFinite (l) && sgt::allFinite (r);
                worstPeak = std::max ({ worstPeak, sgt::peakAbs (l), sgt::peakAbs (r) });
            }
        }
        logMessage ("  worst peak across all patches: " + juce::String (worstPeak, 2));
        expect (allOk, "no NaN / Inf");
        expectLessThan (worstPeak, 16.0f, "bounded output");
    }
};

static LadderTests ladderTests;
static HpfTests hpfTests;
static KeytrackTests keytrackTests;
static StabilityTests stabilityTests;
