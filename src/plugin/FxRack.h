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
    std::array<std::atomic<float>, 8> vis {};
    int type = 0;   // set by makeUnit; the rack only mixes a unit whose type matches its slot
};

std::unique_ptr<Unit> makeUnit (int type);

class Rack
{
public:
    static constexpr int kSlots = 3;
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
    void process (float* uL, float* uR, float* lL, float* lR, int n, double bpm) noexcept;

    // impulse response for the convolvers: WAV/AIFF bytes (from a drop), or empty = default hall
    bool loadImpulse (const void* data, size_t size, const juce::String& name);
    const ImpulseResponse& getImpulse() const noexcept { return ir; }
    juce::ValueTree toTree() const;
    void fromTree (const juce::ValueTree& t);

    float getVis (int slot, int i) const noexcept;
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
    };

    void processChunk (float* uL, float* uR, float* lL, float* lR, int n, double bpm) noexcept;
    void buildSlot (Slot& s, int type, bool resetParams);
    void takePending (Slot& s) noexcept;
    void resetToDefaults (Slot& s, int type);
    void makeDefaultImpulse();

    std::array<Slot, kSlots> slots;
    std::atomic<float>* mode {};
    double fs = 48000.0;
    int maxBlock = 512;
    std::array<std::vector<float>, 4> dry, tmp;
    ImpulseResponse ir;
    juce::MemoryBlock irFile;          // the dropped file's bytes, saved with the state
    bool irChanged = true;
    int irVersion = 0;
};
} // namespace fx
