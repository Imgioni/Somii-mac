#include "Popovers.h"

#include "Panel.h"
#include "UiHelpers.h"

#include "core/WaveTable.h"

namespace sgui
{

namespace
{
using R = juce::Rectangle<int>;
using Target = MainPanel::Target;

Target layerTarget (int layer) { return layer == 0 ? Target::Upper : Target::Lower; }

int indexOf (juce::RangedAudioParameter* p)
{
    return p != nullptr ? juce::roundToInt (p->convertFrom0to1 (p->getValue())) : 0;
}

void setIndex (juce::RangedAudioParameter* p, int index)
{
    if (p == nullptr) return;
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (index)));
    p->endChangeGesture();
}

// A text cell that can be clicked.
class TextCell : public juce::Component
{
public:
    TextCell (juce::String t, float h, juce::Justification j) : text (std::move (t)), height (h), just (j) {}
    juce::String text;
    float height;
    juce::Justification just;
    std::function<void()> onClick;
    void paint (juce::Graphics& g) override
    {
        drawText (g, text, getLocalBounds().toFloat().reduced (3.0f, 0.0f), height, just,
                  isMouseOver() && onClick ? col::accent.darker (0.2f) : col::text);
    }
    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
};

// One matrix amount, −100 … +100 % [p.85]: drag vertically, double-click to clear.
class MatrixCell : public ParamControl
{
public:
    MatrixCell (MainPanel& p, int l, int s, int d) : panel (p), layer (l), src (s), dest (d) {}

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat().reduced (1.0f, 0.5f);
        if (param == nullptr)
        {
            g.setColour (col::section.darker (0.1f));
            g.fillRect (b);
            g.setColour (col::border);
            for (float x = b.getX() - b.getHeight(); x < b.getRight(); x += 5.0f)
                g.drawLine (x, b.getBottom(), x + b.getHeight(), b.getY(), 0.6f);
            return;
        }
        // recessed well
        g.setColour (isMouseOver() ? col::section.brighter (0.06f) : col::section.darker (0.035f));
        g.fillRect (b);
        g.setColour (juce::Colours::black.withAlpha (0.08f));
        g.drawHorizontalLine (juce::roundToInt (b.getY()), b.getX(), b.getRight());
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawHorizontalLine (juce::roundToInt (b.getBottom() - 1.0f), b.getX(), b.getRight());
        const float v = norm * 2.0f - 1.0f;
        const float cx = b.getCentreX();
        if (std::abs (v) > 1.0e-4f)
        {
            const float w = b.getWidth() * 0.5f * std::abs (v);
            const auto bar = v > 0.0f ? juce::Rectangle<float> (cx, b.getY() + 2.0f, w, b.getHeight() - 4.0f)
                                      : juce::Rectangle<float> (cx - w, b.getY() + 2.0f, w, b.getHeight() - 4.0f);
            g.setGradientFill (juce::ColourGradient (col::accent.brighter (0.2f), bar.getX(), bar.getY(), col::accent.darker (0.15f), bar.getX(), bar.getBottom(), false));
            g.fillRoundedRectangle (bar, 1.5f);
            drawText (g, (v > 0.0f ? "+" : "") + juce::String (juce::roundToInt (v * 100.0f)), b, 9.0f, juce::Justification::centred, col::text);
        }
        g.setColour (col::border.withAlpha (0.8f));
        g.drawVerticalLine (juce::roundToInt (cx), b.getY() + 1.0f, b.getBottom() - 1.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (showContextMenu (e) || param == nullptr) return;
        panel.selectModulation (layer, src, dest);
        start = norm;
        beginGesture();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! gestureActive) return;
        const float scale = e.mods.isCtrlDown() ? 800.0f : 160.0f;
        setNormInGesture (start + static_cast<float> (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY()) / scale);
    }
    void mouseUp (const juce::MouseEvent&) override { endGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { resetToDefault(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    MainPanel& panel;
    int layer, src, dest;
    float start = 0.5f;
};

// One sequencer step [pp.96–97].
class StepCell : public juce::Component
{
public:
    StepCell (SequencerPopover& o, int i) : owner (o), index (i) {}

    juce::Rectangle<float> flagRect (int f) const
    {
        const auto b = getLocalBounds().toFloat().reduced (3.0f);
        const float w = (b.getWidth() - 4.0f) / 3.0f;
        return { b.getX() + static_cast<float> (f) * (w + 2.0f), b.getBottom() - 14.0f, w, 14.0f };
    }

    void paint (juce::Graphics& g) override
    {
        const auto& seq = owner.sequence();
        const auto& st = seq.steps[static_cast<size_t> (index)];
        const bool inLength = index < seq.length;
        const auto b = getLocalBounds().toFloat().reduced (1.0f);

        g.setColour (inLength ? col::section.brighter (0.05f) : col::section.darker (0.07f));
        g.fillRoundedRectangle (b, 3.0f);
        if (owner.playStep() == index)
        {
            g.setColour (col::accent);
            g.fillRoundedRectangle (b.withHeight (5.0f), 2.0f);
        }
        g.setColour (owner.selected == index ? col::accent : col::border);
        g.drawRoundedRectangle (b.reduced (0.5f), 3.0f, owner.selected == index ? 2.0f : 1.0f);
        if (owner.isRecording() && owner.recStep() == index)
        {
            g.setColour (juce::Colour (0xffff2a1a).withAlpha (0.8f));
            g.drawRoundedRectangle (b.reduced (2.5f), 3.0f, 1.5f);
        }

        drawText (g, juce::String (index + 1), b.reduced (4.0f, 6.0f).removeFromTop (12.0f), 10.0f,
                  juce::Justification::centredLeft, inLength ? col::text : col::tick);
        juce::StringArray lines;
        if (st.rest) lines.add ("REST");
        else if (st.count == 0) lines.add ("-");
        else
        {
            juce::String line;
            for (int n = 0; n < st.count; ++n)
            {
                line << noteName (st.notes[static_cast<size_t> (n)]) << " ";
                if (n % 2 == 1) { lines.add (line.trim()); line.clear(); }
            }
            if (line.isNotEmpty()) lines.add (line.trim());
        }
        if (st.hasBend) lines.add ("BEND " + juce::String (juce::roundToInt (st.bend * 100.0f)));
        auto area = b.reduced (4.0f, 0.0f).withTrimmedTop (20.0f).withTrimmedBottom (18.0f);
        for (int i = 0; i < juce::jmin (4, lines.size()); ++i)
            drawText (g, lines[i], area.removeFromTop (12.0f), 10.0f, juce::Justification::centredLeft,
                      inLength ? col::text : col::tick, i == 0);

        const char* names[] { "SLD", "ACC", "RST" };
        const bool lit[] { st.tie, st.accent, st.rest };
        for (int f = 0; f < 3; ++f)
        {
            const auto r = flagRect (f);
            drawButtonFace (g, r, lit[f], true, false);
            drawText (g, names[f], r, 8.5f, juce::Justification::centred, lit[f] ? juce::Colours::white : col::text);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        owner.selected = index;
        if (e.mods.isPopupMenu())
        {
            juce::PopupMenu m;
            m.addSectionHeader ("STEP " + juce::String (index + 1));
            m.addItem ("Sequence ends here (LENGTH " + juce::String (index + 1) + ")", [this] { owner.setLength (index + 1); });
            m.addItem ("Clear this step", [this] { owner.editStep (index, [] (sg::SeqStep& s) { s = sg::SeqStep(); }); });
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
        }
        else
        {
            for (int f = 0; f < 3; ++f)
                if (flagRect (f).contains (e.position))
                    owner.editStep (index, [f] (sg::SeqStep& s)
                    {
                        if (f == 0) s.tie = ! s.tie;           // SLIDE track
                        else if (f == 1) s.accent = ! s.accent; // ACCENT track
                        else s.rest = ! s.rest;                 // REST track
                    });
        }
        if (auto* p = getParentComponent()) p->repaint();
    }

private:
    SequencerPopover& owner;
    int index;
};

// One alternative waveform slot [pp.33–34].
class WaveTile : public juce::Component
{
public:
    WaveTile (MainPanel& p, int l, int s) : panel (p), layer (l), slot (s) {}

    void paint (juce::Graphics& g) override
    {
        const int a = indexOf (panel.resolve (layerTarget (layer), "dds1.altA"));
        const int b = indexOf (panel.resolve (layerTarget (layer), "dds1.altB"));
        auto area = getLocalBounds().toFloat().reduced (2.0f);
        const bool lit = a == slot || b == slot;
        g.setColour (isMouseOver() ? col::section.brighter (0.08f) : col::section.brighter (0.03f));
        g.fillRoundedRectangle (area, 3.0f);
        g.setColour (lit ? col::accent : col::border);
        g.drawRoundedRectangle (area.reduced (0.5f), 3.0f, lit ? 2.0f : 1.0f);

        auto screen = area.reduced (5.0f).withTrimmedBottom (16.0f);
        g.setColour (col::screen);
        g.fillRoundedRectangle (screen, 2.0f);
        const auto table = sg::AltWaveBank::factory().get (slot);
        const auto& cycle = table->getCycle();
        juce::Path path;
        const auto r = screen.reduced (4.0f, 6.0f);
        for (int i = 0; i <= 96; ++i)
        {
            const float t = static_cast<float> (i) / 96.0f;
            const float v = cycle[static_cast<size_t> (juce::jmin (sg::WaveTable::kSize - 1, static_cast<int> (t * sg::WaveTable::kSize)))];
            const float y = r.getCentreY() - v * r.getHeight() * 0.5f;
            if (i == 0) path.startNewSubPath (r.getX(), y); else path.lineTo (r.getX() + t * r.getWidth(), y);
        }
        g.setColour (col::accent);
        g.strokePath (path, juce::PathStrokeType (1.3f));

        drawText (g, juce::String (slot + 1) + " " + juce::String (table->getName()), area.removeFromBottom (18.0f).reduced (4.0f, 0.0f),
                  9.0f, juce::Justification::centredLeft, col::text);
        auto badges = screen.reduced (3.0f).removeFromTop (12.0f);
        if (a == slot) { drawButtonFace (g, badges.removeFromLeft (14.0f), true, true, false); drawText (g, "A", badges.withX (screen.getX() + 3.0f).withWidth (14.0f), 9.0f, juce::Justification::centred, juce::Colours::white); badges.removeFromLeft (3.0f); }
        if (b == slot) { const auto bb = badges.removeFromLeft (14.0f); drawButtonFace (g, bb, true, true, false); drawText (g, "B", bb, 9.0f, juce::Justification::centred, juce::Colours::white); }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        setIndex (panel.resolve (layerTarget (layer), e.mods.isPopupMenu() ? "dds1.altB" : "dds1.altA"), slot);
        if (auto* p = getParentComponent()) p->repaint();
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    MainPanel& panel;
    int layer, slot;
};
} // namespace

// ── PopoverBase ─────────────────────────────────────────────────────────────────────────────

PopoverBase::PopoverBase (MainPanel& p, juce::String t, int w, int h) : panel (p), title (std::move (t)), boxW (w), boxH (h)
{
    addAndMakeVisible (body);
    closeButton = &panel.make<ActionButton> (*this, R(), "CLOSE", [this] { requestClose(); });
    closeButton->dark = true;
    resized();
}

Section& PopoverBase::canvas()
{
    if (canvasSection == nullptr)
    {
        canvasSection = &panel.make<Section> (body, R { 0, 0, boxW - 24, boxH - 50 }, juce::String(), 10.0f);
        canvasSection->frame = false;
    }
    return *canvasSection;
}

void PopoverBase::resized()
{
    const auto area = getLocalBounds().isEmpty() ? R { 0, 0, MainPanel::kWidth, MainPanel::kHeight } : getLocalBounds();
    box = R { boxW, boxH }.withCentre (area.getCentre());
    body.setBounds (box.reduced (12).withTrimmedTop (26));
    closeButton->setBounds (box.getRight() - 72, box.getY() + 8, 60, 20);
}

void PopoverBase::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.38f));
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (box.toFloat().translated (0.0f, 4.0f).expanded (2.0f), 6.0f);
    drawSectionFrame (g, box.toFloat());
    drawText (g, title, box.toFloat().reduced (14.0f, 8.0f).removeFromTop (22.0f), 15.0f, juce::Justification::centredLeft);
    g.setColour (col::accent);
    g.fillRect (juce::Rectangle<float> (static_cast<float> (box.getX()) + 14.0f, static_cast<float> (box.getY()) + 32.0f, 36.0f, 2.0f));
}

void PopoverBase::mouseDown (const juce::MouseEvent& e)
{
    if (! box.contains (e.getPosition())) requestClose();
}

void PopoverBase::requestClose()
{
    juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<MainPanel> (&panel)]
    {
        if (sp != nullptr) sp->closePopover();
    });
}

// ── MatrixPopover ───────────────────────────────────────────────────────────────────────────

MatrixPopover::MatrixPopover (MainPanel& p, int l)
    : PopoverBase (p, "MODULATION MATRIX", 840, 650), layer (l)
{
    title = u8 ("MODULATION MATRIX  \xc2\xb7  ") + layerName (l) + " LAYER";
    auto& c = canvas();
    constexpr int nameW = 170, cellW = 76, cellH = 15;

    for (int s = 0; s < sg::kMatrixSources; ++s)
    {
        auto& hdr = panel.make<ActionButton> (c, R { nameW + s * cellW + 1, 0, cellW - 2, 22 },
                                              juce::String (s + 1) + "  " + sg::kMatrixSourceNames[static_cast<size_t> (s)], [this, s]
        {
            juce::PopupMenu m;
            m.addItem ("Clear all mappings from " + juce::String (sg::kMatrixSourceNames[static_cast<size_t> (s)]), [this, s] { clearWhere (s, -1); });   // p.89
            m.showMenuAsync ({});
        });
        hdr.setTooltip ("Source " + juce::String (s + 1) + " [p.84]; click to clear its mappings");
    }

    int y = 28;
    for (int d = 0; d < sg::kMatrixDests; ++d)
    {
        if (d == 0 || d == sg::kFixedDests)
        {
            c.addLegend ({ 0.0f, static_cast<float> (y), static_cast<float> (nameW + 8 * cellW), 14.0f },
                         d == 0 ? "MATRIX DESTINATIONS A-H  [p.84]" : "DIRECT PARAMETER MAPPINGS 1-24  [p.88]", 9.5f,
                         juce::Justification::centredLeft);
            y += d == 0 ? 15 : 17;
        }
        const juce::String prefix = d < sg::kFixedDests ? juce::String::charToString (static_cast<juce::juce_wchar> ('A' + d))
                                                        : juce::String (d - sg::kFixedDests + 1);
        auto& name = panel.make<TextCell> (c, R { 0, y, nameW - 4, cellH }, prefix + "  " + sg::kMatrixDestNames[static_cast<size_t> (d)],
                                           9.5f, juce::Justification::centredLeft);
        name.onClick = [this, d]
        {
            juce::PopupMenu m;
            m.addItem ("Clear all mappings to " + juce::String (sg::kMatrixDestNames[static_cast<size_t> (d)]), [this, d] { clearWhere (-1, d); });   // p.89
            m.showMenuAsync ({});
        };
        for (int s = 0; s < sg::kMatrixSources; ++s)
        {
            auto& cell = panel.make<MatrixCell> (c, R { nameW + s * cellW, y, cellW, cellH }, panel, layer, s, d);
            if (sg::matrixExcluded (s, d))
                cell.setTooltip ("Already wired to its own control, not duplicated [p.87, DD-12]");
            else
                panel.bindControl (cell, Target::Global, matrixId (layer, s, d));
        }
        y += cellH;
    }
    panel.make<ActionButton> (c, R { 0, y + 10, 120, 22 }, "CLEAR ALL", [this] { clearWhere (-1, -1); }).dark = true;
    c.addLegend ({ 130.0f, static_cast<float> (y + 10), 560.0f, 22.0f },
                 u8 ("DRAG A CELL UP / DOWN FOR -100 ... +100 %  \xc2\xb7  DOUBLE-CLICK CLEARS  \xc2\xb7  CTRL = FINE"), 9.0f, juce::Justification::centredLeft);
    startTimerHz (10);
}

void MatrixPopover::clearWhere (int src, int dest)
{
    for (int s = 0; s < sg::kMatrixSources; ++s)
        for (int d = 0; d < sg::kMatrixDests; ++d)
        {
            if ((src >= 0 && s != src) || (dest >= 0 && d != dest) || sg::matrixExcluded (s, d)) continue;
            if (auto* p = panel.resolve (Target::Global, matrixId (layer, s, d)))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->getDefaultValue());
                p->endChangeGesture();
            }
        }
}

// ── SequencerPopover ────────────────────────────────────────────────────────────────────────

SequencerPopover::SequencerPopover (MainPanel& p, int l)
    : PopoverBase (p, "SEQUENCER", 1200, 556), layer (l)
{
    title = u8 ("SEQUENCER  \xc2\xb7  ") + layerName (l) + " LAYER";
    seenVersion = panel.bridge().readSequence (layer, seq);
    auto& c = canvas();
    const Target T = layerTarget (layer);

    // memories [p.98]
    c.addLegend ({ 0.0f, 0.0f, 56.0f, 22.0f }, "MEMORY", 9.5f, juce::Justification::centredLeft);
    auto& slot = panel.drop (c, { 56, 1, 74, 20 }, T, "seq.slot");
    slot.itemName = [] (int i) { return "SEQ " + juce::String (i + 1); };
    panel.make<ActionButton> (c, R { 136, 1, 56, 20 }, "LOAD", [this] { send (UiCommand::Type::SeqLoad, memorySlot()); })
        .setTooltip ("Copy the selected memory into the working sequence");
    panel.make<ActionButton> (c, R { 196, 1, 56, 20 }, "STORE", [this] { send (UiCommand::Type::SeqStore, memorySlot()); })
        .setTooltip ("Copy the working sequence into the selected memory");
    panel.make<ActionButton> (c, R { 256, 1, 56, 20 }, "CLEAR", [this] { send (UiCommand::Type::SeqClear); })
        .setTooltip ("Clear the working sequence (SEQ REC + MOD ASSIGN on the hardware)");

    // LENGTH track [p.97]
    c.addLegend ({ 336.0f, 0.0f, 50.0f, 22.0f }, "LENGTH", 9.5f, juce::Justification::centredLeft);
    panel.make<ActionButton> (c, R { 388, 1, 24, 20 }, "-", [this] { setLength (seq.length - 1); });
    panel.make<ActionButton> (c, R { 456, 1, 24, 20 }, "+", [this] { setLength (seq.length + 1); });

    // SEQ REC [p.96]
    auto& rec = panel.make<ActionButton> (c, R { 504, 1, 110, 20 }, "REC FROM STEP", [this]
    {
        const bool on = ! isRecording();
        send (UiCommand::Type::SeqRecord, on ? 1 : 0, on ? selected : -1);
    });
    rec.isLit = [this] { return isRecording() && blink < 0.6; };
    rec.setTooltip ("Play notes or chords; each is stored when all keys are released, then the next step is armed [p.96]");

    c.extraPaint = [this] (juce::Graphics& g)
    {
        drawText (g, juce::String (seq.length), { 412.0f, 0.0f, 44.0f, 22.0f }, 12.0f, juce::Justification::centred);
        juce::String info = isRecording() ? "RECORDING STEP " + juce::String (recStep() + 1)
                                          : (playStep() >= 0 ? "PLAYING STEP " + juce::String (playStep() + 1) : "STOPPED");
        info << u8 ("   \xc2\xb7   PLAYS WITH ARP/SEQ ON AND MODE = SEQ; THE KEY YOU HOLD TRANSPOSES FROM C4 [p.97]");
        drawText (g, info, { 630.0f, 0.0f, 540.0f, 22.0f }, 9.5f, juce::Justification::centredLeft, col::tick);

        const auto& st = seq.steps[static_cast<size_t> (selected)];
        juce::String notes;
        for (int n = 0; n < st.count; ++n) notes << noteName (st.notes[static_cast<size_t> (n)]) << "  ";
        drawText (g, "STEP " + juce::String (selected + 1), { 0.0f, 438.0f, 70.0f, 22.0f }, 12.0f, juce::Justification::centredLeft);
        drawText (g, notes.isEmpty() ? juce::String ("NO NOTES") : notes, { 72.0f, 438.0f, 260.0f, 22.0f }, 11.0f,
                  juce::Justification::centredLeft, col::text, false);
        drawText (g, juce::String (juce::roundToInt (st.velocity * 100.0f)) + " %", { 742.0f, 438.0f, 44.0f, 22.0f }, 11.0f, juce::Justification::centred);
    };

    // the 64 steps: 4 rows of 16 [p.95]
    constexpr int cellW = 73, cellH = 96;
    for (int i = 0; i < 64; ++i)
        panel.make<StepCell> (c, R { (i % 16) * cellW, 30 + (i / 16) * (cellH + 4), cellW - 2, cellH }, *this, i);

    // selected-step editing
    auto& add = panel.make<ActionButton> (c, R { 340, 439, 90, 20 }, "ADD NOTE", nullptr);
    add.action = [this, &add]
    {
        juce::PopupMenu m;
        for (int octave = 1; octave <= 7; ++octave)
        {
            juce::PopupMenu sub;
            for (int n = 0; n < 12; ++n)
            {
                const int note = (octave + 1) * 12 + n;
                sub.addItem (noteName (note), [this, note]
                {
                    editStep (selected, [note] (sg::SeqStep& s)
                    {
                        if (s.count >= s.notes.size()) return;
                        s.notes[s.count++] = static_cast<int8_t> (note);
                        std::sort (s.notes.begin(), s.notes.begin() + s.count);
                        s.rest = false;
                    });
                });
            }
            m.addSubMenu ("Octave " + juce::String (octave), sub);
        }
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&add));
    };
    panel.make<ActionButton> (c, R { 434, 439, 100, 20 }, "REMOVE NOTE", [this]
    {
        editStep (selected, [] (sg::SeqStep& s) { if (s.count > 0) --s.count; });
    });
    panel.make<ActionButton> (c, R { 538, 439, 90, 20 }, "CLEAR STEP", [this]
    {
        editStep (selected, [] (sg::SeqStep& s) { s = sg::SeqStep(); });
    });
    c.addLegend ({ 640.0f, 438.0f, 70.0f, 22.0f }, "VELOCITY", 9.5f, juce::Justification::centredRight);
    panel.make<ActionButton> (c, R { 716, 439, 24, 20 }, "-", [this]
    {
        editStep (selected, [] (sg::SeqStep& s) { s.velocity = juce::jlimit (0.05f, 1.0f, s.velocity - 0.05f); });
    });
    panel.make<ActionButton> (c, R { 788, 439, 24, 20 }, "+", [this]
    {
        editStep (selected, [] (sg::SeqStep& s) { s.velocity = juce::jlimit (0.05f, 1.0f, s.velocity + 0.05f); });
    });
    c.addLegend ({ 0.0f, 470.0f, 1170.0f, 14.0f },
                 u8 ("CLICK A STEP TO SELECT  \xc2\xb7  SLD = SLIDE (TIE TO THE NEXT STEP)  \xc2\xb7  ACC = ACCENT  \xc2\xb7  RST = REST  \xc2\xb7  RIGHT-CLICK: SET LENGTH / CLEAR"),
                 9.0f, juce::Justification::centredLeft);

    startTimerHz (15);
}

int SequencerPopover::memorySlot() const
{
    return indexOf (panel.resolve (layerTarget (layer), "seq.slot"));
}

int SequencerPopover::playStep() const
{
    const auto& f = panel.bridge().layer[static_cast<size_t> (layer)];
    if (! f.running.load()) return -1;
    const int len = juce::jlimit (1, 64, seq.length);
    return ((f.step.load() - 1) % len + len) % len;
}

int SequencerPopover::recStep() const  { return panel.bridge().layer[static_cast<size_t> (layer)].recStep.load(); }
bool SequencerPopover::isRecording() const { return panel.bridge().layer[static_cast<size_t> (layer)].recording.load(); }

void SequencerPopover::send (UiCommand::Type type, int a, int b)
{
    UiCommand c;
    c.type = type;
    c.layer = layer;
    c.a = a;
    c.b = b;
    panel.bridge().push (c);
}

void SequencerPopover::editStep (int index, const std::function<void (sg::SeqStep&)>& change)
{
    if (index < 0 || index >= 64) return;
    auto& st = seq.steps[static_cast<size_t> (index)];
    change (st);                       // shown at once; the audio thread's copy follows
    UiCommand c;
    c.type = UiCommand::Type::SeqSetStep;
    c.layer = layer;
    c.a = index;
    c.step = st;
    panel.bridge().push (c);
    body.repaint();
}

void SequencerPopover::setLength (int length)
{
    seq.length = juce::jlimit (1, 64, length);
    send (UiCommand::Type::SeqSetLength, seq.length);
    body.repaint();
}

void SequencerPopover::timerCallback()
{
    blink = std::fmod (blink + 1.0 / 15.0 * 2.0, 1.0);
    if (panel.bridge().getSequenceVersion() != seenVersion)
        seenVersion = panel.bridge().readSequence (layer, seq);
    body.repaint();
}

// ── AltWavePopover ──────────────────────────────────────────────────────────────────────────

AltWavePopover::AltWavePopover (MainPanel& p, int l) : PopoverBase (p, "ALTERNATIVE WAVEFORMS", 900, 500), layer (l)
{
    title = u8 ("ALTERNATIVE WAVEFORMS  \xc2\xb7  ") + layerName (l) + " LAYER";
    auto& c = canvas();
    constexpr int tileW = 109, tileH = 86;
    for (int group = 0; group < 2; ++group)
    {
        const int y0 = group * (2 * tileH + 30);
        c.addLegend ({ 0.0f, static_cast<float> (y0), 600.0f, 16.0f },
                     u8 (group == 0 ? "GROUP 1  \xc2\xb7  WAVEFORMS 1-16" : "GROUP 2  \xc2\xb7  ALT WAVEFORMS 17-32"), 10.0f,
                     juce::Justification::centredLeft);
        for (int i = 0; i < 16; ++i)
        {
            const int slot = group * 16 + i;
            panel.make<WaveTile> (c, R { (i % 8) * tileW, y0 + 18 + (i / 8) * tileH, tileW - 3, tileH - 3 }, panel, layer, slot)
                .setHelpText ("ALT " + juce::String (slot + 1));
        }
    }
    const int yb = 2 * (2 * tileH + 30) + 2;
    c.addLegend ({ 0.0f, static_cast<float> (yb), 560.0f, 20.0f },
                 u8 ("LEFT-CLICK = ALT A (HEARD AT THE ALT POSITION)  \xc2\xb7  RIGHT-CLICK = ALT B (REACHED WITH PWM/WAVE) [pp.33-34]"), 9.0f,
                 juce::Justification::centredLeft);
    panel.make<ActionButton> (c, R { 700, yb, 170, 20 }, "SET DDS 1 WAVEFORM TO ALT", [this]
    {
        setIndex (panel.resolve (layerTarget (layer), "dds1.wave"), 5);
    }).dark = true;
    startTimerHz (8);
}

// ── SettingsPopover ─────────────────────────────────────────────────────────────────────────

SettingsPopover::SettingsPopover (MainPanel& p) : PopoverBase (p, "SETTINGS", 560, 370)
{
    auto& c = canvas();
    c.addLegend ({ 0.0f, 0.0f, 300.0f, 16.0f }, "MIDI  [pp.99-101]", 11.0f, juce::Justification::centredLeft);
    c.addLegend ({ 0.0f, 26.0f, 140.0f, 20.0f }, "BASE CHANNEL", 9.5f, juce::Justification::centredLeft);
    auto& ch = panel.drop (c, { 150, 26, 180, 20 }, Target::Global, "global.midiChannel");
    ch.itemName = [] (int i) { return "CH " + juce::String (i + 1) + (i < 15 ? "  (LOWER ON " + juce::String (i + 2) + ")" : juce::String()); };
    panel.toggle (c, { 0, 56, 250, 22 }, Target::Global, "global.ccRx", "RECEIVE MIDI CC", Toggle::Style::LedButton);
    panel.toggle (c, { 0, 84, 330, 22 }, Target::Global, "global.clockRx", "FOLLOW HOST TEMPO AND TRANSPORT (EXT CLK)", Toggle::Style::LedButton);

    c.addLegend ({ 0.0f, 128.0f, 300.0f, 16.0f }, "DISPLAY", 11.0f, juce::Justification::centredLeft);
    const float scales[] { 0.6f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };
    for (int i = 0; i < 6; ++i)
    {
        const float s = scales[i];
        auto& b = panel.make<ActionButton> (c, R { i * 64, 152, 60, 22 }, juce::String (juce::roundToInt (s * 100.0f)) + " %", [this, s]
        {
            if (panel.onScaleChange) panel.onScaleChange (s);
        });
        b.isLit = [this, s] { return panel.getScale && std::abs (panel.getScale() - s) < 0.01f; };
    }
    c.addLegend ({ 0.0f, 178.0f, 500.0f, 14.0f }, "OR DRAG THE WINDOW CORNER (60 - 200 %)", 8.5f, juce::Justification::centredLeft);

    c.addLegend ({ 0.0f, 212.0f, 300.0f, 16.0f }, "SYSTEM", 11.0f, juce::Justification::centredLeft);
    panel.make<ActionButton> (c, R { 0, 236, 150, 24 }, "GLOBAL RESET", [this]
    {
        // Global settings back to their defaults; stored sounds are not touched [p.101].
        for (auto* id : { "global.midiChannel", "global.ccRx", "global.clockRx", "global.transpose", "global.fineTune" })
            if (auto* prm = panel.resolve (Target::Global, id))
            {
                prm->beginChangeGesture();
                prm->setValueNotifyingHost (prm->getDefaultValue());
                prm->endChangeGesture();
            }
    }).dark = true;
    c.addLegend ({ 160.0f, 236.0f, 360.0f, 24.0f }, "MIDI CHANNEL, CC RECEIVE, CLOCK, TRANSPOSE AND FINE TUNE", 8.5f, juce::Justification::centredLeft);
    c.addLegend ({ 0.0f, 290.0f, 520.0f, 14.0f }, u8 ("20 VOICES  \xc2\xb7  2X OVERSAMPLING  \xc2\xb7  OUTPUTS: MAIN, UPPER, LOWER"), 9.0f,
                 juce::Justification::centredLeft);
}

} // namespace sgui
