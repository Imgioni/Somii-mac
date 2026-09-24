#pragma once

// Small DSP blocks shared by the FX rack's effects (docs/fx/FX_PROMPTS.md). Allocation happens
// only in prepare(); everything else is real-time safe.

#include "core/Effects.h"   // sg::DelayLine (power-of-two ring, Catmull-Rom reads)

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace fx
{
constexpr float kPi = 3.14159265358979f, kTwoPi = 6.28318530717959f;

inline float clampf (float x, float lo, float hi) noexcept { return x < lo ? lo : (x > hi ? hi : x); }
inline float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept { return 20.0f * std::log10 (g > 1.0e-9f ? g : 1.0e-9f); }
// soft limiter used in every feedback path: transparent below ~0.5, never exceeds ±1.5
inline float softClip (float x) noexcept { return 1.5f * std::tanh (x * (1.0f / 1.5f)); }
inline float fixDenorm (float x) noexcept { return std::abs (x) < 1.0e-15f ? 0.0f : x; }
// coefficient of a one-pole smoother reaching ~63 % in `seconds`
inline float smoothCoeff (float seconds, double fs) noexcept
{
    return seconds <= 0.0f ? 1.0f : 1.0f - std::exp (-1.0f / (seconds * static_cast<float> (fs)));
}

struct Rng
{
    uint32_t s = 0x9E3779B9u;
    float next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return static_cast<float> (s) * 2.3283064e-10f; }   // 0..1
    float bi() noexcept { return next() * 2.0f - 1.0f; }
};

struct OnePole   // low-pass; hp() = input - lp
{
    float a = 1.0f, z = 0.0f;
    void setHz (float hz, double fs) noexcept { a = 1.0f - std::exp (-kTwoPi * clampf (hz, 1.0f, 0.49f * static_cast<float> (fs)) / static_cast<float> (fs)); }
    float lp (float x) noexcept { z += a * (x - z); z = fixDenorm (z); return z; }
    float hp (float x) noexcept { return x - lp (x); }
    void reset() noexcept { z = 0.0f; }
};

// RBJ cookbook biquad, transposed direct form II.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    enum Kind { LP, HP, BP, Notch, Bell, LowShelf, HighShelf };
    void set (Kind k, float hz, float q, float gainDb, double fs) noexcept
    {
        hz = clampf (hz, 5.0f, 0.49f * static_cast<float> (fs));
        q = clampf (q, 0.05f, 40.0f);
        const float w = kTwoPi * hz / static_cast<float> (fs), c = std::cos (w), s = std::sin (w), al = s / (2.0f * q);
        const float A = std::pow (10.0f, gainDb / 40.0f);
        float B0, B1, B2, A0, A1, A2;
        switch (k)
        {
            case LP:    B0 = (1 - c) / 2; B1 = 1 - c; B2 = B0; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break;
            case HP:    B0 = (1 + c) / 2; B1 = -(1 + c); B2 = B0; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break;
            case BP:    B0 = al; B1 = 0; B2 = -al; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break;
            case Notch: B0 = 1; B1 = -2 * c; B2 = 1; A0 = 1 + al; A1 = -2 * c; A2 = 1 - al; break;
            case Bell:  B0 = 1 + al * A; B1 = -2 * c; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * c; A2 = 1 - al / A; break;
            case LowShelf:
            {
                const float sq = 2 * std::sqrt (A) * al;
                B0 = A * ((A + 1) - (A - 1) * c + sq); B1 = 2 * A * ((A - 1) - (A + 1) * c); B2 = A * ((A + 1) - (A - 1) * c - sq);
                A0 = (A + 1) + (A - 1) * c + sq; A1 = -2 * ((A - 1) + (A + 1) * c); A2 = (A + 1) + (A - 1) * c - sq; break;
            }
            default:
            {
                const float sq = 2 * std::sqrt (A) * al;
                B0 = A * ((A + 1) + (A - 1) * c + sq); B1 = -2 * A * ((A - 1) + (A + 1) * c); B2 = A * ((A + 1) + (A - 1) * c - sq);
                A0 = (A + 1) - (A - 1) * c + sq; A1 = 2 * ((A - 1) - (A + 1) * c); A2 = (A + 1) - (A - 1) * c - sq; break;
            }
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }
    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = fixDenorm (b1 * x - a1 * y + z2);
        z2 = fixDenorm (b2 * x - a2 * y);
        return y;
    }
    void reset() noexcept { z1 = z2 = 0.0f; }
};

// Linkwitz-Riley 4th order split: low = LP²(x), high = HP²(x); low + high is all-pass flat.
struct LR4
{
    Biquad l1, l2, h1, h2;
    void set (float hz, double fs) noexcept
    {
        l1.set (Biquad::LP, hz, 0.7071f, 0, fs); l2 = l1;
        h1.set (Biquad::HP, hz, 0.7071f, 0, fs); h2 = h1;
        l1.reset(); l2.reset(); h1.reset(); h2.reset();
    }
    void setKeep (float hz, double fs) noexcept   // retune without clearing state
    {
        Biquad a, b; a.set (Biquad::LP, hz, 0.7071f, 0, fs); b.set (Biquad::HP, hz, 0.7071f, 0, fs);
        auto copy = [] (Biquad& d, const Biquad& s) { d.b0 = s.b0; d.b1 = s.b1; d.b2 = s.b2; d.a1 = s.a1; d.a2 = s.a2; };
        copy (l1, a); copy (l2, a); copy (h1, b); copy (h2, b);
    }
    void split (float x, float& lo, float& hi) noexcept { lo = l2.process (l1.process (x)); hi = h2.process (h1.process (x)); }
};

// Topology-preserving state-variable filter (Simper). Outputs low, band, high.
struct Svf
{
    float g = 0.1f, k = 1.4f, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    float lo = 0, bp = 0, hi = 0;
    void set (float hz, float res01, double fs) noexcept
    {
        g = std::tan (kPi * clampf (hz, 10.0f, 0.47f * static_cast<float> (fs)) / static_cast<float> (fs));
        k = 2.0f - 1.96f * clampf (res01, 0.0f, 1.0f);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void process (float x) noexcept
    {
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = fixDenorm (2 * v1 - ic1); ic2 = fixDenorm (2 * v2 - ic2);
        lo = v2; bp = v1; hi = x - k * v1 - v2;
    }
    void reset() noexcept { ic1 = ic2 = 0.0f; }
};

// Peak envelope follower with separate attack and release.
struct EnvFollower
{
    float att = 0.01f, rel = 0.001f, env = 0.0f;
    void set (float attackS, float releaseS, double fs) noexcept { att = smoothCoeff (attackS, fs); rel = smoothCoeff (releaseS, fs); }
    float process (float x) noexcept { x = std::abs (x); env += (x > env ? att : rel) * (x - env); env = fixDenorm (env); return env; }
};

// Granular pitch shifter: two read taps sweep through a short window, crossfaded (Hann).
struct PitchShifter
{
    sg::DelayLine line;
    float phase = 0.0f, window = 2400.0f;
    void prepare (double fs, float windowMs = 60.0f)
    {
        window = windowMs * 0.001f * static_cast<float> (fs);
        line.prepare (static_cast<int> (window) + 16);
        phase = 0.0f;
    }
    void reset() noexcept { line.clear(); phase = 0.0f; }
    float process (float x, float ratio) noexcept
    {
        line.push (x);
        phase += (1.0f - ratio) / window;          // delay grows when shifting down, shrinks when up
        phase -= std::floor (phase);
        const float p2 = phase + 0.5f - std::floor (phase + 0.5f);
        const float d1 = 2.0f + phase * window, d2 = 2.0f + p2 * window;
        const float g1 = std::sin (kPi * phase), g2 = std::sin (kPi * p2);   // sin² sums to 1
        return line.read (d1) * g1 * g1 + line.read (d2) * g2 * g2;
    }
};

// 8-line feedback delay network, Hadamard mixing, modulated lengths, in-loop damping.
class Fdn
{
public:
    static constexpr int N = 8;
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& l : lines) l.prepare (static_cast<int> (0.25 * fs * 2.2) + 64);
        for (int i = 0; i < N; ++i) lfoPh[i] = static_cast<float> (i) / N;
        reset();
    }
    void reset() noexcept { for (auto& l : lines) l.clear(); for (auto& d : damp) d.reset(); for (auto& d : lowcut) d.reset(); }
    // size 0..1, rt60 seconds, damping Hz, mod 0..1, freeze, modulation rate Hz
    void set (float size, float rt60, float dampHz, float mod, bool freeze, float modRateHz = 0.35f) noexcept
    {
        static constexpr float base[N] = { 0.0297f, 0.0371f, 0.0411f, 0.0437f, 0.0533f, 0.0617f, 0.0693f, 0.0787f };
        const float scale = 0.3f + 1.7f * clampf (size, 0.0f, 1.0f);
        for (int i = 0; i < N; ++i)
        {
            len[i] = base[i] * scale * static_cast<float> (fs);
            fb[i] = freeze ? 1.0f : std::pow (10.0f, -3.0f * (len[i] / static_cast<float> (fs)) / std::max (0.05f, rt60));
            damp[i].setHz (freeze ? 20000.0f : dampHz, fs);
            lowcut[i].setHz (freeze ? 1.0f : 30.0f, fs);
        }
        modDepth = mod * 0.0025f * static_cast<float> (fs);
        modInc = modRateHz / static_cast<float> (fs);
    }
    // stereo in (already pre-delayed), stereo out (wet only)
    void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        float v[N];
        for (int i = 0; i < N; ++i)
        {
            lfoPh[i] += modInc * (1.0f + 0.13f * static_cast<float> (i));
            if (lfoPh[i] >= 1.0f) lfoPh[i] -= 1.0f;
            const float d = len[i] + modDepth * std::sin (kTwoPi * lfoPh[i]);
            v[i] = lines[i].read (std::max (1.0f, d));
        }
        // fast Walsh-Hadamard (orthogonal after 1/sqrt(8))
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h << 1)
                for (int j = i; j < i + h; ++j) { const float a = v[j], b = v[j + h]; v[j] = a + b; v[j + h] = a - b; }
        outL = (v[0] + v[2] + v[4] + v[6]) * 0.25f;
        outR = (v[1] + v[3] + v[5] + v[7]) * 0.25f;
        for (int i = 0; i < N; ++i)
        {
            float y = v[i] * 0.35355339f * fb[i];
            y = lowcut[i].hp (damp[i].lp (y));
            lines[i].push (fixDenorm (y + ((i & 1) ? inR : inL) * 0.5f));
        }
    }

private:
    double fs = 48000.0;
    std::array<sg::DelayLine, N> lines;
    std::array<OnePole, N> damp, lowcut;
    float len[N] {}, fb[N] {}, lfoPh[N] {};
    float modDepth = 0.0f, modInc = 0.0f;
};

// Schroeder all-pass (diffusion)
struct Allpass
{
    sg::DelayLine line;
    float len = 100.0f, g = 0.6f;
    void prepare (int maxLen) { line.prepare (maxLen + 4); }
    void reset() noexcept { line.clear(); }
    float process (float x) noexcept
    {
        const float d = line.read (len);
        const float v = x - g * d;
        line.push (fixDenorm (v));
        return d + g * v;
    }
};

// first-order all-pass (phaser stage); a from the break frequency
struct Allpass1
{
    float z = 0.0f;
    static float coeff (float hz, double fs) noexcept { const float t = std::tan (kPi * clampf (hz, 10.0f, 0.45f * static_cast<float> (fs)) / static_cast<float> (fs)); return (t - 1.0f) / (t + 1.0f); }
    float process (float x, float a) noexcept { const float y = a * x + z; z = fixDenorm (x - a * y); return y; }
};

// Stereo pan law (equal power) for p in −1…+1
inline void panGains (float p, float& gl, float& gr) noexcept
{
    const float a = (clampf (p, -1.0f, 1.0f) + 1.0f) * kPi * 0.25f;
    gl = std::cos (a) * 1.41421356f; gr = std::sin (a) * 1.41421356f;
}

// quarter-note-based delay length in seconds
inline float beatsToSeconds (float beats, double bpm) noexcept { return beats * 60.0f / static_cast<float> (bpm > 1.0 ? bpm : 120.0); }

} // namespace fx
