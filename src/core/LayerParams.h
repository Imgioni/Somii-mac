#pragma once

// Plain-data view of one layer's patch parameters, as consumed by the DSP core.
// Continuous panel controls are normalised 0..1 (panel legend 0–10 ÷ 10) unless noted.
// Page numbers refer to the Owner's Manual; IDs are in docs/PARAMETERS.md.

#include "Matrix.h"

namespace sg
{

enum class Dds1Wave  { Sine, Saw, Square, Triangle, Noise, Alt };          // p.32
enum class Dds2Wave  { Sine, Saw, Square, Triangle, Noise, Pulse };        // p.36
enum class Dds2Mode  { Norm, Ring, Sync };                                 // p.36–37; LFO range: SubOff/SubSquare/SubSine
enum class Tri       { Off, Half, On };                                    // KEYTRACK, DYNAMICS, SUPER
enum class Drive     { Off, One, Two };                                    // p.42
// VCF STYLE (SPKR addition): SG = the Super Gemini's SSI ladder, ThirdWave = the Groove Synthesis
// 3rd Wave's cleaner 4-pole with output saturation, preceded by its state-variable filter.
enum class VcfStyle  { Sg, ThirdWave };
enum class SvfMode   { Lp, Notch, Hp };   // the MODE knob sweeps between these
enum class EnvSource { Env1, Both, Env2 };                                 // p.42
enum class VcaEnv    { Env2, Gate, GateRelease };                          // p.45
enum class Env1Mode  { Normal, Inverted, Loop };                           // p.48
enum class Lfo1Wave  { Triangle, RevSaw, SampleHold, Square, HF, HFTrk };  // p.58–59
enum class Lfo1Mode  { FreeNorm, OnceDds1, ResetDds2 };  // p.59
// SPKR addition, not on the hardware: how the per-voice LFO 1s relate in FREE mode.
// Locked = every voice steps together (how 002 has always behaved); PerVoice = each voice card
// runs its own LFO with its own phase and a slight rate tolerance, like the real analog voices.
enum class Lfo1Phase { Locked, PerVoice };
enum class OscDest   { Dds1, Both, Dds2 };                                 // p.60, p.72
enum class PwmSource { Manual, Lfo1, Env1 };                               // p.62
enum class Lfo2Wave  { Sine, RevSaw, SampleHold, Square, Saw, Noise };     // p.70
enum class Lfo2Trigger { Trig, AtTrig, On };                               // p.71
enum class VoiceMode { Solo, Legato, Poly1, Poly2 };                       // p.90
enum class ArpMode   { Up, Down, UpDown, Random, Seq };                    // p.94

inline constexpr int kDds1RangeCount = 6;   // 64' 32' 16' 8' 4' 2'
inline constexpr int kDds2RangeLfo   = 0;   // LFO 32' 16' 8' 4' 2'

// Octave offset relative to 8' (index 3) for both DDS range switches.
inline constexpr int rangeOctave (int index) noexcept { return index - 3; }

struct LayerParams
{
    // DDS 1 [pp.31–34]
    Dds1Wave dds1Wave  = Dds1Wave::Saw;
    int      dds1Range = 3;          // 8'
    int      altA      = 0;          // W1 (0-based)
    int      altB      = 1;          // W2

    // DDS 1 CUSTOM (SPKR): a user sample plays in place of the waveform
    bool  smpOn    = false;
    bool  smpLoop  = false;
    bool  smpSlice = false;          // SLICE: each key from ROOT KEY up plays one slice at its own speed
    int   smpSlices = 0;             // 0 = AUTO (transients), else 4 / 8 / 16 / 32 / 64 equal slices
    float smpSense = 0.5f;           // AUTO: how many transients cut
    float smpStart = 0.0f;           // fraction of the sample
    float smpEnd   = 1.0f;
    float smpLoopStart = 0.0f;       // LOOP returns here from END (playback starts at smpStart)
    float smpLevel = 0.8f;           // T-dB like VCA LEVEL: 0.8 = 0 dB
    int   smpRoot  = 60;             // the key that plays the sample at its own pitch
    float smpFine  = 0.0f;           // cents

    // DDS 2 [pp.35–39]
    Dds2Wave dds2Wave  = Dds2Wave::Square;
    int      dds2Range = 3;          // 8'
    float    dds2Tune  = 0.0f;       // semitones −7…+7
    Dds2Mode dds2Mode  = Dds2Mode::Norm;

    // Mixer [p.40]
    float mix = 0.0f;                // 0 = DDS 1 only, 0.5 = equal, 1 = DDS 2 / sub
    float pan = 0.0f;                // −1 … +1

    // VCF [pp.41–43]
    Drive     drive      = Drive::Off;
    float     hpf        = 0.0f;
    float     lpf        = 1.0f;
    float     res        = 0.0f;
    EnvSource envSource  = EnvSource::Env1;
    Tri       vcfKeytrack = Tri::Off;
    float     vcfEnvAmt  = 0.0f;
    float     vcfLfo1Amt = 0.0f;
    float     vcfDds2Amt = 0.0f;
    // Independent 3rd Wave filter bank; envelope amounts are centred at 0.5.
    float twCutoff = 1.0f, twRes = 0.0f, twEnv = 0.5f, twKey = 0.0f;
    bool twComp = true;
    float svfEnv = 0.5f, svfVelocity = 0.0f, svfKey = 0.0f;
    VcfStyle  vcfStyle    = VcfStyle::Sg;
    float     vcfSat      = 0.0f;     // SATURATION: overdrives the low-pass output
    float     vcfVelocity = 0.0f;     // VELOCITY: key velocity to cutoff
    bool      svfOn       = false;
    float     svfCutoff   = 1.0f;
    float     svfRes      = 0.0f;
    float     svfModeMix  = 0.0f;     // 0 low-pass, 0.5 notch, 1 high-pass
    bool      svfBand     = false;    // BAND PASS switch

    // VCA [pp.44–45]
    float  vcaLevel   = 0.8f;        // T-dB, 0.8 = 0 dB
    float  vcaLfo1Amt = 0.0f;
    float  vcaDds2Amt = 0.0f;
    VcaEnv vcaEnv     = VcaEnv::Env2;
    Tri    dynamics   = Tri::Off;

    // ENV 1 [pp.46–51]
    float    e1AttackHold = 0.0f;
    float    e1Attack     = 0.0f;
    float    e1DecayHold  = 0.0f;
    float    e1Decay      = 0.2236f;  // 500 ms (taper::envTime)
    float    e1Sustain    = 0.0f;
    float    e1Release    = 0.2236f;
    Env1Mode e1Mode       = Env1Mode::Normal;
    Tri      e1Keytrack   = Tri::Off;

    // ENV 2 [pp.52–53]
    float e2Attack    = 0.0f;
    float e2DecayHold = 0.0f;
    float e2Decay     = 0.2236f;
    float e2Sustain   = 1.0f;
    float e2Release   = 0.1f;         // 100 ms

    // LFO 1 [pp.54–59]
    Lfo1Wave lfo1Wave    = Lfo1Wave::Triangle;
    float    lfo1Rate    = 0.6667f;   // ≈ 5 Hz on the LF taper
    float    lfo1Delay   = 0.0f;
    float    lfo1LrPhase = 0.0f;
    Lfo1Mode lfo1Mode    = Lfo1Mode::FreeNorm;
    Lfo1Phase lfo1PhaseMode = Lfo1Phase::Locked;   // MODE 1 / MODE 2

    // DDS Modulator [pp.60–63]
    float     pitchLfo1Amt = 0.0f;
    float     pitchEnv1Amt = 0.0f;
    OscDest   pitchDest    = OscDest::Both;
    Tri       superMode    = Tri::Off;
    float     pwDetune     = 0.0f;
    float     drift        = 0.0f;
    float     pwmWave      = 0.0f;
    PwmSource pwmSource    = PwmSource::Manual;
    float     crossMod     = 0.0f;

    // Performance-section values stored per patch [pp.68–76]
    float       benderDds   = 2.0f / 12.0f;   // 0..1 = 0..12 semitones (max one octave) [p.69]
    float       benderVcf   = 0.0f;
    OscDest     destOsc     = OscDest::Both;  // DEST osc selector for bender + LFO 2 pitch [p.72]
    Lfo2Wave    lfo2Wave    = Lfo2Wave::Sine;
    float       lfo2Rate    = 0.6667f;        // ≈ 5 Hz
    float       lfo2Delay   = 0.0f;
    Lfo2Trigger lfo2Trigger = Lfo2Trigger::Trig;
    float       lfo2RateMod = 0.0f;
    float       lfo2Dds     = 0.2f;           // light vibrato on the mod wheel (DD-7)
    float       lfo2Vcf     = 0.0f;
    float       lfo2Vca     = 0.0f;
    float       portaTime   = 0.0f;
    int         octave      = 0;              // −2 … +2 [p.75]

    // Voice assign [pp.90–91]
    VoiceMode voiceMode  = VoiceMode::Poly1;
    bool      unison     = false;
    int       unisonSize = 2;                 // 1 half · 2 all · 3 octave · 4 fifth + octave
    bool      binaural   = true;

    // Effects [pp.64–67]
    int   chorus        = 0;          // 0 off, 1 = I, 2 = II, 3 = I+II
    float delayTime     = 0.8481f;    // ≈ 350 ms (DD-7)
    float delayFeedback = 0.3f;
    float delaySend     = 0.0f;
    bool  freeze        = false;

    // Arpeggiator / sequencer [pp.92–98]
    bool    arpOn       = false;
    int     arpClockDiv = 4;          // 1/16 [p.92]
    bool    arpSync     = false;      // SYNC: LFO 1 rate, delay time and the arp step follow the clock
    float   arpRate     = 0.5f;       // SYNC off: free arp step length (taper::arpStepMs)
    int     arpRange    = 1;          // 1–4 octaves
    int     arpSwing    = 0;          // 0–4
    ArpMode arpMode     = ArpMode::Up;
    int     seqSlot     = 0;          // linked sequence 0–15

    // Modulation matrix amounts, −1 … +1 [pp.84–89]
    float matrix[kMatrixSources][kMatrixDests] {};
};

} // namespace sg
