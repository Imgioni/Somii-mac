#pragma once

// One complete monaural voice [manual p.xiii signal path]:
//   DDS 1 + DDS 2 (or sub) → MIX → HPF → LPF (SSI ladder, DRIVE) → VCA
// with ENV 1, ENV 2, the fixed VCA envelopes, LFO 2 (per voice), portamento, DRIFT, per-voice
// analog tolerances and the modulation matrix. A binaural "super voice" is two of these.

#include "Dds1.h"
#include "Dds2.h"
#include "Envelope.h"
#include "Filters.h"
#include "LayerControl.h"
#include "Lfo1.h"

#include <array>

namespace sg
{

// Performance state shared by every voice of a layer (set by LayerEngine / PerformanceEngine).
struct LayerMods
{
    float pitchOffset = 0.0f;     // octave·12 + transpose + fine tune + detunes, semitones
    float keyOffset = 0.0f;       // octave·12 + transpose (what keytracking follows)
    float bend = 0.0f;            // bender horizontal / pitch wheel, −1 … +1 [p.69]
    float push = 0.0f;            // bender vertical / mod wheel (CC 1), 0 … 1
    float channelAT = 0.0f;       // channel pressure, 0 … 1
    float expression = 0.0f;      // expression pedal (CC 11), 0 … 1
    float ribbonPos = 0.0f;       // ribbon position 0 … 1 (matrix source)
    float ribbonSemis = 0.0f;     // relative ribbon bend in pitch mode [p.77]
    bool ribbonToPitch = true;    // off once RIBN is used in the matrix [p.77]
    double lfo2Phase = 0.0;       // layer-wide LFO 2 phase (cycles), panel LED only
    float lfo2Inc = 0.0f;         // layer-wide LFO 2 cycles per host sample
};

class Voice
{
public:
    static constexpr float kOutputGain = 0.25f;
    // Pitch, cutoff, shape and matrix modulation are recomputed every kControlDivider host samples
    // (12 kHz at 48 kHz) with the filter coefficient interpolated in between; envelopes and VCA
    // gain run every sample (DD-38).
    static constexpr int kControlDivider = 4;
    static constexpr float kStealFadeSeconds = 0.003f;

    void prepare (float hostRate, int oversampling, uint32_t seed, bool analogTolerance);
    void reset();

    void setMirrored (bool m) noexcept { mirrored = m; }
    void setSampleChannel (int ch) noexcept { sampleCh = ch; }   // CUSTOM: 0 left, 1 right, 2 mono sum

    // declick: if the voice is still sounding, fade it out over 3 ms before the new note starts
    // (voice stealing). glideFrom: the note portamento slides from [p.73] - kGlideOwn = this voice's
    // own pitch (mono), kNoGlide = none, else a note number (poly: the layer's last played note).
    static constexpr float kNoGlide = -1.0f, kGlideOwn = -2.0f;
    void start (int note, float velocity, float unisonSemis, bool declick, float glideFrom);
    // LEGATO: new pitch without retriggering the envelopes [p.90].
    void changeNote (int note, float unisonSemis) noexcept;
    void release() noexcept;
    void kill() noexcept;
    void setPolyAftertouch (float v) noexcept { polyAT = clampf (v, 0.0f, 1.0f); }
    // the performance trigger toggle pulls every voice's LFO 2 back into phase [p.72]
    void resyncLfo2() noexcept { lfo2Phase = 0.0; }

    bool isActive() const noexcept   { return active; }
    bool isGateOn() const noexcept   { return gate || stealing; }   // a stealing voice already belongs to its new note
    bool isStealing() const noexcept { return stealing; }
    int getNote() const noexcept     { return stealing ? pending.note : note; }
    float getLevel() const noexcept  { return lastVcaEnv; }
    float getMod (MDest d) const noexcept { return modSum[static_cast<size_t> (d)]; }
    unsigned getEnv1Cycles() const noexcept { return env1.getCycleCount(); }
    float getEnv1Level() const noexcept { return env1.getLevel(); }
    float getCutoffOctaves() const noexcept { return cutOctBase; }   // control-rate cutoff, octaves above 20 Hz
    float getPitch() const noexcept  { return pitchNow; }
    float samplePlayhead() const noexcept { return active ? dds1.samplePlayhead() : -1.0f; }

    // Once per control block (≤ 32 host samples).
    void setupBlock (const LayerControl& c, float blockSeconds) noexcept;
    // Runs n (≤ kMaxGroup) host samples: per-sample control (envelopes, VCA gain, control-rate
    // updates) followed by one tight render loop of n × os oversampled samples into out.
    // lfoValue / lfoUni: the shared LFO 1 at this voice's phase offset, one value per host sample.
    static constexpr int kMaxGroup = 8;
    void process (const LayerControl& c, const LayerMods& m, int sampleInBlock, int n,
                  const float* lfoValue, const float* lfoUni, float* out, int os) noexcept;
    // Same as process() for two active voices (a super voice's pair). Their audio paths are
    // interleaved sample by sample so the CPU overlaps the two latency-bound filter recursions.
    static void processPair (Voice& a, Voice& b, const LayerControl& c, const LayerMods& m, int sampleInBlock, int n,
                             const float* lvA, const float* luA, const float* lvB, const float* luB,
                             float* outA, float* outB, int os) noexcept;

    float getHfPhase() const noexcept { return hfPhase; }
    void setHfPhase (float p) noexcept { hfPhase = p; }
    void setHfOffset (float off) noexcept { hfOffset = off; }

private:
    float control (const LayerControl& c, const LayerMods& m, int sampleInBlock, float lfoValue, float lfoUni) noexcept;
    void render (float* out, int n, int os, const float* gains) noexcept;
    float renderSample (float gainBase) noexcept;   // one oversampled sample of the audio path
    void begin (int note, float velocity, float unisonSemis, float glideFrom, bool resetEnvelopes);
    void fullUpdate (const LayerControl& c, const LayerMods& m, int sampleInBlock,
                     float e1s, float e1u, float e2, float lv, float lu, float lg, bool hf) noexcept;
    float eff (const LayerControl& c, MDest d, float base) const noexcept
    {
        return c.routes.has (d) ? clampf (base + modSum[static_cast<size_t> (d)], 0.0f, 1.0f) : base;
    }

    float hostFs = 48000.0f, osFs = 96000.0f;
    int osFactor = 2;
    bool analog = true, mirrored = false;
    int sampleCh = 2;
    float sampleBoost = 1.0f;     // CUSTOM: undoes kOutputGain so a sample plays at its own level

    Dds1 dds1;
    Dds2 dds2;
    SubOsc sub;
    OnePoleHpf hpf;
    SsiLadder lpf;
    CurtisFilter lpf3w;          // VCF STYLE = 3W
    StateVariable svf;          // the 3rd Wave's state-variable filter, ahead of the low-pass
    Envelope env1, env2, envFixed;
    Rng rng;

    bool active = false, gate = false;
    int note = 60;
    float velocity = 1.0f, velGain = 1.0f;
    float samplesSinceOn = 0.0f;
    int controlCountdown = 0;
    bool cutoffPrimed = false;
    bool needDds2 = true;
    bool superHalf = false;

    // pitch / portamento
    float unisonSemis = 0.0f;
    float pitchNow = 60.0f, pitchTarget = 60.0f;
    bool hasPlayed = false;
    float polyAT = 0.0f;
    float ribbonHeld = 0.0f;       // ribbon bend frozen at note release [p.77]

    // voice stealing: fade out, then start the pending note
    struct Pending { int note = 60; float velocity = 1.0f; float unison = 0.0f; float glideFrom = kNoGlide; } pending;
    bool stealing = false;
    float stealGain = 1.0f, stealStep = 0.0f;

    // analog tolerances (fixed per voice) and drift (slow random walk)
    float tolPitchCents = 0.0f, tolCutOct = 0.0f, tolEnv = 1.0f;
    float driftPitch = 0.0f, driftCut = 0.0f, driftEnv = 0.0f;

    // matrix
    std::array<float, kMatrixDests> modSum {};

    // ── values computed in the control updates, consumed per sample / in render() ──
    float vcaGainEff = 1.0f, vcaLfoDepthEff = 0.0f, lfo1DelayEff = 0.0f;
    float lfo2TremDepth = 0.0f, lfo2Uni = 0.0f, lfo2Out = 0.0f;
    double lfo2Phase = 0.0;        // this voice's own LFO 2 phase, free-running
    float tolLfo2 = 1.0f;          // its rate tolerance, like a real analog LFO
    uint32_t lfo2Seed = 0;
    float cutOctBase = 9.0f;
    bool cutAudioRate = false;
    float dds2CutOct = 0.0f, hfCutOct = 0.0f;
    float amDepth = 0.0f, hfTremDepth = 0.0f;
    float xmodOct = 0.0f;
    bool sync = false, ring = false, dds2Audio = true, subOn = false, subSquare = true;
    bool thirdWave = false, svfOn = false;
    float mix = 0.0f;
    float hfInc = 0.0f, hfPhase = 0.0f, hfOffset = 0.0f;
    bool hfOn = false;
    float hfInject1 = 0.0f, hfInject2 = 0.0f;
    float hfPitch1Oct = 0.0f, hfPitch2Oct = 0.0f;
    float dds2Prev = 0.0f;
    float lastVcaEnv = 0.0f;
};

} // namespace sg
