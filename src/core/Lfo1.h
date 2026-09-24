#pragma once

// LFO 1, low-frequency part [manual pp.54–59]. One instance per super voice; in non-binaural
// mode it is shared by the two voices of the pair [p.54]. The HF / HF TRK modes are audio-rate
// sines rendered inside each voice (so HF TRK can track that voice's own note).
//
// Shapes and polarity [p.58–59]: TRIANGLE bipolar, REV SAW unipolar, S&H bipolar, SQUARE unipolar.

#include "DspMath.h"
#include "LayerParams.h"

namespace sg
{

class Lfo1
{
public:
    void prepare (float hostRate, uint32_t seed) noexcept
    {
        fs = hostRate;
        rng.seed (seed);
        shPrev = rng.bipolar(); shCur = rng.bipolar(); shNext = rng.bipolar();
    }

    void setRate (float hz) noexcept { inc = hz / fs; }
    void setShape (Lfo1Wave w, Lfo1Mode m, bool noiseAtTopRate) noexcept { wave = w; mode = m; shNoise = noiseAtTopRate; }

    // Note-on: ONCE and RESET restart the cycle [p.59].
    void noteOn() noexcept
    {
        if (isHF()) return;
        if (mode == Lfo1Mode::OnceDds1 || mode == Lfo1Mode::ResetDds2) { phase = 0.0f; finished = false; }
    }

    void tick() noexcept
    {
        if (isHF() || finished) return;
        phase += inc;
        if (phase >= 1.0f)
        {
            if (mode == Lfo1Mode::OnceDds1) { phase = 1.0f; finished = true; return; }
            phase -= std::floor (phase);
            shPrev = shCur; shCur = shNext; shNext = rng.bipolar();
        }
        if (wave == Lfo1Wave::SampleHold && shNoise) noiseValue = rng.bipolar();
    }

    // Value for a voice whose phase is offset by `offset` cycles (LR PHASE, 0..1).
    float value (float offset) const noexcept
    {
        if (isHF()) return 0.0f;
        if (finished) return 0.0f;          // ONCE: one duty cycle, then rest [p.59]
        float p = phase + offset;
        const bool ahead = p >= 1.0f;
        if (ahead) p -= 1.0f;
        switch (wave)
        {
            case Lfo1Wave::Triangle:   return p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f);
            case Lfo1Wave::RevSaw:     return 1.0f - p;
            case Lfo1Wave::Square:     return p < 0.5f ? 1.0f : 0.0f;
            case Lfo1Wave::SampleHold: return shNoise ? noiseValue : (ahead ? shNext : shCur);
            case Lfo1Wave::HF:
            case Lfo1Wave::HFTrk:      return 0.0f;
        }
        return 0.0f;
    }

    // Unipolar version used by destinations that only go one way (PWM/WAVE, tremolo).
    float unipolar (float offset) const noexcept
    {
        const float v = value (offset);
        return isBipolar() ? 0.5f * (v + 1.0f) : v;
    }

    bool isBipolar() const noexcept { return wave == Lfo1Wave::Triangle || wave == Lfo1Wave::SampleHold; }
    bool isHF() const noexcept      { return wave == Lfo1Wave::HF || wave == Lfo1Wave::HFTrk; }
    float getPhase() const noexcept { return phase; }

private:
    float fs = 48000.0f, inc = 0.0f, phase = 0.0f;
    Lfo1Wave wave = Lfo1Wave::Triangle;
    Lfo1Mode mode = Lfo1Mode::FreeNorm;
    bool finished = false, shNoise = false;
    float shPrev = 0.0f, shCur = 0.0f, shNext = 0.0f, noiseValue = 0.0f;
    Rng rng;
};

} // namespace sg
