#pragma once

// Pop-over pages: the full modulation matrix, the sequencer step editor, the alternative
// waveform browser and the settings page. Each dims the panel; click outside or × to close.

#include "Controls.h"
#include "core/ArpSeq.h"
#include "plugin/UiBridge.h"

namespace sgui
{

class MainPanel;

class PopoverBase : public juce::Component
{
public:
    PopoverBase (MainPanel& panel, juce::String title, int width, int height);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void resized() override;

    // Borderless legend canvas covering the body; controls are added to it.
    Section& canvas();

protected:
    void requestClose();
    MainPanel& panel;
    juce::Component body;     // controls live here, in body coordinates
    juce::String title;
    int boxW, boxH;
    juce::Rectangle<int> box;
    ActionButton* closeButton = nullptr;
    Section* canvasSection = nullptr;
};

// 8 sources × 32 destinations of one layer [pp.84–89]; drag a cell for its amount.
class MatrixPopover : public PopoverBase, private juce::Timer
{
public:
    MatrixPopover (MainPanel& panel, int layer);

private:
    void timerCallback() override { body.repaint(); }
    void clearWhere (int src, int dest);   // −1 = any
    int layer;
};

// Step editor [pp.95–98]: 64 steps with notes, SLIDE, ACCENT, REST; LENGTH; SEQ REC; the 16
// memories (LOAD / STORE) and CLEAR.
class SequencerPopover : public PopoverBase, private juce::Timer
{
public:
    SequencerPopover (MainPanel& panel, int layer);

    const sg::Sequence& sequence() const noexcept { return seq; }
    int selected = 0;
    int playStep() const;
    int recStep() const;
    bool isRecording() const;
    void editStep (int index, const std::function<void (sg::SeqStep&)>& change);
    void setLength (int length);

private:
    void timerCallback() override;
    void send (UiCommand::Type type, int a = 0, int b = 0);
    int memorySlot() const;
    int layer;
    sg::Sequence seq;
    uint32_t seenVersion = ~0u;
    double blink = 0.0;
};

// The 32 alternative waveforms in two groups of 16 [pp.33–34, 102]: left-click = ALT A,
// right-click = ALT B.
class AltWavePopover : public PopoverBase, private juce::Timer
{
public:
    AltWavePopover (MainPanel& panel, int layer);

private:
    void timerCallback() override { body.repaint(); }
    int layer;
};

// Settings (hamburger menu): global MIDI settings [pp.99–101], display scale, global reset.
class SettingsPopover : public PopoverBase
{
public:
    explicit SettingsPopover (MainPanel& panel);
};

} // namespace sgui
