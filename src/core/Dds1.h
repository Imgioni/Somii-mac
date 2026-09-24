#pragma once

// DDS 1 — "super waveform" core [manual pp.31–34, 60–62].
// One centroid oscillator plus six sister oscillators, all free-running (phase reset only in
// SUPER ½). WAVEFORM selects SINE/SAW/SQUARE/TRIANGLE/NOISE/ALT; PWM/WAVE morphs toward the
// next waveform to the right (ALT: channel A → B) [p.32 table, p.62].

#include "LayerParams.h"
#include "PhaseOsc.h"
#include "Sample.h"
#include "WaveTable.h"

#include <array>

namespace sg
{

class Dds1
{
public:
    static constexpr int kOscCount = 7;   // index 0 = centroid

    void prepare (float osRate, float hostRate, uint32_t seed);
    void reset();

    // SUPER ½ resets every oscillator's phase on each note [p.61]; otherwise free-running.
    void noteOn (bool resetPhases);

    // Per control block.
    void setShape (Dds1Wave wave, const WaveTable* altA, const WaveTable* altB) noexcept;
    void setSuper (Tri superMode, float detuneAmount, bool mirrored) noexcept;
    // CUSTOM (SPKR): a user sample replaces the waveform while s != nullptr. Per control block.
    // root = the key that plays it at its own pitch; ch: 0 left, 1 right, 2 mono sum.
    void setSample (const Sample* s, bool loop, float start, float end, float loopStart, float level,
                    int root, float fineCents, int ch) noexcept;
    // Per host sample: centroid increment (cycles per OS sample) and morph position m ∈ [0,1].
    void setFrequency (float baseInc, float morph) noexcept;

    // One oversampled tick. fm = frequency multiplier (cross mod / audio-rate pitch mod).
    // Returns the one-sample-delayed output; centroidWrap = wrap fraction or -1.
    float tick (float fm, float& centroidWrap) noexcept;

    float getCentroidInc() const noexcept { return inc[0]; }

private:
    struct Source
    {
        enum Kind { Classic, Noise, Table } kind = Classic;
        Shape shape = Shape::Saw;
        const WaveTable* table = nullptr;
    };

    float osFs = 96000.0f, hostFs = 48000.0f, headroom = 0.7f;
    Rng rng;

    // Padded to 8 lanes for the SIMD fast path; lane 7 always has inc = gain = 0.
    static constexpr int kLanes = 8;
    alignas (16) std::array<float, kLanes> phase {};
    alignas (16) std::array<float, kLanes> ratio {};    // frequency ratio vs centroid
    alignas (16) std::array<float, kLanes> gain {};     // normalised mixing gain
    alignas (16) std::array<float, kLanes> inc {};
    std::array<int, kOscCount>   levelA {}, levelB {};
    int activeCount = 1;

    Source srcA, srcB;
    const Sample* smp = nullptr;
    bool smpLoop = false, smpRestart = true;
    int smpCh = 2;
    double smpLo = 0.0, smpHi = 1.0;          // play region, frames
    double smpLoopLo = 0.0;                   // LOOP START, frames
    float smpGain = 1.0f;
    double smpXf = 1.0;                       // loop crossfade / one-shot fade-out, frames
    float smpScale = 1.0f;                    // sample frames per centroid cycle
    std::array<double, kOscCount> spos {};    // play heads, one per oscillator
    float morph = 0.0f;
    BlepDelay delay;
    float noiseGainSum = 1.0f;
    Tri lastSuper = Tri::On;
    float lastDetune = -1.0f;
    bool lastMirrored = false;

    static Source sourceFor (Dds1Wave w, bool second, const WaveTable* a, const WaveTable* b) noexcept;
    float readSource (const Source& s, int osc, int level, float naiveNoise) const noexcept;

    // Fast path: a single classic shape, no morph (the usual case).
    template <Shape S> float classicPass (float fm, BlepAcc& acc, float& centroidWrap) noexcept;
    // SUPER ON/½ with no discontinuity in any of the seven oscillators this sample (≈ 98 % of
    // samples): all phases advance and mix in two SSE registers. Returns false to fall back.
    template <Shape S> bool simdPass (float fm, float& sum) noexcept;
    float generalPass (float fm, BlepAcc& acc, float& centroidWrap) noexcept;
    float samplePass (float fm, BlepAcc& acc, float& centroidWrap) noexcept;
};

} // namespace sg
