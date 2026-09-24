#pragma once

// LFO 2 shapes [manual p.70]: SINE (bipolar), REV SAW, SQUARE, SAW (unipolar +), S&H (bipolar),
// NOISE (white). LFO 2 runs per voice so the matrix can modulate it polyphonically [p.72]; all
// voices share one layer-wide phase and only drift apart when their rate is modulated
// individually. S&H values are a hash of the cycle number, so synced voices agree.

#include "DspMath.h"
#include "LayerParams.h"

namespace sg
{

inline float hashBipolar (uint32_t x) noexcept
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return static_cast<float> (static_cast<int32_t> (x)) * (1.0f / 2147483648.0f);
}

inline bool lfo2IsBipolar (Lfo2Wave w) noexcept
{
    return w == Lfo2Wave::Sine || w == Lfo2Wave::SampleHold || w == Lfo2Wave::Noise;
}

inline float lfo2Shape (Lfo2Wave w, double phase, Rng& noise) noexcept
{
    const double cycle = std::floor (phase);
    const float p = static_cast<float> (phase - cycle);
    switch (w)
    {
        case Lfo2Wave::Sine:       return sin2pi (p);
        case Lfo2Wave::RevSaw:     return 1.0f - p;
        case Lfo2Wave::SampleHold: return hashBipolar (static_cast<uint32_t> (static_cast<int64_t> (cycle)));
        case Lfo2Wave::Square:     return p < 0.5f ? 1.0f : 0.0f;
        case Lfo2Wave::Saw:        return p;
        case Lfo2Wave::Noise:      return noise.bipolar();
    }
    return 0.0f;
}

} // namespace sg
