#pragma once

// Look of the instrument (DD-71, DD-73): the original synth's colours — light grey machined metal,
// white and orange knobs and caps, charcoal sub-panels, orange printed rules — rendered as a real
// object in the manner of classic-synth recreations: end cheeks, a rolled front lip into the
// keybed, engraved panel graphics, LED push-keys, skirted knobs, and amber screens under glass.
// One light source (top left) sets every highlight and shadow.

#include <juce_gui_basics/juce_gui_basics.h>

namespace sgui
{
namespace col
{
inline const juce::Colour panel      { 0xfff3ede3 };   // bead-blasted off-white, lit from the left
inline const juce::Colour panelShade { 0xffdcd7d0 };
inline const juce::Colour section    { 0xfff0ebe2 };   // pop-over pages
inline const juce::Colour border     { 0xffccc7be };
inline const juce::Colour engrave    { 0xffb7b2aa };   // printed / engraved outlines
inline const juce::Colour accent     { 0xffe2402a };   // vermillion: LEDs, LOWER caps, rules
inline const juce::Colour text       { 0xff3c3c3a };
inline const juce::Colour tick       { 0xff8e8982 };
inline const juce::Colour dim        { 0xffbdb8b0 };
inline const juce::Colour dark       { 0xff27282a };
inline const juce::Colour darkPanel  { 0xff424243 };   // oscillator / envelope sub-panels
inline const juce::Colour darkText   { 0xffdddee0 };
inline const juce::Colour footer     { 0xff161718 };
inline const juce::Colour buttonOff  { 0xfff7f2e9 };
inline const juce::Colour screen     { 0xff0b0c0d };
inline const juce::Colour lcdText    { 0xffffffff };   // trace inside the glass displays
inline const juce::Colour glassText  { 0xffbcc8d6 };   // legends etched into the smoked glass
inline const juce::Colour upper      { 0xfff6f4ee };   // UPPER layer LED (white caps)
inline const juce::Colour lower      { 0xffe2402a };   // LOWER layer LED (orange caps)
} // namespace col

// Knob / fader cap colour: UPPER layer white, LOWER layer orange, global controls black.
enum class Tone { White, Orange, Black };

juce::Font uiFont (float height, bool bold = true);

void drawText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, float height,
               juce::Justification just = juce::Justification::centred, juce::Colour colour = col::text, bool bold = true,
               int maxLines = 1);

// Secondary (SHIFT) legend in inverse colours under a primary label.
void drawSubLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, bool active, bool onDark = false);

void drawLed (juce::Graphics& g, juce::Point<float> centre, float radius, float level, juce::Colour colour = col::accent);

// ── materials ──
void drawMetal (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour base, float lightAmount = 1.0f);   // brushed + grain
void drawEngravedFrame (juce::Graphics& g, juce::Rectangle<float> r, float corner = 4.0f);
void drawRecess (juce::Graphics& g, juce::Rectangle<float> r, float corner, juce::Colour fill);
void drawDarkPanel (juce::Graphics& g, juce::Rectangle<float> r);
void drawScrew (juce::Graphics& g, juce::Point<float> centre, float radius);
void drawCheek (juce::Graphics& g, juce::Rectangle<float> r, bool left);
void drawLip (juce::Graphics& g, juce::Rectangle<float> r);
void drawKeybed (juce::Graphics& g, juce::Rectangle<float> r);

// ── controls ──
void drawHwKnob (juce::Graphics& g, juce::Rectangle<float> circle, float norm, Tone tone, int steps, bool bipolar,
                 bool onDark, bool enabled, float modNorm = -1.0f);
void drawHwFader (juce::Graphics& g, juce::Rectangle<float> slot, float capY, float capWidth, Tone tone, bool scale,
                  bool bipolar, bool onDark, bool enabled, float modY = -1.0f);
// Push-key with an LED window (the synth's panel buttons): lit = pressed in, LED on.
void drawKey (juce::Graphics& g, juce::Rectangle<float> r, bool lit, bool enabled, bool hover, bool pressed = false);
void drawKeyLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, bool lit, bool enabled);
void drawButtonFace (juce::Graphics& g, juce::Rectangle<float> r, bool on, bool enabled, bool hover);   // pop-over pages
void drawSectionFrame (juce::Graphics& g, juce::Rectangle<float> r);                                   // pop-over pages
void drawInsetBox (juce::Graphics& g, juce::Rectangle<float> r);
void drawDarkBox (juce::Graphics& g, juce::Rectangle<float> r, bool enabled);                          // dropdown well

// ── screens ──
void drawScreen (juce::Graphics& g, juce::Rectangle<float> r);            // bezel + glass + phosphor ground
void drawScreenGlass (juce::Graphics& g, juce::Rectangle<float> r);       // reflection on top of the content
void drawLcd (juce::Graphics& g, juce::Rectangle<float> r);
void drawLcdText (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, float height,
                  juce::Justification just = juce::Justification::centredLeft, float brightness = 1.0f);
void strokeTrace (juce::Graphics& g, const juce::Path& path, float alpha = 1.0f, float thickness = 1.5f);

class SgLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SgLookAndFeel();
    juce::Font getPopupMenuFont() override;
    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
};

enum class Glyph { Sine, Saw, Square, Triangle, Noise, Pulse, Alt, RevSaw, SampleHold, Hf };
void drawGlyph (juce::Graphics& g, Glyph glyph, juce::Rectangle<float> r, juce::Colour colour, float thickness = 1.4f);

} // namespace sgui
