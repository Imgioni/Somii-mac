// Tapers, math helpers, decimator and wavetables.

#include "TestHelpers.h"

#include "core/Decimator.h"
#include "core/Envelope.h"
#include "core/Tapers.h"
#include "core/WaveTable.h"

using namespace sg;

class TaperTests final : public juce::UnitTest
{
public:
    TaperTests() : juce::UnitTest ("Tapers match manual end points", "core") {}

    void runTest() override
    {
        beginTest ("Envelope times 1 ms … 10 s [pp.47–53]");
        expectWithinAbsoluteError (taper::envTime (0.0f), 0.001f, 1.0e-6f);
        expectWithinAbsoluteError (taper::envTime (1.0f), 10.0f, 1.0e-3f);
        expectEquals (taper::holdTime (0.0f), 0.0f, "hold at minimum = no effect [p.47]");
        expectWithinAbsoluteError (taper::holdTime (1.0f), 10.0f, 1.0e-3f);

        beginTest ("LFO rates [pp.54, 59, 70]");
        expectWithinAbsoluteError (taper::lfoLowHz (0.0f), 0.05f, 1.0e-6f);
        expectWithinAbsoluteError (taper::lfoLowHz (1.0f), 50.0f, 1.0e-3f);
        expectWithinAbsoluteError (taper::lfoHighHz (0.0f), 20.0f, 1.0e-4f);
        expectWithinAbsoluteError (taper::lfoHighHz (1.0f), 20000.0f, 0.5f);
        expectWithinAbsoluteError (taper::lfo1Delay (1.0f), 10.0f, 1.0e-5f);
        expectWithinAbsoluteError (taper::lfo2Delay (1.0f), 5.0f, 1.0e-5f);

        beginTest ("Delay, portamento, DDS 2 LFO range [pp.38, 65, 74]");
        expectWithinAbsoluteError (taper::delayTime (0.0f), 0.001f, 1.0e-6f);
        expectWithinAbsoluteError (taper::delayTime (1.0f), 1.0f, 1.0e-5f);
        expectEquals (taper::portaSecondsPerOctave (0.0f), 0.0f, "portamento off at leftmost");
        expectWithinAbsoluteError (taper::portaSecondsPerOctave (1.0f), 10.0f, 1.0e-4f);
        expectWithinAbsoluteError (taper::dds2LfoHz (-7.0f), 0.1f, 1.0e-5f);
        expectWithinAbsoluteError (taper::dds2LfoHz (7.0f), 100.0f, 1.0e-3f);

        beginTest ("Level: −∞ … 0 dB … +4 dB [pp.44, 79]");
        expectEquals (taper::levelGain (0.0f), 0.0f);
        expectWithinAbsoluteError (taper::levelGain (0.8f), 1.0f, 1.0e-6f);
        expectWithinAbsoluteError (20.0f * std::log10 (taper::levelGain (1.0f)), 4.0f, 1.0e-4f);
        expectWithinAbsoluteError (taper::levelFromGain (taper::levelGain (0.5f)), 0.5f, 1.0e-5f);
        expectWithinAbsoluteError (taper::levelFromGain (taper::levelGain (0.9f)), 0.9f, 1.0e-5f);

        beginTest ("ENV 1 keytrack scale [p.49]");
        expectWithinAbsoluteError (envKeytrackScale (1.0f, 72), 0.5f, 1.0e-4f);       // ON: an octave up halves decay/release
        expectWithinAbsoluteError (envKeytrackScale (0.5f, 72), 0.70711f, 1.0e-4f);   // ½: quarter-tone steps
        expectWithinAbsoluteError (envKeytrackScale (0.0f, 96), 1.0f, 1.0e-6f);
    }
};

class MathTests final : public juce::UnitTest
{
public:
    MathTests() : juce::UnitTest ("Fast math accuracy", "core") {}

    void runTest() override
    {
        beginTest ("fastExp2 within 0.05 cent");
        float worst = 0.0f;
        for (float x = -20.0f; x <= 20.0f; x += 0.0137f)
            worst = std::max (worst, std::abs (fastExp2 (x) / std::exp2 (x) - 1.0f));
        expectLessThan (worst, 3.0e-5f);

        beginTest ("sin2pi");
        float worstSin = 0.0f;
        for (float p = -2.0f; p <= 2.0f; p += 0.00123f)
            worstSin = std::max (worstSin, std::abs (sin2pi (p) - std::sin (2.0f * kPi * p)));
        expectLessThan (worstSin, 2.0e-5f);

        beginTest ("fastTan over the prewarp range");
        float worstTan = 0.0f;
        for (float x = 0.0f; x <= 1.45f; x += 0.001f)
            worstTan = std::max (worstTan, std::abs (fastTan (x) / std::tan (x) - 1.0f));
        expectLessThan (worstTan, 1.0e-3f);
    }
};

class DecimatorTests final : public juce::UnitTest
{
public:
    DecimatorTests() : juce::UnitTest ("Decimator passband and stopband", "core") {}

    void runTest() override
    {
        for (int factor : { 2, 4, 8 })
        {
            beginTest ("factor " + juce::String (factor));
            const double osFs = sgt::kFs * factor;

            auto measure = [&] (double hz)
            {
                Decimator d;
                d.prepare (factor, 32);
                const int nHost = 48000;
                std::vector<float> in (static_cast<size_t> (32 * factor)), out (static_cast<size_t> (nHost));
                double ph = 0.0;
                for (int pos = 0; pos < nHost; pos += 32)
                {
                    for (auto& s : in) { s = static_cast<float> (std::sin (2.0 * juce::MathConstants<double>::pi * ph)); ph += hz / osFs; }
                    d.process (in.data(), out.data() + pos, 32);
                }
                return out;
            };

            // Passband: 1 kHz and 18 kHz pass at unity.
            for (double hz : { 1000.0, 18000.0 })
            {
                auto y = measure (hz);
                const double a = sgt::toneAmplitude (y, 4800, 48000, hz, sgt::kFs);
                expectWithinAbsoluteError (a, 1.0, 0.012, juce::String (hz) + " Hz passband gain");
            }

            // Stopband: a 30 kHz tone would alias to 18 kHz; it must be at least 85 dB down.
            auto y = measure (30000.0);
            const double alias = sgt::toneAmplitude (y, 4800, 48000, 18000.0, sgt::kFs);
            logMessage ("  alias of 30 kHz at " + juce::String (factor) + "x: "
                        + juce::String (20.0 * std::log10 (std::max (alias, 1e-12)), 1) + " dB");
            expectLessThan (alias, 5.6e-5, "30 kHz alias rejection");
        }
    }
};

class WaveTableTests final : public juce::UnitTest
{
public:
    WaveTableTests() : juce::UnitTest ("Alt waveform tables", "core") {}

    void runTest() override
    {
        const auto& bank = AltWaveBank::factory();

        beginTest ("32 slots, peak-normalised, finite");
        for (int i = 0; i < AltWaveBank::kSlots; ++i)
        {
            const auto t = bank.get (i);
            expect (t != nullptr);
            float peak = 0.0f;
            bool finite = true;
            for (int k = 0; k < WaveTable::kSize; ++k)
            {
                const float v = t->read (static_cast<float> (k) / WaveTable::kSize, 0);
                finite = finite && std::isfinite (v);
                peak = std::max (peak, std::abs (v));
            }
            expect (finite, "slot " + juce::String (i + 1));
            expectWithinAbsoluteError (peak, 1.0f, 0.02f, "slot " + juce::String (i + 1) + " peak");
        }

        beginTest ("Top mip level is a single harmonic");
        const auto t = bank.get (3);   // Pulse 25
        std::vector<float> cyc (static_cast<size_t> (WaveTable::kSize));
        for (int k = 0; k < WaveTable::kSize; ++k) cyc[static_cast<size_t> (k)] = t->read (static_cast<float> (k) / WaveTable::kSize, WaveTable::kLevels - 1);
        const double h1 = sgt::toneAmplitude (cyc, 0, cyc.size(), 1.0, WaveTable::kSize);
        const double h2 = sgt::toneAmplitude (cyc, 0, cyc.size(), 2.0, WaveTable::kSize);
        const double h3 = sgt::toneAmplitude (cyc, 0, cyc.size(), 3.0, WaveTable::kSize);
        expectGreaterThan (h1, 0.01);
        expectLessThan (h2 + h3, 1.0e-4);

        beginTest ("Level selection");
        expectEquals (WaveTable::levelFor (1000.0f), 0);
        expectEquals (WaveTable::levelFor (512.0f), 0);
        expectEquals (WaveTable::levelFor (300.0f), 1);
        expectEquals (WaveTable::levelFor (0.5f), WaveTable::kLevels - 1);
    }
};

static TaperTests taperTests;
static MathTests mathTests;
static DecimatorTests decimatorTests;
static WaveTableTests waveTableTests;
