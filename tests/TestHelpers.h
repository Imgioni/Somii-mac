#pragma once

// Shared helpers for the headless DSP tests: rendering, spectra and frequency estimation.

#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>

#include "core/LayerEngine.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace sgt
{

constexpr double kFs = 48000.0;

struct Stereo
{
    std::vector<float> l, r;
};

inline void prepareEngine (sg::LayerEngine& e, const sg::LayerParams& p, int os = 2)
{
    e.setAnalogTolerance (false);   // deterministic pitch/cutoff for measurements
    e.prepare (kFs, 512, os);
    e.setParams (p);
}

// Plays `note` for `seconds`; releases at releaseAt (seconds) if ≥ 0.
inline Stereo renderNote (sg::LayerEngine& e, int note, float velocity, double seconds, double releaseAt = -1.0)
{
    const int n = static_cast<int> (seconds * kFs);
    const int rel = releaseAt >= 0.0 ? static_cast<int> (releaseAt * kFs) : -1;
    Stereo out;
    out.l.assign (static_cast<size_t> (n), 0.0f);
    out.r.assign (static_cast<size_t> (n), 0.0f);

    e.noteOn (note, velocity);
    int pos = 0;
    while (pos < n)
    {
        int len = std::min (256, n - pos);
        if (rel > pos && rel < pos + len) len = rel - pos;
        e.process (out.l.data() + pos, out.r.data() + pos, len);
        pos += len;
        if (pos == rel) e.noteOff (note);
    }
    return out;
}

inline float rms (const std::vector<float>& x, size_t from = 0, size_t to = 0)
{
    if (to == 0 || to > x.size()) to = x.size();
    double s = 0.0;
    for (size_t i = from; i < to; ++i) s += static_cast<double> (x[i]) * x[i];
    return to > from ? static_cast<float> (std::sqrt (s / static_cast<double> (to - from))) : 0.0f;
}

inline float peakAbs (const std::vector<float>& x, size_t from = 0, size_t to = 0)
{
    if (to == 0 || to > x.size()) to = x.size();
    float p = 0.0f;
    for (size_t i = from; i < to; ++i) p = std::max (p, std::abs (x[i]));
    return p;
}

inline bool allFinite (const std::vector<float>& x)
{
    for (float v : x) if (! std::isfinite (v)) return false;
    return true;
}

// Frequency from rising zero crossings (with linear interpolation), after removing the mean.
inline double zeroCrossingHz (const std::vector<float>& x, double fs, size_t from, size_t to)
{
    to = std::min (to, x.size());
    double mean = 0.0;
    for (size_t i = from; i < to; ++i) mean += x[i];
    mean /= static_cast<double> (to - from);

    double first = -1.0, last = -1.0;
    int count = 0;
    for (size_t i = from + 1; i < to; ++i)
    {
        const double a = x[i - 1] - mean, b = x[i] - mean;
        if (a < 0.0 && b >= 0.0)
        {
            const double t = static_cast<double> (i - 1) + a / (a - b);
            if (first < 0.0) first = t;
            last = t;
            ++count;
        }
    }
    if (count < 2) return 0.0;
    return (count - 1) * fs / (last - first);
}

// Windowed magnitude spectrum (Blackman–Harris); magnitudes normalised so a full-scale sine = 1.
struct Spectrum
{
    std::vector<float> mag;
    double binHz = 1.0;

    float magAt (double hz, double tolHz = 3.0) const
    {
        const int lo = std::max (0, static_cast<int> ((hz - tolHz) / binHz));
        const int hi = std::min (static_cast<int> (mag.size()) - 1, static_cast<int> ((hz + tolHz) / binHz) + 1);
        float m = 0.0f;
        for (int i = lo; i <= hi; ++i) m = std::max (m, mag[static_cast<size_t> (i)]);
        return m;
    }

    static float db (float v) { return 20.0f * std::log10 (std::max (v, 1.0e-12f)); }

    // Strongest bin in [fmin, fmax], refined with parabolic interpolation.
    double peakHz (double fmin, double fmax) const
    {
        const int lo = std::max (1, static_cast<int> (fmin / binHz));
        const int hi = std::min (static_cast<int> (mag.size()) - 2, static_cast<int> (fmax / binHz));
        int best = lo;
        for (int i = lo; i <= hi; ++i) if (mag[static_cast<size_t> (i)] > mag[static_cast<size_t> (best)]) best = i;
        const double a = std::log (std::max (mag[static_cast<size_t> (best - 1)], 1.0e-12f));
        const double b = std::log (std::max (mag[static_cast<size_t> (best)], 1.0e-12f));
        const double c = std::log (std::max (mag[static_cast<size_t> (best + 1)], 1.0e-12f));
        const double denom = a - 2.0 * b + c;
        const double off = std::abs (denom) > 1e-12 ? 0.5 * (a - c) / denom : 0.0;
        return (best + off) * binHz;
    }

    // Loudest component that is NOT a harmonic of f0 (below fmax), in dB relative to `ref`.
    float worstNonHarmonicDb (double f0, double fmax, float ref, int guardBins = 7) const
    {
        float worst = 0.0f;
        const int hi = std::min (static_cast<int> (mag.size()) - 1, static_cast<int> (fmax / binHz));
        for (int i = static_cast<int> (30.0 / binHz); i <= hi; ++i)
        {
            const double f = i * binHz;
            const double k = std::round (f / f0);
            if (k >= 1.0 && std::abs (f - k * f0) <= guardBins * binHz) continue;
            worst = std::max (worst, mag[static_cast<size_t> (i)]);
        }
        return db (worst) - db (ref);
    }
};

inline Spectrum spectrum (const std::vector<float>& x, size_t from, int order, double fs)
{
    const int n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> buf (static_cast<size_t> (2 * n), 0.0f);
    double wsum = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double t = 2.0 * juce::MathConstants<double>::pi * i / (n - 1);
        const double w = 0.35875 - 0.48829 * std::cos (t) + 0.14128 * std::cos (2 * t) - 0.01168 * std::cos (3 * t);
        const size_t idx = from + static_cast<size_t> (i);
        buf[static_cast<size_t> (i)] = static_cast<float> ((idx < x.size() ? x[idx] : 0.0f) * w);
        wsum += w;
    }
    fft.performFrequencyOnlyForwardTransform (buf.data(), true);
    Spectrum s;
    s.binHz = fs / n;
    s.mag.resize (static_cast<size_t> (n / 2 + 1));
    for (int i = 0; i <= n / 2; ++i) s.mag[static_cast<size_t> (i)] = static_cast<float> (buf[static_cast<size_t> (i)] * 2.0 / wsum);
    return s;
}

// Goertzel amplitude of one frequency over a window (full-scale sine → 1).
inline double toneAmplitude (const std::vector<float>& x, size_t from, size_t to, double hz, double fs)
{
    const double w = 2.0 * juce::MathConstants<double>::pi * hz / fs;
    const double c = 2.0 * std::cos (w);
    double s1 = 0.0, s2 = 0.0;
    for (size_t i = from; i < to; ++i)
    {
        const double s0 = x[i] + c * s1 - s2;
        s2 = s1; s1 = s0;
    }
    const double re = s1 - s2 * std::cos (w), im = s2 * std::sin (w);
    return 2.0 * std::sqrt (re * re + im * im) / static_cast<double> (to - from);
}

} // namespace sgt
