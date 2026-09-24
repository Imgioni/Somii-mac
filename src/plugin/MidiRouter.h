#pragma once

// Applies incoming CC / NRPN / RPN parameter changes (core/MidiMap.h, pp.115–128) to the host
// parameters. Parameter pointers are resolved once, so the audio thread never allocates.

#include <juce_audio_processors/juce_audio_processors.h>

#include "core/MidiDecoder.h"

class MidiRouter
{
public:
    void bind (juce::AudioProcessorValueTreeState& state);
    void setBaseChannel (int channel) noexcept { decoder.setBaseChannel (channel); }
    void setReceiveEnabled (bool on) noexcept  { decoder.setReceiveEnabled (on); }

    // Returns false for performance controllers, which the caller passes on to the engine.
    bool handleController (int channel, int cc, int value);

private:
    void apply (const sg::ParamChange& c);

    sg::MidiDecoder decoder;
    std::array<std::array<juce::RangedAudioParameter*, 2>, 128> layerParams {};
    std::array<juce::RangedAudioParameter*, 128> globalParams {};
    std::array<juce::RangedAudioParameter*, 2> bendRange {};
    juce::RangedAudioParameter *fineTune = nullptr, *transpose = nullptr, *midiChannel = nullptr, *clockRx = nullptr;
};
