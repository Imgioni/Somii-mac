#pragma once

// Turns incoming CC / NRPN / RPN traffic into parameter changes [manual pp.99–100, 115–128].
// JUCE-free so it can be tested headless; the plugin maps each change onto a host parameter.

#include "MidiMap.h"

#include <algorithm>
#include <array>

namespace sg
{

struct ParamChange
{
    enum class Type : uint8_t { Normalized, Index, Bool };

    // slot 0–127: the CC table entry (also for NRPN 1024 + CC); ≥ 200: the special targets below
    int slot = 0;
    int layer = -1;            // 0 upper, 1 lower, -1 global
    Type type = Type::Normalized;
    float value = 0.0f;        // Normalized 0…1, Index = choice index / integer value, Bool 0/1
};

namespace midislot
{
inline constexpr int kBendRange = 200;   // RPN 0 → bender DDS amount of the channel's layer
inline constexpr int kFineTune  = 201;   // RPN 1 → global fine tune (DD-22)
inline constexpr int kTranspose = 202;   // RPN 2 → global transpose
inline constexpr int kMidiChannel = 203, kClockTx = 204, kClockRx = 205, kPcTx = 206, kPcRx = 207;   // global NRPNs
}

class MidiDecoder
{
public:
    void setBaseChannel (int channel) noexcept { base = std::clamp (channel, 1, 16); }
    void setReceiveEnabled (bool on) noexcept  { rx = on; }            // TX/RX E · CC receive [p.99]
    int layerFor (int channel) const noexcept  { return channel == base ? 0 : (channel == base + 1 ? 1 : -1); }

    // Returns false for performance controllers (the engine handles those); true otherwise.
    template <typename Emit>
    bool controller (int channel, int cc, int value, Emit&& emit)
    {
        if (channel < 1 || channel > 16 || cc < 0 || cc > 127) return true;
        const auto& e = midimap::kCc[static_cast<size_t> (cc)];
        const int layer = layerFor (channel);

        switch (e.kind)
        {
            case CcKind::Perf: return false;
            case CcKind::None: return true;
            case CcKind::Special:
                if (rx) special (channel, cc, value, emit);
                return true;
            default: break;
        }
        if (! rx) return true;
        if (e.scope == CcScope::Layer && layer < 0) return true;

        ParamChange c;
        c.slot = cc;
        c.layer = e.scope == CcScope::Layer ? layer : -1;
        switch (e.kind)
        {
            case CcKind::Continuous: c.type = ParamChange::Type::Normalized; c.value = static_cast<float> (value) / 127.0f; break;
            case CcKind::Stepped:    c.type = ParamChange::Type::Index; c.value = static_cast<float> (midimap::stepIndex (e, value)); break;
            case CcKind::Switch:     c.type = ParamChange::Type::Bool; c.value = value >= 64 ? 1.0f : 0.0f; break;
            case CcKind::Index:      c.type = ParamChange::Type::Index; c.value = static_cast<float> (std::clamp (value, 0, 15)); break;
            default: return true;
        }
        emit (c);
        return true;
    }

private:
    struct Channel
    {
        int nrpnMsb = 127, nrpnLsb = 127, rpnMsb = 127, rpnLsb = 127;
        bool rpnSelected = false;
        int dataMsb = 0, dataLsb = 0;
        bool haveLsb = false;
    };

    template <typename Emit>
    void special (int channel, int cc, int value, Emit&& emit)
    {
        auto& s = chans[static_cast<size_t> (channel - 1)];
        switch (cc)
        {
            case 99:  s.nrpnMsb = value; s.rpnSelected = false; break;
            case 98:  s.nrpnLsb = value; s.rpnSelected = false; break;
            case 101: s.rpnMsb = value;  s.rpnSelected = true;  break;
            case 100: s.rpnLsb = value;  s.rpnSelected = true;  break;
            case 6:   s.dataMsb = value; s.haveLsb = false; applyData (channel, s, emit); break;
            case 38:  s.dataLsb = value; s.haveLsb = true;  applyData (channel, s, emit); break;
            case 96:  s.dataMsb = std::min (127, s.dataMsb + 1); s.haveLsb = false; applyData (channel, s, emit); break;
            case 97:  s.dataMsb = std::max (0, s.dataMsb - 1);   s.haveLsb = false; applyData (channel, s, emit); break;
            default:  break;   // bank select (library, Phase 6), mod amount (DD-31), channel mode messages
        }
    }

    template <typename Emit>
    void applyData (int channel, const Channel& s, Emit&& emit)
    {
        const int layer = layerFor (channel);
        const int v14 = s.haveLsb ? (s.dataMsb << 7 | s.dataLsb) : (s.dataMsb * 16383 + 63) / 127;
        ParamChange c;

        if (s.rpnSelected)
        {
            const int rpn = s.rpnMsb << 7 | s.rpnLsb;
            if (rpn == 0 && layer >= 0)
            {
                // Pitch bend sensitivity, MSB = semitones (max one octave) [p.123]
                c = { midislot::kBendRange, layer, ParamChange::Type::Normalized, static_cast<float> (std::min (12, s.dataMsb)) / 12.0f };
                emit (c);
            }
            else if (rpn == 1)
            {
                // Channel fine tuning: 00 00 = −100 ct, 40 00 = A440, 7F 7F = +100 ct [p.123]
                const float cents = (static_cast<float> (v14) - 8192.0f) / 8192.0f * 100.0f;
                c = { midislot::kFineTune, -1, ParamChange::Type::Normalized, std::clamp ((cents + 100.0f) / 200.0f, 0.0f, 1.0f) };
                emit (c);
            }
            else if (rpn == 2)
            {
                // Channel coarse tuning, MSB only: 00 = −12, 40 = A440, 7F = +12 [p.123]
                const int semis = std::clamp (s.dataMsb - 64, -12, 12);
                c = { midislot::kTranspose, -1, ParamChange::Type::Index, static_cast<float> (semis) };
                emit (c);
            }
            return;
        }

        const int nrpn = s.nrpnMsb << 7 | s.nrpnLsb;
        if (nrpn >= midimap::kNrpnPatchBase && nrpn < midimap::kNrpnPatchBase + 128)
        {
            const int cc = nrpn - midimap::kNrpnPatchBase;
            const auto& e = midimap::kCc[static_cast<size_t> (cc)];
            if (e.kind != CcKind::Continuous) return;                    // 14-bit NRPNs mirror continuous CCs only
            if (e.scope == CcScope::Layer && layer < 0) return;
            c = { cc, e.scope == CcScope::Layer ? layer : -1, ParamChange::Type::Normalized, static_cast<float> (v14) / 16383.0f };
            emit (c);
            return;
        }

        switch (nrpn)
        {
            case midimap::kNrpnMidiChannel: c = { midislot::kMidiChannel, -1, ParamChange::Type::Index, static_cast<float> (std::clamp (s.dataMsb, 0, 15) + 1) }; break;
            case midimap::kNrpnClockTx:     c = { midislot::kClockTx, -1, ParamChange::Type::Bool, s.dataMsb > 0 ? 1.0f : 0.0f }; break;
            case midimap::kNrpnClockRx:     c = { midislot::kClockRx, -1, ParamChange::Type::Bool, s.dataMsb > 0 ? 1.0f : 0.0f }; break;
            case midimap::kNrpnPcTx:        c = { midislot::kPcTx, -1, ParamChange::Type::Bool, s.dataMsb > 0 ? 1.0f : 0.0f }; break;
            case midimap::kNrpnPcRx:        c = { midislot::kPcRx, -1, ParamChange::Type::Bool, s.dataMsb > 0 ? 1.0f : 0.0f }; break;
            default: return;
        }
        emit (c);
    }

    std::array<Channel, 16> chans {};
    int base = 1;
    bool rx = true;
};

} // namespace sg
