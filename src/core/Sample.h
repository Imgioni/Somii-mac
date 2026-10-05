#pragma once

// A user sample for DDS 1's CUSTOM wave (SPKR addition, not on the hardware): stereo float
// frames at the file's own level (a dropped file plays as loud as it is), each channel carrying
// one guard frame so read() never branches.

#include <algorithm>
#include <cmath>
#include <vector>

namespace sg
{

struct Sample
{
    std::vector<float> l, r;   // frames + 1 guard each
    int frames = 0;
    float rate = 48000.0f;
    // SLICE mode: the transients, in time order - frame position and strength (1 = the strongest)
    std::vector<double> onsetPos;
    std::vector<float> onsetStr;

    // Energy-rise onset finder: the log energy of 5 ms hops against the two before it; a local peak
    // of that rise is a transient. At most 128 kept (the strongest), at least 60 ms apart. Message
    // thread only, once per file.
    void findOnsets()
    {
        onsetPos.clear(); onsetStr.clear();
        const int hop = std::max (32, static_cast<int> (rate * 0.005f)), n = frames / hop;
        if (n < 4) return;
        std::vector<float> e (static_cast<size_t> (n)), d (static_cast<size_t> (n), 0.0f);
        for (int k = 0; k < n; ++k)
        {
            double s = 0.0;
            for (int i = k * hop; i < (k + 1) * hop; ++i) { const float m = 0.5f * (l[static_cast<size_t> (i)] + r[static_cast<size_t> (i)]); s += m * m; }
            e[static_cast<size_t> (k)] = 10.0f * std::log10 (static_cast<float> (s / hop) + 1.0e-9f);
        }
        for (int k = 2; k < n; ++k)
            d[static_cast<size_t> (k)] = std::max (0.0f, e[static_cast<size_t> (k)] - std::max (e[static_cast<size_t> (k - 1)], e[static_cast<size_t> (k - 2)]))
                                       * (e[static_cast<size_t> (k)] > -60.0f ? 1.0f : 0.0f);   // silence never starts a slice
        const int gap = std::max (1, static_cast<int> (rate * 0.06f) / hop);
        std::vector<std::pair<float, int>> peaks;
        for (int k = 2; k < n; ++k)
        {
            const float v = d[static_cast<size_t> (k)];
            if (v < 3.0f) continue;                                  // a rise of 3 dB or more
            bool top = true;
            for (int j = std::max (0, k - gap); j <= std::min (n - 1, k + gap) && top; ++j) top = j == k || d[static_cast<size_t> (j)] < v || (d[static_cast<size_t> (j)] == v && j > k);
            if (top) peaks.push_back ({ v, k });
        }
        std::sort (peaks.begin(), peaks.end(), [] (auto& a, auto& b) { return a.first > b.first; });
        if (peaks.size() > 128) peaks.resize (128);
        const float strongest = peaks.empty() ? 1.0f : peaks.front().first;
        std::sort (peaks.begin(), peaks.end(), [] (auto& a, auto& b) { return a.second < b.second; });
        for (const auto& [v, k] : peaks)
        {
            onsetPos.push_back (std::max (0.0, static_cast<double> (k - 1) * hop));   // the hop before the rise: catch the attack
            onsetStr.push_back (v / strongest);
        }
    }
    // The page's picture of the sample, `points` columns: min / max (-1..1 of the loudest peak), RMS (0..1)
    // and how each column's energy splits between lows (< 200 Hz), mids and highs (> 2.5 kHz), which the
    // page turns into the waveform's colours. Positions are 64-bit: points * frames passes 2^31 for files
    // longer than about 75 s (that overflow crashed the plugin, 2026-10-05). Message thread, once per file.
    struct Overview { std::vector<float> lo, hi, rms, low, mid, high; };
    Overview overview (int points) const
    {
        Overview o;
        for (auto* v : { &o.lo, &o.hi, &o.rms, &o.low, &o.mid, &o.high }) v->assign (static_cast<size_t> (points), 0.0f);
        if (frames < 1 || points < 1) return o;
        const float k1 = std::exp (-6.2831853f * 200.0f / rate), k2 = std::exp (-6.2831853f * 2500.0f / rate);
        float lp1 = 0.0f, lp2 = 0.0f, peak = 1.0e-9f;
        for (int p = 0; p < points; ++p)
        {
            const long long a = static_cast<long long> (p) * frames / points;
            const long long e = std::max (a + 1, static_cast<long long> (p + 1) * frames / points);
            float lo = 0.0f, hi = 0.0f;
            double sq = 0.0, el = 0.0, em = 0.0, eh = 0.0;
            for (long long i = a; i < e; ++i)
            {
                const float x = 0.5f * (l[static_cast<size_t> (i)] + r[static_cast<size_t> (i)]);
                lo = std::min (lo, x); hi = std::max (hi, x); sq += static_cast<double> (x) * x;
                lp1 = x + k1 * (lp1 - x); lp2 = x + k2 * (lp2 - x);
                const float bl = lp1, bm = lp2 - lp1, bh = x - lp2;
                el += bl * bl; em += bm * bm; eh += bh * bh;
            }
            const auto s = static_cast<size_t> (p);
            const double sum = el + em + eh + 1.0e-12;
            o.lo[s] = lo; o.hi[s] = hi; o.rms[s] = static_cast<float> (std::sqrt (sq / static_cast<double> (e - a)));
            o.low[s] = static_cast<float> (el / sum); o.mid[s] = static_cast<float> (em / sum); o.high[s] = static_cast<float> (eh / sum);
            peak = std::max ({ peak, -lo, hi });
        }
        for (size_t s = 0; s < static_cast<size_t> (points); ++s) { o.lo[s] /= peak; o.hi[s] /= peak; o.rms[s] = std::min (1.0f, o.rms[s] / peak); }
        return o;
    }
    // SENSITIVITY 0..1 -> the weakest transient that still cuts (the page uses the same rule)
    static float sliceThreshold (float sense) noexcept { const float t = 1.0f - sense; return 0.02f + 0.9f * t * t; }

    // Linear read at frame position pos ∈ [0, frames). ch: 0 left, 1 right, 2 mono sum.
    float read (double pos, int ch) const noexcept
    {
        const int i = static_cast<int> (pos);
        const float f = static_cast<float> (pos - i);
        const float a = l[static_cast<size_t> (i)] + f * (l[static_cast<size_t> (i) + 1] - l[static_cast<size_t> (i)]);
        if (ch == 0) return a;
        const float b = r[static_cast<size_t> (i)] + f * (r[static_cast<size_t> (i) + 1] - r[static_cast<size_t> (i)]);
        return ch == 1 ? b : 0.5f * (a + b);
    }
};

} // namespace sg
