#pragma once

// The FX rack (docs/fx/FX_PROMPTS.md): three slots, SERIAL or PARALLEL, each processed per layer
// with its own copy of the effect. The layer fader under each slot sets how much of the effect
// each layer gets (middle = both fully, like the DDS 1 / DDS 2 mix).
//
// Threads: effects are built and prepared on the message thread (timer / prepareToPlay /
// setState), handed to the audio thread through `pending`, and the replaced one comes back
// through `retired` to be deleted on the message thread. Neither side ever blocks.

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "FxDefs.h"

#include <array>
#include <atomic>
#include <memory>

namespace fx
{
struct Ctx
{
    const float* v = nullptr;      // real parameter values, fxdefs order (p1 = v[0])
    double bpm = 120.0;
    double fs = 48000.0;
    double ppq = 0.0;              // host position in quarter notes at the block start (valid while playing)
    bool playing = false;          // host transport running
    const float* ext = nullptr;    // the slot's extra values (fxdefs::kExt), e.g. CARVE's waves
    uint32_t noteOns = 0;          // the layer's note-on count: a change means a new note
};

// An impulse response shared by every convolver in the rack (message thread only).
struct ImpulseResponse
{
    juce::AudioBuffer<float> buffer;   // at `rate`
    double rate = 48000.0;
    juce::String name;
};

class Unit
{
public:
    virtual ~Unit() = default;
    virtual void prepare (double fs, int maxBlock) = 0;
    virtual void reset() = 0;
    // in place; writes the fully wet signal (the rack applies DRY / WET and the layer amount)
    virtual void process (float* l, float* r, int n, const Ctx& c) = 0;
    // message thread, ~30 Hz: for work that must not run on the audio thread (IR reloads)
    virtual void messageTick (const float* /*v*/, const ImpulseResponse& /*ir*/, bool /*irChanged*/) {}
    // live values for the slot's display: meaning depends on the effect (FX_PROMPTS.md)
    static constexpr int kVis = 16;
    std::array<std::atomic<float>, kVis> vis {};
    int type = 0;   // set by makeUnit; the rack only mixes a unit whose type matches its slot
};

std::unique_ptr<Unit> makeUnit (int type);

class Rack
{
public:
    static constexpr int kSlots = 3;
    // FX modulation (right-click any FX control): target 0..kNP-1 = p1..pNP, kNP = DRY / WET; each target
    // can take every source in fxdefs::kModSourceNames, with an amount of -1..1 of the control's range
    static constexpr int kTargets = fxdefs::kNP + 1;
    static constexpr int kLayerSources = 11;   // LayerEngine::kFxSources; the rack adds INPUT and RANDOM
    struct LayerIn { float src[kLayerSources]; uint32_t noteOns; };
    ~Rack();

    void bind (juce::AudioProcessorValueTreeState& state);
    void prepare (double fs, int maxBlock);                // audio stopped
    // message thread: build effects for changed types, collect retired ones, IR housekeeping.
    // resetParams: a type change made by the user resets that slot to the type's defaults.
    void messageTick (bool resetParams);
    // a loaded state brings its own parameter values: the next build of each slot must not reset them
    void stateLoaded() noexcept { for (auto& s : slots) s.noReset = true; }
    // message thread: exchange two slots (type, on, mix, layer and every p), keeping their settings
    void swapSlots (int a, int b);
    // audio thread; the four buffers are upper L/R and lower L/R, processed in place
    // `in` (optional, two entries: upper, lower) are the layers' sources at the end of this block
    void process (float* uL, float* uR, float* lL, float* lR, int n, double bpm, double ppq = 0.0, bool playing = false, const LayerIn* in = nullptr) noexcept;

    // EXT values and modulation routes: the message thread writes, the audio thread reads. A change made by
    // the rack itself (state load, swap, type change) bumps getDataVersion() so the editor re-reads them.
    void setExt (int slot, int i, float v) noexcept;
    float getExt (int slot, int i) const noexcept;
    void setMod (int slot, int target, int src, float amount) noexcept;
    float getMod (int slot, int target, int src) const noexcept;
    float getModNow (int slot, int target) const noexcept;   // the offset applied now (the layer the display follows)
    bool hasMods (int slot) const noexcept;
    int getDataVersion() const noexcept { return dataVersion.load(); }

    // impulse response for the convolvers: WAV/AIFF bytes (from a drop), or empty = default hall
    bool loadImpulse (const void* data, size_t size, const juce::String& name);
    const ImpulseResponse& getImpulse() const noexcept { return ir; }
    juce::ValueTree toTree() const;
    void fromTree (const juce::ValueTree& t);

    float getVis (int slot, int i) const noexcept;
    // message thread: the slot's wet output as kBands log-spaced levels in dB (20 Hz - 20 kHz),
    // and its peak level; all -90 dB / 0 once the slot has gone quiet or idle
    static constexpr int kBands = 48;
    void spectrum (int slot, float* out) const;
    float outputLevel (int slot) const noexcept;
    int getImpulseVersion() const noexcept { return irVersion; }   // message thread

private:
    struct Slot
    {
        std::atomic<float>* type {};
        std::atomic<float>* on {};
        std::atomic<float>* mix {};
        std::atomic<float>* layer {};
        std::array<std::atomic<float>*, fxdefs::kNP> p {};
        juce::RangedAudioParameter* typeParam {};
        std::array<juce::RangedAudioParameter*, fxdefs::kNP> pParam {};
        juce::RangedAudioParameter* mixParam {};
        juce::RangedAudioParameter* onParam {};
        juce::RangedAudioParameter* layerParam {};

        int builtType = -1;                                  // message thread
        bool noReset = false;                                // message thread
        std::array<Unit*, 2> current {};                     // message thread's view of the newest unit
        std::array<std::atomic<Unit*>, 2> pending {};        // message -> audio
        std::array<std::atomic<Unit*>, 2> retired {};        // audio -> message
        std::array<Unit*, 2> active {};                      // audio thread
        std::array<float, 2> amt { 0.0f, 0.0f };            // smoothed layer amount (audio)
        std::array<float, 2> fade { 1.0f, 1.0f };           // fade-in after a swap (audio)
        float mixSm = 1.0f;
        std::array<std::atomic<float>, fxdefs::kExt> ext {};
        bool extSet = false;                                 // message thread: false = give the type's defaults at the next build
        std::array<std::atomic<float>, kTargets * fxdefs::kModSources> mod {};
        std::atomic<int> mods { 0 };                         // routes in use (0 = nothing to scan)
        std::array<std::atomic<float>, kTargets> modNow {};
        std::array<float, 2> inEnv {};                       // the INPUT source, per layer (audio)
    };

    void processChunk (float* uL, float* uR, float* lL, float* lR, int n, double bpm, double ppq, bool playing) noexcept;
    void setExtDefaults (Slot& s, int type) noexcept;
    void countMods (Slot& s) noexcept;
    void buildSlot (Slot& s, int type, bool resetParams);
    void takePending (Slot& s) noexcept;
    void resetToDefaults (Slot& s, int type);
    void makeDefaultImpulse();

    std::array<Slot, kSlots> slots;
    // each slot's wet output (the layer the display follows), audio -> message, for the analyser
    static constexpr int kScopeOrder = 11, kScope = 1 << kScopeOrder;
    struct Scope { std::array<std::atomic<float>, kScope> buf {}; std::atomic<int> pos { 0 }; std::atomic<float> peak { 0.0f }; };
    std::array<Scope, kSlots> scopes;
    mutable std::array<int, kSlots> seenPos {}, stale {};
    std::atomic<float>* mode {};
    double fs = 48000.0;
    int maxBlock = 512;
    std::array<std::vector<float>, 4> dry, tmp;
    ImpulseResponse ir;
    juce::MemoryBlock irFile;          // the dropped file's bytes, saved with the state
    bool irChanged = true;
    int irVersion = 0;
    std::atomic<int> dataVersion { 0 };
    // modulation sources (audio): each layer's values at the last block end and the next, interpolated per
    // 64-sample piece; INPUT (per slot) and RANDOM (a new value each beat, glided) are made here
    std::array<std::array<float, kLayerSources>, 2> srcPrev {}, srcNext {};
    std::array<std::array<float, fxdefs::kModSources>, 2> srcNow {};
    std::array<uint32_t, 2> noteOns {};
    std::array<float, 2> rnd {}, rndTo {};
    double rndBeat = -1.0, rndClock = 0.0;
    uint32_t rndState = 0x9E3779B9u;
    std::array<std::array<float, fxdefs::kExt>, kSlots> extNow {};
};
} // namespace fx
