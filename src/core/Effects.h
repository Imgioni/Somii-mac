#pragma once

// Per-layer effects [manual pp.64–67]: a classic dual-mode stereo chorus followed by a stereo delay
// fed from a SEND (the dry signal always passes, p.xiii / p.67). Chorus → delay in series [p.64].

#include "DspMath.h"

#include <vector>

namespace sg
{

// Power-of-two ring buffer with 4-point (Catmull-Rom) interpolated reads.
class DelayLine
{
public:
    void prepare (int maxDelaySamples);
    void clear() noexcept;
    void push (float x) noexcept { buf[static_cast<size_t> (w)] = x; w = (w + 1) & mask; }
    // Sample written `delay` samples before the most recent push (delay ≥ 1).
    float read (float delay) const noexcept;

private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};

class Chorus
{
public:
    enum Mode { Off = 0, I = 1, II = 2, Both = 3 };   // CHORUS I, II, I+II [p.64]

    void prepare (double sampleRate);
    void reset() noexcept;
    void setMode (int mode) noexcept { pendingMode = mode < 0 ? 0 : (mode > 3 ? 3 : mode); }
    void process (float* l, float* r, int n) noexcept;

private:
    float tap (int ch, float delayMs) const noexcept { return lines[ch].read (delayMs * msToSamples); }

    double fs = 48000.0;
    float msToSamples = 48.0f;
    DelayLine lines[2];
    int activeMode = 0, pendingMode = 0;
    float wetGain = 0.0f, wetStep = 0.001f;
    float phaseSlow = 0.0f, phaseFast = 0.0f;
    float lpA = 0.5f;
    float lp1[2] {}, lp2[2] {};
};

class StereoDelay
{
public:
    void prepare (double sampleRate, double maxSeconds);
    void reset() noexcept;
    // time in seconds, feedback 0…1 (1 = infinite repeats, p.66), send 0…1 [p.67].
    // freeze: new input stops entering the loop and the loop recirculates unchanged [p.67].
    void setParams (float timeSeconds, float feedback, float send, bool freeze) noexcept;
    void process (float* l, float* r, int n) noexcept;   // in place: out = in + echoes
    float getTimeSeconds() const noexcept { return timeSmoothed / static_cast<float> (fs); }

private:
    double fs = 48000.0;
    DelayLine lines[2];
    float maxSamples = 48000.0f;
    float timeTarget = 16800.0f, timeSmoothed = 16800.0f;
    float fbTarget = 0.3f, fb = 0.3f, sendTarget = 0.0f, send = 0.0f;
    float freezeTarget = 0.0f, freezeMix = 0.0f;
    float timeCoeff = 0.0f, gainCoeff = 0.0f, freezeCoeff = 0.0f;
};

} // namespace sg
