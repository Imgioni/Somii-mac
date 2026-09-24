#include "MidiRouter.h"

void MidiRouter::bind (juce::AudioProcessorValueTreeState& state)
{
    auto find = [&state] (const juce::String& id) { return state.getParameter (id); };

    for (int cc = 0; cc < 128; ++cc)
    {
        const auto& e = sg::midimap::kCc[static_cast<size_t> (cc)];
        if (e.target == nullptr) continue;
        if (e.scope == sg::CcScope::Layer)
        {
            layerParams[static_cast<size_t> (cc)][0] = find (juce::String ("upper.") + e.target);
            layerParams[static_cast<size_t> (cc)][1] = find (juce::String ("lower.") + e.target);
        }
        else
        {
            globalParams[static_cast<size_t> (cc)] = find (e.target);
        }
    }
    bendRange = { find ("upper.bender.ddsAmt"), find ("lower.bender.ddsAmt") };
    fineTune = find ("global.fineTune");
    transpose = find ("global.transpose");
    midiChannel = find ("global.midiChannel");
    clockRx = find ("global.clockRx");
}

bool MidiRouter::handleController (int channel, int cc, int value)
{
    return decoder.controller (channel, cc, value, [this] (const sg::ParamChange& c) { apply (c); });
}

void MidiRouter::apply (const sg::ParamChange& c)
{
    juce::RangedAudioParameter* p = nullptr;
    if (c.slot < 128)
        p = c.layer >= 0 ? layerParams[static_cast<size_t> (c.slot)][static_cast<size_t> (c.layer)]
                         : globalParams[static_cast<size_t> (c.slot)];
    else
        switch (c.slot)
        {
            case sg::midislot::kBendRange:   p = c.layer >= 0 ? bendRange[static_cast<size_t> (c.layer)] : nullptr; break;
            case sg::midislot::kFineTune:    p = fineTune; break;
            case sg::midislot::kTranspose:   p = transpose; break;
            case sg::midislot::kMidiChannel: p = midiChannel; break;
            case sg::midislot::kClockRx:     p = clockRx; break;
            default: break;   // clock / program-change transmit: no MIDI output in this build (DD-57)
        }
    if (p == nullptr) return;   // a control that doesn't exist (yet)

    float norm = c.value;
    if (c.type == sg::ParamChange::Type::Index) norm = p->convertTo0to1 (c.value);
    p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
}
