#pragma once

// Photographed hardware parts, embedded as PNGs and cut out at load (DD-74).
//
// Each sheet holds every part of one kind side by side, so the whole set shares a single light
// rig. On first use the sheet is split into its parts, each trimmed to its own bounds. Sheets
// arrive either on the flat magenta ground they were generated on or already cut out; both work.
//
// A knob is photographed from slightly above, so it cannot be rotated as a whole — that would make
// it tumble rather than turn. The body is drawn fixed (its highlight stays where the light is) and
// the pointer is drawn over it, swept around the ellipse the flat top face makes on screen.
//
// Every accessor returns an invalid image when its sheet is missing, and every drawing routine
// falls back to the drawn version, so the panel still works with no artwork at all.

#include "Theme.h"

namespace sgui::art
{

// The flat top face of a knob, as fractions of that knob's own bounding box. Measured once from
// resources/knobs.png; all three knobs in the set share it, being one render in three colours.
inline constexpr float kKnobFaceCx = 0.487f, kKnobFaceCy = 0.4125f;
inline constexpr float kKnobFaceRx = 0.398f, kKnobFaceRy = 0.350f;

const juce::Image& knob (Tone tone);
const juce::Image& faderCap (Tone tone);
const juce::Image& faderTrack();
const juce::Image& button (bool onDark, bool lit);
const juce::Image& cheek();
const juce::Image& ribbon();
const juce::Image& glass();
const juce::Image& panelTile();

// The key light as a ready-to-draw overlay: warm where the light lands, cool in the shadow,
// transparent where the map was neutral.
const juce::Image& shading();

// Stretch an image to fill r without distorting its corners: the corners are drawn at their own
// size and only the edges and middle stretch. `inset` is the corner size as a fraction of the
// image's shorter side.
void drawNineSlice (juce::Graphics& g, const juce::Image& image, juce::Rectangle<float> r, float inset);

} // namespace sgui::art
