#pragma once

// Per-control-block values derived once per layer from the (smoothed) LayerParams and shared by
// every voice of the layer. Tapers from Tapers.h are applied here, never per voice.

#include "Clock.h"
#include "LayerParams.h"
#include "Sample.h"
#include "Tapers.h"
#include "WaveTable.h"

namespace sg
{

struct LayerControl
{
    // DDS 1
    Dds1Wave dds1Wave = Dds1Wave::Saw;
    int dds1Octave = 0;
    const WaveTable* altA = nullptr;
    const WaveTable* altB = nullptr;
    const Sample* sample = nullptr;   // CUSTOM on with a sample loaded (set by LayerEngine)

    // DDS 2
    Dds2Wave dds2Wave = Dds2Wave::Square;
    bool dds2IsLfo = false;
    int dds2Octave = 0;
    float dds2Tune = 0.0f;
    float dds2LfoHz = 1.0f;
    Dds2Mode dds2Mode = Dds2Mode::Norm;

    // Mixer
    float mix = 0.0f;

    // VCF
    Drive drive = Drive::Off;
    float hpfHz = 10.0f;
    float lpfOctaves = 10.0f;      // fader range in octaves above 20 Hz (keytrack-reduced)
    float lpfFader = 1.0f;
    float res = 0.0f;
    EnvSource envSource = EnvSource::Env1;
    float keytrackK = 0.0f;        // 0, 0.5, 1
    float vcfEnvOct = 0.0f, vcfLfoOct = 0.0f, vcfDds2Oct = 0.0f;
    // 3rd Wave style
    VcfStyle vcfStyle = VcfStyle::Sg;
    float vcfSat = 0.0f, vcfVelOct = 0.0f;
    bool twComp = true;
    float svfEnvOct = 0.0f, svfVelOct = 0.0f, svfKeyK = 0.0f;
    bool svfOn = false, svfBand = false;
    float svfHz = 20000.0f, svfRes = 0.0f, svfModeMix = 0.0f;

    // VCA
    float vcaGain = 1.0f;
    float vcaLfoDepth = 0.0f, vcaDds2Depth = 0.0f;
    VcaEnv vcaEnv = VcaEnv::Env2;
    float dynamicsK = 0.0f;

    // ENV 1 (seconds)
    float e1AH = 0.0f, e1A = 0.001f, e1DH = 0.0f, e1D = 0.5f, e1S = 0.0f, e1R = 0.5f;
    Env1Mode e1Mode = Env1Mode::Normal;
    float e1KeytrackK = 0.0f;
    // ENV 2
    float e2A = 0.001f, e2DH = 0.0f, e2D = 0.5f, e2S = 1.0f, e2R = 0.1f;

    // LFO 1
    Lfo1Wave lfo1Wave = Lfo1Wave::Triangle;
    Lfo1Mode lfo1Mode = Lfo1Mode::FreeNorm;
    bool lfo1PerVoice = false;          // MODE 2: each voice card has its own LFO 1
    float lfo1Hz = 5.0f;           // LF rate, or HF base rate in HF / HF TRK
    float lfo1DelayS = 0.0f;
    float lfo1LrPhase = 0.0f;      // cycles, 0..1
    bool lfo1ShNoise = false;

    // DDS Modulator
    float pitchLfoSemis = 0.0f, pitchEnvSemis = 0.0f;
    OscDest pitchDest = OscDest::Both;
    Tri superMode = Tri::Off;
    float detune = 0.0f;
    float drift = 0.0f;
    float pwmAmount = 0.0f;
    float pwBase = 0.0f;
    PwmSource pwmSource = PwmSource::Manual;
    float xmodOct = 0.0f;

    // Performance section (per patch)
    float benderSemis = 2.0f, benderVcfOct = 0.0f;
    OscDest destOsc = OscDest::Both;
    Lfo2Wave lfo2Wave = Lfo2Wave::Sine;
    float lfo2Hz = 5.0f, lfo2DelayS = 0.0f, lfo2RateModOct = 0.0f;
    Lfo2Trigger lfo2Trigger = Lfo2Trigger::Trig;
    float lfo2PitchSemis = 0.0f, lfo2VcfOct = 0.0f, lfo2VcaDepth = 0.0f;
    float portaSecPerOct = 0.0f;

    // Effects
    int chorus = 0;
    float delaySeconds = 0.35f, delayFeedback = 0.3f, delaySend = 0.0f;
    bool freeze = false;

    // Clock (set by the layer before update()): SYNC makes LFO 1 RATE and DELAY TIME pick note
    // values of the tempo [pp.55–56, 65–66, 92].
    float bpm = 120.0f;
    bool sync = false;

    // Matrix routes and the normalised (smoothed) panel values they modulate.
    MatrixRoutes routes;
    const LayerParams* norm = nullptr;

    // Delay time for a (possibly matrix-modulated) normalised TIME value.
    float delaySecondsFor (float x) const noexcept
    {
        if (sync)
            return static_cast<float> (clockdiv::kDelayBeats[static_cast<size_t> (clockdiv::index (x, 16))] * 60.0 / bpm);
        return taper::delayTime (x);
    }
    // LFO 1 LF rate for a normalised RATE value.
    float lfo1LowHzFor (float x) const noexcept
    {
        if (sync)
            return static_cast<float> (bpm / 60.0 / clockdiv::kLfo1Beats[static_cast<size_t> (clockdiv::index (x, 16))]);
        return taper::lfoLowHz (x);
    }

    static float triK (Tri t) noexcept { return t == Tri::On ? 1.0f : (t == Tri::Half ? 0.5f : 0.0f); }

    void update (const LayerParams& params, const AltWaveBank& bank) noexcept
    {
        norm = &params;
        updateFrom (params, bank);
    }

private:
    void updateFrom (const LayerParams& p, const AltWaveBank& bank) noexcept
    {
        dds1Wave = p.dds1Wave;
        dds1Octave = rangeOctave (p.dds1Range);
        altA = bank.get (p.altA).get();
        altB = bank.get (p.altB).get();

        dds2Wave = p.dds2Wave;
        dds2IsLfo = p.dds2Range == kDds2RangeLfo;
        dds2Octave = rangeOctave (p.dds2Range);
        dds2Tune = p.dds2Tune;
        dds2LfoHz = taper::dds2LfoHz (p.dds2Tune);
        dds2Mode = p.dds2Mode;

        mix = p.mix;

        drive = p.drive;
        vcfStyle = p.vcfStyle;
        const bool tw = vcfStyle == VcfStyle::ThirdWave;
        twComp = p.twComp;
        svfEnvOct = 10.0f * (2.0f * p.svfEnv - 1.0f);
        svfVelOct = 4.0f * p.svfVelocity;
        svfKeyK = 2.0f * p.svfKey;
        vcfSat = p.vcfSat;
        vcfVelOct = tw ? 4.0f * p.vcfVelocity : 0.0f;                       // VELOCITY: up to 4 octaves of cutoff
        svfOn = p.svfOn;
        svfBand = p.svfBand;
        svfRes = p.svfRes;
        svfModeMix = p.svfModeMix;
        svfHz = 20.0f * fastExp2 (11.5f * clampf (p.svfCutoff, 0.0f, 1.0f));
        hpfHz = taper::hpfHz (p.hpf);
        keytrackK = tw ? 2.0f * p.twKey : triK (p.vcfKeytrack);
        // Fully open = 20 Hz · 2^11.5 ≈ 58 kHz (clamped to 0.45 · oversampled rate) so a wide-open
        // 4-pole ladder doesn't dull the top end. With keytrack on, the fader alone can't open the
        // filter fully; the rest of the range is reached by envelope / pedal modulation [p.43, DD-13].
        lpfOctaves = tw ? 11.5f : 11.5f - 2.0f * keytrackK;
        lpfFader = tw ? p.twCutoff : p.lpf;
        res = tw ? p.twRes : p.res;
        envSource = tw ? EnvSource::Env1 : p.envSource;
        vcfEnvOct = tw ? 10.0f * (2.0f * p.twEnv - 1.0f) : taper::vcfEnvOctaves (p.vcfEnvAmt);
        vcfLfoOct = tw ? 0.0f : taper::vcfLfoOctaves (p.vcfLfo1Amt);
        vcfDds2Oct = tw ? 0.0f : taper::vcfDds2Octaves (p.vcfDds2Amt);

        vcaGain = taper::levelGain (p.vcaLevel);
        vcaLfoDepth = p.vcaLfo1Amt;
        vcaDds2Depth = p.vcaDds2Amt;
        vcaEnv = p.vcaEnv;
        dynamicsK = triK (p.dynamics);

        e1AH = taper::holdTime (p.e1AttackHold);
        e1A = taper::envTime (p.e1Attack);
        e1DH = taper::holdTime (p.e1DecayHold);
        e1D = taper::envTime (p.e1Decay);
        e1S = p.e1Sustain;
        e1R = taper::envTime (p.e1Release);
        e1Mode = p.e1Mode;
        e1KeytrackK = triK (p.e1Keytrack);

        e2A = taper::envTime (p.e2Attack);
        e2DH = taper::holdTime (p.e2DecayHold);
        e2D = taper::envTime (p.e2Decay);
        e2S = p.e2Sustain;
        e2R = taper::envTime (p.e2Release);

        lfo1Wave = p.lfo1Wave;
        lfo1Mode = p.lfo1Mode;
        lfo1PerVoice = p.lfo1PhaseMode == Lfo1Phase::PerVoice;
        const bool hf = p.lfo1Wave == Lfo1Wave::HF || p.lfo1Wave == Lfo1Wave::HFTrk;
        sync = p.arpSync;
        lfo1Hz = hf ? taper::lfoHighHz (p.lfo1Rate) : lfo1LowHzFor (p.lfo1Rate);
        lfo1ShNoise = p.lfo1Wave == Lfo1Wave::SampleHold && p.lfo1Rate >= 0.995f;   // S&H at max rate = noise [p.59]
        lfo1DelayS = taper::lfo1Delay (p.lfo1Delay);
        lfo1LrPhase = p.lfo1LrPhase;

        pitchLfoSemis = taper::pitchLfoSemis (p.pitchLfo1Amt);
        pitchEnvSemis = taper::pitchEnvSemis (p.pitchEnv1Amt);
        pitchDest = p.pitchDest;
        superMode = p.superMode;
        detune = p.pwDetune;
        drift = p.drift;
        pwmAmount = p.pwmWave;
        pwBase = p.pwDetune;
        pwmSource = p.pwmSource;
        xmodOct = taper::crossModOctaves (p.crossMod);

        benderSemis = taper::benderSemis (p.benderDds);
        benderVcfOct = taper::benderVcfOctaves (p.benderVcf);
        destOsc = p.destOsc;
        lfo2Wave = p.lfo2Wave;
        lfo2Hz = taper::lfoLowHz (p.lfo2Rate);
        lfo2DelayS = taper::lfo2Delay (p.lfo2Delay);
        lfo2RateModOct = taper::lfo2RateModOctaves (p.lfo2RateMod);
        lfo2Trigger = p.lfo2Trigger;
        lfo2PitchSemis = taper::lfo2PitchSemis (p.lfo2Dds);
        lfo2VcfOct = taper::lfo2VcfOctaves (p.lfo2Vcf);
        lfo2VcaDepth = p.lfo2Vca;
        portaSecPerOct = taper::portaSecondsPerOctave (p.portaTime);

        chorus = p.chorus;
        delaySeconds = delaySecondsFor (p.delayTime);
        delayFeedback = p.delayFeedback;
        delaySend = p.delaySend;
        freeze = p.freeze;

        routes.build (p.matrix);
    }
};

} // namespace sg
