#include "WaveTable.h"

#include <cmath>
#include <complex>
#include <cstdint>
#include <functional>

namespace sg
{

namespace
{
using cd = std::complex<double>;
constexpr double kPiD = 3.14159265358979323846;

// In-place iterative radix-2 FFT (init-time only). inverse = true computes Σ X e^{+iωn} (unscaled).
void fft (std::vector<cd>& a, bool inverse)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = 2.0 * kPiD / static_cast<double> (len) * (inverse ? 1.0 : -1.0);
        const cd wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            cd w (1.0, 0.0);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const cd u = a[i + j];
                const cd v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}
} // namespace

std::shared_ptr<const WaveTable> WaveTable::fromHarmonics (const std::vector<float>& amp,
                                                           const std::vector<float>& phase,
                                                           std::string name)
{
    auto table = std::make_shared<WaveTable>();
    table->name = std::move (name);

    const int n = kSize;
    double scale = 1.0;

    for (int level = 0; level < kLevels; ++level)
    {
        const int maxH = kMaxHarm >> level;
        std::vector<cd> spec (static_cast<size_t> (n), cd (0.0, 0.0));
        for (int k = 1; k <= maxH && k < static_cast<int> (amp.size()); ++k)
        {
            const double a = amp[static_cast<size_t> (k)];
            if (a == 0.0) continue;
            const double ph = k < static_cast<int> (phase.size()) ? phase[static_cast<size_t> (k)] : 0.0;
            // x[n] = Σ a·sin(2πkn/N + φ)  ⇔  X[k] = N·a·e^{iφ} / (2i)
            const cd xk = cd (0.0, -0.5 * n * a) * cd (std::cos (ph), std::sin (ph));
            spec[static_cast<size_t> (k)] = xk;
            spec[static_cast<size_t> (n - k)] = std::conj (xk);
        }
        fft (spec, true);

        auto& t = table->levels[static_cast<size_t> (level)];
        t.resize (static_cast<size_t> (n + 1));

        if (level == 0)
        {
            double peak = 0.0;
            for (int i = 0; i < n; ++i) peak = std::max (peak, std::abs (spec[static_cast<size_t> (i)].real() / n));
            scale = peak > 1e-12 ? 1.0 / peak : 1.0;
        }
        for (int i = 0; i < n; ++i)
            t[static_cast<size_t> (i)] = static_cast<float> (spec[static_cast<size_t> (i)].real() / n * scale);
        t[static_cast<size_t> (n)] = t[0];
    }
    return table;
}

std::shared_ptr<const WaveTable> WaveTable::fromCycle (const float* cycle, std::string name)
{
    const int n = kSize;
    std::vector<cd> spec (static_cast<size_t> (n));
    for (int i = 0; i < n; ++i) spec[static_cast<size_t> (i)] = cd (cycle[i], 0.0);
    fft (spec, false);

    std::vector<float> amp (kMaxHarm + 1, 0.0f), phase (kMaxHarm + 1, 0.0f);
    for (int k = 1; k <= kMaxHarm; ++k)
    {
        // a·e^{iφ} = 2i·X[k]/N
        const cd c = cd (0.0, 2.0) * spec[static_cast<size_t> (k)] / static_cast<double> (n);
        amp[static_cast<size_t> (k)]   = static_cast<float> (std::abs (c));
        phase[static_cast<size_t> (k)] = static_cast<float> (std::arg (c));
    }
    return fromHarmonics (amp, phase, std::move (name));
}

// ── Factory bank ────────────────────────────────────────────────────────────────────────────

namespace
{
using CycleFn = std::function<double (double)>;   // phase [0,1) → value

WaveTablePtr fromFunction (const char* name, const CycleFn& f)
{
    std::vector<float> cycle (WaveTable::kSize);
    for (int i = 0; i < WaveTable::kSize; ++i)
        cycle[static_cast<size_t> (i)] = static_cast<float> (f (static_cast<double> (i) / WaveTable::kSize));
    return WaveTable::fromCycle (cycle.data(), name);
}

WaveTablePtr fromSpectrum (const char* name, const std::function<double (int)>& ampOf)
{
    std::vector<float> amp (WaveTable::kMaxHarm + 1, 0.0f), ph (WaveTable::kMaxHarm + 1, 0.0f);
    for (int k = 1; k <= WaveTable::kMaxHarm; ++k) amp[static_cast<size_t> (k)] = static_cast<float> (ampOf (k));
    return WaveTable::fromHarmonics (amp, ph, name);
}

double frac (double x) { return x - std::floor (x); }
double saw (double p) { return 2.0 * frac (p) - 1.0; }

// Resonant-peak weighting centred on harmonic c with width w.
double peak (int k, double c, double w) { const double d = (k - c) / w; return 1.0 / (1.0 + d * d); }

uint32_t hashU (uint32_t x) { x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16; return x; }
double hashUnit (uint32_t seed, int k) { return (hashU (seed * 7919u + static_cast<uint32_t> (k)) & 0xffffff) / 16777216.0; }
} // namespace

AltWaveBank::AltWaveBank()
{
    int s = 0;
    auto add = [this, &s] (WaveTablePtr t) { slots[static_cast<size_t> (s++)] = std::move (t); };

    // Group 1 — "waveforms" (W1–W16)
    add (fromSpectrum ("Organ 8+4", [] (int k) { return k == 1 ? 1.0 : k == 2 ? 0.6 : k == 4 ? 0.3 : 0.0; }));
    add (fromSpectrum ("Drawbar Full", [] (int k) {
        switch (k) { case 1: return 1.0; case 2: return 0.8; case 3: return 0.7; case 4: return 0.6; case 6: return 0.5;
                     case 8: return 0.45; case 10: return 0.3; case 12: return 0.25; case 16: return 0.2; default: return 0.0; } }));
    add (fromSpectrum ("Hollow", [] (int k) { return (k % 2) ? 1.0 / std::pow (k, 1.5) : 0.0; }));
    add (fromFunction ("Pulse 25", [] (double p) { return p < 0.25 ? 1.0 : -1.0; }));
    add (fromFunction ("Pulse 10", [] (double p) { return p < 0.10 ? 1.0 : -1.0; }));
    add (fromSpectrum ("Formant Ah", [] (int k) { return (1.0 / k) * (0.15 + peak (k, 7, 1.5) + 0.7 * peak (k, 12, 2.0)); }));
    add (fromSpectrum ("Formant Oh", [] (int k) { return (1.0 / k) * (0.15 + peak (k, 4, 1.2) + 0.6 * peak (k, 8, 1.5)); }));
    add (fromSpectrum ("Formant Ee", [] (int k) { return (1.0 / k) * (0.15 + peak (k, 3, 1.0) + 0.9 * peak (k, 22, 3.0)); }));
    add (fromFunction ("Sync 2.5", [] (double p) { return saw (2.5 * p); }));
    add (fromFunction ("Sync 3.7", [] (double p) { return saw (3.7 * p); }));
    add (fromFunction ("Sync 5.3", [] (double p) { return saw (5.3 * p); }));
    add (fromSpectrum ("Resonant 8", [] (int k) { return (1.0 / k) * (1.0 + 6.0 * peak (k, 8, 0.8)); }));
    add (fromSpectrum ("Resonant 16", [] (int k) { return (1.0 / k) * (1.0 + 10.0 * peak (k, 16, 1.0)); }));
    add (fromSpectrum ("Pluck Comb", [] (int k) { return (k % 5 == 0) ? 0.0 : 1.0 / k; }));
    add (fromSpectrum ("Reed", [] (int k) { return (k % 2 ? 1.0 : 0.3) / k; }));
    add (fromSpectrum ("Bell Partials", [] (int k) {
        switch (k) { case 1: return 1.0; case 3: return 0.6; case 7: return 0.45; case 12: return 0.3; case 19: return 0.2; case 27: return 0.12; default: return 0.0; } }));

    // Group 2 — "alt_waveforms" (W17–W32)
    add (fromFunction ("Sine Fold 1", [] (double p) { return std::sin (2.5 * std::sin (2.0 * kPiD * p)); }));
    add (fromFunction ("Sine Fold 2", [] (double p) { return std::sin (4.5 * std::sin (2.0 * kPiD * p)); }));
    add (fromFunction ("Soft Square", [] (double p) { return std::tanh (4.0 * std::sin (2.0 * kPiD * p)); }));
    add (fromFunction ("Half Rect", [] (double p) { return std::max (0.0, std::sin (2.0 * kPiD * p)); }));
    add (fromFunction ("Full Rect", [] (double p) { return std::abs (std::sin (2.0 * kPiD * p)); }));
    add (fromFunction ("Saw + 12th", [] (double p) { return saw (p) + 0.5 * saw (3.0 * p); }));
    add (fromFunction ("Square Octaves", [] (double p) { return (frac (p) < 0.5 ? 1.0 : -1.0) + 0.5 * (frac (2.0 * p) < 0.5 ? 1.0 : -1.0); }));
    add (fromSpectrum ("Digital 1", [] (int k) { return hashUnit (1, k) / k; }));
    add (fromSpectrum ("Digital 2", [] (int k) { return hashUnit (2, k) / std::sqrt (static_cast<double> (k)) * (k < 64 ? 1.0 : 0.3); }));
    add (fromSpectrum ("Digital 3", [] (int k) { return (k % 2) ? hashUnit (3, k) / k : 0.0; }));
    add (fromFunction ("Staircase 4", [] (double p) { return std::floor (p * 4.0) / 1.5 - 1.0; }));
    add (fromFunction ("Staircase 8", [] (double p) { return std::floor (p * 8.0) / 3.5 - 1.0; }));
    add (fromFunction ("Exp Saw", [] (double p) { return 2.0 * std::exp (-4.0 * p) - 1.0; }));
    add (fromFunction ("Parabola", [] (double p) { const double x = 2.0 * p - 1.0; return 1.0 - 2.0 * x * x; }));
    add (fromFunction ("Double Pulse", [] (double p) { return (p < 0.15 || (p >= 0.5 && p < 0.58)) ? 1.0 : -1.0; }));
    add (fromSpectrum ("Noise Cycle", [] (int k) { return hashUnit (32, k) * (k < 200 ? 1.0 : 0.5); }));
}

const AltWaveBank& AltWaveBank::factory()
{
    static const AltWaveBank bank;
    return bank;
}

} // namespace sg
