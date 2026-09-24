#pragma once

// Small lookups shared by the panel and its pop-overs.

#include <juce_core/juce_core.h>

#include "core/Matrix.h"
#include "core/MidiMap.h"

namespace sgui
{

// Literal with non-ASCII characters (·, →) as a juce::String.
inline juce::String u8 (const char* text) { return juce::String (juce::CharPointer_UTF8 (text)); }

inline juce::String layerPrefix (int layer) { return layer == 0 ? "upper" : "lower"; }
inline juce::String layerName (int layer)   { return layer == 0 ? "UPPER" : "LOWER"; }

// Host parameter ID of one matrix cell [pp.84–88].
inline juce::String matrixId (int layer, int src, int dest)
{
    const juce::String s = sg::kMatrixSourceIds[static_cast<size_t> (src)];
    return dest < sg::kFixedDests
        ? layerPrefix (layer) + ".mtx." + s + "." + sg::kMatrixFixedIds[static_cast<size_t> (dest)]
        : layerPrefix (layer) + ".mtxd." + s + "." + juce::String (dest - sg::kFixedDests + 1);
}

// Layer-parameter suffix of each matrix destination (A–H, then direct 1–24) [pp.84, 88].
inline const char* matrixDestSuffix (int dest)
{
    static constexpr const char* suffixes[sg::kMatrixDests] {
        "lfo1.rate", "ddsMod.crossMod", "ddsMod.pwmWave", "mixer.mix", "vcf.hpf", "vcf.res", "env1.decay", "fx.delayTime",
        "dds2.tune", "vcf.lpf", "vcf.envAmt", "vcf.lfo1Amt", "vcf.dds2Amt", "vca.envLevel", "vca.lfo1Amt", "vca.dds2Amt",
        "env1.attack", "env1.sustain", "env1.release", "env2.attack", "env2.decay", "env2.sustain", "env2.release",
        "lfo1.delay", "lfo1.lrPhase", "lfo2.rate", "lfo2.delay", "ddsMod.lfo1Amt", "ddsMod.pwDetune", "porta.time",
        "fx.delaySend", "fx.delayFeedback" };
    return suffixes[dest];
}

inline int matrixDestForSuffix (const juce::String& suffix)
{
    for (int d = 0; d < sg::kMatrixDests; ++d)
        if (suffix == matrixDestSuffix (d)) return d;
    return -1;
}

// The MIDI CC that addresses a parameter [pp.116–122], or −1.
inline int ccForParameter (const juce::String& paramId)
{
    const juce::String suffix = paramId.startsWith ("upper.") || paramId.startsWith ("lower.") ? paramId.fromFirstOccurrenceOf (".", false, false) : paramId;
    for (int cc = 0; cc < 128; ++cc)
    {
        const auto& e = sg::midimap::kCc[static_cast<size_t> (cc)];
        if (e.target != nullptr && suffix == e.target) return cc;
    }
    return -1;
}

inline juce::String noteName (int n)
{
    static const char* names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);   // 60 = C4
}

} // namespace sgui
