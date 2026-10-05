#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "FxRack.h"
#include "MidiRouter.h"
#include "Parameters.h"
#include "UiBridge.h"
#include "core/Performance.h"
#include "core/Sample.h"

#include <map>

class GeminusWebSession;

class SuperGeminiProcessor final : public juce::AudioProcessor,
                                   private juce::Timer
{
public:
    SuperGeminiProcessor();
    ~SuperGeminiProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    sg::PerformanceEngine& getEngine() noexcept { return engine; }
    UiBridge& getUiBridge() noexcept { return ui; }
    fx::Rack& getFxRack() noexcept { return fxRack; }

    GeminusWebSession& getWebSession();

    // Message thread: every patch parameter of the layer back to its default; the performance
    // switches to SINGLE on that layer [p.22, DD-7]. HOLD and FREEZE belong to the performance.
    void loadInitPatch (int layer);

    // DDS 1 CUSTOM (message thread): WAV / AIFF / FLAC / OGG / MP3 bytes, or empty to clear.
    // The file itself is saved with the state, so projects and patches carry their samples.
    struct CustomSample
    {
        juce::MemoryBlock file;
        juce::String name;
        std::shared_ptr<const sg::Sample> now, prev;   // prev outlives the audio thread's last use of it
        int version = 0;
    };
    static constexpr double kCustomMaxSeconds = 90.0;   // long enough to slice a section of a song
    bool loadCustomSample (int layer, const void* data, size_t size, const juce::String& name);
    const CustomSample& getCustomSample (int layer) const noexcept { return customSamples[static_cast<size_t> (layer & 1)]; }

    juce::AudioProcessorValueTreeState apvts;

private:
    void handleMidi (const juce::MidiMessage& m);
    void applyUiCommand (const UiCommand& c);
    void publishUiFeed (const juce::AudioBuffer<float>& mainBus, int n);
    void timerCallback() override;
    void mirrorGroup (const juce::StringArray& suffixes, bool justSwitchedToBoth);
    sg::ClockInfo readClock();
    juce::ValueTree sequencesToTree();
    void sequencesFromTree (const juce::ValueTree& tree);

    MidiRouter router;
    UiBridge ui;

    static constexpr int kOversampling = 2;   // Settings page arrives in Phase 6

    sg::PerformanceEngine engine;
    std::array<sgp::LayerParamRefs, 2> layerRefs;
    std::array<sg::LayerParams, 2> layerParams;
    sgp::PerformanceParamRefs perfRefs;
    sg::PerformanceParams perfParams;
    std::array<bool, 2> holdState { false, false };

    std::array<std::vector<float>, 4> scratch;   // upper L/R, lower L/R
    fx::Rack fxRack;
    std::atomic<float>* ribbonParam {};          // perf.ribbon
    float lastRibbon = 0.5f;                             // FX 1-3, after the layer effects, before the master sum

    // DEST / PORTAMENTO layer selectors = BOTH: keep the two layers' values in step [pp.73–75]
    int lastModLayer = -1, lastPortaLayer = -1;
    std::map<juce::String, float> lastSeen;

    std::array<CustomSample, 2> customSamples;
    juce::ValueTree customSamplesToTree() const;
    void customSamplesFromTree (const juce::ValueTree& t);

    std::unique_ptr<GeminusWebSession> webSession; // destroyed before APVTS and UI bridge

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SuperGeminiProcessor)
};
