#pragma once

// Modulation matrix per layer [manual pp.84–89]: 8 sources × (8 matrix destinations A–H +
// 24 direct-parameter destinations), each pair with its own −100…+100 % amount.
// A mapping adds amount × source to the destination's normalised panel value (0..1), so 100 %
// sweeps the whole range of the control (DD-28).

#include <array>
#include <cstdint>

namespace sg
{

enum class MSrc : uint8_t { Dds2, Lfo2, Env1, Velocity, Aftertouch, Expression, Ribbon, Note, Count };   // buttons 1–8 [p.84]

enum class MDest : uint8_t
{
    // Matrix destinations A–H [p.84]
    Lfo1Rate, CrossMod, WaveMod, OscMix, Hpf, Res, Env1Decay, DelayTime,
    // Direct parameter mappings 1–24 [p.88]
    Dds2Tune, LpfCutoff, VcfEnvAmt, VcfLfo1Amt, VcfDds2Amt, VcaEnvLevel, VcaLfo1Amt, VcaDds2Amt,
    Env1Attack, Env1Sustain, Env1Release, Env2Attack, Env2Decay, Env2Sustain, Env2Release,
    Lfo1Delay, Lfo1LrPhase, Lfo2Rate, Lfo2Delay, DdsModLfo1Amt, PwDetune, PortaTime, DelaySend, DelayFeedback,
    Count
};

inline constexpr int kMatrixSources = static_cast<int> (MSrc::Count);
inline constexpr int kMatrixDests   = static_cast<int> (MDest::Count);
inline constexpr int kFixedDests    = 8;

// "Already existing modulation mappings will not be duplicated" [p.87] — pairs that already have a
// dedicated amount control on the panel (DD-12).
inline constexpr bool matrixExcluded (int src, int dest) noexcept
{
    const auto s = static_cast<MSrc> (src);
    const auto d = static_cast<MDest> (dest);
    if (d == MDest::LpfCutoff && (s == MSrc::Lfo2 || s == MSrc::Dds2 || s == MSrc::Env1)) return true;
    if (d == MDest::Lfo2Rate && s == MSrc::Aftertouch) return true;
    return false;
}

inline constexpr std::array<const char*, kMatrixSources> kMatrixSourceIds   { "dds2", "lfo2", "env1", "vel", "at", "expr", "ribbon", "note" };
inline constexpr std::array<const char*, kMatrixSources> kMatrixSourceNames { "DDS 2", "LFO 2", "ENV 1", "VEL", "AT", "EXPR", "RIBN", "NOTE" };
inline constexpr std::array<const char*, kFixedDests>    kMatrixFixedIds    { "lfo1Rate", "xmod", "wave", "mix", "hpf", "res", "env1Decay", "dlyTime" };
inline constexpr std::array<const char*, kMatrixDests>   kMatrixDestNames
{
    "LFO 1 Rate", "Cross Mod", "Wave Mod", "Osc Mix", "HPF", "Resonance", "ENV 1 Decay", "Delay Time",
    "DDS 2 Tune", "LPF Cutoff", "VCF Env Amt", "VCF LFO 1 Amt", "VCF DDS 2 Amt", "VCA Env Level", "VCA LFO 1 Amt", "VCA DDS 2 Amt",
    "ENV 1 Attack", "ENV 1 Sustain", "ENV 1 Release", "ENV 2 Attack", "ENV 2 Decay", "ENV 2 Sustain", "ENV 2 Release",
    "LFO 1 Delay", "LFO 1 LR Phase", "LFO 2 Rate", "LFO 2 Delay", "DDS Mod LFO 1 Amt", "PW / Detune", "Portamento Time",
    "Delay Send", "Delay Feedback"
};

// Sparse list of the active (non-zero, allowed) mappings, rebuilt once per control block.
struct MatrixRoutes
{
    struct Route { uint8_t src, dest; float amount; };

    std::array<Route, kMatrixSources * kMatrixDests> routes {};
    int count = 0;
    uint32_t destMask = 0;
    uint8_t srcMask = 0;

    bool has (MDest d) const noexcept    { return (destMask >> static_cast<int> (d)) & 1u; }
    bool uses (MSrc s) const noexcept    { return (srcMask >> static_cast<int> (s)) & 1u; }

    void build (const float (&amount)[kMatrixSources][kMatrixDests]) noexcept
    {
        count = 0; destMask = 0; srcMask = 0;
        for (int s = 0; s < kMatrixSources; ++s)
            for (int d = 0; d < kMatrixDests; ++d)
            {
                const float a = amount[s][d];
                if (a == 0.0f || matrixExcluded (s, d)) continue;
                routes[static_cast<size_t> (count++)] = { static_cast<uint8_t> (s), static_cast<uint8_t> (d), a };
                destMask |= 1u << d;
                srcMask = static_cast<uint8_t> (srcMask | (1u << s));
            }
    }
};

} // namespace sg
