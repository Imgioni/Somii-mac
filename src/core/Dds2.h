#pragma once

// DDS 2 — algorithmic core [manual pp.35–39]: SINE/SAW/SQUARE/TRIANGLE/NOISE/PULSE, phase reset
// to zero on every note [p.35], hard SYNC to DDS 1 [p.37]. RING and the sub-oscillator are
// assembled in the voice because they need the DDS 1 signal.

#include "LayerParams.h"
#include "PhaseOsc.h"

namespace sg
{

inline Shape dds2Shape (Dds2Wave w) noexcept
{
    switch (w)
    {
        case Dds2Wave::Sine:     return Shape::Sine;
        case Dds2Wave::Saw:      return Shape::Saw;
        case Dds2Wave::Square:   return Shape::Square;
        case Dds2Wave::Triangle: return Shape::Triangle;
        case Dds2Wave::Noise:    return Shape::Noise;
        case Dds2Wave::Pulse:    return Shape::Pulse;
    }
    return Shape::Saw;
}

class Dds2
{
public:
    void prepare (uint32_t seed) { rng.seed (seed); osc.reset(); }
    void noteOn() noexcept { osc.resetRequested = true; }

    void setup (Dds2Wave w, float incPerSample, float pulseWidth) noexcept
    {
        shape = dds2Shape (w);
        inc = incPerSample;
        pw = clampf (pulseWidth, 0.02f, 0.98f);
    }

    // syncAt = DDS 1 centroid wrap fraction when SYNC is active, else -1.
    float tick (float fm, float syncAt) noexcept
    {
        float wrap;
        value = osc.tick (shape, clampf (inc * fm, 0.0f, 0.45f), pw, syncAt, rng, wrap);
        return value;
    }

    float getValue() const noexcept { return value; }
    float getPhase() const noexcept { return osc.phase; }

private:
    PhaseOsc osc;
    Rng rng;
    Shape shape = Shape::Square;
    float inc = 0.0f, pw = 0.5f, value = 0.0f;
};

// Sub-oscillator: one octave below DDS 1, square or sine, only with DDS 2 RANGE = LFO [p.39].
class SubOsc
{
public:
    void reset() noexcept { osc.reset(); wrapCount = 0; }

    float tick (bool square, float dds1Inc, float centroidWrap) noexcept
    {
        float sync = -1.0f;
        if (centroidWrap >= 0.0f)
        {
            if ((wrapCount & 1) == 0) sync = centroidWrap;   // lock to every second DDS 1 cycle
            ++wrapCount;
        }
        float wrap;
        return osc.tick (square ? Shape::Square : Shape::Sine, 0.5f * dds1Inc, 0.5f, sync, rng, wrap);
    }

private:
    PhaseOsc osc;
    Rng rng;
    unsigned wrapCount = 0;
};

} // namespace sg
