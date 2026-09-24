#pragma once

// The whole front panel on a 1774 × 887 design canvas (build prompt §4–6), scaled by the editor.
//
// Top editor row: the layer chosen with the LAYER buttons / tabs. Two strips below: UPPER over
// LOWER, as on the hardware (DD-14). Knobs and caps follow the original synth's colours: UPPER
// white, LOWER orange, global black (DD-71). Every control is a linked view of one host parameter.

#include "Controls.h"
#include "plugin/PluginProcessor.h"

#include <array>
#include <memory>
#include <vector>

namespace sgui
{

class MainPanel : public juce::Component, private juce::Timer
{
public:
    // The instrument: the panel face inside metal cheeks, a lower shelf carrying the bender,
    // ribbon and nameplate, the rolled front lip, then the keyboard.
    // Two full layer rows in the hardware's section order, after the design canvas
    // (design/gen.mjs): LFO 1 | DDS MOD | OSC | MIXER | VCF | VCA | ENVELOPES | DLY.
    static constexpr int kCheek  = 44;
    static constexpr int kRowY   = 96, kRowH = 396, kBandH = 150;
    static constexpr int kBand1Y = kRowY   + kRowH + 8;      // 500
    static constexpr int kRow2Y  = kBand1Y + kBandH + 16;    // 666
    static constexpr int kBand2Y = kRow2Y  + kRowH + 8;      // 1070
    static constexpr int kFaceW  = 2240;
    static constexpr int kFaceH  = kBand2Y + kBandH + 20;    // 1240
    static constexpr int kShelfY = kFaceH, kShelfH = 96;
    static constexpr int kLipY   = kShelfY + kShelfH, kLipH = 22;
    static constexpr int kKeysY  = kLipY + kLipH, kKeysH = 168;
    static constexpr int kStatusY = kKeysY + kKeysH + 8;    // 1150
    static constexpr int kWidth  = kFaceW + 2 * kCheek;     // 1862
    static constexpr int kHeight = kStatusY + 32;           // 1182

    explicit MainPanel (SuperGeminiProcessor& processor);
    ~MainPanel() override;

    void paint (juce::Graphics& g) override;

    int getEditLayer() const noexcept { return editLayer; }
    void setEditLayer (int layer);

    enum class Popover { None, Matrix, Sequencer, AltWaves, Settings };
    void openPopover (Popover which, int layer);
    void closePopover();
    juce::Component* getOpenPopover() const noexcept { return popover.get(); }

    // Every host parameter some control can reach (both layers, every tab and pop-over).
    juce::StringArray getReachableParameterIds();

    std::function<void (float scale)> onScaleChange;   // Settings → editor
    std::function<float()> getScale;

    // MODULATION section of a strip: which source → destination its AMOUNT knob edits.
    void selectModulation (int layer, int src, int dest) { setModSelection (layer, src, dest); }
    juce::String getModulationParamId (int layer) const;

    // For the UI check tool
    std::vector<ParamControl*> controlsFor (const juce::String& paramId, bool byPrimary = false) const;
    void setShiftLatched (bool on) { shiftLatched = on; timerCallback(); }
    void refresh() { timerCallback(); }
    void setStripTabs (int envTab, int lfoTab);

    // Target of a binding: fixed, the layer edited in the top row, or the layer the DEST /
    // PORTAMENTO selector points at [pp.73–75].
    enum class Target { Global, Upper, Lower, Edit, ModLayer, PortaLayer };

    juce::RangedAudioParameter* resolve (Target t, const juce::String& id) const;
    void bindControl (ParamControl& c, Target t, const juce::String& id, const juce::String& secondaryId = {});
    void unbindControlsIn (juce::Component& container);
    void showValue (ParamControl& c, bool active);
    UiBridge& bridge() { return proc.getUiBridge(); }
    SuperGeminiProcessor& processor() { return proc; }

    template <typename T, typename... Args>
    T& make (juce::Component& parent, juce::Rectangle<int> r, Args&&... args)
    {
        auto c = std::make_unique<T> (std::forward<Args> (args)...);
        auto& ref = *c;
        parent.addAndMakeVisible (ref);
        ref.setBounds (r);
        owned.push_back (std::move (c));
        return ref;
    }

    Knob& knob (juce::Component& parent, int x, int y, Knob::Size s, Target t, const juce::String& id, const juce::String& title,
                juce::String lo = "0", juce::String hi = "10", const juce::String& secondary = {}, const juce::String& sub = {});
    Fader& fader (juce::Component& parent, juce::Rectangle<int> r, Target t, const juce::String& id, const juce::String& title,
                  const juce::String& secondary = {}, const juce::String& sub = {}, bool scale = true);
    SegGroup& seg (juce::Component& parent, juce::Rectangle<int> r, Target t, const juce::String& id, juce::StringArray labels,
                   bool vertical = false, juce::Array<int> values = {});
    Toggle& toggle (juce::Component& parent, juce::Rectangle<int> r, Target t, const juce::String& id, const juce::String& label,
                    Toggle::Style style = Toggle::Style::Lit);
    Dropdown& drop (juce::Component& parent, juce::Rectangle<int> r, Target t, const juce::String& id);
    Section& section (juce::Component& parent, juce::Rectangle<int> r, const juce::String& title, float titleHeight = 15.0f);

private:
    struct Binding { ParamControl* control; Target target; juce::String id, secondaryId; };

    void buildHeader();
    void buildMaster (juce::Rectangle<int> r);
    void buildDdsModulator (juce::Rectangle<int> r, Target t, int layer);
    void buildOscillators (juce::Rectangle<int> r, Target t, int layer);
    void buildMixer (juce::Rectangle<int> r, Target t, int layer);
    void buildVcf (juce::Rectangle<int> r, Target t, int layer);
    void buildVca (juce::Rectangle<int> r, Target t, int layer);
    void buildEnvelopes (juce::Rectangle<int> r, Target t, int layer);
    void buildChorus (juce::Rectangle<int> r, Target t, int layer);
    void buildDelay (juce::Rectangle<int> r, Target t, int layer);
    void buildLayerRow (int layer, int y);      // one full hardware row
    void buildStrip (int layer, int y);         // per-layer voice / mod / arp band

    Tone toneFor (Target t) const;
    void applyTones();
    void rebind (Target t);
    void setEnvTab (Fader* const* faders, int tab);
    Binding* findBinding (const ParamControl* c);
    void timerCallback() override;
    void updateDynamicState();
    void updateLeds (double dt);
    void setModSelection (int layer, int src, int dest);
    void populateModulateMenu (ParamControl& c, juce::PopupMenu& m);
    int layerOf (const ParamControl& c) const;
    float paramValue (const juce::String& id) const;
    int paramIndex (const juce::String& id) const;
    void setParamIndex (const juce::String& id, int index);
    void drawDds1 (juce::Graphics& g, juce::Rectangle<float> r, int layer);
    void drawDds2 (juce::Graphics& g, juce::Rectangle<float> r, int layer);

    void buildKeyboardArea();
    void buildStatusBar();
    void updateModulationRings();
    void drawScopeTrace (juce::Graphics& g, juce::Rectangle<float> r);
    void drawEnvelopeTrace (juce::Graphics& g, juce::Rectangle<float> r, int layer);
    void drawFilterTrace (juce::Graphics& g, juce::Rectangle<float> r, int layer);
    void drawLfoTrace (juce::Graphics& g, juce::Rectangle<float> r, int layer, int which);

    SuperGeminiProcessor& proc;
    juce::AudioProcessorValueTreeState& state;
    juce::Component* face = nullptr;                  // the panel face inside the chassis
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<Binding> bindings;
    std::array<WaveDisplay*, 2> lfoScope {};
    WaveDisplay* scopeDisplay = nullptr;
    juce::Component* statusBar = nullptr;
    std::array<double, 2> lfoScopePhase {};
    std::array<float, 2> layerGlow {};
    float tempoGlow = 0.0f;
    std::array<float, 2> delayGlow {};

    int editLayer = 0;
    int lastModLayer = -1, lastPortaLayer = -1;
    bool shiftLatched = false, shiftActive = false;
    double lastTime = 0.0;

    // header
    MenuBox* patchBox = nullptr;
    ActionButton* layerButtons[2] {};
    ActionButton* learnButton = nullptr;
    TabBar* layerTabs = nullptr;
    ValueBubble* bubble = nullptr;

    // top row pieces that change with state
    LevelMeter* meter = nullptr;
    Led* tempoLed = nullptr;
    Led* portaLed = nullptr;
    SegGroup* octaveSeg = nullptr;
    ActionButton* shiftButton = nullptr;
    unsigned lastLoopCycles[2] {};
    std::array<float, 2> loopFlash {};
    float meterL = 0.0f, meterR = 0.0f;
    double tempoPhase = 0.0, blinkPhase = 0.0;
    std::array<double, 2> delayPhase {};

    // Everything a full layer row owns. Both rows are built from the same code, so every
    // control that used to be a single top-row instance lives here, one per layer.
    struct StripUi
    {
        WaveDisplay* dds1Display = nullptr;
        WaveDisplay* dds2Display = nullptr;
        WaveDisplay* envDisplay = nullptr;
        WaveDisplay* filterDisplay = nullptr;
        Knob* dds2Tune = nullptr;
        SegGroup* dds2ModeSeg = nullptr;
        SegGroup* dds2SubSeg = nullptr;
        SegGroup* mixerSubSeg = nullptr;
        SegGroup* hfRouteSeg = nullptr;
        SegGroup* env1ModeSeg = nullptr;
        SegGroup* env1KeySeg = nullptr;
        Led* xmodLeds[2] {};
        Led* ringLed = nullptr;
        Led* loopLed = nullptr;
        Led* chorusLed = nullptr;
        Led* delayLed = nullptr;
        Section* delaySection = nullptr;
        Section* chorusSection = nullptr;
        TabBar* rowEnvTabs = nullptr;
        Fader* rowEnvFaders[6] {};
        Section* layerSection = nullptr;
        ActionButton* editButton = nullptr;
        MenuBox* patch = nullptr;
        TabBar* envTabs = nullptr;
        Fader* envFaders[6] {};
        TabBar* lfoTabs = nullptr;
        juce::Component* lfoPages[2] {};
        Led* lfoLed = nullptr;
        SegGroup* lfo1ModeSeg = nullptr;
        Knob* lrPhase[2] {};
        MenuBox* modSource = nullptr;
        MenuBox* modDest = nullptr;
        Knob* modAmount = nullptr;
        int modSrc = 1, modDst = 0;
        Section* modSection = nullptr;
        ActionButton* recButton = nullptr;
        Section* arpSection = nullptr;
        Section* voiceSection = nullptr;
        double lfoPhase = 0.0;
    };
    std::array<StripUi, 2> strips;

    std::unique_ptr<juce::Component> popover;
    Popover popoverKind = Popover::None;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainPanel)
};

} // namespace sgui
