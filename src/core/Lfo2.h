#pragma once

// LFO 2 shapes [manual p.70]: SINE (bipolar), REV SAW, SQUARE, SAW (unipolar +), S&H (bipolar),
// NOISE (white). Every voice has its own LFO 2 and they free-run independently [p.72]: "the
// phase of the per-voice LFOs can be synchronised again by toggling the leftmost toggle switch",
// which only means something if they drift apart. In BINAURAL the two voices of a note are panned
// hard left and right, so that independence is what gives LFO 2 its stereo movement. S&H hashes
// the cycle number together with the voice seed, so each voice draws its own values.

#include "DspMath.h"
#include "LayerParams.h"
#include "Lfo1.h"   // kShGlide / shGlide: the rounded S&H steps

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

inline float lfo2Shape (Lfo2Wave w, double phase, Rng& noise, uint32_t seed = 0) noexcept
{
    const double cycle = std::floor (phase);
    const float p = static_cast<float> (phase - cycle);
    switch (w)
    {
        case Lfo2Wave::Sine:       return sin2pi (p);
        case Lfo2Wave::RevSaw:     return 1.0f - p;
        case Lfo2Wave::SampleHold:
        {
            const auto c = static_cast<uint32_t> (static_cast<int64_t> (cycle));
            return shGlide (hashBipolar ((c - 1u) * 2654435761u + seed), hashBipolar (c * 2654435761u + seed), p);
        }
        case Lfo2Wave::Square:     return p < 0.5f ? 1.0f : 0.0f;
        case Lfo2Wave::Saw:        return p;
        case Lfo2Wave::Noise:      return noise.bipolar();
    }
    return 0.0f;
}

} // namespace sg
