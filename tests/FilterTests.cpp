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

class FilterEngineTests final : public juce::UnitTest
{
public:
    FilterEngineTests() : juce::UnitTest ("VCF STYLE: two separate engines", "filter") {}

    static sg::LayerParams patch (sg::VcfStyle style)
    {
        sg::LayerParams p;
        p.vcfStyle = style;
        p.lpf = 0.45f;
        p.res = 0.6f;
        p.twCutoff = p.lpf;
        p.twRes = p.res;
        p.e2Release = 0.0398f;   // 16 ms
        return p;
    }
    static double rms (const std::vector<float>& v, size_t from)
    {
        double s2 = 0.0;
        for (size_t i = from; i < v.size(); ++i) s2 += static_cast<double> (v[i]) * v[i];
        return std::sqrt (s2 / std::max<size_t> (1, v.size() - from));
    }
    static std::vector<float> render (const sg::LayerParams& p)
    {
        sg::LayerEngine e; sgt::prepareEngine (e, p);
        return sgt::renderNote (e, 45, 1.0f, 1.0).l;
    }

    void runTest() override
    {
        beginTest ("Super Gemini Drive 1 matches Off with no resonance [UDO clarification]");
        {
            SsiLadder off, one;
            off.setCutoff (4000.0f, 96000.0f); one.setCutoff (4000.0f, 96000.0f);
            off.setResonance (0.0f, Drive::Off); one.setResonance (0.0f, Drive::One);
            float difference = 0.0f;
            for (int i = 0; i < 9600; ++i)
            {
                const float x = 0.8f * std::sin (kTwoPi * 220.0f * i / 96000.0f);
                difference = std::max (difference, std::abs (off.process (x) - one.process (x)));
            }
            expectLessThan (difference, 1.0e-7f);
        }
        beginTest ("Independent bipolar LPF and SVF envelope controls and keyboard tracking");
        {
            const auto& bank = AltWaveBank::factory();
            LayerParams p;
            LayerControl c;
            p.vcfStyle = VcfStyle::ThirdWave;
            p.twEnv = 0.0f; p.svfEnv = 1.0f; p.twKey = 0.5f; p.svfKey = 1.0f;
            c.update (p, bank);
            expectWithinAbsoluteError (c.vcfEnvOct, -10.0f, 0.001f);
            expectWithinAbsoluteError (c.svfEnvOct, 10.0f, 0.001f);
            expectWithinAbsoluteError (c.keytrackK, 1.0f, 0.001f);
            expectWithinAbsoluteError (c.svfKeyK, 2.0f, 0.001f);
        }
        beginTest ("The two styles are different filters, not one with different settings");
        {
            auto sg1 = render (patch (sg::VcfStyle::Sg));
            auto tw = render (patch (sg::VcfStyle::ThirdWave));
            auto specSg = sgt::spectrum (sg1, 4800, 14, sgt::kFs);
            auto specTw = sgt::spectrum (tw, 4800, 14, sgt::kFs);
            // same note, same cutoff and resonance: compare the harmonic series each one passes
            double diff = 0.0;
            for (int h = 1; h <= 12; ++h)
            {
                const double f0 = 110.0 * h;
                const double a = sgt::Spectrum::db (specSg.magAt (f0)), b = sgt::Spectrum::db (specTw.magAt (f0));
                diff = std::max (diff, std::abs (a - b));
            }
            logMessage ("  biggest harmonic difference between the engines: " + juce::String (diff, 2) + " dB");
            expectGreaterThan (diff, 3.0, "the engines shape the sound differently");
        }

        beginTest ("Each pole saturates (OTA), unlike the ladder's single input stage");
        {
            // drive both filters hard with the same tone and compare where the harmonics land
            auto harmonics = [] (bool curtis)
            {
                std::vector<float> out (static_cast<size_t> (sgt::kFs));
                CurtisFilter c; SsiLadder l;
                c.reset(); c.setCutoff (4000.0f, sgt::kFs); c.setResonance (0.3f); c.setSaturation (0.0f);
                l.reset(); l.setCutoff (4000.0f, sgt::kFs); l.setResonance (0.3f, Drive::Off);
                for (size_t i = 0; i < out.size(); ++i)
                {
                    const float x = 2.0f * std::sin (kTwoPi * 220.0f * static_cast<float> (i) / sgt::kFs);
                    out[i] = curtis ? c.process (x) : l.process (x);
                }
                auto sp = sgt::spectrum (out, static_cast<int> (out.size() / 2), 14, sgt::kFs);
                const float f1 = sp.magAt (220.0);
                return std::make_pair (sp.magAt (660.0) / std::max (1.0e-9f, f1), sp.magAt (1100.0) / std::max (1.0e-9f, f1));
            };
            const auto cur = harmonics (true), lad = harmonics (false);
            logMessage ("  driven hard - 3rd/5th harmonic: Curtis " + juce::String (cur.first, 4) + "/" + juce::String (cur.second, 4)
                        + "   ladder " + juce::String (lad.first, 4) + "/" + juce::String (lad.second, 4));
            // the ladder bends once at its input, so it colours earlier; the OTA stages bend
            // gently inside each pole. Either way the two must not land in the same place.
            const float ratio = std::max (cur.first, lad.first) / std::max (1.0e-9f, std::min (cur.first, lad.first));
            expectGreaterThan (ratio, 2.0f, "the two topologies distort differently");
        }

        beginTest ("Only the selected engine is in the path");
        {
            // SG style: the 3rd Wave controls must do nothing at all
            auto a = patch (sg::VcfStyle::Sg);
            auto b = a; b.vcfSat = 1.0f; b.svfOn = true; b.svfCutoff = 0.2f; b.svfRes = 0.9f; b.svfModeMix = 1.0f;
            b.vcfVelocity = 1.0f; b.twCutoff = 0.1f; b.twRes = 1.0f; b.twEnv = 0.0f; b.twKey = 1.0f;
            const auto ra = render (a), rb = render (b);
            double worst = 0.0;
            for (size_t i = 0; i < ra.size(); ++i) worst = std::max (worst, std::abs (static_cast<double> (ra[i]) - rb[i]));
            logMessage ("  SG style, 3rd Wave controls moved: largest sample difference " + juce::String (worst, 9));
            expectLessThan (worst, 1.0e-9, "SATURATION and the state-variable filter are out of the SG path");

            // 3W style: the Super Gemini's DRIVE and HPF must do nothing
            auto c = patch (sg::VcfStyle::ThirdWave);
            auto d = c; d.drive = sg::Drive::Two; d.hpf = 0.8f;
            d.lpf = 0.1f; d.res = 0.95f; d.vcfEnvAmt = 1.0f;
            d.vcfKeytrack = sg::Tri::On; d.envSource = sg::EnvSource::Both;
            d.vcfLfo1Amt = 1.0f; d.vcfDds2Amt = 1.0f;
            const auto rc = render (c), rd = render (d);
            double worst2 = 0.0;
            for (size_t i = 0; i < rc.size(); ++i) worst2 = std::max (worst2, std::abs (static_cast<double> (rc[i]) - rd[i]));
            logMessage ("  3W style, DRIVE and HPF moved: largest sample difference " + juce::String (worst2, 9));
            expectLessThan (worst2, 1.0e-9, "DRIVE and the HPF are out of the 3rd Wave path");
        }
    }
};

class ThirdWaveFilterTests final : public juce::UnitTest
{
public:
    ThirdWaveFilterTests() : juce::UnitTest ("3rd Wave filter style", "filter") {}

    // level of a 100 Hz tone through a filter, well below any cutoff setting used here
    static double bassLevel (std::function<float (float)> process, double hz = 100.0)
    {
        double sum = 0.0;
        const int n = static_cast<int> (sgt::kFs);
        for (int i = 0; i < n; ++i)
        {
            const float x = 0.3f * std::sin (kTwoPi * static_cast<float> (hz) * i / sgt::kFs);
            const float y = process (x);
            if (i > n / 2) sum += static_cast<double> (y) * y;
        }
        return std::sqrt (sum / (n / 2));
    }

    void runTest() override
    {
        beginTest ("3W output stays finite across cutoff, resonance, saturation and sample rates");
        for (float rate : { 44100.0f, 48000.0f, 96000.0f })
            for (float cutoff : { 20.0f, 1000.0f, 18000.0f })
                for (bool compensation : { false, true })
                {
                    CurtisFilter filter;
                    filter.setCutoff (cutoff, rate);
                    filter.setResonance (1.0f, compensation);
                    filter.setSaturation (1.0f);
                    bool bounded = true;
                    for (int i = 0; i < 12000; ++i)
                    {
                        const float y = filter.process (i < 6000 ? 0.8f * std::sin (kTwoPi * 220.0f * i / rate) : 0.0f);
                        bounded = bounded && std::isfinite (y) && std::abs (y) < 2.0f;
                    }
                    expect (bounded);
                }
        beginTest ("Resonance compensation preserves bass [3rd Wave v1.9 p.57]");
        {
            auto sgBass = [] (float res)
            {
                SsiLadder f; f.reset(); f.setCutoff (1000.0f, sgt::kFs); f.setResonance (res, Drive::Off);
                return bassLevel ([&f] (float x) { return f.process (x); });
            };
            auto twBass = [] (float res)
            {
                CurtisFilter f; f.reset(); f.setCutoff (1000.0f, sgt::kFs); f.setResonance (res); f.setSaturation (0.0f);
                return bassLevel ([&f] (float x) { return f.process (x); });
            };
            const double sgLoss = sgBass (0.9) / sgBass (0.0), twLoss = twBass (0.9) / twBass (0.0);
            logMessage ("  bass at resonance 0.9 vs 0: SG " + juce::String (sgLoss, 3) + "  3W " + juce::String (twLoss, 3));
            expectLessThan (sgLoss, 0.8, "the Super Gemini ladder thins out, as a 24 dB/oct filter does");
            expectGreaterThan (twLoss, 0.85, "3W compensates, so the low end stays");
        }

        beginTest ("SATURATION raises level into output distortion [3rd Wave v1.9 p.24]");
        {
            auto run = [] (float sat, double& rms, double& thd)
            {
                CurtisFilter f; f.reset(); f.setCutoff (18000.0f, sgt::kFs); f.setResonance (0.0f); f.setSaturation (sat);
                std::vector<float> out (static_cast<size_t> (sgt::kFs));
                for (size_t i = 0; i < out.size(); ++i)
                    out[i] = f.process (0.5f * std::sin (kTwoPi * 220.0f * static_cast<float> (i) / sgt::kFs));
                double sum = 0.0;
                for (size_t i = out.size() / 2; i < out.size(); ++i) sum += static_cast<double> (out[i]) * out[i];
                rms = std::sqrt (sum / (out.size() / 2));
                auto spec = sgt::spectrum (out, static_cast<int> (out.size() / 2), 14, sgt::kFs);
                const float f1 = spec.magAt (220.0), h3 = spec.magAt (660.0), h5 = spec.magAt (1100.0);
                thd = (h3 + h5) / std::max (1.0e-9f, f1);
            };
            double rms0 = 0.0, thd0 = 0.0, rms1 = 0.0, thd1 = 0.0;
            run (0.0f, rms0, thd0);
            run (1.0f, rms1, thd1);
            const double db = 20.0 * std::log10 (rms1 / rms0);
            logMessage ("  saturation 0 -> 1: level " + juce::String (db, 2) + " dB, harmonics "
                        + juce::String (thd0, 4) + " -> " + juce::String (thd1, 4));
            expectGreaterThan (thd1, thd0 * 20.0, "saturation adds harmonic distortion");
            expectGreaterThan (db, 3.0, "output saturation must not be automatically level compensated");
            double rmsMid = 0.0, thdMid = 0.0;
            run (0.2f, rmsMid, thdMid);
            expectGreaterThan (rmsMid, rms0, "initial saturation travel raises volume");
        }

        beginTest ("State-variable filter: low-pass / notch / high-pass, and no self-oscillation");
        {
            auto level = [] (float mode, bool band, double hz)
            {
                StateVariable f; f.reset(); f.setCutoff (1000.0f, sgt::kFs); f.setResonance (0.5f); f.setMode (mode, band);
                double sum = 0.0;
                const int n = static_cast<int> (sgt::kFs);
                for (int i = 0; i < n; ++i)
                {
                    const float y = f.process (0.3f * std::sin (kTwoPi * static_cast<float> (hz) * i / sgt::kFs));
                    if (i > n / 2) sum += static_cast<double> (y) * y;
                }
                return std::sqrt (sum / (n / 2));
            };
            const double lpLow = level (0.0f, false, 100.0), lpHigh = level (0.0f, false, 8000.0);
            const double hpLow = level (1.0f, false, 100.0), hpHigh = level (1.0f, false, 8000.0);
            const double notchAt = level (0.5f, false, 1000.0), notchLow = level (0.5f, false, 100.0);
            const double bpAt = level (0.0f, true, 1000.0), bpLow = level (0.0f, true, 100.0);
            logMessage ("  LP 100/8k " + juce::String (lpLow, 3) + "/" + juce::String (lpHigh, 4)
                        + "   HP " + juce::String (hpLow, 4) + "/" + juce::String (hpHigh, 3)
                        + "   notch 1k/100 " + juce::String (notchAt, 4) + "/" + juce::String (notchLow, 3)
                        + "   BP 1k/100 " + juce::String (bpAt, 3) + "/" + juce::String (bpLow, 4));
            expectGreaterThan (lpLow, lpHigh * 10.0, "low-pass passes the low tone");
            expectGreaterThan (hpHigh, hpLow * 10.0, "high-pass passes the high tone");
            expectLessThan (notchAt, notchLow * 0.5, "notch cuts at the cutoff");
            expectGreaterThan (bpAt, bpLow * 5.0, "band pass favours the cutoff");

            // resonance must not run away: the filter cannot self-oscillate [3rd Wave p.53]
            StateVariable osc; osc.reset(); osc.setCutoff (1000.0f, sgt::kFs); osc.setResonance (1.0f); osc.setMode (0.0f, false);
            float peak = 0.0f;
            for (int i = 0; i < 48000; ++i)
            {
                const float x = i < 64 ? 1.0f : 0.0f;     // a single impulse, then silence
                peak = std::max (peak, std::abs (osc.process (x)));
            }
            float tail = 0.0f;
            for (int i = 0; i < 4800; ++i) tail = std::max (tail, std::abs (osc.process (0.0f)));
            logMessage ("  max resonance: peak " + juce::String (peak, 3) + ", tail after 1 s " + juce::String (tail, 6));
            expectLessThan (tail, 0.01f, "rings out instead of self-oscillating");
        }
    }
};

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

static FilterEngineTests filterEngineTests;
static ThirdWaveFilterTests thirdWaveFilterTests;
static LadderTests ladderTests;
static HpfTests hpfTests;
static KeytrackTests keytrackTests;
static StabilityTests stabilityTests;
