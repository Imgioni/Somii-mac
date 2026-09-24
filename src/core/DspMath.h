#pragma once

// Small, allocation-free math helpers shared by the DSP core. No JUCE dependency.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace sg
{

constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float clampf (float x, float lo, float hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }
inline float lerpf (float a, float b, float t) noexcept   { return a + (b - a) * t; }

// 2^x, relative error < 2e-5 (≈0.03 cent) — used for every pitch and cutoff conversion.
inline float fastExp2 (float x) noexcept
{
    x = clampf (x, -126.0f, 126.0f);
    const float xi = std::floor (x);
    const float f  = x - xi;
    const float p  = 1.0f + f * (0.693147181f + f * (0.240226507f + f * (0.0555041087f
                          + f * (0.00961812911f + f * (0.00133335581f + f * 0.000154035304f)))));
    const auto bits = static_cast<uint32_t> (static_cast<int32_t> (xi) + 127) << 23;
    float scale;
    std::memcpy (&scale, &bits, sizeof (float));
    return p * scale;
}

// sin(2π·p) for any p; |error| < 4e-6.
inline float sin2pi (float p) noexcept
{
    float u = p - std::floor (p) - 0.5f;          // [-0.5, 0.5)
    if (u > 0.25f)       u = 0.5f - u;
    else if (u < -0.25f) u = -0.5f - u;
    const float x  = kTwoPi * u;                  // [-π/2, π/2]
    const float x2 = x * x;
    const float s  = x * (1.0f + x2 * (-1.66666667e-1f + x2 * (8.33333333e-3f
                          + x2 * (-1.98412698e-4f + x2 * 2.75573192e-6f))));
    return -s;                                    // sin(2π(u+0.5)) = -sin(2πu)
}

// tan(x) for x in [0, 1.5] — prewarping for the zero-delay-feedback filters.
inline float fastTan (float x) noexcept
{
    const float x2 = x * x;
    const float num = x * (135135.0f + x2 * (-17325.0f + x2 * 378.0f));
    const float den = 135135.0f + x2 * (-62370.0f + x2 * (3150.0f - 28.0f * x2));
    return num / den;
}

// Soft saturator with tanh-like shape, unity slope at 0, bounded to ±1.
inline float softClip (float x) noexcept
{
    if (x > 3.0f)  return 1.0f;
    if (x < -3.0f) return -1.0f;
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float semitonesToRatio (float st) noexcept { return fastExp2 (st * (1.0f / 12.0f)); }
inline float noteToHz (float note) noexcept       { return 440.0f * fastExp2 ((note - 69.0f) * (1.0f / 12.0f)); }
inline float dbToGain (float db) noexcept          { return std::pow (10.0f, db * 0.05f); }

// xorshift32 — deterministic, per-voice noise and randomisation.
struct Rng
{
    uint32_t state = 0x9e3779b9u;

    void seed (uint32_t s) noexcept { state = s != 0 ? s : 0x9e3779b9u; }

    uint32_t next() noexcept
    {
        uint32_t x = state;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        return state = x;
    }

    float bipolar() noexcept { return static_cast<float> (static_cast<int32_t> (next())) * (1.0f / 2147483648.0f); }
    float unipolar() noexcept { return static_cast<float> (next() >> 8) * (1.0f / 16777216.0f); }
};

// One-pole smoother coefficient for time constant tau seconds at rate fs.
inline float onePoleCoeff (float tauSeconds, float fs) noexcept
{
    if (tauSeconds <= 0.0f) return 1.0f;
    return 1.0f - std::exp (-1.0f / (tauSeconds * fs));
}

} // namespace sg
