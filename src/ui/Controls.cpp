#include "Controls.h"

#include "Artwork.h"

namespace sgui
{

// ── ParamControl ────────────────────────────────────────────────────────────────────────────

void ParamControl::setBindings (juce::RangedAudioParameter* p, juce::RangedAudioParameter* s)
{
    primary = p;
    secondary = s;
    bind (shift && secondary != nullptr ? secondary : primary);
}

void ParamControl::setShift (bool on)
{
    if (shift == on) return;
    shift = on;
    if (! gestureActive) bind (shift && secondary != nullptr ? secondary : primary);
}

void ParamControl::holdSecondary (bool on)
{
    bind ((on || shift) && secondary != nullptr ? secondary : primary);
}

void ParamControl::bind (juce::RangedAudioParameter* p)
{
    if (p == param && (attachment != nullptr || p == nullptr)) return;
    if (gestureActive) endGesture();
    attachment.reset();
    param = p;
    if (param != nullptr)
    {
        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
        {
            norm = param->convertTo0to1 (v);
            valueChanged();
            if (onChange) onChange();
        }, nullptr);
        attachment->sendInitialUpdate();
    }
    else
    {
        norm = 0.0f;
    }
    repaint();
}

int ParamControl::getNumSteps() const noexcept
{
    // AudioParameterInt reports isDiscrete() == false, but it has whole-number steps.
    if (param == nullptr || ! (param->isDiscrete() || dynamic_cast<juce::AudioParameterInt*> (param) != nullptr)) return 0;
    return param->getNumSteps();
}

int ParamControl::getIndex() const noexcept
{
    const int steps = getNumSteps();
    return steps > 1 ? juce::roundToInt (norm * static_cast<float> (steps - 1)) : 0;
}

juce::String ParamControl::getValueText() const
{
    return param != nullptr ? param->getText (norm, 64) : juce::String();
}

void ParamControl::beginGesture()
{
    if (attachment == nullptr || gestureActive) return;
    gestureActive = true;
    attachment->beginGesture();
}

void ParamControl::setNormInGesture (float n)
{
    if (attachment == nullptr) return;
    attachment->setValueAsPartOfGesture (param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, n)));
    if (onValueDisplay) onValueDisplay (*this, true);
}

void ParamControl::endGesture()
{
    if (attachment == nullptr || ! gestureActive) return;
    gestureActive = false;
    attachment->endGesture();
    if (onValueDisplay) onValueDisplay (*this, false);
}

void ParamControl::setNormComplete (float n)
{
    if (attachment == nullptr) return;
    attachment->setValueAsCompleteGesture (param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, n)));
}

void ParamControl::setIndexComplete (int index)
{
    const int steps = getNumSteps();
    if (steps > 1) setNormComplete (static_cast<float> (juce::jlimit (0, steps - 1, index)) / static_cast<float> (steps - 1));
}

void ParamControl::resetToDefault()
{
    if (param != nullptr) setNormComplete (param->getDefaultValue());
}

bool ParamControl::showContextMenu (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() || param == nullptr) return false;
    juce::PopupMenu menu;
    menu.addSectionHeader (param->getName (64) + ": " + getValueText());
    menu.addItem ("Reset to default", [this] { resetToDefault(); });
    if (onPopulateMenu) onPopulateMenu (*this, menu);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
    return true;
}

juce::Colour ParamControl::legendColour() const
{
    const bool enabled = isEnabled() && param != nullptr;
    if (onDark) return col::darkText.withMultipliedAlpha (enabled ? 1.0f : 0.45f);
    return enabled ? col::text : col::dim;
}

// ── Knob ────────────────────────────────────────────────────────────────────────────────────

Knob::Knob (juce::String t, Size s, juce::String lo, juce::String hi)
    : title (std::move (t)), minTick (std::move (lo)), maxTick (std::move (hi)), size (s)
{
}

float Knob::getDiameter() const noexcept
{
    switch (size)
    {
        case Size::Large:  return 48.0f;
        case Size::Medium: return 33.0f;
        case Size::Small:  return 26.0f;
        case Size::Tiny:   return 21.0f;
    }
    return 33.0f;
}

juce::Rectangle<float> Knob::knobArea() const
{
    const float d = getDiameter();
    const float titleH = title.isEmpty() ? 0.0f : (size == Size::Tiny ? 10.0f : 12.0f);
    const float subH = subLabel.isEmpty() ? 0.0f : 10.0f;
    return { (static_cast<float> (getWidth()) - d) * 0.5f, titleH + subH + 6.0f, d, d };
}

void Knob::paint (juce::Graphics& g)
{
    const bool on = isEnabled() && param != nullptr;
    const float titleH = size == Size::Tiny ? 10.0f : 12.0f;
    const float fontH = size == Size::Large ? 11.5f : (size == Size::Tiny ? 8.5f : 10.0f);
    if (title.isNotEmpty())
        drawText (g, title, { -4.0f, 0.0f, static_cast<float> (getWidth()) + 8.0f, titleH }, fontH, juce::Justification::centred, legendColour());
    if (subLabel.isNotEmpty())
        drawSubLabel (g, subLabel, { 2.0f, titleH + 0.5f, static_cast<float> (getWidth()) - 4.0f, 9.0f }, showsSecondary(), onDark);

    const auto k = knobArea();
    drawHwKnob (g, k, norm, tone, getNumSteps(), bipolar, onDark, on, modNorm);

    const auto c = k.getCentre();
    const float r = k.getWidth() * 0.5f;
    const float tw = juce::jmax (18.0f, r * 1.1f);
    const float th = size == Size::Tiny ? 7.5f : 8.0f;
    const auto tickColour = (onDark ? juce::Colour (0xffb5b6b9) : col::tick).withMultipliedAlpha (on ? 1.0f : 0.5f);
    drawText (g, minTick, { c.x - r - tw * 0.5f - 2.0f, k.getBottom() - 1.0f, tw, 10.0f }, th, juce::Justification::centred, tickColour, false);
    drawText (g, maxTick, { c.x + r - tw * 0.5f + 2.0f, k.getBottom() - 1.0f, tw, 10.0f }, th, juce::Justification::centred, tickColour, false);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (showContextMenu (e) || param == nullptr || ! isEnabled()) return;
    altSwitched = e.mods.isAltDown() && hasSecondary();   // Alt-drag = secondary function
    if (altSwitched) holdSecondary (true);
    dragStartNorm = norm;
    beginGesture();
    if (onValueDisplay) onValueDisplay (*this, true);
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    if (! gestureActive) return;
    const int steps = getNumSteps();
    if (steps > 1)
    {
        const int start = juce::roundToInt (dragStartNorm * static_cast<float> (steps - 1));
        const int index = juce::jlimit (0, steps - 1, start - juce::roundToInt (static_cast<float> (e.getDistanceFromDragStartY()) / 16.0f));
        setNormInGesture (static_cast<float> (index) / static_cast<float> (steps - 1));
        return;
    }
    const float sensitivity = e.mods.isCtrlDown() || e.mods.isCommandDown() ? 1000.0f : 200.0f;
    setNormInGesture (dragStartNorm - static_cast<float> (e.getDistanceFromDragStartY()) / sensitivity);
}

void Knob::mouseUp (const juce::MouseEvent&)
{
    endGesture();
    if (altSwitched) { holdSecondary (false); altSwitched = false; }
}

void Knob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (isEnabled()) resetToDefault();
}

void Knob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (param == nullptr || ! isEnabled()) return;
    const int steps = getNumSteps();
    const float delta = (w.deltaY != 0.0f ? w.deltaY : w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
    if (steps > 1) { if (delta != 0.0f) setIndexComplete (getIndex() + (delta > 0.0f ? 1 : -1)); }
    else           setNormComplete (norm + delta * (e.mods.isCtrlDown() ? 0.01f : 0.05f));
}

// ── Fader ───────────────────────────────────────────────────────────────────────────────────

Fader::Fader (juce::String t, bool scale) : title (std::move (t)), showScale (scale) {}

juce::Rectangle<float> Fader::trackArea() const
{
    const float top = (title.isEmpty() ? 4.0f : 14.0f) + (subLabel.isEmpty() ? 0.0f : 10.0f) + 4.0f;
    const float x = static_cast<float> (getWidth()) * 0.5f + (showScale ? 2.5f : 0.0f) - 2.0f;
    return { x, top, 4.0f, static_cast<float> (getHeight()) - top - 6.0f };
}

float Fader::capY() const
{
    const auto t = trackArea();
    return t.getBottom() - 5.5f - norm * (t.getHeight() - 11.0f);
}

void Fader::paint (juce::Graphics& g)
{
    const bool on = isEnabled() && param != nullptr;
    if (title.isNotEmpty())
        drawText (g, title, { -4.0f, 0.0f, static_cast<float> (getWidth()) + 8.0f, 12.0f }, 9.5f, juce::Justification::centred, legendColour());
    if (subLabel.isNotEmpty())
        drawSubLabel (g, subLabel, { 1.0f, 13.0f, static_cast<float> (getWidth()) - 2.0f, 9.0f }, showsSecondary(), onDark);
    const float capW = juce::jmin (static_cast<float> (getWidth()) - 6.0f, 22.0f);
    const auto t = trackArea();
    const float modY = modNorm < 0.0f ? -1.0f : t.getBottom() - 5.5f - juce::jlimit (0.0f, 1.0f, modNorm) * (t.getHeight() - 11.0f);
    drawHwFader (g, t, capY(), capW, tone, showScale, bipolar, onDark, on, modY);
}

void Fader::mouseDown (const juce::MouseEvent& e)
{
    if (showContextMenu (e) || param == nullptr || ! isEnabled()) return;
    altSwitched = e.mods.isAltDown() && hasSecondary();
    if (altSwitched) holdSecondary (true);
    const auto t = trackArea();
    if (std::abs (e.position.y - capY()) > 7.0f)   // click on the slot: jump there
        setNormComplete ((t.getBottom() - 5.5f - e.position.y) / (t.getHeight() - 11.0f));
    dragStartNorm = norm;
    beginGesture();
    if (onValueDisplay) onValueDisplay (*this, true);
}

void Fader::mouseDrag (const juce::MouseEvent& e)
{
    if (! gestureActive) return;
    const float scale = e.mods.isCtrlDown() || e.mods.isCommandDown() ? 0.2f : 1.0f;
    setNormInGesture (dragStartNorm - scale * static_cast<float> (e.getDistanceFromDragStartY()) / (trackArea().getHeight() - 11.0f));
}

void Fader::mouseUp (const juce::MouseEvent&)
{
    endGesture();
    if (altSwitched) { holdSecondary (false); altSwitched = false; }
}

void Fader::mouseDoubleClick (const juce::MouseEvent&)
{
    if (isEnabled()) resetToDefault();
}

void Fader::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (param == nullptr || ! isEnabled()) return;
    const float delta = (w.deltaY != 0.0f ? w.deltaY : w.deltaX) * (w.isReversed ? -1.0f : 1.0f);
    setNormComplete (norm + delta * (e.mods.isCtrlDown() ? 0.01f : 0.05f));
}

// ── SegGroup ────────────────────────────────────────────────────────────────────────────────

SegGroup::SegGroup (juce::StringArray l, bool v) : labels (std::move (l)), vertical (v)
{
    for (int i = 0; i < labels.size(); ++i) values.add (i);
}

juce::Rectangle<float> SegGroup::segmentRect (int i) const
{
    const int n = juce::jmax (1, labels.size());
    const auto b = getLocalBounds().toFloat().reduced (1.0f, 1.0f).withTrimmedBottom (1.0f);
    constexpr float gap = 3.0f;
    if (vertical)
    {
        const float h = (b.getHeight() - gap * static_cast<float> (n - 1)) / static_cast<float> (n);
        return { b.getX(), b.getY() + static_cast<float> (i) * (h + gap), b.getWidth(), h };
    }
    const float w = (b.getWidth() - gap * static_cast<float> (n - 1)) / static_cast<float> (n);
    return { b.getX() + static_cast<float> (i) * (w + gap), b.getY(), w, b.getHeight() };
}

int SegGroup::segmentAt (juce::Point<float> p) const
{
    for (int i = 0; i < labels.size(); ++i)
        if (segmentRect (i).expanded (1.5f).contains (p)) return i;
    return -1;
}

void SegGroup::paint (juce::Graphics& g)
{
    const int index = getIndex();
    const bool on = isEnabled() && param != nullptr;
    for (int i = 0; i < labels.size(); ++i)
    {
        const auto r = segmentRect (i);
        const bool segOn = on && (segmentEnabled == nullptr || segmentEnabled (i));
        const bool lit = on && values[i] == index;
        drawKey (g, r, lit && segOn, segOn, hover == i);
        if (i == flashSegment && flashOn)
        {
            g.setColour (col::accent);
            g.drawRoundedRectangle (r.expanded (1.0f), 3.0f, 2.0f);
        }
        const auto textColour = segOn ? col::text : col::dim;
        if (i < glyphs.size())
            drawGlyph (g, glyphs[i], r.withTrimmedTop (6.0f).reduced (r.getWidth() * 0.2f, r.getHeight() * 0.2f), textColour, 1.4f);
        else
            drawKeyLabel (g, labels[i], r, lit && segOn, segOn);
    }
}

void SegGroup::mouseDown (const juce::MouseEvent& e)
{
    if (showContextMenu (e) || param == nullptr || ! isEnabled()) return;
    const int i = segmentAt (e.position);
    if (i < 0 || (segmentEnabled != nullptr && ! segmentEnabled (i))) return;
    if (offValue >= 0 && values[i] == getIndex()) setIndexComplete (offValue);
    else                                          setIndexComplete (values[i]);
}

void SegGroup::mouseMove (const juce::MouseEvent& e)
{
    const int h = segmentAt (e.position);
    if (h != hover) { hover = h; repaint(); }
}

void SegGroup::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
    repaint();
}

// ── ChorusButtons ───────────────────────────────────────────────────────────────────────────

juce::Rectangle<float> ChorusButtons::buttonRect (int i) const
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    constexpr float gap = 5.0f;
    return vertical ? b.withHeight ((b.getHeight() - gap) * 0.5f).withY (b.getY() + static_cast<float> (i) * (b.getHeight() + gap) * 0.5f)
                    : b.withWidth ((b.getWidth() - gap) * 0.5f).withX (b.getX() + static_cast<float> (i) * (b.getWidth() + gap) * 0.5f);
}

void ChorusButtons::paint (juce::Graphics& g)
{
    const int mode = getIndex();   // 0 OFF, 1 I, 2 II, 3 I+II
    const bool enabled = isEnabled() && param != nullptr;
    for (int i = 0; i < 2; ++i)
    {
        const bool lit = enabled && (mode & (1 << i)) != 0;
        const auto r = buttonRect (i);
        drawKey (g, r, lit, enabled, false);
        drawKeyLabel (g, i == 0 ? "CHORUS I" : "CHORUS II", r, lit, enabled);
    }
}

void ChorusButtons::mouseDown (const juce::MouseEvent& e)
{
    if (showContextMenu (e) || param == nullptr || ! isEnabled()) return;
    for (int i = 0; i < 2; ++i)
        if (buttonRect (i).contains (e.position))
            setIndexComplete (getIndex() ^ (1 << i));
}

// ── Toggle ──────────────────────────────────────────────────────────────────────────────────

Toggle::Toggle (juce::String l, Style s) : label (std::move (l)), style (s) {}

void Toggle::paint (juce::Graphics& g)
{
    const bool on = norm >= 0.5f;
    const bool enabled = isEnabled() && param != nullptr;
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    if (style == Style::Lit)
    {
        drawKey (g, b, on && enabled, enabled, isMouseOver());
        drawKeyLabel (g, label, b, on && enabled, enabled);
        return;
    }
    // Small dark keycap with an LED, legend to the right.
    const float s = juce::jmin (b.getHeight(), 18.0f);
    const auto sq = juce::Rectangle<float> (s, s).withY (b.getCentreY() - s * 0.5f).withX (b.getX());
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (sq.translated (0.6f, 1.4f).expanded (0.5f), 3.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff47484b), sq.getX(), sq.getY(), juce::Colour (0xff17181a), sq.getX(), sq.getBottom(), false));
    g.fillRoundedRectangle (sq, 2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.18f));
    g.drawHorizontalLine (juce::roundToInt (sq.getY() + 1.2f), sq.getX() + 2.0f, sq.getRight() - 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (sq.reduced (0.4f), 2.5f, 0.8f);
    drawLed (g, sq.getCentre(), s * 0.18f, on && enabled ? 1.0f : 0.0f, ledColour);
    drawText (g, label, b.withTrimmedLeft (s + 5.0f), juce::jmin (10.0f, b.getHeight() * 0.62f), juce::Justification::centredLeft,
              enabled ? col::text : col::dim);
}

void Toggle::mouseDown (const juce::MouseEvent& e)
{
    if (showContextMenu (e) || param == nullptr || ! isEnabled()) return;
    if (momentary)
    {
        if (e.mods.isAltDown()) { latchedByAlt = norm < 0.5f; setNormComplete (latchedByAlt ? 1.0f : 0.0f); return; }
        latchedByAlt = false;
        setNormComplete (1.0f);
        return;
    }
    setNormComplete (norm >= 0.5f ? 0.0f : 1.0f);
}

void Toggle::mouseUp (const juce::MouseEvent& e)
{
    if (momentary && ! latchedByAlt && ! e.mods.isPopupMenu() && param != nullptr && ! e.mods.isAltDown())
        setNormComplete (0.0f);
}

// ── Dropdown / Stepper / MenuBox ────────────────────────────────────────────────────────────

namespace
{
void drawChevron (juce::Graphics& g, juce::Rectangle<float> b, bool enabled)
{
    juce::Path chevron;
    const float cx = b.getRight() - 8.0f, cy = b.getCentreY();
    chevron.addTriangle (cx - 3.5f, cy - 1.8f, cx + 3.5f, cy - 1.8f, cx, cy + 2.2f);
    g.setColour (juce::Colours::white.withAlpha (enabled ? 0.85f : 0.35f));
    g.fillPath (chevron);
}
} // namespace

void Dropdown::paint (juce::Graphics& g)
{
    const bool enabled = isEnabled() && param != nullptr;
    const auto b = getLocalBounds().toFloat().reduced (0.5f, 1.0f);
    drawDarkBox (g, b, enabled);
    const juce::String text = param == nullptr ? juce::String ("-") : (itemName ? itemName (getIndex()) : getValueText());
    drawText (g, text, b.reduced (6.0f, 0.0f).withTrimmedRight (10.0f), juce::jmin (11.0f, b.getHeight() * 0.62f), juce::Justification::centredLeft,
              juce::Colours::white.withAlpha (enabled ? 0.95f : 0.4f), false);
    drawChevron (g, b, enabled);
}

void Dropdown::mouseDown (const juce::MouseEvent& e)
{
    if (param == nullptr || ! isEnabled()) return;
    if (e.mods.isPopupMenu()) { showContextMenu (e); return; }
    const int steps = getNumSteps();
    if (steps < 2) return;
    juce::PopupMenu menu;
    const int current = getIndex();
    for (int i = 0; i < steps; ++i)
    {
        const float n = static_cast<float> (i) / static_cast<float> (steps - 1);
        const juce::String name = itemName ? itemName (i) : param->getText (n, 64);
        menu.addItem (juce::PopupMenu::Item (name).setTicked (i == current).setAction ([this, i] { setIndexComplete (i); }));
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()));
}

void Stepper::paint (juce::Graphics& g)
{
    const bool enabled = isEnabled() && param != nullptr;
    const auto b = getLocalBounds().toFloat().reduced (0.5f, 1.0f);
    drawDarkBox (g, b, enabled);
    drawText (g, getValueText(), b.reduced (12.0f, 0.0f), juce::jmin (11.0f, b.getHeight() * 0.64f), juce::Justification::centred,
              juce::Colours::white.withAlpha (enabled ? 0.95f : 0.4f), false);
    g.setColour (juce::Colours::white.withAlpha (enabled ? 0.8f : 0.3f));
    juce::Path l, r;
    const float cy = b.getCentreY();
    l.addTriangle (b.getX() + 4.0f, cy, b.getX() + 8.5f, cy - 3.5f, b.getX() + 8.5f, cy + 3.5f);
    r.addTriangle (b.getRight() - 4.0f, cy, b.getRight() - 8.5f, cy - 3.5f, b.getRight() - 8.5f, cy + 3.5f);
    g.fillPath (l);
    g.fillPath (r);
}

void Stepper::mouseDown (const juce::MouseEvent& e)
{
    if (showContextMenu (e) || param == nullptr || ! isEnabled()) return;
    setIndexComplete (getIndex() + (e.position.x < static_cast<float> (getWidth()) * 0.5f ? -1 : 1));
}

void MenuBox::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f, 1.0f);
    drawDarkBox (g, b, isEnabled());
    drawText (g, text ? text() : juce::String(), b.reduced (6.0f, 0.0f).withTrimmedRight (10.0f), juce::jmin (11.0f, b.getHeight() * 0.62f),
              juce::Justification::centredLeft, juce::Colours::white.withAlpha (isEnabled() ? 0.95f : 0.4f), false);
    drawChevron (g, b, isEnabled());
}

void MenuBox::mouseDown (const juce::MouseEvent&)
{
    if (! isEnabled() || ! populate) return;
    juce::PopupMenu menu;
    populate (menu);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()));
}

// ── ActionButton ────────────────────────────────────────────────────────────────────────────

ActionButton::ActionButton (juce::String l, std::function<void()> a) : label (std::move (l)), action (std::move (a)) {}

void ActionButton::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    const bool lit = (isLit && isLit()) || down;
    if (dark)
    {
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (b.translated (0.5f, 1.3f).expanded (0.6f), 3.0f);
        const auto top = isMouseOver() ? juce::Colour (0xff4a4b4f) : juce::Colour (0xff3c3d40);
        g.setGradientFill (juce::ColourGradient (top, b.getX(), b.getY(), juce::Colour (0xff1e1f21), b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle (b, 2.5f);
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.drawHorizontalLine (juce::roundToInt (b.getY() + 1.3f), b.getX() + 2.5f, b.getRight() - 2.5f);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (b.reduced (0.4f), 2.5f, 0.9f);
        drawText (g, label, b, juce::jmin (10.0f, b.getHeight() * 0.6f), juce::Justification::centred,
                  lit ? col::lcdText : juce::Colours::white.withAlpha (0.92f));
        return;
    }
    drawKey (g, b, lit, isEnabled(), isMouseOver(), down);
    if (useGlyph) drawGlyph (g, glyph, b.withTrimmedTop (6.0f).reduced (b.getWidth() * 0.25f, b.getHeight() * 0.24f),
                             isEnabled() ? col::text : col::dim);
    else          drawKeyLabel (g, label, b, lit, isEnabled());
}

void ActionButton::mouseDown (const juce::MouseEvent&)
{
    down = true;
    repaint();
}

void ActionButton::mouseUp (const juce::MouseEvent& e)
{
    down = false;
    repaint();
    if (isEnabled() && getLocalBounds().contains (e.getPosition()) && action) action();
}

// ── Led / TabBar / Section ──────────────────────────────────────────────────────────────────

void Led::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    drawLed (g, b.getCentre(), juce::jmin (b.getWidth(), b.getHeight()) * 0.22f, level, colour);
}

TabBar::TabBar (juce::StringArray n) : names (std::move (n)) {}

void TabBar::setCurrent (int i)
{
    if (i == current) return;
    current = i;
    repaint();
    if (onChange) onChange (i);
}

void TabBar::paint (juce::Graphics& g)
{
    const int n = juce::jmax (1, names.size());
    const float gap = 4.0f;
    const auto area = getLocalBounds().toFloat().reduced (1.0f);
    const float w = (area.getWidth() - gap * static_cast<float> (n - 1)) / static_cast<float> (n);
    for (int i = 0; i < names.size(); ++i)
    {
        const juce::Rectangle<float> r (area.getX() + static_cast<float> (i) * (w + gap), area.getY(), w, area.getHeight());
        drawKey (g, r, i == current, true, false);
        drawKeyLabel (g, names[i], r, i == current, true);
    }
}

void TabBar::mouseDown (const juce::MouseEvent& e)
{
    const int n = juce::jmax (1, names.size());
    setCurrent (juce::jlimit (0, n - 1, static_cast<int> (e.position.x / (static_cast<float> (getWidth()) / static_cast<float> (n)))));
}

Section::Section (juce::String t, float th) : title (std::move (t)), titleHeight (th)
{
    setInterceptsMouseClicks (false, true);
}

void Section::addLegend (juce::Rectangle<float> area, juce::String text, float height, juce::Justification just, bool onDarkPanel)
{
    legends.add ({ area, std::move (text), height, just, onDarkPanel });
    repaint();
}

void Section::paint (juce::Graphics& g)
{
    // Printed panel section (DD-77): no floating plate — the section sits directly on the metal,
    // separated from its neighbour by a vermillion rule, with its name set small at the top left.
    const auto bounds = getLocalBounds().toFloat();
    if (frame)
    {
        g.setColour (col::accent.withAlpha (0.9f));
        g.fillRect (juce::Rectangle<float> (0.0f, 3.0f, 1.4f, bounds.getHeight() - 6.0f));
    }
    float x = 9.0f;
    if (hasLed)
    {
        drawLed (g, { 14.0f, 6.0f + titleHeight * 0.5f }, 4.0f, ledLevel, ledColour);
        x = 23.0f;
    }
    if (title.isNotEmpty())
    {
        const auto font = uiFont (titleHeight, true);
        const float tw = juce::GlyphArrangement::getStringWidth (font, title);
        drawText (g, title, { x, 3.0f, tw + 6.0f, titleHeight + 4.0f }, titleHeight, juce::Justification::centredLeft);
    }

    for (const auto& b : boxes) drawInsetBox (g, b);
    for (const auto& b : darkBoxes) drawDarkPanel (g, b);
    g.setColour (col::border);
    for (const auto& d : dividers) g.fillRect (d);
    for (const auto& l : legends)
        drawText (g, l.text, l.area, l.height, l.just, l.onDarkPanel ? col::darkText : col::text, true);
    if (extraPaint) extraPaint (g);
}

// ── WaveDisplay / LevelMeter / RibbonStrip / ValueBubble ────────────────────────────────────

void WaveDisplay::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f, 1.0f);
    drawScreen (g, b);
    const auto inner = b.reduced (4.0f);
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (inner.toNearestInt());
        g.setColour (col::lcdText.withAlpha (0.07f));                       // graticule
        for (int i = 1; i < 4; ++i)
            g.drawVerticalLine (juce::roundToInt (inner.getX() + inner.getWidth() * static_cast<float> (i) / 4.0f), inner.getY() + 2.0f, inner.getBottom() - 2.0f);
        g.drawHorizontalLine (juce::roundToInt (inner.getCentreY()), inner.getX() + 2.0f, inner.getRight() - 2.0f);
        if (drawer) drawer (g, inner.reduced (4.0f, 4.0f));
        drawScreenGlass (g, inner);
    }
}

void LevelMeter::setLevels (float l, float r)
{
    if (std::abs (l - left) > 0.005f || std::abs (r - right) > 0.005f) { left = l; right = r; repaint(); }
}

void LevelMeter::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    drawDarkBox (g, b, true);
    constexpr int segs = 14;
    const float colW = (b.getWidth() - 7.0f) * 0.5f;
    const float segH = (b.getHeight() - 5.0f) / static_cast<float> (segs);
    auto toSegs = [] (float level)
    {
        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        return juce::jlimit (0.0f, static_cast<float> (segs), (db + 54.0f) / 57.0f * static_cast<float> (segs));   // −54 … +3 dB
    };
    const float lit[2] { toSegs (left), toSegs (right) };
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < segs; ++i)
        {
            const juce::Rectangle<float> r (b.getX() + 2.5f + static_cast<float> (c) * (colW + 2.0f),
                                            b.getBottom() - 2.5f - static_cast<float> (i + 1) * segH + 0.8f, colW, segH - 1.6f);
            const auto colour = i >= segs - 2 ? juce::Colour (0xffff2a1a) : (i >= segs - 5 ? col::lcdText : col::accent);
            const float on = juce::jlimit (0.0f, 1.0f, lit[c] - static_cast<float> (i));
            g.setColour (colour.withAlpha (0.12f + 0.88f * on));
            g.fillRect (r);
        }
}

float RibbonStrip::position (const juce::MouseEvent& e) const
{
    return juce::jlimit (0.0f, 1.0f, (e.position.x - 5.0f) / juce::jmax (1.0f, static_cast<float> (getWidth()) - 10.0f));
}

void RibbonStrip::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    if (const auto& photo = art::ribbon(); photo.isValid())
    {
        art::drawNineSlice (g, photo, b, 0.16f);              // end caps keep their shape
    }
    else
    {
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRoundedRectangle (b.translated (0.8f, 1.8f), 4.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4b4e), b.getX(), b.getY(), juce::Colour (0xff1c1d1f), b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawHorizontalLine (juce::roundToInt (b.getY() + 1.5f), b.getX() + 5.0f, b.getRight() - 5.0f);
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        for (float x = b.getX() + b.getWidth() / 10.0f; x < b.getRight() - 4.0f; x += b.getWidth() / 10.0f)
            g.fillRect (juce::Rectangle<float> (x, b.getY() + 3.0f, 1.0f, b.getHeight() - 6.0f));
    }
    drawText (g, "RIBBON", b, juce::jmin (9.0f, b.getHeight() * 0.5f), juce::Justification::centred, juce::Colours::white.withAlpha (0.32f));
    if (currentPos >= 0.0f)
    {
        const float span = b.getWidth() - 10.0f;
        const float xa = b.getX() + 5.0f + touchPos * span, xb = b.getX() + 5.0f + currentPos * span;
        g.setColour (col::accent.withAlpha (0.3f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (xa, xb), b.getY() + 3.0f, juce::jmax (xa, xb), b.getBottom() - 3.0f));
        g.setColour (col::accent);
        g.fillRoundedRectangle (juce::Rectangle<float> (3.0f, b.getHeight() - 6.0f).withCentre ({ xb, b.getCentreY() }), 1.5f);
    }
}

void RibbonStrip::mouseDown (const juce::MouseEvent& e)
{
    touchPos = currentPos = position (e);
    if (onTouch) onTouch (touchPos);
    repaint();
}

void RibbonStrip::mouseDrag (const juce::MouseEvent& e)
{
    currentPos = position (e);
    if (onMove) onMove (currentPos);
    repaint();
}

void RibbonStrip::mouseUp (const juce::MouseEvent&)
{
    touchPos = currentPos = -1.0f;
    if (onRelease) onRelease();
    repaint();
}

// ── BenderPad ───────────────────────────────────────────────────────────────────────────────

void BenderPad::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    // recessed well in the panel
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawRoundedRectangle (b.translated (0.0f, 1.2f), 6.0f, 1.2f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1b1c), b.getX(), b.getY(), juce::Colour (0xff3a3b3e), b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle (b, 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (b.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawVerticalLine (juce::roundToInt (b.getCentreX()), b.getY() + 8.0f, b.getBottom() - 8.0f);

    // lever
    const float cx = b.getCentreX() + bend * (b.getWidth() * 0.5f - 20.0f);
    const float cy = b.getBottom() - 24.0f - push * (b.getHeight() - 52.0f);
    const auto cap = juce::Rectangle<float> (34.0f, 24.0f).withCentre ({ cx, cy });
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (cap.translated (1.5f, 3.0f), 5.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff646569), cap.getX(), cap.getY(), juce::Colour (0xff141517), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawHorizontalLine (juce::roundToInt (cap.getY() + 1.5f), cap.getX() + 3.0f, cap.getRight() - 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    for (int i = -2; i <= 2; ++i)
        g.drawVerticalLine (juce::roundToInt (cap.getCentreX() + static_cast<float> (i) * 5.0f), cap.getY() + 6.0f, cap.getBottom() - 6.0f);
    drawText (g, "BENDER", b.withHeight (13.0f).translated (0.0f, 4.0f), 8.5f, juce::Justification::centred,
              juce::Colours::white.withAlpha (0.45f));
}

void BenderPad::update (const juce::MouseEvent& e)
{
    const auto b = getLocalBounds().toFloat().reduced (22.0f);
    bend = juce::jlimit (-1.0f, 1.0f, (e.position.x - b.getCentreX()) / juce::jmax (1.0f, b.getWidth() * 0.5f));
    push = juce::jlimit (0.0f, 1.0f, (b.getBottom() - e.position.y) / juce::jmax (1.0f, b.getHeight()));
    if (onMove) onMove (bend, push);
    repaint();
}

void BenderPad::mouseDown (const juce::MouseEvent& e) { update (e); }
void BenderPad::mouseDrag (const juce::MouseEvent& e) { update (e); }

void BenderPad::mouseUp (const juce::MouseEvent&)
{
    bend = push = 0.0f;   // spring back
    if (onMove) onMove (0.0f, 0.0f);
    repaint();
}

// ── Keyboard ────────────────────────────────────────────────────────────────────────────────

namespace
{
// Black keys are not centred on the white-key boundary: they sit toward the outside of their
// group of two or three, as on a real keybed.
float blackOffset (int note)
{
    switch (((note % 12) + 12) % 12)
    {
        case 1:  return -0.11f;   // C#
        case 3:  return  0.11f;   // D#
        case 6:  return -0.14f;   // F#
        case 8:  return  0.00f;   // G#
        default: return  0.14f;   // A#
    }
}
} // namespace

juce::Rectangle<float> Keyboard::keyRect (int note) const
{
    int whites = 0;
    for (int n = low; n <= high; ++n) if (! isBlack (n)) ++whites;
    const float ww = static_cast<float> (getWidth()) / static_cast<float> (juce::jmax (1, whites));
    int index = 0;
    for (int n = low; n < note; ++n) if (! isBlack (n)) ++index;
    const float h = static_cast<float> (getHeight());
    if (! isBlack (note)) return { static_cast<float> (index) * ww, 0.0f, ww, h };
    const float bw = ww * 0.55f;
    return { static_cast<float> (index) * ww - bw * 0.5f + blackOffset (note) * ww, 0.0f, bw, h * 0.63f };
}

void Keyboard::paint (juce::Graphics& g)
{
    const float h = static_cast<float> (getHeight());
    const float front = juce::jmax (7.0f, h * 0.055f);       // the visible front face of a white key

    // ── white keys ──
    for (int n = low; n <= high; ++n)
    {
        if (isBlack (n)) continue;
        const auto key = keyRect (n).reduced (0.6f, 0.0f);
        const bool down = n == held;

        juce::Path body;
        body.addRoundedRectangle (key.getX(), key.getY() - 8.0f, key.getWidth(), key.getHeight() + 8.0f, 5.0f, 5.0f, false, false, true, true);
        juce::ColourGradient top (down ? juce::Colour (0xffe8e3da) : juce::Colour (0xfff4f4f2), key.getX(), key.getY(),
                                  down ? juce::Colour (0xffd6cfc5) : juce::Colour (0xffe9e9e6), key.getX(), key.getBottom(), false);
        top.addColour (0.55, down ? juce::Colour (0xffeee9e0) : juce::Colours::white);
        top.addColour (0.90, down ? juce::Colour (0xffdad4ca) : juce::Colour (0xffeeeeeb));
        g.setGradientFill (top);
        g.fillPath (body);

        // ivory grain across the key, and the light falling from the left
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (body);
            juce::ColourGradient side (juce::Colours::white.withAlpha (0.55f), key.getX(), key.getCentreY(),
                                       juce::Colours::black.withAlpha (0.10f), key.getRight(), key.getCentreY(), false);
            side.addColour (0.35, juce::Colours::transparentBlack);
            g.setGradientFill (side);
            g.fillRect (key);
        }

        // front face, stepped out below the playing surface
        const auto face = key.withTop (key.getBottom() - front);
        juce::Path facePath;
        facePath.addRoundedRectangle (face.getX(), face.getY(), face.getWidth(), face.getHeight(), 4.0f, 4.0f, false, false, true, true);
        g.setGradientFill (juce::ColourGradient (down ? juce::Colour (0xffcdc7bd) : juce::Colour (0xffe4e4e1), face.getX(), face.getY(),
                                                 down ? juce::Colour (0xffb9b3a9) : juce::Colour (0xffcfcfcb), face.getX(), face.getBottom(), false));
        g.fillPath (facePath);
        g.setColour (juce::Colours::black.withAlpha (0.16f));
        g.drawHorizontalLine (juce::roundToInt (face.getY()), face.getX() + 1.0f, face.getRight() - 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawHorizontalLine (juce::roundToInt (face.getY() + 1.0f), face.getX() + 1.0f, face.getRight() - 1.0f);

        // the gap to the next key
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.drawVerticalLine (juce::roundToInt (key.getRight()), key.getY(), key.getBottom() - 2.0f);

        if (n % 12 == 0)
            drawText (g, "C" + juce::String (n / 12 - 1), key.withTop (key.getBottom() - front - 16.0f).withHeight (14.0f), 8.5f,
                      juce::Justification::centred, col::tick.withAlpha (0.7f));
    }

    // ── black keys ──
    for (int n = low; n <= high; ++n)
    {
        if (! isBlack (n)) continue;
        const auto r = keyRect (n);
        const bool down = n == held;

        // soft shadow onto the white keys below and to the right
        {
            juce::ColourGradient sh (juce::Colours::black.withAlpha (0.30f), r.getX(), r.getBottom(),
                                     juce::Colours::transparentBlack, r.getX(), r.getBottom() + 18.0f, false);
            g.setGradientFill (sh);
            g.fillRect (r.withY (r.getBottom()).withHeight (18.0f).expanded (2.5f, 0.0f));
            juce::ColourGradient side (juce::Colours::black.withAlpha (0.22f), r.getRight(), r.getCentreY(),
                                       juce::Colours::transparentBlack, r.getRight() + 7.0f, r.getCentreY(), false);
            g.setGradientFill (side);
            g.fillRect (juce::Rectangle<float> (r.getRight(), r.getY(), 7.0f, r.getHeight()));
        }

        const float lift = down ? 1.5f : 0.0f;
        juce::Path body;
        body.addRoundedRectangle (r.getX(), r.getY() - 8.0f, r.getWidth(), r.getHeight() + 8.0f - lift, 3.0f, 3.0f, false, false, true, true);
        g.setGradientFill (juce::ColourGradient (juce::Colour (down ? 0xff2b2c2e : 0xff35363a), r.getX(), r.getY(),
                                                 juce::Colour (0xff08090a), r.getX(), r.getBottom(), false));
        g.fillPath (body);
        {
            juce::Graphics::ScopedSaveState s (g);
            g.reduceClipRegion (body);
            // the moulded sides catch the light on the left, fall away on the right
            juce::ColourGradient across (juce::Colours::white.withAlpha (0.20f), r.getX(), r.getCentreY(),
                                         juce::Colours::black.withAlpha (0.55f), r.getRight(), r.getCentreY(), false);
            across.addColour (0.30, juce::Colours::transparentBlack);
            g.setGradientFill (across);
            g.fillRect (r);
            // narrow specular down the crown
            juce::ColourGradient spec (juce::Colours::white.withAlpha (down ? 0.10f : 0.22f), r.getX(), r.getY(),
                                       juce::Colours::transparentBlack, r.getX(), r.getBottom() - front, false);
            g.setGradientFill (spec);
            g.fillRect (juce::Rectangle<float> (r.getX() + r.getWidth() * 0.22f, r.getY(), r.getWidth() * 0.16f, r.getHeight() - front));
        }
        // lit front face
        const auto face = juce::Rectangle<float> (r.getX(), r.getBottom() - front * 0.8f - lift, r.getWidth(), front * 0.8f);
        juce::Path facePath;
        facePath.addRoundedRectangle (face.getX(), face.getY(), face.getWidth(), face.getHeight(), 3.0f, 3.0f, false, false, true, true);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4b4c50), face.getX(), face.getY(), juce::Colour (0xff141517), face.getX(), face.getBottom(), false));
        g.fillPath (facePath);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawHorizontalLine (juce::roundToInt (face.getY()), face.getX() + 1.0f, face.getRight() - 1.0f);
    }

    // shadow of the panel lip over the back of the keys
    juce::ColourGradient top (juce::Colours::black.withAlpha (0.55f), 0.0f, 0.0f, juce::Colours::transparentBlack, 0.0f, 16.0f, false);
    g.setGradientFill (top);
    g.fillRect (juce::Rectangle<float> (static_cast<float> (getWidth()), 16.0f));
}

int Keyboard::noteAt (juce::Point<float> p) const
{
    for (int n = low; n <= high; ++n)                              // the black keys lie on top
        if (isBlack (n) && keyRect (n).contains (p)) return n;
    for (int n = low; n <= high; ++n)
        if (! isBlack (n) && keyRect (n).contains (p)) return n;
    return -1;
}

void Keyboard::press (int note, float velocity)
{
    if (note == held) return;
    if (held >= 0 && onNoteOff) onNoteOff (held);
    held = note;
    if (held >= 0 && onNoteOn) onNoteOn (held, velocity);
    repaint();
}

void Keyboard::mouseDown (const juce::MouseEvent& e)
{
    const int n = noteAt (e.position);
    if (n < 0) return;
    const auto r = keyRect (n);
    press (n, juce::jlimit (0.25f, 1.0f, 0.35f + 0.65f * (e.position.y - r.getY()) / juce::jmax (1.0f, r.getHeight())));
}

void Keyboard::mouseDrag (const juce::MouseEvent& e)
{
    const int n = noteAt (e.position);
    if (n >= 0 && n != held) press (n, 0.8f);
}

void Keyboard::mouseUp (const juce::MouseEvent&)
{
    press (-1, 0.0f);
}

void ValueBubble::show (juce::Component& target, const juce::String& t)
{
    text = t;
    auto* parent = getParentComponent();
    if (parent == nullptr) return;
    const auto area = parent->getLocalArea (&target, target.getLocalBounds());
    const auto font = uiFont (12.5f, true);
    const int w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, text)) + 18;
    setBounds (juce::Rectangle<int> (w, 20).withCentre ({ area.getCentreX(), area.getY() - 12 }).constrainedWithin (parent->getLocalBounds()));
    setVisible (true);
    toFront (false);
    repaint();
}

void ValueBubble::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    drawLcd (g, b);
    drawLcdText (g, text, b, 12.5f, juce::Justification::centred);
}

} // namespace sgui
