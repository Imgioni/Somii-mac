// Whole-voice behaviour: lifecycle, polyphony, mixer, VCA, binaural stereo, CPU budget.

#include "TestHelpers.h"

#include <chrono>

using namespace sg;

class LifecycleTests final : public juce::UnitTest
{
public:
    LifecycleTests() : juce::UnitTest ("Voice lifecycle and VCA", "voice") {}

    void runTest() override
    {
        beginTest ("Note sounds, releases, then frees its voice");
        {
            LayerParams p;
            p.e2Release = 0.5f;   // 100 ms
            LayerEngine e; sgt::prepareEngine (e, p);
            auto s = sgt::renderNote (e, 60, 1.0f, 1.0, 0.4);
            expectGreaterThan (sgt::rms (s.l, 4800, 19200), 0.05f, "audible while held");
            expectLessThan (sgt::peakAbs (s.l, static_cast<size_t> (0.9 * sgt::kFs)), 1.0e-3f, "silent after release");
            expectEquals (e.getActiveVoiceCount(), 0, "voice freed");
        }

        beginTest ("20 voices; the 11th binaural note steals [p.xiii]");
        {
            LayerParams p;
            LayerEngine e; sgt::prepareEngine (e, p);
            std::vector<float> l (480), r (480);
            for (int i = 0; i < 10; ++i) e.noteOn (48 + i, 0.8f);
            e.process (l.data(), r.data(), 480);
            expectEquals (e.getActiveVoiceCount(), 20);
            e.noteOn (80, 0.8f);
            e.process (l.data(), r.data(), 480);
            expectEquals (e.getActiveVoiceCount(), 20);
        }

        beginTest ("Non-binaural: 20 independent monaural voices [p.91]");
        {
            LayerParams p;
            p.binaural = false;
            LayerEngine e; sgt::prepareEngine (e, p);
            std::vector<float> l (480), r (480);
            for (int i = 0; i < 20; ++i) e.noteOn (40 + i, 0.8f);
            e.process (l.data(), r.data(), 480);
            expectEquals (e.getActiveVoiceCount(), 20);
        }

        beginTest ("Non-binaural SPREAD: successive notes alternate left / right [p.58]");
        {
            LayerParams p;
            p.binaural = false;
            p.lfo1LrPhase = 1.0f;       // SPREAD full: slot 0 hard left, slot 1 hard right
            p.e2Release = 0.3f;
            LayerEngine e; sgt::prepareEngine (e, p);
            std::vector<float> l (4800), r (4800);
            juce::String sides;
            for (int i = 0; i < 8; ++i)
            {
                e.noteOn (60 + i, 0.8f);
                e.process (l.data(), r.data(), 4800);
                double el = 0.0, er = 0.0;
                for (size_t k = 0; k < l.size(); ++k) { el += l[k] * l[k]; er += r[k] * r[k]; }
                sides << (el > er ? "L" : "R");
                e.noteOff (60 + i);
                for (int b = 0; b < 10; ++b) e.process (l.data(), r.data(), 4800);   // release fully
            }
            logMessage ("  sides: " + sides);
            expectEquals (sides, juce::String ("LRLRLRLR"));
        }

        beginTest ("ENV LEVEL at minimum = silence [p.45]");
        {
            LayerParams p;
            p.vcaLevel = 0.0f;
            LayerEngine e; sgt::prepareEngine (e, p);
            auto s = sgt::renderNote (e, 60, 1.0f, 0.3);
            expectLessThan (sgt::peakAbs (s.l), 1.0e-6f);
        }

        beginTest ("Fixed GATE envelope frees ENV 2 [p.45]");
        {
            LayerParams p;
            p.vcaEnv = VcaEnv::Gate;
            p.e2Attack = 1.0f;        // a 10 s ENV 2 attack must not affect the VCA
            LayerEngine e; sgt::prepareEngine (e, p);
            auto s = sgt::renderNote (e, 60, 1.0f, 0.6, 0.3);
            expectGreaterThan (sgt::rms (s.l, 480, 4800), 0.05f, "immediately at full level");
            expectLessThan (sgt::peakAbs (s.l, static_cast<size_t> (0.35 * sgt::kFs)), 1.0e-3f, "no release stage");
        }

        beginTest ("DYNAMICS: velocity sets level only when enabled [p.45]");
        {
            auto level = [] (Tri dyn, float vel)
            {
                LayerParams p;
                p.dynamics = dyn;
                LayerEngine e; sgt::prepareEngine (e, p);
                auto s = sgt::renderNote (e, 60, vel, 0.5);
                return sgt::rms (s.l, 4800);
            };
            expectWithinAbsoluteError (level (Tri::Off, 0.2f) / level (Tri::Off, 1.0f), 1.0f, 0.01f);
            expectLessThan (level (Tri::On, 0.2f) / level (Tri::On, 1.0f), 0.25f);
            const float half = level (Tri::Half, 0.2f) / level (Tri::Half, 1.0f);
            expect (half > 0.3f && half < 0.9f, "½ is in between");
        }
    }
};

class MixerStereoTests final : public juce::UnitTest
{
public:
    MixerStereoTests() : juce::UnitTest ("Mixer and binaural stereo", "voice") {}

    void runTest() override
    {
        beginTest ("MIX crossfades DDS 1 ↔ DDS 2 [p.40]");
        {
            auto content = [] (float mix, float& at440, float& at659)
            {
                LayerParams p;
                p.dds1Wave = Dds1Wave::Sine;
                p.dds2Wave = Dds2Wave::Sine;
                p.dds2Tune = 7.0f;
                p.mix = mix;
                LayerEngine e; sgt::prepareEngine (e, p);
                auto s = sgt::renderNote (e, 69, 1.0f, 1.0);
                auto spec = sgt::spectrum (s.l, 4800, 15, sgt::kFs);
                at440 = sgt::Spectrum::db (spec.magAt (440.0));
                at659 = sgt::Spectrum::db (spec.magAt (440.0 * std::pow (2.0, 7.0 / 12.0)));
            };
            float a, b;
            content (0.0f, a, b); expectLessThan (b - a, -80.0f, "full left = DDS 1 only");
            content (1.0f, a, b); expectLessThan (a - b, -80.0f, "full right = DDS 2 only");
            content (0.5f, a, b); expectWithinAbsoluteError (a - b, 0.0f, 0.5f, "12 o'clock = equal");
        }

        beginTest ("Binaural: L and R identical at LR PHASE 0, different at 50 % [pp.56–57]");
        {
            auto diff = [] (float lr)
            {
                LayerParams p;
                p.lfo1Wave = Lfo1Wave::Triangle;
                p.lfo1Rate = 0.5f;
                p.lfo1LrPhase = lr;
                p.vcaLfo1Amt = 1.0f;
                p.vcfLfo1Amt = 0.6f;
                p.lpf = 0.7f;
                LayerEngine e; sgt::prepareEngine (e, p);
                auto s = sgt::renderNote (e, 60, 1.0f, 1.0);
                float d = 0.0f;
                for (size_t i = 0; i < s.l.size(); ++i) d = std::max (d, std::abs (s.l[i] - s.r[i]));
                return d;
            };
            const float same = diff (0.0f), opposite = diff (0.5f);
            logMessage ("  max |L−R|: " + juce::String (same, 5) + " at 0 %, " + juce::String (opposite, 3) + " at 50 %");
            expectLessThan (same, 1.0e-4f);
            expectGreaterThan (opposite, 0.05f);
        }

        beginTest ("SUPER spreads the sisters differently left and right [p.61]");
        {
            LayerParams p;
            p.superMode = Tri::On;
            p.pwDetune = 0.8f;
            LayerEngine e; sgt::prepareEngine (e, p);
            auto s = sgt::renderNote (e, 60, 1.0f, 1.0);
            double lr = 0.0, ll = 0.0, rr = 0.0;
            for (size_t i = 4800; i < s.l.size(); ++i) { lr += s.l[i] * s.r[i]; ll += s.l[i] * s.l[i]; rr += s.r[i] * s.r[i]; }
            const double corr = lr / std::sqrt (ll * rr);
            logMessage ("  L/R correlation " + juce::String (corr, 3));
            expectLessThan (corr, 0.9, "wide stereo image");
        }
    }
};

class BenchmarkTests final : public juce::UnitTest
{
public:
    BenchmarkTests() : juce::UnitTest ("CPU benchmark", "bench") {}

    void runTest() override
    {
        for (int os : { 2, 4 })
        for (auto superMode : { Tri::Off, Tri::On })
        {
            beginTest ("20 voices, SUPER " + juce::String (superMode == Tri::On ? "ON (7 osc)" : "OFF")
                       + ", filter env, " + juce::String (os) + "x oversampling");
            LayerParams p;
            p.superMode = superMode;
            p.pwDetune = 0.6f;
            p.dds2Wave = Dds2Wave::Pulse;
            p.mix = 0.5f;
            p.lpf = 0.5f; p.res = 0.4f; p.vcfEnvAmt = 0.4f;
            p.e1Mode = Env1Mode::Loop;
            p.vcfLfo1Amt = 0.2f;
            LayerEngine e; sgt::prepareEngine (e, p, os);
            for (int i = 0; i < 10; ++i) e.noteOn (48 + i * 3, 0.8f);

            const int block = 256;
            const double seconds = 10.0;
            std::vector<float> l (block), r (block);
            const auto t0 = std::chrono::steady_clock::now();
            for (int pos = 0; pos < static_cast<int> (seconds * sgt::kFs); pos += block)
                e.process (l.data(), r.data(), block);
            const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            const double load = 100.0 * elapsed / seconds;
            logMessage ("  " + juce::String (load, 1) + " % of one core (target < 15 % per the build prompt)");
            expectEquals (e.getActiveVoiceCount(), 20);
            expectLessThan (load, 100.0, "renders faster than real time");
        }

        for (auto superMode : { Tri::Off, Tri::On })
        {
            // The build prompt's budget: 20 voices plus both layers' effects at the default 2x.
            beginTest ("Prompt budget: 20 voices + both layers' chorus I+II and delay, SUPER "
                       + juce::String (superMode == Tri::On ? "ON" : "OFF") + ", 2x");
            LayerParams p;
            p.superMode = superMode;
            p.pwDetune = 0.6f;
            p.dds2Wave = Dds2Wave::Pulse;
            p.mix = 0.5f;
            p.lpf = 0.5f; p.res = 0.4f; p.vcfEnvAmt = 0.4f;
            p.e1Mode = Env1Mode::Loop;
            p.vcfLfo1Amt = 0.2f;
            p.chorus = 3; p.delaySend = 0.5f; p.delayFeedback = 0.6f;
            LayerEngine upper, lower;
            sgt::prepareEngine (upper, p);
            sgt::prepareEngine (lower, p);
            for (int i = 0; i < 10; ++i) upper.noteOn (48 + i * 3, 0.8f);

            const int block = 256;
            const double seconds = 10.0;
            std::vector<float> l (block), r (block), l2 (block), r2 (block);
            const auto t0 = std::chrono::steady_clock::now();
            for (int pos = 0; pos < static_cast<int> (seconds * sgt::kFs); pos += block)
            {
                upper.process (l.data(), r.data(), block);
                lower.process (l2.data(), r2.data(), block);
            }
            const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            const double load = 100.0 * elapsed / seconds;
            logMessage ("  " + juce::String (load, 1) + " % of one core (target < 15 % per the build prompt)");
            expectEquals (upper.getActiveVoiceCount(), 20);
            expectLessThan (load, 100.0, "renders faster than real time");
        }

        beginTest ("Component breakdown (ns per oversampled voice sample, 2x)");
        {
            // One second of 20 voices at 2x = 1.92 M calls per component.
            constexpr int kCalls = 20 * 96000;
            volatile float sink = 0.0f;
            auto time = [&] (const char* name, auto&& fn)
            {
                float acc = 0.0f;
                const auto t0 = std::chrono::steady_clock::now();
                for (int i = 0; i < kCalls; ++i) acc += fn();
                const double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
                sink = sink + acc;
                logMessage ("  " + juce::String (name).paddedRight (' ', 24) + juce::String (1.0e9 * s / kCalls, 2)
                            + " ns  (" + juce::String (100.0 * s, 1) + " % of one core for 20 voices)");
            };

            Dds1 off, on;
            off.prepare (96000.0f, 48000.0f, 1); off.setShape (Dds1Wave::Saw, nullptr, nullptr);
            off.setSuper (Tri::Off, 0.6f, false); off.setFrequency (261.6f / 96000.0f, 0.0f);
            on.prepare (96000.0f, 48000.0f, 2); on.setShape (Dds1Wave::Saw, nullptr, nullptr);
            on.setSuper (Tri::On, 0.6f, false); on.setFrequency (261.6f / 96000.0f, 0.0f);
            float w = 0.0f;
            time ("DDS 1 saw, SUPER OFF", [&] { return off.tick (1.0f, w); });
            time ("DDS 1 saw, SUPER ON", [&] { return on.tick (1.0f, w); });

            PhaseOsc pulse; Rng rng; rng.seed (3);
            time ("DDS 2 pulse", [&] { return pulse.tick (Shape::Pulse, 262.0f / 96000.0f, 0.3f, -1.0f, rng, w); });

            OnePoleHpf hpf; hpf.setCutoff (40.0f, 96000.0f);
            float x = 0.1f;
            time ("HPF", [&] { x = -x; return hpf.process (x); });

            SsiLadder lpf; lpf.setResonance (0.4f, Drive::Off); lpf.setCutoff (2000.0f, 96000.0f);
            int n = 0;
            time ("LPF ladder", [&] { if ((++n & 1) == 0) lpf.advanceRamp(); x = -x; return lpf.process (x); });
            time ("LPF ladder + ramp", [&] {
                if ((++n & 7) == 0) lpf.rampCutoff ((n & 8) ? 1500.0f : 2500.0f, 96000.0f, 4);
                if ((n & 1) == 0) lpf.advanceRamp();
                x = -x; return lpf.process (x); });
        }
    }
};

static LifecycleTests lifecycleTests;
static MixerStereoTests mixerStereoTests;
static BenchmarkTests benchmarkTests;
