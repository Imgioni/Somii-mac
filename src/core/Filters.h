#pragma once

// VCF [manual pp.41–43]: 1-pole 6 dB/oct HPF followed by a 4-pole 24 dB/oct resonant low-pass
// in the style of the SSI polysynth filter (OTA ladder). Both are zero-delay-feedback (TPT)
// designs, so cutoff can be modulated at audio rate (DDS 2 → cutoff) without instability.

#include "DspMath.h"
#include "LayerParams.h"

namespace sg
{

class OnePoleHpf
{
public:
    void reset() noexcept { s = 0.0f; }

    void setCutoff (float hz, float fs) noexcept
    {
        if (hz == lastHz && fs == lastFs) return;
        lastHz = hz; lastFs = fs;
        const float g = fastTan (kPi * clampf (hz, 1.0f, 0.45f * fs) / fs);
        G = g / (1.0f + g);
    }

    float process (float x) noexcept
    {
        const float v = (x - s) * G;
        const float lp = v + s;
        s = lp + v;
        return x - lp;
    }

private:
    float s = 0.0f, G = 0.0f, lastHz = -1.0f, lastFs = -1.0f;
};

class SsiLadder
{
public:
    void reset() noexcept { s1 = s2 = s3 = s4 = 0.0f; }

    // res ∈ [0,1]; the top of the range self-oscillates [p.42].
    void setResonance (float res, Drive d) noexcept
    {
        drive = d;
        k = 4.25f * res * (0.35f + 0.65f * res);
        switch (d)
        {
            case Drive::Off: inGain = 0.35f; comp = 0.0f;  outGain = 1.0f / 0.35f; break;   // clean
            case Drive::One: inGain = 0.35f; comp = 0.85f; outGain = 1.0f / 0.35f;  break;   // subtle + res compensation
            case Drive::Two: inGain = 4.0f;  comp = 0.0f;  outGain = 0.55f;        break;   // heavy overdrive
        }
        inScale = inGain * (1.0f + comp * k);
        updateCoeffs();
    }

    void setCutoff (float hz, float fs) noexcept
    {
        G = gainFor (hz, fs);
        dG = 0.0f;
        rampLeft = 0;
        updateCoeffs();
    }

    // Glide linearly to a new cutoff over `steps` calls of advanceRamp() (control-rate updates).
    void rampCutoff (float hz, float fs, int steps) noexcept
    {
        const float target = gainFor (hz, fs);
        dG = (target - G) / static_cast<float> (steps);
        rampLeft = steps;
    }

    void advanceRamp() noexcept
    {
        if (rampLeft > 0) { G += dG; --rampLeft; updateCoeffs(); }
    }

    float process (float x) noexcept
    {
        // Linear ZDF estimate of the 4th stage, then a saturating OTA input stage:
        //   y4est = (G⁴·xin + S) / (1 + k·G⁴),  S = Σ G^(4−i)·(1−G)·s_i
        //   u     = softClip (xin − k·y4est) = softClip (a·xin − Σ e_i·s_i)
        // with a and e_i precomputed in updateCoeffs(). Each TPT stage is written as
        // y = G·x + (1−G)·s, s' = 2y − s, which keeps the per-sample dependency chain short.
        const float xin = x * inScale;
        const float fb = (e1 * s1 + e2 * s2) + (e3 * s3 + e4 * s4);
        const float u = softClip (a * xin - fb);

        const float y1 = G * u  + b * s1;  s1 = 2.0f * y1 - s1;
        const float y2 = G * y1 + b * s2;  s2 = 2.0f * y2 - s2;
        const float y3 = G * y2 + b * s3;  s3 = 2.0f * y3 - s3;
        const float y4 = G * y3 + b * s4;  s4 = 2.0f * y4 - s4;
        return y4 * outGain;
    }

private:
    static float gainFor (float hz, float fs) noexcept
    {
        const float g = fastTan (kPi * clampf (hz, 5.0f, 0.45f * fs) / fs);
        return g / (1.0f + g);
    }

    // Everything in the ZDF solve that depends only on G and k (changes at most per host sample
    // while ramping, or per sample under audio-rate cutoff modulation).
    void updateCoeffs() noexcept
    {
        b = 1.0f - G;
        const float G2 = G * G;
        const float G4 = G2 * G2;
        const float kd = k / (1.0f + k * G4);
        a = 1.0f - kd * G4;
        e1 = kd * G2 * G * b; e2 = kd * G2 * b; e3 = kd * G * b; e4 = kd * b;
    }

    float s1 = 0.0f, s2 = 0.0f, s3 = 0.0f, s4 = 0.0f;
    float G = 0.5f, dG = 0.0f, k = 0.0f;
    float b = 0.5f, a = 1.0f, e1 = 0.0f, e2 = 0.0f, e3 = 0.0f, e4 = 0.0f;
    int rampLeft = 0;
    float inGain = 0.35f, outGain = 1.0f / 0.35f, comp = 0.0f, inScale = 0.35f;
    Drive drive = Drive::Off;
};

// ── 3rd Wave style filters (VCF STYLE = 3W; SPKR addition, not on the Super Gemini) ──────────
//
// Groove Synthesis 3rd Wave manual pp.53-54: a classic 4-pole 24 dB/octave low-pass whose
// SATURATION control "overdrives the output of the Low-Pass Filter", with resonance compensation
// so that raising RESONANCE does not thin the bass; and, before it in series, a zero-delay 2-pole
// state-variable filter that sweeps low-pass -> notch -> high-pass, has a band-pass switch, cannot
// self-oscillate, and likewise keeps its low end as resonance rises.

// Behavioural four-pole model with optional resonance compensation and output saturation.
// Historical class name: Groove Synthesis identifies the hardware as an SSI2140, not Curtis.
// This approximation is not a component-calibrated model of that circuit.
class CurtisFilter
{
public:
    // the OTA transconductance bend: soft, symmetric, unity slope at zero
    static float ota (float v) noexcept { const float a2 = std::abs (v); return v / (1.0f + a2 * (0.6f + 0.25f * a2)); }

    void reset() noexcept { s1 = s2 = s3 = s4 = 0.0f; }

    void setResonance (float res, bool compensate = true) noexcept
    {
        k = 4.6f * clampf (res, 0.0f, 1.0f);
        comp = compensate ? 1.0f + 0.9f * k : 1.0f;                      // resonance compensation: keep the low end
        updateCoeffs();
    }

    // sat 0..1; 0 bypasses the saturator so the clean path stays clean
    void setSaturation (float sat) noexcept
    {
        saturation = clampf (sat, 0.0f, 1.0f);
        drive = 1.0f + 9.0f * saturation * saturation;
    }

    void setCutoff (float hz, float fs) noexcept { G = gainFor (hz, fs); dG = 0.0f; rampLeft = 0; updateCoeffs(); }

    void rampCutoff (float hz, float fs, int steps) noexcept
    {
        const float target = gainFor (hz, fs);
        dG = (target - G) / static_cast<float> (steps);
        rampLeft = steps;
    }

    void advanceRamp() noexcept { if (rampLeft > 0) { G += dG; --rampLeft; updateCoeffs(); } }

    float process (float x) noexcept
    {
        // This model bends the differential signal in each integrator. The SG model above
        // instead applies its nonlinearity to the input/feedback sum.
        const float fb = (e1 * s1 + e2 * s2) + (e3 * s3 + e4 * s4);
        const float u = a * (x * comp) - fb;

        const float y1 = s1 + G * ota (u  - s1);  s1 = 2.0f * y1 - s1;
        const float y2 = s2 + G * ota (y1 - s2);  s2 = 2.0f * y2 - s2;
        const float y3 = s3 + G * ota (y2 - s3);  s3 = 2.0f * y3 - s3;
        const float y4 = s4 + G * ota (y3 - s4);  s4 = 2.0f * y4 - s4;
        // Continuous onset, no automatic gain compensation: level rises into output clipping.
        return saturation > 1.0e-5f ? softClip (y4 * drive * saturation) / saturation : y4;
    }

private:
    static float gainFor (float hz, float fs) noexcept
    {
        const float g = fastTan (kPi * clampf (hz, 5.0f, 0.45f * fs) / fs);
        return g / (1.0f + g);
    }
    void updateCoeffs() noexcept
    {
        b = 1.0f - G;
        const float G2 = G * G, G4 = G2 * G2;
        const float kd = k / (1.0f + k * G4);
        a = 1.0f - kd * G4;
        e1 = kd * G2 * G * b; e2 = kd * G2 * b; e3 = kd * G * b; e4 = kd * b;
    }

    float s1 = 0.0f, s2 = 0.0f, s3 = 0.0f, s4 = 0.0f;
    float G = 0.5f, dG = 0.0f, k = 0.0f, comp = 1.0f;
    float b = 0.5f, a = 1.0f, e1 = 0.0f, e2 = 0.0f, e3 = 0.0f, e4 = 0.0f;
    int rampLeft = 0;
    float drive = 1.0f, saturation = 0.0f;
};

// Zero-delay 2-pole state-variable filter. MODE sweeps low-pass (0) -> notch (0.5) -> high-pass (1);
// BAND PASS overrides it. Resonance is capped below self-oscillation, as the 3rd Wave's is.
class StateVariable
{
public:
    void reset() noexcept { ic1 = ic2 = 0.0f; }

    void setCutoff (float hz, float fs) noexcept { g = fastTan (kPi * clampf (hz, 10.0f, 0.45f * fs) / fs); updateCoeffs(); }
    void setResonance (float res) noexcept { k = 2.0f - 1.85f * clampf (res, 0.0f, 1.0f); updateCoeffs(); }
    void setMode (float m, bool bandPass) noexcept { mode = clampf (m, 0.0f, 1.0f); band = bandPass; }

    float process (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;

        const float lp = v2, bp = v1, hp = x - k * v1 - v2;   // high-pass is taken off the input
        if (band) return bp * k;                     // normalised so the band-pass sits at unity
        const float notch = lp + hp;
        return mode <= 0.5f ? lp + (notch - lp) * (mode * 2.0f)
                            : notch + (hp - notch) * ((mode - 0.5f) * 2.0f);
    }

private:
    void updateCoeffs() noexcept
    {
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    float g = 0.1f, k = 1.0f, a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f, mode = 0.0f;
    bool band = false;
};

} // namespace sg
