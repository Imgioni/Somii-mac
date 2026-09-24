#pragma once

// Normalised control position (0..1, i.e. the 0–10 panel legend / 10) → physical value.
// End points are from the manual; curves are DD-13 (docs/PARAMETERS.md §6).

#include "DspMath.h"

namespace sg::taper
{

// ENV A / D / R: 1 ms … 10 s [manual pp.47–53]
inline float envTime (float x) noexcept    { return 0.001f * std::pow (10.0f, 4.0f * clampf (x, 0.0f, 1.0f)); }
// ENV AH / DH: 0 (no effect) … 10 s [p.47]
inline float holdTime (float x) noexcept   { return x <= 0.0f ? 0.0f : envTime (x); }
// LFO 1 / LFO 2 low-frequency rate: 0.05 … 50 Hz [pp.54, 70]
inline float lfoLowHz (float x) noexcept   { return 0.05f * std::pow (1000.0f, clampf (x, 0.0f, 1.0f)); }
// LFO 1 HF / HF TRK rate: 20 Hz … 20 kHz [p.59, DD-8]
inline float lfoHighHz (float x) noexcept  { return 20.0f * std::pow (1000.0f, clampf (x, 0.0f, 1.0f)); }
// LFO 1 DELAY 0 … 10 s [p.54]; LFO 2 DELAY 0 … 5 s [p.70]
inline float lfo1Delay (float x) noexcept  { return 10.0f * x * x; }
inline float lfo2Delay (float x) noexcept  { return 5.0f * x * x; }
// HPF cutoff: 10 Hz … 5 kHz
inline float hpfHz (float x) noexcept      { return 10.0f * std::pow (500.0f, clampf (x, 0.0f, 1.0f)); }
// DELAY TIME: 1 ms … 1 s [p.65]
// ARP RATE with SYNC off: step length 20 ms … 2000 ms, logarithmic (200 ms at the centre).
inline float arpStepMs (float x) noexcept  { return 20.0f * std::pow (100.0f, clampf (x, 0.0f, 1.0f)); }
inline float delayTime (float x) noexcept  { return 0.001f * std::pow (1000.0f, clampf (x, 0.0f, 1.0f)); }
// PORTAMENTO: 0 = off, max = 10 s per octave [p.74]
inline float portaSecondsPerOctave (float x) noexcept
{
    return x <= 0.0f ? 0.0f : 10.0f * std::pow (10.0f, -4.0f * (1.0f - clampf (x, 0.0f, 1.0f)));
}
// ENV LEVEL / MASTER VOLUME: −∞ … 0 dB (at 80 %) … +4 dB [pp.44, 79]
inline float levelGain (float x) noexcept
{
    x = clampf (x, 0.0f, 1.0f);
    if (x <= 0.8f) { const float r = x / 0.8f; return r * r; }
    return std::pow (10.0f, x - 0.8f);    // +20·(x−0.8) dB
}
// Inverse of levelGain, for defaults expressed in dB.
inline float levelFromGain (float g) noexcept
{
    if (g <= 0.0f) return 0.0f;
    if (g <= 1.0f) return 0.8f * std::sqrt (g);
    return clampf (0.8f + std::log10 (g), 0.8f, 1.0f);
}
// DDS 2 in LFO range: TUNE −7…+7 → ≈ 0.1 … 100 Hz [p.38]
inline float dds2LfoHz (float tuneSemis) noexcept
{
    return 0.1f * std::pow (1000.0f, clampf ((tuneSemis + 7.0f) / 14.0f, 0.0f, 1.0f));
}

// ── Modulation depths (DD-13) ───────────────────────────────────────────────────────────────
// DDS Modulator pitch depth, LFO 1: up to ±12 semitones, quadratic for fine vibrato near zero.
inline float pitchLfoSemis (float x) noexcept   { return 12.0f * x * x; }
// DDS Modulator pitch depth, ENV 1: up to +36 semitones (sync sweeps).
inline float pitchEnvSemis (float x) noexcept   { return 36.0f * x * x; }
// CROSS MOD: exponential FM depth in octaves per unit of modulator.
inline float crossModOctaves (float x) noexcept { return 5.0f * x * x; }
// SUPER detune: outermost sister offset in cents at full depth.
inline float superSpreadCents (float x) noexcept { return 50.0f * x * std::sqrt (x); }
// VCF modulation depths in octaves.
inline float vcfEnvOctaves (float x) noexcept   { return 10.0f * x; }
inline float vcfLfoOctaves (float x) noexcept   { return 4.0f * x * x; }
inline float vcfDds2Octaves (float x) noexcept  { return 4.0f * x * x; }
// DRIFT
inline float driftCents (float x) noexcept      { return 12.0f * x; }
inline float driftOctaves (float x) noexcept    { return 0.25f * x; }
// Bender [p.69]: DDS fader = pitch range, max one octave; VCF fader at max fully opens/closes.
inline float benderSemis (float x) noexcept     { return 12.0f * x; }
inline float benderVcfOctaves (float x) noexcept { return 8.0f * x; }
// LFO 2 [pp.70–72]
inline float lfo2PitchSemis (float x) noexcept  { return 12.0f * x * x; }
inline float lfo2VcfOctaves (float x) noexcept  { return 4.0f * x * x; }
inline float lfo2RateModOctaves (float x) noexcept { return 3.0f * x; }   // push / AT speeds LFO 2 up by up to 3 octaves
// Ribbon: a full-length slide bends one octave (DD-23)
inline constexpr float kRibbonSemis = 12.0f;

} // namespace sg::taper
