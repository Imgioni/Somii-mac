#pragma once

// Host parameter layout. IDs, ranges and defaults follow docs/PARAMETERS.md; every definition
// in Parameters.cpp cites its manual page. Later phases append parameters; IDs never change.

#include <juce_audio_processors/juce_audio_processors.h>

#include "core/LayerParams.h"
#include "core/Performance.h"

#include <array>

namespace sgp
{

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

// Perf-section parameter groups that follow the DEST / PORTAMENTO layer selectors [pp.73–75].
extern const juce::StringArray kModLayerGroup;
extern const juce::StringArray kPortaLayerGroup;

// Cached atomics for one layer, read once per block on the audio thread.
class LayerParamRefs
{
public:
    void bind (juce::AudioProcessorValueTreeState& state, const juce::String& layerPrefix);
    void read (sg::LayerParams& out) const noexcept;
    bool hold() const noexcept { return holdParam->load (std::memory_order_relaxed) >= 0.5f; }

private:
    std::atomic<float> *dds1Wave{}, *dds1Range{}, *altA{}, *altB{};
    std::atomic<float> *smpOn{}, *smpLoop{}, *smpStart{}, *smpEnd{}, *smpLoopStart{}, *smpLevel{}, *smpRoot{}, *smpFine{}, *smpSlices{}, *smpSense{};
    std::atomic<float> *dds2Wave{}, *dds2Range{}, *dds2Tune{}, *dds2Mode{};
    std::atomic<float> *mix{}, *pan{};
    std::atomic<float> *drive{}, *hpf{}, *lpf{}, *res{}, *envSource{}, *keytrack{}, *vcfEnv{}, *vcfLfo{}, *vcfDds2{};
    std::atomic<float> *twCutoff{}, *twRes{}, *twEnv{}, *twKey{}, *twComp{}, *svfEnv{}, *svfVelocity{}, *svfKey{};
    std::atomic<float> *vcfStyle{}, *vcfSat{}, *vcfVel{};
    std::atomic<float> *svfOn{}, *svfCutoff{}, *svfRes{}, *svfMode{}, *svfBand{};
    std::atomic<float> *vcaLevel{}, *vcaLfo{}, *vcaDds2{}, *vcaEnv{}, *dynamics{};
    std::atomic<float> *e1AH{}, *e1A{}, *e1DH{}, *e1D{}, *e1S{}, *e1R{}, *e1Mode{}, *e1Kt{};
    std::atomic<float> *e2A{}, *e2DH{}, *e2D{}, *e2S{}, *e2R{};
    std::atomic<float> *lfoWave{}, *lfoRate{}, *lfoDelay{}, *lfoLr{}, *lfoMode{}, *lfoPhaseMode{};
    std::atomic<float> *pLfo{}, *pEnv{}, *pDest{}, *superMode{}, *pw{}, *drift{}, *pwm{}, *pwmSrc{}, *xmod{};
    std::atomic<float> *benderDds{}, *benderVcf{}, *destOsc{};
    std::atomic<float> *lfo2Wave{}, *lfo2Rate{}, *lfo2Delay{}, *lfo2Trig{}, *lfo2RateMod{}, *lfo2Dds{}, *lfo2Vcf{}, *lfo2Vca{};
    std::atomic<float> *porta{}, *octave{};
    std::atomic<float> *voiceMode{}, *unison{}, *unisonSize{}, *binaural{}, *holdParam{};
    std::atomic<float> *chorus{}, *delayTime{}, *delayFeedback{}, *delaySend{}, *freeze{};
    std::atomic<float> *arpOn{}, *arpDiv{}, *arpSync{}, *arpRate{}, *arpRange{}, *arpSwing{}, *arpMode{}, *seqSlot{};
    std::array<std::array<std::atomic<float>*, sg::kMatrixDests>, sg::kMatrixSources> matrix {};
};

class PerformanceParamRefs
{
public:
    void bind (juce::AudioProcessorValueTreeState& state);
    void read (sg::PerformanceParams& out) const noexcept;
    float masterVolume() const noexcept { return master->load (std::memory_order_relaxed); }
    float tempo() const noexcept        { return tempoP->load (std::memory_order_relaxed); }
    bool ccReceive() const noexcept     { return ccRx->load (std::memory_order_relaxed) >= 0.5f; }
    bool hostClock() const noexcept     { return clockRx->load (std::memory_order_relaxed) >= 0.5f; }
    int modLayer() const noexcept   { return static_cast<int> (modLayerP->load() + 0.5f); }    // 0 BOTH, 1 LOWER, 2 UPPER
    int portaLayer() const noexcept { return static_cast<int> (portaLayerP->load() + 0.5f); }

private:
    std::atomic<float> *master{}, *transpose{}, *fineTune{}, *midiChannel{};
    std::atomic<float> *mode{}, *single{}, *split{}, *lowerDetune{}, *perfDetune{}, *modLayerP{}, *portaLayerP{};
    std::atomic<float> *tempoP{}, *ccRx{}, *clockRx{};
};

} // namespace sgp
