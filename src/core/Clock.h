#pragma once

// Tempo and clock divisions [manual pp.55–56, 65–66, 79, 92–93].

#include "DspMath.h"

#include <array>

namespace sg
{

// Tempo source for one host block: the host's tempo/transport when EXT CLK receive is on
// (DD-3), otherwise the internal TEMPO control (30–300 BPM [p.79]).
struct ClockInfo
{
    double bpm = 120.0;
    bool hostPlaying = false;     // transport running and sync to host enabled
    double ppq = 0.0;             // host position in quarter notes at the block start
};

namespace clockdiv
{
// LFO 1 RATE with SYNC on: cycle length in beats, fader bottom → top [pp.55–56].
inline constexpr std::array<double, 16> kLfo1Beats { 32.0, 16.0, 8.0, 4.0, 2.0, 1.5, 4.0 / 3.0, 1.0,
                                                     0.75, 2.0 / 3.0, 0.5, 0.375, 1.0 / 3.0, 0.25, 0.1875, 1.0 / 6.0 };
// DELAY TIME with SYNC on: delay in beats, knob min → max [pp.65–66].
inline constexpr std::array<double, 16> kDelayBeats { 1.0 / 12.0, 3.0 / 32.0, 1.0 / 8.0, 1.0 / 6.0, 3.0 / 16.0, 0.25, 1.0 / 3.0, 0.375,
                                                      0.5, 2.0 / 3.0, 0.75, 1.0, 4.0 / 3.0, 1.5, 2.0, 8.0 / 3.0 };
// CLK DIV: 1/1, 1/2, 1/4, 1/8, 1/16, 1/32, 1/4T, 1/8T — step length in beats [p.92, CC 107].
inline constexpr std::array<double, 8> kClockDivBeats { 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 2.0 / 3.0, 1.0 / 3.0 };
inline constexpr std::array<const char*, 8> kClockDivNames { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T" };
inline constexpr std::array<const char*, 16> kLfo1Names { "8 whole", "4 whole", "2 whole", "1/1", "1/2", "1/4.", "1/2T", "1/4",
                                                          "1/8.", "1/4T", "1/8", "1/16.", "1/8T", "1/16", "1/32.", "1/16T" };
inline constexpr std::array<const char*, 16> kDelayNames { "1/32T", "1/64.", "1/32", "1/16T", "1/32.", "1/16", "1/8T", "1/16.",
                                                           "1/8", "1/4T", "1/8.", "1/4", "1/2T", "1/4.", "1/2", "1/1T" };

// Continuous fader position → one of `count` detents.
inline int index (float fader, int count) noexcept
{
    const int i = static_cast<int> (fader * static_cast<float> (count));
    return i < 0 ? 0 : (i >= count ? count - 1 : i);
}

// SWING 0…4 [p.93]: the second step of each pair is delayed to this fraction of the pair (DD-52).
inline constexpr std::array<double, 5> kSwingFraction { 0.5, 0.54, 0.58, 0.625, 0.67 };
} // namespace clockdiv

} // namespace sg
