#pragma once

// Band-limited phase oscillator for the classic DDS shapes.
//
// The hardware avoids band-limiting by clocking its DDS far above audio rate [manual p.xii].
// In software we get the same open top end from: (1) a 4-point polyBLEP / polyBLAMP (cubic
// B-spline kernel) correction at every waveform discontinuity, and (2) running the oscillator
// stage oversampled. Output is delayed by two samples so corrections can reach the samples
// before a discontinuity as well as after it — which is also what makes hard sync clean.
//
// All shapes are phase-aligned with the sine (fundamental = +sin 2πφ) so that PWM/WAVE morphs
// between neighbouring waveforms never cancel the fundamental [p.32].

#include "DspMath.h"

namespace sg
{

enum class Shape { Sine, Saw, Square, Triangle, Pulse, Noise };

// Naive (aliasing) value of a classic shape at phase p ∈ [0,1).
inline float naiveShape (Shape s, float p, float pw) noexcept
{
    switch (s)
    {
        case Shape::Sine:     return sin2pi (p);
        case Shape::Saw:      return 1.0f - 2.0f * p;                       // falling ramp
        case Shape::Square:   return p < 0.5f ? 1.0f : -1.0f;
        case Shape::Pulse:    return p < pw ? 1.0f : -1.0f;
        case Shape::Triangle: return p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f);
        case Shape::Noise:    return 0.0f;
    }
    return 0.0f;
}

// Band-limiting residuals for one discontinuity between samples N−1 and N at fraction d ∈ (0,1].
// They touch samples N−2, N−1, N and N+1.
struct BlepAcc
{
    float m2 = 0.0f;   // sample N−2
    float m1 = 0.0f;   // sample N−1
    float c0 = 0.0f;   // sample N (the one being computed)
    float c1 = 0.0f;   // sample N+1

    // Step of height h (integrated cubic B-spline minus the ideal step).
    void step (float d, float h) noexcept
    {
        const float v = 1.0f - d;
        const float v2 = v * v, d2 = d * d;
        m2 += h * (v2 * v2) * (1.0f / 24.0f);
        m1 += h * ((1.0f / 24.0f) + (v + 1.5f * v2 + v2 * v - 0.75f * v2 * v2) * (1.0f / 6.0f));
        c0 -= h * ((1.0f / 24.0f) + (d + 1.5f * d2 + d2 * d - 0.75f * d2 * d2) * (1.0f / 6.0f));
        c1 -= h * (d2 * d2) * (1.0f / 24.0f);
    }

    // Slope change of dslope (units per sample).
    void corner (float d, float dslope) noexcept
    {
        const float v = 1.0f - d;
        m2 += dslope * edge (v);
        m1 += dslope * mid (v);
        c0 += dslope * mid (d);
        c1 += dslope * edge (d);
    }

private:
    static float edge (float x) noexcept { const float x2 = x * x; return x2 * x2 * x * (1.0f / 120.0f); }
    static float mid (float v) noexcept
    {
        const float v2 = v * v;
        return (1.0f / 120.0f) + v * (1.0f / 24.0f)
             + (0.5f * v2 + 0.5f * v2 * v + 0.25f * v2 * v2 - 0.15f * v2 * v2 * v) * (1.0f / 6.0f);
    }
};

// Advances phase p by inc over the sub-interval [t0, t1] of one sample (t in sample units),
// adding weighted corrections for shape s into acc. Returns the fraction at which the first
// wrap happened, or -1. Divisions only happen at discontinuities.
inline float advancePhase (float& p, float inc, float t0, float t1, Shape s, float pw,
                           float weight, BlepAcc& acc) noexcept
{
    float firstWrap = -1.0f;
    if (inc <= 0.0f) return firstWrap;

    float t = t0;
    for (int guard = 0; guard < 8; ++guard)
    {
        float eventPhase = 1.0f;
        float h = 0.0f;
        bool corner = false;

        switch (s)
        {
            case Shape::Saw:      h = 2.0f; break;
            case Shape::Square:   if (p < 0.5f) { eventPhase = 0.5f; h = -2.0f; } else h = 2.0f; break;
            case Shape::Pulse:    if (p < pw)   { eventPhase = pw;   h = -2.0f; } else h = 2.0f; break;
            case Shape::Triangle:
                corner = true;
                if (p < 0.25f)      { eventPhase = 0.25f; h = -8.0f * inc; }
                else if (p < 0.75f) { eventPhase = 0.75f; h =  8.0f * inc; }
                else                { h = 0.0f; }
                break;
            case Shape::Sine:
            case Shape::Noise:    break;
        }

        const float remaining = (t1 - t) * inc;
        if (p + remaining < eventPhase)      // common case: no discontinuity this sample
        {
            p += remaining;
            break;
        }

        const float tEvent = t + (eventPhase - p) / inc;
        if (weight != 0.0f && h != 0.0f)
        {
            if (corner) acc.corner (tEvent, h * weight);
            else        acc.step (tEvent, h * weight);
        }

        if (eventPhase >= 1.0f)
        {
            p = 0.0f;
            if (firstWrap < 0.0f) firstWrap = tEvent;
        }
        else
        {
            p = eventPhase;
        }
        t = tEvent;
    }

    if (p >= 1.0f) p -= std::floor (p);   // numeric safety
    return firstWrap;
}

// Two-sample output delay line shared by every band-limited oscillator.
struct BlepDelay
{
    float s2 = 0.0f, s1 = 0.0f, carry = 0.0f;

    void clear() noexcept { s2 = s1 = carry = 0.0f; }

    float push (float naiveNow, const BlepAcc& acc) noexcept
    {
        const float out = s2 + acc.m2;
        s2 = s1 + acc.m1;
        s1 = naiveNow + carry + acc.c0;
        carry = acc.c1;
        return out;
    }
};

// Single classic-shape oscillator (DDS 2 and the sub-oscillator).
struct PhaseOsc
{
    float phase = 0.0f;
    BlepDelay delay;
    bool resetRequested = false;

    void reset (float ph = 0.0f) noexcept { phase = ph; delay.clear(); resetRequested = false; }

    // syncAt: fraction of this sample where a hard reset happens (from the master), or -1.
    // Returns the (two-sample delayed) output; wrapOut receives the first natural wrap fraction.
    float tick (Shape s, float inc, float pw, float syncAt, Rng& rng, float& wrapOut) noexcept
    {
        BlepAcc acc;
        wrapOut = -1.0f;

        if (resetRequested) { syncAt = 0.0f; resetRequested = false; }

        // Fast path: no sync and no edge inside this sample.
        if (syncAt < 0.0f && s != Shape::Noise)
        {
            float edge = 1.0f;
            if (s == Shape::Square)        edge = phase < 0.5f ? 0.5f : 1.0f;
            else if (s == Shape::Pulse)    edge = phase < pw ? pw : 1.0f;
            else if (s == Shape::Triangle) edge = phase < 0.25f ? 0.25f : (phase < 0.75f ? 0.75f : 1.0f);
            const float pn = phase + inc;
            if (pn < edge)
            {
                phase = pn;
                return delay.push (naiveShape (s, phase, pw), acc);
            }
        }

        if (s == Shape::Noise)
        {
            advancePhase (phase, inc, 0.0f, 1.0f, s, pw, 0.0f, acc);
            return delay.push (rng.bipolar(), acc);
        }

        if (syncAt >= 0.0f)
        {
            wrapOut = advancePhase (phase, inc, 0.0f, syncAt, s, pw, 1.0f, acc);
            const float before = naiveShape (s, phase, pw);
            phase = 0.0f;
            acc.step (syncAt, naiveShape (s, 0.0f, pw) - before);
            const float w2 = advancePhase (phase, inc, syncAt, 1.0f, s, pw, 1.0f, acc);
            if (wrapOut < 0.0f) wrapOut = w2;
        }
        else
        {
            wrapOut = advancePhase (phase, inc, 0.0f, 1.0f, s, pw, 1.0f, acc);
        }

        return delay.push (naiveShape (s, phase, pw), acc);
    }
};

} // namespace sg
