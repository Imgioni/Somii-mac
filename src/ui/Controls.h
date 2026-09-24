#pragma once

// Panel controls, each bound to one host parameter through juce::ParameterAttachment (so host
// automation, MIDI CC and the editor stay in step). A control can be re-bound at any time — the
// top editor row follows the selected layer, and SHIFT swaps controls to their secondary function.

#include "Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>

namespace sgui
{

class ParamControl : public juce::Component, public juce::SettableTooltipClient
{
public:
    ParamControl() = default;
    ~ParamControl() override = default;

    // primary / secondary (SHIFT or Alt-drag) parameters; either may be null.
    void setBindings (juce::RangedAudioParameter* primary, juce::RangedAudioParameter* secondary = nullptr);
    void setShift (bool on);
    juce::RangedAudioParameter* getParam() const noexcept { return param; }
    juce::RangedAudioParameter* getPrimary() const noexcept { return primary; }
    bool showsSecondary() const noexcept { return param != nullptr && param == secondary; }
    bool hasSecondary() const noexcept { return secondary != nullptr; }

    float getNorm() const noexcept { return norm; }
    int getIndex() const noexcept;                 // choice / int / bool parameters
    int getNumSteps() const noexcept;              // 0 = continuous
    juce::String getValueText() const;

    std::function<void (ParamControl&, bool active)> onValueDisplay;   // while the user changes it
    std::function<void (ParamControl&, juce::PopupMenu&)> onPopulateMenu;
    std::function<void()> onChange;

    bool onDark = false;       // sits on a charcoal sub-panel
    Tone tone = Tone::White;   // knob / fader cap colour

    // Live modulation read-out: where modulation has pushed this parameter (0…1), −1 = none.
    void setModNorm (float n) { if (std::abs (n - modNorm) > 0.002f) { modNorm = n; repaint(); } }
    float getModNorm() const noexcept { return modNorm; }

protected:
    void bind (juce::RangedAudioParameter* p);
    void beginGesture();
    void setNormInGesture (float n);
    void endGesture();
    void setNormComplete (float n);
    void setIndexComplete (int index);
    void resetToDefault();
    void holdSecondary (bool on);   // Alt-drag: the secondary function while the mouse is down
    bool showContextMenu (const juce::MouseEvent& e);
    juce::Colour legendColour() const;
    virtual void valueChanged() { repaint(); }

    juce::RangedAudioParameter* param = nullptr;
    float norm = 0.0f;
    float modNorm = -1.0f;
    bool gestureActive = false;

private:
    juce::RangedAudioParameter* primary = nullptr;
    juce::RangedAudioParameter* secondary = nullptr;
    bool shift = false;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// Round knob: vertical drag (Ctrl = fine), wheel, double-click = default, Alt-drag = secondary
// function, right-click menu.
class Knob : public ParamControl
{
public:
    enum class Size { Large, Medium, Small, Tiny };

    Knob (juce::String title, Size size, juce::String minTick = "0", juce::String maxTick = "10");

    juce::String title, subLabel, minTick, maxTick;
    bool bipolar = false;
    Size size;

    float getDiameter() const noexcept;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

private:
    juce::Rectangle<float> knobArea() const;
    float dragStartNorm = 0.0f;
    bool altSwitched = false;
};

class Fader : public ParamControl
{
public:
    explicit Fader (juce::String title, bool showScale = true);

    juce::String title, subLabel;
    bool showScale;
    bool bipolar = false;

    juce::Rectangle<float> trackArea() const;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

private:
    float capY() const;
    float dragStartNorm = 0.0f;
    bool altSwitched = false;
};

// N-segment radio group for a choice parameter (three-position hardware switches, build
// prompt §7 rule 6). values[i] = the parameter index segment i selects.
class SegGroup : public ParamControl
{
public:
    SegGroup (juce::StringArray labels, bool vertical = false);

    juce::StringArray labels;
    juce::Array<int> values;
    bool vertical;
    int offValue = -1;                       // ≥ 0: clicking the lit segment selects this value
    juce::Array<Glyph> glyphs;
    std::function<bool (int segment)> segmentEnabled;
    int flashSegment = -1;                   // segment drawn with a flashing outline (TRANSPOSED)
    bool flashOn = false;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

private:
    int segmentAt (juce::Point<float> p) const;
    juce::Rectangle<float> segmentRect (int i) const;
    int hover = -1;
};

// CHORUS I / CHORUS II lit buttons for the 4-way chorus parameter: OFF, I, II, I+II = both lit [p.64].
class ChorusButtons : public ParamControl
{
public:
    explicit ChorusButtons (bool isVertical) : vertical (isVertical) {}
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;

private:
    juce::Rectangle<float> buttonRect (int i) const;
    bool vertical;
};

// Bool parameter as a lit panel button, or a small keycap with an LED.
class Toggle : public ParamControl
{
public:
    enum class Style { Lit, LedButton };

    Toggle (juce::String label, Style style = Style::Lit);

    juce::String label;
    Style style;
    bool momentary = false;                  // on while held; Alt-click latches (DD-9)
    juce::Colour ledColour = col::accent;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    bool latchedByAlt = false;
};

// Recessed dark dropdown listing a choice parameter's values.
class Dropdown : public ParamControl
{
public:
    Dropdown() = default;
    std::function<juce::String (int index)> itemName;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
};

// ◀ value ▶ for stepped parameters.
class Stepper : public ParamControl
{
public:
    Stepper() = default;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
};

// Dark dropdown box for editor-side choices (not a parameter).
class MenuBox : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<juce::String()> text;
    std::function<void (juce::PopupMenu&)> populate;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
};

// A panel button that runs an action (not a parameter).
class ActionButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    ActionButton (juce::String label, std::function<void()> action);
    juce::String label;
    std::function<void()> action;
    std::function<bool()> isLit;
    Glyph glyph = Glyph::Sine;
    bool useGlyph = false;
    bool dark = false;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    bool down = false;
};

class Led : public juce::Component
{
public:
    explicit Led (juce::Colour c = col::accent) : colour (c) { setInterceptsMouseClicks (false, false); }
    void setLevel (float l) { if (std::abs (l - level) > 0.01f) { level = l; repaint(); } }
    juce::Colour colour;
    void paint (juce::Graphics& g) override;

private:
    float level = 0.0f;
};

class TabBar : public juce::Component
{
public:
    explicit TabBar (juce::StringArray names);
    juce::StringArray names;
    int current = 0;
    std::function<void (int)> onChange;
    void setCurrent (int i);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
};

// A titled, raised panel section; legends, recessed boxes and charcoal sub-panels are painted
// from lists. The title is followed by a thin orange rule, as printed on the hardware.
class Section : public juce::Component
{
public:
    explicit Section (juce::String title, float titleHeight = 15.0f);

    void addLegend (juce::Rectangle<float> area, juce::String text, float height = 9.0f,
                    juce::Justification just = juce::Justification::centred, bool onDarkPanel = false);
    void addDivider (juce::Rectangle<float> line) { dividers.add (line); }
    void addBox (juce::Rectangle<float> box) { boxes.add (box); }
    void addDarkBox (juce::Rectangle<float> box) { darkBoxes.add (box); }
    juce::String title;
    juce::Colour ledColour;
    bool hasLed = false;
    bool frame = true;                       // false: a borderless page inside a section
    float ruleEnd = -1.0f;                   // where the title rule stops (< 0: the right edge)
    float ledLevel = 0.0f;
    std::function<void (juce::Graphics&)> extraPaint;
    void setLedLevel (float l) { if (std::abs (l - ledLevel) > 0.01f) { ledLevel = l; repaint (0, 0, 24, 24); } }

    void paint (juce::Graphics& g) override;

private:
    struct Legend { juce::Rectangle<float> area; juce::String text; float height; juce::Justification just; bool onDarkPanel; };
    float titleHeight;
    juce::Array<Legend> legends;
    juce::Array<juce::Rectangle<float>> dividers, boxes, darkBoxes;
};

// Amber screen with a custom trace.
class WaveDisplay : public juce::Component
{
public:
    std::function<void (juce::Graphics&, juce::Rectangle<float>)> drawer;
    void paint (juce::Graphics& g) override;
};

class LevelMeter : public juce::Component
{
public:
    void setLevels (float l, float r);
    void paint (juce::Graphics& g) override;

private:
    float left = 0.0f, right = 0.0f;
};

class RibbonStrip : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void (float)> onTouch, onMove;
    std::function<void()> onRelease;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    float position (const juce::MouseEvent& e) const;
    float touchPos = -1.0f, currentPos = -1.0f;
};

// Bender: sideways = pitch bend, push (up) = LFO 2 depth / mod wheel [p.69]; springs back.
class BenderPad : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void (float bend, float push)> onMove;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    void update (const juce::MouseEvent& e);
    float bend = 0.0f, push = 0.0f;
};

// Playable keyboard; the lower part of a key plays louder.
class Keyboard : public juce::Component
{
public:
    Keyboard (int lowNote, int highNote) : low (lowNote), high (highNote) {}
    std::function<void (int note, float velocity)> onNoteOn;
    std::function<void (int note)> onNoteOff;
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;

private:
    static bool isBlack (int note) { const int n = note % 12; return n == 1 || n == 3 || n == 6 || n == 8 || n == 10; }
    juce::Rectangle<float> keyRect (int note) const;
    int noteAt (juce::Point<float> p) const;
    void press (int note, float velocity);
    int low, high, held = -1;
};

// Floating value read-out shown while a control is dragged.
class ValueBubble : public juce::Component
{
public:
    ValueBubble() { setInterceptsMouseClicks (false, false); setAlwaysOnTop (true); }
    void show (juce::Component& target, const juce::String& text);
    void paint (juce::Graphics& g) override;

private:
    juce::String text;
};

} // namespace sgui
