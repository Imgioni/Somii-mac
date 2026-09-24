#include "Decimator.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace sg
{

namespace
{
double besselI0 (double x)
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 50; ++k)
    {
        term *= (x / (2.0 * k)) * (x / (2.0 * k));
        sum += term;
        if (term < 1e-12 * sum) break;
    }
    return sum;
}
} // namespace

void HalfbandStage::design (int taps, double beta)
{
    numTaps = taps;
    const int centre = (taps - 1) / 2;
    const double i0b = besselI0 (beta);

    oddTaps.clear();
    double sum = 0.0;
    for (int off = 1; off <= centre; off += 2)
    {
        const double x = static_cast<double> (off);
        const double sinc = std::sin (0.5 * 3.14159265358979323846 * x) / (3.14159265358979323846 * x);
        const double r = static_cast<double> (off) / centre;
        const double w = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
        oddTaps.push_back (static_cast<float> (sinc * w));
        sum += 2.0 * sinc * w;
    }
    // Normalise to unity DC gain: centre tap is 0.5, odd taps sum to 0.5.
    const double scale = 0.5 / sum;
    for (auto& h : oddTaps) h = static_cast<float> (h * scale);

    historyLen = taps - 1;
    reset();
}

void HalfbandStage::reset()
{
    work.assign (static_cast<size_t> (historyLen), 0.0f);
}

void HalfbandStage::process (const float* in, float* out, int nOut)
{
    const int nIn = 2 * nOut;
    work.resize (static_cast<size_t> (historyLen + nIn));
    std::memcpy (work.data() + historyLen, in, sizeof (float) * static_cast<size_t> (nIn));

    const int centre = (numTaps - 1) / 2;
    const int nOdd = static_cast<int> (oddTaps.size());
    const float* w = work.data();

    for (int k = 0; k < nOut; ++k)
    {
        // Output aligned to input index 2k+1; newest sample of the window is w[2k+1 + historyLen].
        const float* c = w + (2 * k + 1) + historyLen - centre;   // centre sample
        float acc = 0.5f * c[0];
        for (int j = 0; j < nOdd; ++j)
        {
            const int o = 2 * j + 1;
            acc += oddTaps[static_cast<size_t> (j)] * (c[-o] + c[o]);
        }
        out[k] = acc;
    }

    std::memmove (work.data(), work.data() + nIn, sizeof (float) * static_cast<size_t> (historyLen));
    work.resize (static_cast<size_t> (historyLen));
}

void Decimator::prepare (int oversamplingFactor, int maxHostBlock)
{
    factor = oversamplingFactor;
    stages.clear();
    int stageCount = 0;
    for (int f = factor; f > 1; f >>= 1) ++stageCount;
    stages.resize (static_cast<size_t> (stageCount));

    // Stage order: highest rate first. Only the final stage needs a steep transition.
    for (int s = 0; s < stageCount; ++s)
    {
        const bool last = (s == stageCount - 1);
        stages[static_cast<size_t> (s)].design (last ? 127 : 31, last ? 9.5 : 8.0);
        stages[static_cast<size_t> (s)].reserve (std::max (1, maxHostBlock) * factor);
    }
    scratch.assign (static_cast<size_t> (std::max (1, maxHostBlock) * std::max (1, factor)), 0.0f);
}

void Decimator::reset()
{
    for (auto& s : stages) s.reset();
}

void Decimator::process (float* in, float* out, int n)
{
    if (factor <= 1)
    {
        std::memcpy (out, in, sizeof (float) * static_cast<size_t> (n));
        return;
    }
    int len = n * factor;
    float* src = in;
    for (size_t s = 0; s < stages.size(); ++s)
    {
        const bool last = (s + 1 == stages.size());
        float* dst = last ? out : scratch.data();
        stages[s].process (src, dst, len / 2);
        len /= 2;
        if (! last)
        {
            // Move result back into `in` so the next stage reads from a buffer it may overwrite.
            std::memcpy (in, dst, sizeof (float) * static_cast<size_t> (len));
            src = in;
        }
    }
}

float Decimator::latencyHostSamples() const noexcept
{
    float lat = 0.0f;
    float rateDiv = static_cast<float> (factor);
    for (const auto& s : stages)
    {
        lat += static_cast<float> (s.latencyInputSamples()) / rateDiv;
        rateDiv *= 0.5f;
    }
    return lat;
}

} // namespace sg
