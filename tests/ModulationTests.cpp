// Envelopes, LFO 1 and the DDS Modulator [manual pp.46–63].

#include "TestHelpers.h"

#include "core/Envelope.h"
#include "core/Lfo1.h"

using namespace sg;

namespace
{
constexpr float kHost = 48000.0f;

// Samples until the envelope first satisfies pred (or -1).
template <typename Pred>
int samplesUntil (Envelope& env, int maxSamples, Pred pred)
{
    for (int i = 1; i <= maxSamples; ++i)
        if (pred (env.tick())) return i;
    return -1;
}
} // namespace

class EnvelopeTests final : public juce::UnitTest
{
public:
    EnvelopeTests() : juce::UnitTest ("Envelope stages and modes", "mod") {}

    void runTest() override
    {
        beginTest ("ATTACK reaches the peak in the set time [p.47]");
        {
            Envelope env; env.setSampleRate (kHost);
            env.setParams (0.0f, 0.100f, 0.0f, 1.0f, 1.0f, 1.0f, false);
            env.gateOn();
            const int t = samplesUntil (env, 48000, [] (float v) { return v >= 1.0f; });
            expectWithinAbsoluteError (t / kHost, 0.100f, 0.003f);
        }

        beginTest ("DECAY covers 99 % of the way to SUSTAIN in the set time [p.48]");
        {
            Envelope env; env.setSampleRate (kHost);
            env.setParams (0.0f, 0.001f, 0.0f, 0.200f, 0.5f, 1.0f, false);
            env.gateOn();
            samplesUntil (env, 4800, [] (float v) { return v >= 1.0f; });
            const int t = samplesUntil (env, 48000, [] (float v) { return v <= 0.505f; });
            expectWithinAbsoluteError (t / kHost, 0.200f, 0.005f);
        }

        beginTest ("RELEASE time [p.48]");
        {
            Envelope env; env.setSampleRate (kHost);
            env.setParams (0.0f, 0.001f, 0.0f, 0.01f, 1.0f, 0.300f, false);
            env.gateOn();
            for (int i = 0; i < 4800; ++i) env.tick();
            env.gateOff();
            const int t = samplesUntil (env, 96000, [] (float v) { return v <= 0.01f; });
            expectWithinAbsoluteError (t / kHost, 0.300f, 0.005f);
            for (int i = 0; i < 96000; ++i) env.tick();
            expect (env.isIdle());
        }

        beginTest ("ATTACK HOLD delays the attack; min = no effect [p.47]");
        {
            Envelope env; env.setSampleRate (kHost);
            env.setParams (0.050f, 0.001f, 0.0f, 0.1f, 1.0f, 0.1f, false);
            env.gateOn();
            const int t = samplesUntil (env, 48000, [] (float v) { return v > 0.001f; });
            expectWithinAbsoluteError (t / kHost, 0.050f, 0.001f);
        }

        beginTest ("DECAY HOLD keeps the peak before decaying [p.47]");
        {
            Envelope env; env.setSampleRate (kHost);
            env.setParams (0.0f, 0.001f, 0.100f, 0.05f, 0.0f, 0.1f, false);
            env.gateOn();
            samplesUntil (env, 4800, [] (float v) { return v >= 1.0f; });
            const int t = samplesUntil (env, 48000, [] (float v) { return v < 0.999f; });
            expectWithinAbsoluteError (t / kHost, 0.100f, 0.001f);
        }

        beginTest ("LOOP repeats attack → decay hold → decay between SUSTAIN and peak [p.48]");
        {
            Envelope env; env.setSampleRate (kHost);
            env.setParams (0.0f, 0.010f, 0.0f, 0.020f, 0.2f, 0.1f, true);
            env.gateOn();
            float lo = 1.0f, hi = 0.0f;
            for (int i = 0; i < 48000; ++i)
            {
                const float v = env.tick();
                if (i > 4800) { lo = std::min (lo, v); hi = std::max (hi, v); }
            }
            const unsigned cycles = env.getCycleCount();
            logMessage ("  loop cycles in 1 s: " + juce::String (cycles) + ", range " + juce::String (lo, 3) + " … " + juce::String (hi, 3));
            expectWithinAbsoluteError (lo, 0.2f, 0.02f, "loop floor = sustain");
            expectWithinAbsoluteError (hi, 1.0f, 0.001f, "loop peak");
            expect (cycles >= 30 && cycles <= 40, "about one cycle per attack + decay");
            env.gateOff();
            const int t = samplesUntil (env, 48000, [] (float v) { return v <= 0.01f; });
            expect (t > 0, "releases on key-up");
        }
    }
};

class Lfo1Tests final : public juce::UnitTest
{
public:
    Lfo1Tests() : juce::UnitTest ("LFO 1 shapes and modes", "mod") {}

    void runTest() override
    {
        beginTest ("Rate accuracy");
        {
            Lfo1 lfo; lfo.prepare (kHost, 1);
            lfo.setShape (Lfo1Wave::Triangle, Lfo1Mode::FreeNorm, false);
            lfo.setRate (2.0f);
            std::vector<float> v (static_cast<size_t> (kHost * 5));
            for (auto& s : v) { lfo.tick(); s = lfo.value (0.0f); }
            expectWithinAbsoluteError (sgt::zeroCrossingHz (v, kHost, 0, v.size()), 2.0, 0.01);
        }

        beginTest ("Polarity: triangle & S&H bipolar, rev saw & square unipolar [pp.58–59]");
        for (auto w : { Lfo1Wave::Triangle, Lfo1Wave::RevSaw, Lfo1Wave::SampleHold, Lfo1Wave::Square })
        {
            Lfo1 lfo; lfo.prepare (kHost, 7);
            lfo.setShape (w, Lfo1Mode::FreeNorm, false);
            lfo.setRate (20.0f);
            float lo = 1.0f, hi = -1.0f;
            for (int i = 0; i < 48000; ++i) { lfo.tick(); lo = std::min (lo, lfo.value (0.0f)); hi = std::max (hi, lfo.value (0.0f)); }
            const bool bipolar = (w == Lfo1Wave::Triangle || w == Lfo1Wave::SampleHold);
            if (bipolar) expect (lo < -0.5f && hi > 0.5f, "bipolar");
            else         expect (lo >= 0.0f && hi > 0.9f, "unipolar");
        }

        beginTest ("ONCE runs a single cycle [p.59]");
        {
            Lfo1 lfo; lfo.prepare (kHost, 1);
            lfo.setShape (Lfo1Wave::RevSaw, Lfo1Mode::OnceDds1, false);
            lfo.setRate (10.0f);
            lfo.noteOn();
            float first = 0.0f;
            for (int i = 0; i < 2400; ++i) { lfo.tick(); if (i == 10) first = lfo.value (0.0f); }   // 50 ms
            expectGreaterThan (first, 0.9f, "starts at the top of the ramp");
            for (int i = 0; i < 9600; ++i) lfo.tick();
            expectEquals (lfo.value (0.0f), 0.0f, "then stops");
        }

        beginTest ("RESET restarts the phase on note-on [p.59]");
        {
            Lfo1 lfo; lfo.prepare (kHost, 1);
            lfo.setShape (Lfo1Wave::Triangle, Lfo1Mode::ResetDds2, false);
            lfo.setRate (3.0f);
            for (int i = 0; i < 12345; ++i) lfo.tick();
            lfo.noteOn();
            expectEquals (lfo.getPhase(), 0.0f);
        }

        beginTest ("LR PHASE 50 % = opposite phase for the right voice [p.57]");
        {
            Lfo1 lfo; lfo.prepare (kHost, 1);
            lfo.setShape (Lfo1Wave::Triangle, Lfo1Mode::FreeNorm, false);
            lfo.setRate (1.7f);
            float worst = 0.0f;
            for (int i = 0; i < 48000; ++i) { lfo.tick(); worst = std::max (worst, std::abs (lfo.value (0.0f) + lfo.value (0.5f))); }
            expectLessThan (worst, 1.0e-4f);
        }
    }
};

class DdsModTests final : public juce::UnitTest
{
public:
    DdsModTests() : juce::UnitTest ("DDS Modulator pitch mod and cross mod", "mod") {}

    void runTest() override
    {
        beginTest ("LFO 1 → pitch: square LFO at full depth = +1 octave [p.60]");
        {
            LayerParams p;
            p.lfo1Wave = Lfo1Wave::Square;
            p.lfo1Mode = Lfo1Mode::ResetDds2;
            p.lfo1Rate = std::log (1.0f / 0.05f) / std::log (1000.0f);   // 1 Hz
            p.pitchLfo1Amt = 1.0f;                                          // 12 st
            LayerEngine e; sgt::prepareEngine (e, p);
            auto s = sgt::renderNote (e, 69, 1.0f, 1.0);
            const double high = sgt::zeroCrossingHz (s.l, sgt::kFs, 4800, 19200);    // 0.1–0.4 s: square high
            const double low  = sgt::zeroCrossingHz (s.l, sgt::kFs, 28800, 43200);   // 0.6–0.9 s: square low
            logMessage ("  " + juce::String (high, 1) + " Hz / " + juce::String (low, 1) + " Hz");
            expectWithinAbsoluteError (high, 880.0, 3.0);
            expectWithinAbsoluteError (low, 440.0, 1.5);
        }

        beginTest ("ENV 1 → pitch, destination DDS 2 only [p.60]");
        {
            LayerParams p;
            p.e1Sustain = 1.0f;
            p.pitchEnv1Amt = std::sqrt (12.0f / 36.0f);   // +12 st
            p.pitchDest = OscDest::Dds2;
            p.dds2Wave = Dds2Wave::Saw;
            p.mix = 1.0f;
            LayerEngine e; sgt::prepareEngine (e, p);
            auto s = sgt::renderNote (e, 69, 1.0f, 1.0);
            expectWithinAbsoluteError (sgt::zeroCrossingHz (s.l, sgt::kFs, 9600, s.l.size()), 880.0, 1.5);

            p.mix = 0.0f;   // DDS 1 must stay put
            LayerEngine e2; sgt::prepareEngine (e2, p);
            auto s2 = sgt::renderNote (e2, 69, 1.0f, 1.0);
            expectWithinAbsoluteError (sgt::zeroCrossingHz (s2.l, sgt::kFs, 9600, s2.l.size()), 440.0, 0.5);
        }

        // Exponential FM also shifts the average pitch upward — the manual expects this and offers
        // LOWER DETUNE to compensate "pitch offsets caused by cross modulation" [p.80].
        auto analyse = [] (Dds2Mode mode, float xmod, double& centroidHz, float& h2Db)
        {
            LayerParams p;
            p.dds1Wave = Dds1Wave::Sine;
            p.dds2Wave = Dds2Wave::Sine;
            p.dds2Tune = 3.0f;
            p.dds2Mode = mode;
            p.crossMod = xmod;
            p.mix = 0.0f;          // listen to DDS 1 only
            LayerEngine e; sgt::prepareEngine (e, p);
            auto s = sgt::renderNote (e, 57, 1.0f, 1.6);
            auto spec = sgt::spectrum (s.l, 9600, 16, sgt::kFs);
            double num = 0.0, den = 0.0;
            for (size_t i = 1; i < spec.mag.size(); ++i) { num += static_cast<double> (i) * spec.binHz * spec.mag[i]; den += spec.mag[i]; }
            centroidHz = num / den;
            h2Db = sgt::Spectrum::db (spec.magAt (440.0)) - sgt::Spectrum::db (spec.magAt (220.0));
        };

        beginTest ("CROSS MOD: DDS 2 frequency-modulates DDS 1 [p.63]");
        {
            double c0, c1; float h0, h1;
            analyse (Dds2Mode::Norm, 0.0f, c0, h0);
            analyse (Dds2Mode::Norm, 0.6f, c1, h1);
            logMessage ("  DDS 1 spectral centroid " + juce::String (c0, 0) + " → " + juce::String (c1, 0) + " Hz");
            expectGreaterThan (c1, c0 * 1.5, "cross mod adds sidebands to DDS 1");
        }

        beginTest ("SYNC reverses the routing: DDS 1 modulates DDS 2 [p.63]");
        {
            double c; float h2;
            analyse (Dds2Mode::Sync, 0.6f, c, h2);
            logMessage ("  DDS 1 2nd harmonic with SYNC + cross mod: " + juce::String (h2, 1) + " dB");
            expectLessThan (h2, -60.0f, "DDS 1 stays a pure sine when the routing is reversed");
        }
    }
};

static EnvelopeTests envelopeTests;
static Lfo1Tests lfo1Tests;
static DdsModTests ddsModTests;
