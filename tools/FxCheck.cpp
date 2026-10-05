// SGFxCheck: runs every FX-rack effect on a test signal at its default, minimum and maximum
// settings and fails on NaN/inf, runaway output, or an effect that produces nothing.
// Also prints each effect's CPU at its defaults, as a percentage of one core in real time.
// Usage: SGFxCheck.exe [block size, default 256]   (exit code 0 = all passed)

#include "plugin/FxRack.h"

#include <chrono>
#include <cstdlib>
#include <cstdio>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    constexpr double fs = 48000.0;
    const int block = argc > 1 ? std::max (16, std::atoi (argv[1])) : 256, blocks = static_cast<int> (4.0 * fs / block);

    fx::ImpulseResponse ir;                       // a decaying noise burst as long as the default hall (2.2 s)
    constexpr int irLen = static_cast<int> (2.2 * fs);
    ir.buffer.setSize (2, irLen);
    juce::Random rnd (7);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < irLen; ++i) ir.buffer.setSample (c, i, (rnd.nextFloat() * 2 - 1) * std::exp (-i / 24000.0f));
    ir.name = "TEST";

    // These start level: at their defaults they may not change the sound (user, 2026-10-05). Measured on
    // magnitude spectra, so the harmless phase shift of crossovers and oversampling does not count.
    const auto mustBeNeutral = [] (int t)
    {
        using namespace fxdefs;
        return t == Tuba || t == Saturn || t == Distortion || t == Bitcrusher || t == Vulf || t == Faraday || t == Mbcomp
            || t == Stereopan || t == Proq || t == Filter || t == Imager || t == Nudestort;
    };
    const auto spectrumDiffDb = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        constexpr int order = 15, len = 1 << order;
        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> win (len, juce::dsp::WindowingFunction<float>::hann, false);
        std::vector<float> fa (2 * len), fb (2 * len);
        std::copy (a.begin() + 48000, a.begin() + 48000 + len, fa.begin());
        std::copy (b.begin() + 48000, b.begin() + 48000 + len, fb.begin());
        win.multiplyWithWindowingTable (fa.data(), len); win.multiplyWithWindowingTable (fb.data(), len);
        fft.performFrequencyOnlyForwardTransform (fa.data()); fft.performFrequencyOnlyForwardTransform (fb.data());
        double d = 0.0, e = 0.0;
        for (int i = 1; i < len / 2; ++i) { d += (fb[(size_t) i] - fa[(size_t) i]) * (fb[(size_t) i] - fa[(size_t) i]); e += fa[(size_t) i] * fa[(size_t) i]; }
        return 10.0 * std::log10 ((d + 1.0e-20) / (e + 1.0e-20));
    };

    int failures = 0;
    for (int t = 1; t < fxdefs::kNumTypes; ++t)
        for (int setting = 0; setting < 3; ++setting)   // defaults, all minimum, all maximum
        {
            float v[fxdefs::kNP];
            for (int i = 0; i < fxdefs::kNP; ++i)
            {
                const float n = setting == 0 ? fxdefs::kParams[t][i].def : setting == 1 ? 0.0f : 1.0f;
                v[i] = fxdefs::value (t, i, n);
            }
            if (t == fxdefs::Proq) v[26] = 63.0f;   // every EQ band on (they start off)
            auto u = fx::makeUnit (t);
            u->prepare (fs, block);
            u->reset();
            u->messageTick (v, ir, true);
            juce::Thread::sleep (300);               // convolution loads its impulse in the background
            std::vector<float> l (block), r (block);
            fx::Ctx c { v, 120.0, fs };
            double energy = 0.0;
            float peak = 0.0f, tail = 0.0f;   // tail: the silent last 0.5 s
            bool finite = true;
            double busy = 0.0;   // seconds spent in process()
            double diffE = 0.0, inE = 0.0;   // how far the output is from the input (the null)
            std::vector<float> dl (static_cast<size_t> (block)), dr (static_cast<size_t> (block)), inRec, outRec;
            for (int b = 0; b < blocks; ++b)
            {
                for (int i = 0; i < block; ++i)
                {
                    const int n = b * block + i;
                    // a 0.5 s chord burst every second, plus a little noise
                    const float env = (n % 48000) < 24000 ? 0.5f : 0.0f;
                    l[static_cast<size_t> (i)] = env * (std::sin (n * 0.0359f) + 0.5f * std::sin (n * 0.0571f)) * 0.6f + (rnd.nextFloat() - 0.5f) * 0.01f;
                    r[static_cast<size_t> (i)] = env * (std::sin (n * 0.0377f) + 0.5f * std::sin (n * 0.0449f)) * 0.6f + (rnd.nextFloat() - 0.5f) * 0.01f;
                }
                dl = l; dr = r;
                const auto t0 = std::chrono::steady_clock::now();
                u->process (l.data(), r.data(), block, c);
                busy += std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
                if (b % 40 == 0) u->messageTick (v, ir, false);
                for (int i = 0; i < block; ++i)
                {
                    const float a = l[static_cast<size_t> (i)], d = r[static_cast<size_t> (i)];
                    if (! std::isfinite (a) || ! std::isfinite (d)) finite = false;
                    const float m = std::max (std::abs (a), std::abs (d));
                    if (b * block + i >= static_cast<int> (3.5 * fs)) tail = std::max (tail, m); else peak = std::max (peak, m);
                    energy += a * a + d * d;
                    inRec.push_back (dl[static_cast<size_t> (i)]); outRec.push_back (a);
                    const float ea = a - dl[static_cast<size_t> (i)], ed = d - dr[static_cast<size_t> (i)];
                    diffE += ea * ea + ed * ed;
                    inE += dl[static_cast<size_t> (i)] * dl[static_cast<size_t> (i)] + dr[static_cast<size_t> (i)] * dr[static_cast<size_t> (i)];
                }
            }
            const double rms = std::sqrt (energy / (2.0 * blocks * block));
            // defaults: sane level and audible. Extremes may be loud (every gain at +18 dB) or silent
            // (0 % levels, FREEZE with nothing in it), but must never run away: the silent tail may not
            // grow past what the signal itself produced.
            const double specDb = spectrumDiffDb (inRec, outRec);
            const bool level = setting != 0 || ! mustBeNeutral (t) || specDb < -30.0;
            const bool ok = finite && level && tail <= peak * 1.5f + 1.0e-3f && (setting != 0 || (peak < 8.0f && rms > 1.0e-4));
            if (! ok) ++failures;
            // null: the difference from the input in dB below it (very negative = leaves the sound alone)
            const double nullDb = 10.0 * std::log10 ((diffE + 1.0e-20) / (inE + 1.0e-20));
            std::printf ("%-16s %-8s peak %8.3f  tail %8.3f  rms %8.5f  cpu %5.2f %%  null %7.1f dB  spectrum %7.1f dB%s  %s\n", fxdefs::kTypeNames[t],
                         setting == 0 ? "default" : setting == 1 ? "min" : "max", peak, tail, rms, 100.0 * busy / (blocks * block / fs), nullDb, specDb, setting == 0 && mustBeNeutral (t) ? " (must be < -30)" : "", ok ? "ok" : "FAIL");
        }
    // VALVE: turning GAIN up adds saturation, not level (user, 2026-10-05) - within 1.5 dB of the input
    for (const float gainDb : { 0.0f, 10.0f, 20.0f, 30.0f })
    {
        float v[fxdefs::kNP];
        for (int i = 0; i < fxdefs::kNP; ++i) v[i] = fxdefs::value (fxdefs::Tuba, i, fxdefs::kParams[fxdefs::Tuba][i].def);
        v[0] = gainDb;
        auto u = fx::makeUnit (fxdefs::Tuba);
        u->prepare (fs, block); u->reset();
        fx::Ctx c { v, 120.0, fs };
        std::vector<float> l (static_cast<size_t> (block)), r (static_cast<size_t> (block));
        double eIn = 0.0, eOut = 0.0;
        for (int b = 0; b < static_cast<int> (2.0 * fs / block); ++b)
        {
            const bool measure = b * block >= fs;   // the second second, once the match has settled
            for (int i = 0; i < block; ++i)
            {
                const int n = b * block + i;
                l[static_cast<size_t> (i)] = 0.3f * (std::sin (n * 0.0359f) + 0.5f * std::sin (n * 0.0571f));
                r[static_cast<size_t> (i)] = 0.3f * (std::sin (n * 0.0377f) + 0.5f * std::sin (n * 0.0449f));
                if (measure) eIn += l[static_cast<size_t> (i)] * l[static_cast<size_t> (i)] + r[static_cast<size_t> (i)] * r[static_cast<size_t> (i)];
            }
            u->process (l.data(), r.data(), block, c);
            if (measure) for (int i = 0; i < block; ++i) eOut += l[static_cast<size_t> (i)] * l[static_cast<size_t> (i)] + r[static_cast<size_t> (i)] * r[static_cast<size_t> (i)];
        }
        const double db = 10.0 * std::log10 (eOut / eIn);
        const bool ok = std::abs (db) < 1.5;
        if (! ok) ++failures;
        std::printf ("VALVE level at GAIN %4.0f dB: %+5.2f dB from the input  %s\n", gainDb, db, ok ? "ok" : "FAIL");
    }
    std::printf (failures ? "%d FAILED\n" : "all FX checks passed\n", failures);
    return failures ? 1 : 0;
}
