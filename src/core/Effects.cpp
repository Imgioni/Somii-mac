#include "Effects.h"

namespace sg
{

// ── DelayLine ───────────────────────────────────────────────────────────────────────────────

void DelayLine::prepare (int maxDelaySamples)
{
    int size = 1;
    while (size < maxDelaySamples + 8) size <<= 1;
    buf.assign (static_cast<size_t> (size), 0.0f);
    mask = size - 1;
    w = 0;
}

void DelayLine::clear() noexcept
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    w = 0;
}

float DelayLine::read (float delay) const noexcept
{
    const float pos = static_cast<float> (w - 1) - delay;
    const float fl = std::floor (pos);
    const float f = pos - fl;
    const int i = static_cast<int> (fl);
    const float ym1 = buf[static_cast<size_t> ((i - 1) & mask)];
    const float y0  = buf[static_cast<size_t> (i & mask)];
    const float y1  = buf[static_cast<size_t> ((i + 1) & mask)];
    const float y2  = buf[static_cast<size_t> ((i + 2) & mask)];
    const float c1 = 0.5f * (y1 - ym1);
    const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
    const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
    return ((c3 * f + c2) * f + c1) * f + y0;
}

// ── Chorus ──────────────────────────────────────────────────────────────────────────────────
// Modelled on the classic BBD dual-mode chorus: I = slow and subtle, II = denser and faster,
// I+II = a three-phase string-ensemble chorus [p.64] (DD-53). The wet path gets a gentle
// BBD-style low-pass and soft saturation.

void Chorus::prepare (double sampleRate)
{
    fs = sampleRate;
    msToSamples = static_cast<float> (fs / 1000.0);
    lines[0].prepare (static_cast<int> (0.030 * fs));
    lines[1].prepare (static_cast<int> (0.030 * fs));
    wetStep = 1.0f / static_cast<float> (0.020 * fs);            // 20 ms crossfade on mode changes
    lpA = 1.0f - std::exp (-kTwoPi * 9000.0f / static_cast<float> (fs));
    reset();
}

void Chorus::reset() noexcept
{
    lines[0].clear();
    lines[1].clear();
    activeMode = pendingMode;
    wetGain = activeMode != Off ? 1.0f : 0.0f;
    phaseSlow = phaseFast = 0.0f;
    for (int c = 0; c < 2; ++c) lp1[c] = lp2[c] = 0.0f;
}

void Chorus::process (float* l, float* r, int n) noexcept
{
    if (activeMode == Off && pendingMode == Off && wetGain <= 0.0f)
    {
        // Keep the lines filled so switching on doesn't start from silence.
        for (int i = 0; i < n; ++i) { lines[0].push (l[i]); lines[1].push (r[i]); }
        return;
    }

    const float invFs = 1.0f / static_cast<float> (fs);
    for (int i = 0; i < n; ++i)
    {
        // Mode changes crossfade through zero wet level.
        if (pendingMode != activeMode)
        {
            wetGain -= wetStep;
            if (wetGain <= 0.0f) { wetGain = 0.0f; activeMode = pendingMode; }
        }
        else if (activeMode != Off && wetGain < 1.0f)
        {
            wetGain = std::min (1.0f, wetGain + wetStep);
        }

        const float inL = l[i], inR = r[i];
        lines[0].push (softClip (inL * 0.8f) * 1.25f);
        lines[1].push (softClip (inR * 0.8f) * 1.25f);

        float wL = 0.0f, wR = 0.0f;
        switch (activeMode)
        {
            case I:
            {
                phaseSlow += 0.513f * invFs; if (phaseSlow >= 1.0f) phaseSlow -= 1.0f;
                const float t = 4.0f * std::abs (phaseSlow - 0.5f) - 1.0f;             // triangle −1…1
                wL = tap (0, 3.5f + 1.6f * t);
                wR = tap (1, 3.5f - 1.6f * t);
                break;
            }
            case II:
            {
                phaseSlow += 0.863f * invFs; if (phaseSlow >= 1.0f) phaseSlow -= 1.0f;
                const float t = 4.0f * std::abs (phaseSlow - 0.5f) - 1.0f;
                wL = 0.5f * (tap (0, 3.2f + 2.1f * t) + tap (0, 5.4f - 1.4f * t));
                wR = 0.5f * (tap (1, 3.2f - 2.1f * t) + tap (1, 5.4f + 1.4f * t));
                break;
            }
            case Both:
            {
                phaseSlow += 0.62f * invFs; if (phaseSlow >= 1.0f) phaseSlow -= 1.0f;
                phaseFast += 5.9f * invFs;  if (phaseFast >= 1.0f) phaseFast -= 1.0f;
                for (int k = 0; k < 3; ++k)
                {
                    const float off = static_cast<float> (k) / 3.0f;
                    const float dL = 5.0f + 1.1f * sin2pi (phaseSlow + off) + 0.18f * sin2pi (phaseFast + off);
                    const float dR = 5.0f + 1.1f * sin2pi (phaseSlow + off + 0.1667f) + 0.18f * sin2pi (phaseFast + off + 0.1667f);
                    wL += tap (0, dL);
                    wR += tap (1, dR);
                }
                wL *= 0.4f; wR *= 0.4f;
                break;
            }
            default: break;
        }

        // BBD-style gentle low-pass on the wet signal
        lp1[0] += lpA * (wL - lp1[0]); lp2[0] += lpA * (lp1[0] - lp2[0]);
        lp1[1] += lpA * (wR - lp1[1]); lp2[1] += lpA * (lp1[1] - lp2[1]);

        const float dry = 1.0f - 0.2f * wetGain;
        const float wet = 0.7f * wetGain;
        l[i] = dry * inL + wet * lp2[0];
        r[i] = dry * inR + wet * lp2[1];
    }
}

// ── StereoDelay ─────────────────────────────────────────────────────────────────────────────

void StereoDelay::prepare (double sampleRate, double maxSeconds)
{
    fs = sampleRate;
    maxSamples = static_cast<float> (maxSeconds * fs);
    lines[0].prepare (static_cast<int> (maxSamples) + 4);
    lines[1].prepare (static_cast<int> (maxSamples) + 4);
    timeCoeff = onePoleCoeff (0.060f, static_cast<float> (fs));    // tape-like glide when TIME moves
    gainCoeff = onePoleCoeff (0.010f, static_cast<float> (fs));
    freezeCoeff = onePoleCoeff (0.004f, static_cast<float> (fs));
    reset();
}

void StereoDelay::reset() noexcept
{
    lines[0].clear();
    lines[1].clear();
    timeSmoothed = timeTarget;
    fb = fbTarget;
    send = sendTarget;
    freezeMix = freezeTarget;
}

void StereoDelay::setParams (float timeSeconds, float feedback, float sendLevel, bool freeze) noexcept
{
    timeTarget = clampf (timeSeconds * static_cast<float> (fs), 2.0f, maxSamples);
    fbTarget = clampf (feedback, 0.0f, 1.0f);
    sendTarget = clampf (sendLevel, 0.0f, 1.0f);
    freezeTarget = freeze ? 1.0f : 0.0f;
}

void StereoDelay::process (float* l, float* r, int n) noexcept
{
    for (int i = 0; i < n; ++i)
    {
        timeSmoothed += (timeTarget - timeSmoothed) * timeCoeff;
        fb += (fbTarget - fb) * gainCoeff;
        send += (sendTarget - send) * gainCoeff;
        freezeMix += (freezeTarget - freezeMix) * freezeCoeff;

        // FEEDBACK at max (or FREEZE) recirculates at exactly unity: no decay, no filtering [p.66].
        const float loopGain = fb + (1.0f - fb) * freezeMix;
        const float inGain = send * (1.0f - freezeMix);

        const float d = timeSmoothed - 1.0f;
        const float yL = lines[0].read (d);
        const float yR = lines[1].read (d);

        // Linear up to ±1.5; beyond that a soft knee stops a full-feedback loop with constant input
        // from running away (DD-54).
        auto guard = [] (float x) noexcept
        {
            const float a = std::abs (x);
            if (a <= 1.5f) return x;
            const float over = a - 1.5f;
            return std::copysign (1.5f + over / (1.0f + over), x);
        };
        lines[0].push (guard (inGain * l[i] + loopGain * yL));
        lines[1].push (guard (inGain * r[i] + loopGain * yR));

        l[i] += yL;
        r[i] += yR;
    }
}

} // namespace sg
