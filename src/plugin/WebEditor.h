#pragma once

// The plugin window: the ui/ HTML page rendered by juce::WebBrowserComponent.
//
// Two resource modes, chosen at configure time by the GEMINUS_DEV_UI CMake option:
//   dev      the page is loaded from ui/ on disk, so a CSS edit needs only a reload
//   release  every file under ui/ is embedded and served from memory, so the shipped
//            plugin has no external file dependency and needs no network
//
// The page is authored at its natural size and scaled as a unit, so the layout never
// reflows - it is a panel, not a responsive document.

#include "PluginProcessor.h"

#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_extra/juce_gui_extra.h>

class GeminusWebSession final : public juce::Component,
                               private juce::Timer
{
public:
    // The page reports its own size on load (see pageSize below), so these are only the
    // fallback used before the first report - they can never silently go stale again.
    static constexpr int kPageW = 3400, kPageH = 1330;
    // Desktop mode (ui/gen.mjs DK_LINES): one engine at a time at full control size.
    static constexpr int kDesktopPageW = 2092, kDesktopPageH = 1164;   // the page reports its real size (pageSize) and wins

    explicit GeminusWebSession (SuperGeminiProcessor& p);
    ~GeminusWebSession() override;

    void resized() override;
    void paint (juce::Graphics& g) override;

    juce::Point<int> savedSize;
    void fitAfterAttach() { fitPending = true; }

    void reloadUi();                     // dev mode: pick up edits to ui/ without rebuilding
    void adoptPageSize (int w, int h);   // called when the page reports its natural size

private:
    friend struct GeminusLifecycleCheck;
    friend class GeminusWebEditor;
    void trace (const char* event) const;
    // Serves one file out of the embedded ui/ bundle, by the path the page asks for.
    static std::optional<juce::WebBrowserComponent::Resource> provide (const juce::String& url);
    static juce::String mimeFor (const juce::String& path);

    juce::WebBrowserComponent::Options makeOptions();
    void applyZoom();
    void setDesktopLayout (bool shouldUseDesktopLayout);
    void returnFocusToHost();

    // Patch files (.gpatch = the whole plugin state). Default folder under Documents.
    static juce::File defaultPatchFolder();
    juce::var listPatches (const juce::File& folder);
    bool loadPatchFile (const juce::File& f);
    static juce::String readPatchType (const juce::File& f);
    juce::StringArray favourites() const;
    void addPatches (juce::Array<juce::var>& out, const juce::File& folder, const juce::String& bank);
    juce::File savePatchFile (const juce::File& folder, juce::String name, const juce::String& bank, const juce::String& type);
    void setPatchName (const juce::String& name);
    juce::String patchName() const;

    // Display-only state, pushed at 30 Hz as ONE batched event rather than one per value:
    // output meter, voice counts, envelope levels, tempo, sequencer position and the working
    // sequences when they change.
    void timerCallback() override;
    juce::var sequenceToVar (const sg::Sequence& s);
    uint32_t sentSeqVersion = ~0u;
    juce::String sentPatchName;
    int sentIrVersion = -1;
    int sentFxData = -1;
    int sentCustomVersion[2] { -1, -1 };

    // Relays are built from the processor's own parameter list, so the page and the C++
    // never drift apart: a control in ui/ binds by id, and unused relays cost nothing.
    void buildRelays();
    std::vector<std::unique_ptr<juce::WebSliderRelay>>   sliderRelays;
    std::vector<std::unique_ptr<juce::WebToggleButtonRelay>> toggleRelays;
    std::vector<std::unique_ptr<juce::WebComboBoxRelay>> comboRelays;
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>>   sliderAttach;
    std::vector<std::unique_ptr<juce::WebToggleButtonParameterAttachment>> toggleAttach;
    std::vector<std::unique_ptr<juce::WebComboBoxParameterAttachment>> comboAttach;

    // A navigation is only needed on first creation or explicit development reload.
    class Page final : public juce::WebBrowserComponent
    {
    public:
        using juce::WebBrowserComponent::WebBrowserComponent;
        std::function<void()> onLoaded;
        void pageFinishedLoading (const juce::String&) override { if (onLoaded) onLoaded(); }
    };

    SuperGeminiProcessor& proc;
    std::unique_ptr<Page> web;
    std::unique_ptr<juce::PropertiesFile> globalSettings;
    std::unique_ptr<juce::FileChooser> patchChooser;
    juce::MemoryBlock abStore;           // A/B compare: the other state
    bool abIsB = false;
    int pageW = kPageW, pageH = kPageH;   // live page size, reported by the page itself
    bool desktopLayout = false;
    bool pageLoaded = false;
    bool fitPending = true;
    std::function<void()> onLayoutChanged;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeminusWebSession)
};

class GeminusWebEditor final : public juce::AudioProcessorEditor, private juce::Timer,
                               private juce::AsyncUpdater
{
public:
    explicit GeminusWebEditor (SuperGeminiProcessor&);
    ~GeminusWebEditor() override;
    void resized() override;
    void paint (juce::Graphics&) override;
private:
    void applyDesktopLayout();
    void timerCallback() override;
    void handleAsyncUpdate() override;
    GeminusWebSession& session;
    bool attached = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeminusWebEditor)
};
