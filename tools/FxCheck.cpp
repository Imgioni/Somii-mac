// SGFxCheck: runs every FX-rack effect on a test signal at its default, minimum and maximum
// settings and fails on NaN/inf, runaway output, or an effect that produces nothing.
// Usage: SGFxCheck.exe   (exit code 0 = all passed)

#include "plugin/FxRack.h"

#include <cstdio>

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    constexpr double fs = 48000.0;
    constexpr int block = 256, blocks = static_cast<int> (4.0 * fs / block);

    fx::ImpulseResponse ir;                       // a short decaying noise burst
    ir.buffer.setSize (2, 24000);
    juce::Random rnd (7);
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < 24000; ++i) ir.buffer.setSample (c, i, (rnd.nextFloat() * 2 - 1) * std::exp (-i / 4000.0f));
    ir.name = "TEST";

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
                u->process (l.data(), r.data(), block, c);
                if (b % 40 == 0) u->messageTick (v, ir, false);
                for (int i = 0; i < block; ++i)
                {
                    const float a = l[static_cast<size_t> (i)], d = r[static_cast<size_t> (i)];
                    if (! std::isfinite (a) || ! std::isfinite (d)) finite = false;
                    const float m = std::max (std::abs (a), std::abs (d));
                    if (b * block + i >= static_cast<int> (3.5 * fs)) tail = std::max (tail, m); else peak = std::max (peak, m);
                    energy += a * a + d * d;
                }
            }
            const double rms = std::sqrt (energy / (2.0 * blocks * block));
            // defaults: sane level and audible. Extremes may be loud (every gain at +18 dB) or silent
            // (0 % levels, FREEZE with nothing in it), but must never run away: the silent tail may not
            // grow past what the signal itself produced.
            const bool ok = finite && tail <= peak * 1.5f + 1.0e-3f && (setting != 0 || (peak < 8.0f && rms > 1.0e-4));
            if (! ok) ++failures;
            std::printf ("%-16s %-8s peak %8.3f  tail %8.3f  rms %8.5f  %s\n", fxdefs::kTypeNames[t],
                         setting == 0 ? "default" : setting == 1 ? "min" : "max", peak, tail, rms, ok ? "ok" : "FAIL");
        }
    std::printf (failures ? "%d FAILED\n" : "all FX checks passed\n", failures);
    return failures ? 1 : 0;
}
