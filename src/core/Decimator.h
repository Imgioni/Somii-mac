#pragma once

// Cascaded linear-phase half-band FIR decimator: oversampled layer bus → host rate.
// Voices are summed at the oversampled rate first, so this runs once per layer channel, not
// once per voice. The last (2×→1×) stage is long and steep (passband flat to 20 kHz at 48 kHz),
// the earlier stages are short because their transition bands are wide.

#include <vector>

namespace sg
{

class HalfbandStage
{
public:
    void design (int taps, double kaiserBeta);   // taps must be 4k + 3
    void reserve (int maxInputSamples) { work.reserve (static_cast<size_t> (historyLen + maxInputSamples)); }
    void reset();
    // in: 2·nOut samples, out: nOut samples.
    void process (const float* in, float* out, int nOut);
    int latencyInputSamples() const noexcept { return (numTaps - 1) / 2; }

private:
    int numTaps = 0;
    std::vector<float> oddTaps;    // h at centre ± (2j+1), j = 0..
    std::vector<float> work;       // history + block
    int historyLen = 0;
};

class Decimator
{
public:
    // factor ∈ {1, 2, 4, 8, 16}
    void prepare (int oversamplingFactor, int maxHostBlock);
    void reset();
    // in: factor·n samples (clobbered), out: n samples.
    void process (float* in, float* out, int n);
    int getFactor() const noexcept { return factor; }
    float latencyHostSamples() const noexcept;

private:
    int factor = 1;
    std::vector<HalfbandStage> stages;
    std::vector<float> scratch;
};

} // namespace sg
