#pragma once

// MIDI implementation [manual pp.115–128], transcribed from the CC, RPN and NRPN tables.
// Layer-scoped targets are parameter-ID suffixes: the base channel addresses the upper layer,
// base + 1 the lower layer [p.99]. Global targets are full parameter IDs.
// Stepped values are the transmitted values in choice order; received values select the highest
// entry not above them (DD-30). Targets that don't exist yet are ignored by the router.

#include <array>
#include <cstdint>

namespace sg
{

enum class CcKind : uint8_t
{
    None,        // "–" in the table
    Perf,        // performance controller, handled by the engine (mod lever, ribbon, pedals, …)
    Continuous,  // 0–127 over the parameter's range (14-bit via NRPN 1024 + CC)
    Stepped,     // enumerated values
    Switch,      // 0 = off, 64 = on
    Index,       // value is the choice index (CC 14 sequence 0–15)
    Special      // bank select, data entry, NRPN / RPN, data inc/dec, channel mode
};

enum class CcScope : uint8_t { Layer, Global };

struct CcEntry
{
    CcKind kind = CcKind::None;
    CcScope scope = CcScope::Layer;
    const char* target = nullptr;
    std::array<uint8_t, 8> values {};
    uint8_t valueCount = 0;
    const char* name = "-";
    uint8_t page = 0;
};

namespace midimap
{
namespace detail
{
constexpr CcEntry cont (const char* t, const char* n, uint8_t pg, CcScope s = CcScope::Layer)
{
    CcEntry e; e.kind = CcKind::Continuous; e.scope = s; e.target = t; e.name = n; e.page = pg; return e;
}
constexpr CcEntry sw (const char* t, const char* n, uint8_t pg, CcScope s = CcScope::Layer)
{
    CcEntry e; e.kind = CcKind::Switch; e.scope = s; e.target = t; e.name = n; e.page = pg; e.values = { 0, 64 }; e.valueCount = 2; return e;
}
constexpr CcEntry perf (const char* n, uint8_t pg)    { CcEntry e; e.kind = CcKind::Perf; e.name = n; e.page = pg; return e; }
constexpr CcEntry special (const char* n, uint8_t pg) { CcEntry e; e.kind = CcKind::Special; e.name = n; e.page = pg; return e; }

template <size_t N>
constexpr CcEntry step (const char* t, const char* n, uint8_t pg, const uint8_t (&v)[N], CcScope s = CcScope::Layer)
{
    CcEntry e; e.kind = CcKind::Stepped; e.scope = s; e.target = t; e.name = n; e.page = pg;
    for (size_t i = 0; i < N; ++i) e.values[i] = v[i];
    e.valueCount = static_cast<uint8_t> (N);
    return e;
}

constexpr uint8_t k3[]  { 0, 43, 85 };
constexpr uint8_t k4[]  { 0, 32, 64, 96 };
constexpr uint8_t k5[]  { 0, 26, 51, 77, 102 };
constexpr uint8_t k6[]  { 0, 21, 43, 64, 85, 107 };
constexpr uint8_t kUs[] { 1, 2, 3, 4 };
constexpr uint8_t kCd[] { 0, 16, 32, 48, 64, 70, 96, 102 };   // CC 107 as printed (70 = 1/32, DD-32)

constexpr std::array<CcEntry, 128> build()
{
    std::array<CcEntry, 128> t {};
    constexpr auto G = CcScope::Global;
    t[0]   = special ("Bank Select", 116);
    t[1]   = perf ("Modulation Lever", 116);
    t[2]   = perf ("Ribbon Coarse", 116);
    t[3]   = cont ("perf.tempo", "Tempo", 116, G);
    t[4]   = perf ("Foot Controller", 116);
    t[5]   = cont ("porta.time", "Portamento Time", 116);
    t[6]   = special ("Data Entry MSB", 116);
    t[7]   = cont ("vca.envLevel", "VCA Envelope Level", 116);
    t[9]   = special ("Mod Amount/Fine Adjust", 116);
    t[10]  = cont ("mixer.pan", "Pan", 116);
    t[11]  = perf ("Expression", 116);
    t[12]  = cont ("fx.delayTime", "Delay Time", 116);
    t[13]  = cont ("fx.delayFeedback", "Delay Feedback", 116);
    { CcEntry e; e.kind = CcKind::Index; e.target = "seq.slot"; e.name = "Sequence Load"; e.page = 116; t[14] = e; }
    t[16]  = step ("lfo1.wave", "LFO 1 Waveform/HF Mode", 116, k6);
    t[17]  = cont ("lfo1.rate", "LFO 1 Rate", 116);
    t[18]  = cont ("lfo1.delay", "LFO 1 Delay", 116);
    t[19]  = cont ("lfo1.lrPhase", "LFO 1 LR Phase/Pan Spread", 116);
    t[20]  = step ("lfo1.mode", "LFO 1 Mode", 117, k3);
    t[21]  = cont ("ddsMod.lfo1Amt", "DDS LFO 1 Amount", 117);
    t[22]  = cont ("ddsMod.env1Amt", "DDS Envelope 1 Amount", 117);
    t[23]  = step ("ddsMod.dest", "DDS Modulator Destination", 117, k3);
    t[24]  = step ("ddsMod.super", "Super Mode", 117, k3);
    t[25]  = cont ("ddsMod.pwDetune", "PW/Detune", 117);
    t[26]  = cont ("ddsMod.pwmWave", "PWM/Wave Modulation", 117);
    t[27]  = step ("ddsMod.pwmSource", "PWM/Wave Modulation Source", 117, k3);
    t[28]  = cont ("ddsMod.crossMod", "Cross Modulation", 117);
    t[29]  = step ("dds1.wave", "DDS 1 Waveform", 117, k6);
    t[30]  = step ("dds1.range", "DDS 1 Range", 117, k6);
    t[31]  = step ("dds2.wave", "DDS 2 Waveform", 117, k6);
    t[32]  = cont ("env1.decayHold", "Envelope 1 Decay Hold", 117);
    t[33]  = cont ("env2.decayHold", "Envelope 2 Decay Hold", 117);
    t[34]  = perf ("Ribbon Fine", 117);
    t[35]  = cont ("dds2.tune", "DDS 2 Tune", 117);
    t[36]  = step ("dds2.mode", "DDS 2 Mode", 117, k3);
    t[37]  = cont ("mixer.mix", "Oscillator Mix", 118);
    t[38]  = special ("LSB for Control 6 (Data Entry)", 118);
    t[41]  = step ("vcf.drive", "VCF Drive", 118, k3);
    t[42]  = cont ("lfo2.vcaAmt", "VCA LFO 2 Amount", 118);
    t[43]  = step ("vcf.keytrack", "VCF Keytrack", 118, k3);
    t[44]  = step ("vcf.envSource", "VCF Envelope Source", 118, k3);
    t[45]  = cont ("vcf.envAmt", "VCF Envelope Amount", 118);
    t[46]  = cont ("vcf.lfo1Amt", "VCF LFO 1 Amount", 118);
    t[47]  = cont ("vcf.dds2Amt", "VCF DDS 2 Amount", 118);
    t[48]  = step ("vca.dynamics", "VCA Dynamics", 118, k3);
    t[49]  = step ("vca.envMode", "VCA Envelope Mode", 118, k3);
    t[50]  = step ("env1.mode", "Envelope 1 Mode", 118, k3);
    t[51]  = step ("env1.keytrack", "Envelope 1 Keytrack", 118, k3);
    t[52]  = cont ("env1.attackHold", "Envelope 1 Attack Hold", 118);
    t[53]  = cont ("env1.attack", "Envelope 1 Attack", 118);
    t[54]  = cont ("env1.decay", "Envelope 1 Decay", 118);
    t[55]  = cont ("env1.sustain", "Envelope 1 Sustain", 118);
    t[56]  = cont ("env1.release", "Envelope 1 Release", 118);
    t[57]  = cont ("env2.decay", "Envelope 2 Decay", 118);
    t[58]  = cont ("env2.sustain", "Envelope 2 Sustain", 119);
    t[59]  = sw ("manual", "Manual Mode", 119);
    t[60]  = step ("lfo2.trigger", "LFO 2 Trigger Source", 119, k3);
    t[61]  = step ("dest.osc", "Performance Control Destination", 119, k3);
    t[62]  = cont ("lfo2.rate", "LFO 2 Rate", 119);
    t[63]  = cont ("lfo2.delay", "LFO 2 Delay", 119);
    t[64]  = perf ("Sustain Pedal", 119);
    t[65]  = step ("perf.portaLayer", "Portamento Layer Select", 119, k3, G);
    t[67]  = step ("octave", "Octave Select", 119, k5);
    t[69]  = sw ("fx.freeze", "Delay Freeze", 119);
    t[70]  = cont ("lfo2.ddsAmt", "DDS LFO 2 Amount", 119);
    t[71]  = cont ("vcf.res", "VCF Resonance", 119);
    t[72]  = cont ("env2.release", "Envelope 2 Release", 119);
    t[73]  = cont ("env2.attack", "Envelope 2 Attack", 119);
    t[74]  = cont ("vcf.lpf", "VCF Cutoff Frequency", 119);
    t[75]  = cont ("lfo2.vcfAmt", "VCF LFO 2 Amount", 119);
    t[76]  = cont ("bender.ddsAmt", "DDS Pitch Bend Amount", 119);
    t[77]  = cont ("bender.vcfAmt", "VCF Pitch Bend Amount", 119);
    t[78]  = step ("voice.mode", "Voice Assign Mode", 120, k4);
    t[79]  = step ("voice.unisonSize", "Unison Size", 120, kUs);
    t[80]  = sw ("voice.binaural", "Binaural mode", 120);
    t[81]  = sw ("arp.sync", "Clock Sync", 120);
    t[82]  = step ("arp.range", "Arpeggiator Range", 120, k4);
    t[83]  = step ("arp.swing", "Arpeggiator/Sequencer Swing", 120, k5);
    t[84]  = sw ("global.clockRx", "Arpeggiator/Sequencer External Clock", 120, G);
    t[85]  = step ("arp.mode", "Arpeggiator/Sequencer Mode", 120, k5);
    t[86]  = sw ("arp.on", "Arpeggiator/Sequencer On/Off", 120);
    t[87]  = sw ("hold", "Arpeggiator/Sequencer Hold", 120);
    t[91]  = cont ("fx.delaySend", "Delay Send", 120);
    t[92]  = cont ("vca.lfo1Amt", "VCA LFO 1 Amount", 120);
    t[93]  = step ("fx.chorus", "Chorus", 120, k4);
    t[94]  = cont ("ddsMod.drift", "Drift", 120);
    t[95]  = cont ("vcf.hpf", "HPF Cutoff Frequency", 121);
    t[96]  = special ("Data Increment", 121);
    t[97]  = special ("Data Decrement", 121);
    t[98]  = special ("NRPN - LSB", 121);
    t[99]  = special ("NRPN - MSB", 121);
    t[100] = special ("RPN - LSB", 121);
    t[101] = special ("RPN - MSB", 121);
    t[104] = step ("perf.modLayer", "Modulation Layer", 121, k3, G);
    t[105] = step ("lfo2.wave", "LFO 2 Waveform", 121, k6);
    t[106] = step ("dds2.range", "DDS 2 Range", 121, k6);
    t[107] = step ("arp.clockDiv", "Clock Divider", 121, kCd);
    t[108] = cont ("perf.lowerDetune", "Lower Layer Detune", 121, G);
    t[109] = cont ("vca.dds2Amt", "VCA DDS 2 Amount", 121);
    t[110] = cont ("lfo2.rateMod", "LFO 2 Rate Modulation", 121);
    t[111] = cont ("perf.perfDetune", "Performance Detune", 121, G);
    t[120] = perf ("All Sound Off", 122);
    t[121] = perf ("Reset All Controllers", 122);
    t[122] = sw ("global.localControl", "Local Control On/Off", 122, G);
    t[123] = perf ("All Notes Off", 122);
    t[124] = special ("Omni Mode Off", 122);
    t[125] = special ("Omni Mode On", 122);
    t[126] = special ("Mono Mode On", 122);
    t[127] = special ("Poly Mode On", 122);
    return t;
}
} // namespace detail

inline constexpr std::array<CcEntry, 128> kCc = detail::build();

// Received stepped value → choice index (highest listed value not above it).
inline int stepIndex (const CcEntry& e, int value) noexcept
{
    int idx = 0;
    for (int i = 0; i < e.valueCount; ++i)
        if (value >= e.values[static_cast<size_t> (i)]) idx = i;
    return idx;
}

// Patch NRPNs are 1024 + CC for every continuous control [pp.124–128].
inline constexpr int kNrpnPatchBase = 1024;
// Global NRPNs [p.123]
inline constexpr int kNrpnMidiChannel = 2051, kNrpnClockTx = 2052, kNrpnClockRx = 2053, kNrpnPcTx = 2055, kNrpnPcRx = 2056;
} // namespace midimap

} // namespace sg
