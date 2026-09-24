#include "Panel.h"

#include "BrandConfig.h"
#include "Popovers.h"
#include "UiHelpers.h"
#include "Artwork.h"

#include "core/Clock.h"
#include "core/Tapers.h"
#include "core/WaveTable.h"

namespace sgui
{

namespace
{
using R = juce::Rectangle<int>;
using RF = juce::Rectangle<float>;
constexpr float kStripTitle = 12.0f;

juce::Point<int> knobBox (Knob::Size s)
{
    switch (s)
    {
        case Knob::Size::Large:  return { 72, 80 };
        case Knob::Size::Medium: return { 54, 62 };
        case Knob::Size::Small:  return { 46, 54 };
        case Knob::Size::Tiny:   return { 38, 46 };
    }
    return { 54, 62 };
}

const char* const kEnv1Ids[6] { "env1.attackHold", "env1.attack", "env1.decayHold", "env1.decay", "env1.sustain", "env1.release" };
const char* const kEnv2Ids[6] { "", "env2.attack", "env2.decayHold", "env2.decay", "env2.sustain", "env2.release" };
const char* const kEnvTitles[6] { "AH", "A", "DH", "D", "S", "R" };

class HamburgerButton : public juce::Component
{
public:
    std::function<void()> onClick;
    void paint (juce::Graphics& g) override
    {
        g.setColour (isMouseOver() ? col::accent : juce::Colours::white.withAlpha (0.85f));
        const auto b = getLocalBounds().toFloat().reduced (4.0f, 5.0f);
        for (int i = 0; i < 3; ++i)
            g.fillRoundedRectangle (b.getX(), b.getY() + static_cast<float> (i) * (b.getHeight() - 1.6f) * 0.5f, b.getWidth(), 1.6f, 0.8f);
    }
    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
};

// A component whose look is entirely a lambda (status bar, nameplate).
class Painted : public juce::Component
{
public:
    std::function<void (juce::Graphics&, RF)> painter;
    void paint (juce::Graphics& g) override { if (painter) painter (g, getLocalBounds().toFloat()); }
};

void markDark (juce::Component& c)
{
    if (auto* pc = dynamic_cast<ParamControl*> (&c)) pc->onDark = true;
}
} // namespace

// ── construction ────────────────────────────────────────────────────────────────────────────

MainPanel::MainPanel (SuperGeminiProcessor& p) : proc (p), state (p.apvts)
{
    setSize (kWidth, kHeight);
    setOpaque (true);
    editLayer = juce::jlimit (0, 1, static_cast<int> (state.state.getProperty ("uiEditLayer", 0)));

    // The panel face sits inside the chassis; everything printed on it is drawn by paint().
    face = &make<juce::Component> (*this, R { kCheek, 0, kFaceW, kFaceH });
    face->setInterceptsMouseClicks (false, true);

    buildHeader();

    // Two full hardware rows, UPPER over LOWER, each with its own per-layer band beneath
    // it. No edit layer: every control binds straight to its own layer.
    buildLayerRow (0, kRowY);
    buildStrip    (0, kBand1Y);
    buildLayerRow (1, kRow2Y);
    buildStrip    (1, kBand2Y);

    buildKeyboardArea();
    buildStatusBar();

    bubble = &make<ValueBubble> (*this, R { 0, 0, 10, 10 });
    bubble->setVisible (false);

    lastModLayer = paramIndex ("perf.modLayer");
    lastPortaLayer = paramIndex ("perf.portaLayer");
    updateDynamicState();
    lastTime = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

MainPanel::~MainPanel()
{
    stopTimer();
    if (popover != nullptr) closePopover();
    bindings.clear();
    while (! owned.empty()) owned.pop_back();   // children before their parents
}

// ── builders ────────────────────────────────────────────────────────────────────────────────

Section& MainPanel::section (juce::Component& parent, R r, const juce::String& title, float titleHeight)
{
    return make<Section> (parent, r, title, titleHeight);
}

Knob& MainPanel::knob (juce::Component& parent, int x, int y, Knob::Size s, Target t, const juce::String& id, const juce::String& title,
                       juce::String lo, juce::String hi, const juce::String& secondary, const juce::String& sub)
{
    const auto size = knobBox (s);
    auto& k = make<Knob> (parent, R { x, y, size.x, size.y + (sub.isEmpty() ? 0 : 10) }, title, s, lo, hi);
    k.subLabel = sub;
    bindControl (k, t, id, secondary);
    return k;
}

Fader& MainPanel::fader (juce::Component& parent, R r, Target t, const juce::String& id, const juce::String& title,
                         const juce::String& secondary, const juce::String& sub, bool scale)
{
    auto& f = make<Fader> (parent, r, title, scale);
    f.subLabel = sub;
    bindControl (f, t, id, secondary);
    return f;
}

SegGroup& MainPanel::seg (juce::Component& parent, R r, Target t, const juce::String& id, juce::StringArray labels,
                          bool vertical, juce::Array<int> values)
{
    auto& s = make<SegGroup> (parent, r, labels, vertical);
    if (! values.isEmpty()) s.values = values;
    bindControl (s, t, id);
    return s;
}

Toggle& MainPanel::toggle (juce::Component& parent, R r, Target t, const juce::String& id, const juce::String& label, Toggle::Style style)
{
    auto& tg = make<Toggle> (parent, r, label, style);
    bindControl (tg, t, id);
    return tg;
}

Dropdown& MainPanel::drop (juce::Component& parent, R r, Target t, const juce::String& id)
{
    auto& d = make<Dropdown> (parent, r);
    bindControl (d, t, id);
    return d;
}

void MainPanel::buildHeader()
{
    // Output oscilloscope, between the nameplate and the header groups.
    scopeDisplay = &make<WaveDisplay> (*face, R { 470, 10, 460, 62 });
    scopeDisplay->drawer = [this] (juce::Graphics& g, RF r) { drawScopeTrace (g, r); };

    // PATCH — the edited layer's patch [pp.20–22]
    auto& patch = section (*face, { 948, 10, 184, 62 }, "PATCH", 10.0f);
    patchBox = &make<MenuBox> (patch, R { 10, 28, 164, 24 });
    patchBox->text = [this] { return paramValue (layerPrefix (editLayer) + ".manual") >= 0.5f ? juce::String ("MANUAL") : juce::String ("Init Patch"); };
    patchBox->populate = [this] (juce::PopupMenu& m)
    {
        const int l = editLayer;
        m.addSectionHeader (layerName (l) + " LAYER");
        m.addItem ("Load Init Patch", [this, l] { proc.loadInitPatch (l); });
    };
    patchBox->setTooltip ("Patch of the layer shown in the top row. Loading the init patch switches to SINGLE [p.22].");

    // LAYER — LOWER / UPPER select the edited layer (and the playing layer in SINGLE) [pp.82–83];
    // the third button is MANUAL (DD-15).
    auto& layer = section (*face, { 1140, 10, 232, 62 }, "LAYER", 10.0f);
    for (int i = 0; i < 2; ++i)
    {
        const int l = i == 0 ? 1 : 0;   // LOWER, UPPER
        auto& b = make<ActionButton> (layer, R { 10 + i * 72, 28, 66, 24 }, layerName (l), [this, l] { setEditLayer (l); });
        b.isLit = [this, l] { return editLayer == l; };
        layerButtons[l] = &b;
    }
    toggle (layer, { 154, 28, 68, 24 }, Target::Edit, "manual", "MANUAL")
        .setTooltip ("MANUAL: detach this layer from its memory slot (DD-10). CC 59.");

    // KEYBOARD MODE [p.82]
    auto& kb = section (*face, { 1380, 10, 232, 62 }, "KEYBOARD MODE", 10.0f);
    seg (kb, { 10, 28, 212, 24 }, Target::Global, "perf.keyboardMode", { "SINGLE", "DUAL", "SPLIT" });

    // SPLIT POINT: first note of the upper layer; LEARN = hold SPLIT + play a note [p.82]
    auto& sp = section (*face, { 1620, 10, 134, 62 }, "SPLIT POINT", 10.0f);
    auto& split = drop (sp, { 10, 28, 62, 24 }, Target::Global, "perf.splitPoint");
    split.itemName = [] (int i) { return noteName (i); };
    learnButton = &make<ActionButton> (sp, R { 76, 28, 48, 24 }, "LEARN", [this]
    {
        auto& b = bridge();
        b.learnSplit.store (! b.learnSplit.load());
    });
    learnButton->isLit = [this] { return bridge().learnSplit.load() && blinkPhase < 0.5; };
    learnButton->setTooltip ("Click, then play a note: it becomes the first note of the upper layer [p.82].");
}

void MainPanel::buildMaster (R r)
{
    auto& s = section (*face, r, "MASTER / PERFORMANCE");
    knob (s, 2, 28, Knob::Size::Large, Target::Global, "global.masterVolume", "VOLUME", "0", "+4")
        .setTooltip ("MASTER VOLUME: main outputs only, never stored [p.79]");
    meter = &make<LevelMeter> (s, R { 80, 42, 22, 64 });
    s.addLegend ({ 74.0f, 108.0f, 34.0f, 10.0f }, "OUT", 8.0f);
    knob (s, 110, 28, Knob::Size::Medium, Target::Global, "perf.tempo", "TEMPO", "30", "300")
        .setTooltip ("Internal TEMPO, 30-300 BPM [p.79]. Ignored while following the host clock.");
    tempoLed = &make<Led> (s, R { 150, 92, 14, 14 });

    knob (s, 2, 122, Knob::Size::Medium, Target::Global, "global.fineTune", "TUNE", "-100", "+100").bipolar = true;   // p.76
    knob (s, 58, 122, Knob::Size::Medium, Target::Global, "global.transpose", "TRANSPOSE", "-12", "+12").bipolar = true;   // p.75
    auto& det = knob (s, 114, 122, Knob::Size::Medium, Target::Global, "perf.lowerDetune", "DETUNE", "-7", "+7",
                      "perf.perfDetune", "PERF DET");   // p.80
    det.bipolar = true;
    det.setTooltip ("LOWER DETUNE +/-7 st; SHIFT = PERFORMANCE DETUNE [p.80]");

    // OCTAVE −2 … +2 of the layer picked by the portamento layer toggle [p.75]; the middle
    // button flashes while global transpose ≠ 0 ("TRANSPOSED").
    s.addLegend ({ 4.0f, 202.0f, 160.0f, 12.0f }, "OCTAVE", 9.5f);
    octaveSeg = &seg (s, { 6, 216, 158, 22 }, Target::PortaLayer, "octave", { "-2", "-1", "0", "+1", "+2" });
    octaveSeg->flashSegment = 2;

    // PORTAMENTO [pp.73–74]: fader at 0 = off (DD-16); its layer toggle also serves the octave.
    knob (s, 2, 248, Knob::Size::Medium, Target::PortaLayer, "porta.time", "GLIDE");
    portaLed = &make<Led> (s, R { 60, 254, 14, 14 });
    s.addLegend ({ 76.0f, 254.0f, 90.0f, 14.0f }, "PORTAMENTO", 9.0f, juce::Justification::centredLeft);
    seg (s, { 60, 274, 104, 22 }, Target::Global, "perf.portaLayer", { "UPPER", "LOWER", "BOTH" }, false, { 2, 1, 0 })
        .setTooltip ("Layer the GLIDE and OCTAVE controls act on; BOTH copies upper to lower [p.74]");

    toggle (s, { 6, 332, 80, 22 }, Target::Edit, "voice.unison", "UNISON", Toggle::Style::LedButton);   // p.90
    shiftButton = &make<ActionButton> (s, R { 92, 332, 72, 22 }, "SHIFT", [this] { shiftLatched = ! shiftLatched; });
    shiftButton->isLit = [this] { return shiftActive; };
    shiftButton->setTooltip ("SHIFT: controls with an inverse legend switch to that function. Holding Shift or Alt-dragging does the same.");
}

void MainPanel::buildDdsModulator (R r, Target t, int layer)
{
    auto& s = section (*face, r, "DDS MODULATOR");
    const int xs[4] { 6, 88, 170, 252 };
    const char* heads[4] { "PITCH MOD", "PWM / WAVE", "SUPER (DDS 1)", "CROSS MOD" };
    for (int i = 0; i < 4; ++i)
    {
        const float w = i == 3 ? 72.0f : 76.0f;
        s.addBox ({ static_cast<float> (xs[i]), 28.0f, w, 360.0f });
        s.addLegend ({ static_cast<float> (xs[i]), 33.0f, w, 14.0f }, heads[i], 10.5f);
    }

    // PITCH MOD [p.60]
    knob (s, 17, 54, Knob::Size::Medium, t, "ddsMod.lfo1Amt", "LFO 1");
    knob (s, 17, 126, Knob::Size::Medium, t, "ddsMod.env1Amt", "ENV 1");
    s.addLegend ({ 6.0f, 198.0f, 76.0f, 12.0f }, "DEST", 9.5f);
    seg (s, { 12, 212, 64, 72 }, t, "ddsMod.dest", { "DDS 1", "1+2", "DDS 2" }, true);

    // PWM / WAVE [p.62]
    knob (s, 99, 54, Knob::Size::Medium, t, "ddsMod.pwmWave", "AMOUNT");
    s.addLegend ({ 88.0f, 126.0f, 76.0f, 12.0f }, "SOURCE", 9.5f);
    seg (s, { 94, 140, 64, 72 }, t, "ddsMod.pwmSource", { "MANUAL", "LFO 1", "ENV 1" }, true);

    // SUPER (DDS 1) [pp.61–62]
    s.addLegend ({ 170.0f, 54.0f, 76.0f, 12.0f }, "MODE", 9.5f);
    seg (s, { 176, 68, 64, 72 }, t, "ddsMod.super", { "OFF", "1/2", "ON" }, true);
    knob (s, 181, 150, Knob::Size::Medium, t, "ddsMod.pwDetune", "PW/DETUNE", "0", "10", "ddsMod.drift", "DRIFT");
    knob (s, 181, 234, Knob::Size::Medium, t, "ddsMod.drift", "DRIFT");

    // CROSS MOD [p.63]: DDS 2 → DDS 1, reversed when DDS 2 is in SYNC
    knob (s, 261, 54, Knob::Size::Medium, t, "ddsMod.crossMod", "AMOUNT");
    strips[static_cast<size_t> (layer)].xmodLeds[0] = &make<Led> (s, R { 256, 134, 14, 14 });
    s.addLegend ({ 270.0f, 134.0f, 54.0f, 14.0f }, u8 ("DDS 2 \xe2\x86\x92 1"), 8.5f, juce::Justification::centredLeft);
    strips[static_cast<size_t> (layer)].xmodLeds[1] = &make<Led> (s, R { 256, 154, 14, 14 });
    s.addLegend ({ 270.0f, 154.0f, 54.0f, 14.0f }, u8 ("DDS 1 \xe2\x86\x92 2"), 8.5f, juce::Justification::centredLeft);
    s.addLegend ({ 252.0f, 172.0f, 72.0f, 10.0f }, "DIRECTION", 7.5f);
    strips[static_cast<size_t> (layer)].ringLed = &make<Led> (s, R { 256, 200, 14, 14 });
    s.addLegend ({ 270.0f, 200.0f, 54.0f, 14.0f }, "RING", 8.5f, juce::Justification::centredLeft);
    s.addLegend ({ 252.0f, 216.0f, 72.0f, 10.0f }, "(DDS 2 MODE)", 7.5f);
}

void MainPanel::buildOscillators (R r, Target t, int layer)
{
    // Charcoal sub-panels as on the original [pp.31–39].
    auto& s = section (*face, r, "OSCILLATORS");
    s.addDarkBox ({ 6.0f, 28.0f, 190.0f, 360.0f });
    s.addDarkBox ({ 202.0f, 28.0f, 192.0f, 360.0f });
    auto dark = [] (juce::Component& c) -> juce::Component& { markDark (c); return c; };

    // DDS 1
    int bx = 6;
    s.addLegend ({ static_cast<float> (bx + 9), 33.0f, 80.0f, 14.0f }, "DDS 1", 11.0f, juce::Justification::centredLeft, true);
    strips[static_cast<size_t> (layer)].dds1Display = &make<WaveDisplay> (s, R { bx + 6, 50, 106, 42 });
    strips[static_cast<size_t> (layer)].dds1Display->drawer = [this, layer, t] (juce::Graphics& g, RF area) { drawDds1 (g, area, layer); };
    drop (s, { bx + 116, 51, 68, 22 }, t, "dds1.wave");
    auto& w1 = seg (s, { bx + 6, 98, 178, 22 }, t, "dds1.wave", { "SINE", "SAW", "SQR", "TRI", "NOISE", "ALT" });
    w1.glyphs = { Glyph::Sine, Glyph::Saw, Glyph::Square, Glyph::Triangle, Glyph::Noise, Glyph::Alt };
    dark (knob (s, bx + 2, 128, Knob::Size::Medium, t, "dds1.range", "RANGE", "64'", "2'"));
    dark (knob (s, bx + 66, 128, Knob::Size::Medium, t, "ddsMod.pwmWave", "PWM/WAVE"));
    dark (knob (s, bx + 130, 128, Knob::Size::Medium, t, "ddsMod.pwDetune", "DETUNE", "0", "10", "ddsMod.drift", "DRIFT"));
    s.addLegend ({ static_cast<float> (bx + 6), 204.0f, 178.0f, 12.0f }, "SUPER", 9.5f, juce::Justification::centred, true);
    seg (s, { bx + 6, 218, 178, 22 }, t, "ddsMod.super", { "OFF", "1/2", "ON" });
    s.addLegend ({ static_cast<float> (bx + 6), 248.0f, 86.0f, 12.0f }, "ALT A", 9.5f, juce::Justification::centred, true);
    s.addLegend ({ static_cast<float> (bx + 98), 248.0f, 86.0f, 12.0f }, "ALT B", 9.5f, juce::Justification::centred, true);
    drop (s, { bx + 6, 262, 86, 22 }, t, "dds1.altA");
    drop (s, { bx + 98, 262, 86, 22 }, t, "dds1.altB");
    auto& browse = make<ActionButton> (s, R { bx + 6, 292, 178, 24 }, "WAVEFORM BROWSER", [this, layer, t] { openPopover (Popover::AltWaves, layer); });
    browse.dark = true;
    browse.setTooltip ("The 32 alternative waveforms; left-click sets ALT A, right-click ALT B [pp.33-34]");

    // DDS 2
    bx = 202;
    s.addLegend ({ static_cast<float> (bx + 9), 33.0f, 80.0f, 14.0f }, "DDS 2", 11.0f, juce::Justification::centredLeft, true);
    strips[static_cast<size_t> (layer)].dds2Display = &make<WaveDisplay> (s, R { bx + 6, 50, 106, 42 });
    strips[static_cast<size_t> (layer)].dds2Display->drawer = [this, layer, t] (juce::Graphics& g, RF area) { drawDds2 (g, area, layer); };
    drop (s, { bx + 116, 51, 70, 22 }, t, "dds2.wave");
    auto& w2 = seg (s, { bx + 6, 98, 180, 22 }, t, "dds2.wave", { "SINE", "SAW", "SQR", "TRI", "NOISE", "PULSE" });
    w2.glyphs = { Glyph::Sine, Glyph::Saw, Glyph::Square, Glyph::Triangle, Glyph::Noise, Glyph::Pulse };
    dark (knob (s, bx + 2, 128, Knob::Size::Medium, t, "dds2.range", "RANGE", "LFO", "2'"));
    strips[static_cast<size_t> (layer)].dds2Tune = &knob (s, bx + 66, 128, Knob::Size::Medium, t, "dds2.tune", "TUNE", "-7", "+7");
    strips[static_cast<size_t> (layer)].dds2Tune->bipolar = true;
    markDark (*strips[static_cast<size_t> (layer)].dds2Tune);
    dark (knob (s, bx + 130, 128, Knob::Size::Medium, t, "ddsMod.pwDetune", "PW", "0", "10", "ddsMod.drift", "DRIFT"));
    s.addLegend ({ static_cast<float> (bx + 6), 204.0f, 180.0f, 12.0f }, "MODE", 9.5f, juce::Justification::centred, true);
    strips[static_cast<size_t> (layer)].dds2ModeSeg = &seg (s, { bx + 6, 218, 180, 22 }, t, "dds2.mode", { "NORM", "RING", "SYNC" });
    s.addLegend ({ static_cast<float> (bx + 6), 248.0f, 180.0f, 12.0f }, "SUB OSC  (RANGE = LFO)", 9.5f, juce::Justification::centred, true);
    strips[static_cast<size_t> (layer)].dds2SubSeg = &seg (s, { bx + 6, 262, 180, 22 }, t, "dds2.mode", { "OFF", "SQUARE", "SINE" });
}

void MainPanel::buildMixer (R r, Target t, int layer)
{
    auto& s = section (*face, r, "MIXER");
    fader (s, { 6, 30, 42, 226 }, t, "mixer.mix", "MIX", "mixer.pan", "PAN")
        .setTooltip ("MIX: DDS 1 (0) to DDS 2 / sub-osc (10) [p.40]. SHIFT = PAN.");
    fader (s, { 52, 30, 42, 226 }, t, "mixer.pan", "PAN").bipolar = true;
    s.addLegend ({ 2.0f, 264.0f, 96.0f, 12.0f }, "LFO 1 HF ROUTE", 8.5f);
    strips[static_cast<size_t> (layer)].hfRouteSeg = &seg (s, { 10, 278, 80, 64 }, t, "lfo1.mode", { "NORM", "DDS 1", "DDS 2" }, true);
    strips[static_cast<size_t> (layer)].hfRouteSeg->setTooltip ("Where the HF LFO goes (LFO 1 waveform HF / HF TRK) [p.59]");
    s.addLegend ({ 2.0f, 346.0f, 96.0f, 12.0f }, "SUB", 8.5f);
    strips[static_cast<size_t> (layer)].mixerSubSeg = &seg (s, { 10, 360, 80, 22 }, t, "dds2.mode", { "SQR", "SINE" }, false, { 1, 2 });
    strips[static_cast<size_t> (layer)].mixerSubSeg->offValue = 0;
}

void MainPanel::buildVcf (R r, Target t, int layer)
{
    auto& s = section (*face, r, "VCF");
    s.addLegend ({ 8.0f, 28.0f, 154.0f, 12.0f }, "DRIVE", 9.5f);
    seg (s, { 8, 42, 154, 22 }, t, "vcf.drive", { "OFF", "1", "2" });                       // p.42
    knob (s, 2, 70, Knob::Size::Large, t, "vcf.lpf", "LPF");
    knob (s, 90, 78, Knob::Size::Medium, t, "vcf.res", "RES");
    knob (s, 2, 156, Knob::Size::Medium, t, "vcf.hpf", "HPF");
    knob (s, 58, 156, Knob::Size::Medium, t, "vcf.envAmt", "ENV MOD");
    knob (s, 114, 156, Knob::Size::Medium, t, "vcf.lfo1Amt", "LFO MOD");
    knob (s, 2, 226, Knob::Size::Medium, t, "vcf.dds2Amt", "OSC MOD");
    s.addLegend ({ 62.0f, 232.0f, 100.0f, 12.0f }, "KEYTRACK", 9.5f);
    seg (s, { 62, 246, 100, 22 }, t, "vcf.keytrack", { "OFF", "1/2", "ON" });                // p.43
    s.addLegend ({ 8.0f, 300.0f, 154.0f, 12.0f }, "ENV SOURCE", 9.5f);
    seg (s, { 8, 314, 154, 22 }, t, "vcf.envSource", { "ENV 1", "1+2", "ENV 2" });          // p.42
    strips[static_cast<size_t> (layer)].filterDisplay = &make<WaveDisplay> (s, R { 8, 344, 154, 46 });
    strips[static_cast<size_t> (layer)].filterDisplay->drawer = [this, layer, t] (juce::Graphics& g, RF area) { drawFilterTrace (g, area, layer); };
}

void MainPanel::buildVca (R r, Target t, int layer)
{
    auto& s = section (*face, r, "VCA");
    knob (s, 2, 30, Knob::Size::Medium, t, "vca.envLevel", "ENV LEVEL");
    knob (s, 2, 100, Knob::Size::Medium, t, "vca.dds2Amt", "OSC MOD");
    fader (s, { 62, 30, 42, 150 }, t, "vca.lfo1Amt", "LFO MOD");
    s.addLegend ({ 4.0f, 190.0f, 102.0f, 12.0f }, "DYNAMICS", 9.5f);
    seg (s, { 6, 204, 98, 22 }, t, "vca.dynamics", { "OFF", "1/2", "ON" });                 // p.45
    s.addLegend ({ 4.0f, 234.0f, 102.0f, 12.0f }, "ENVELOPE", 9.5f);
    seg (s, { 6, 248, 98, 68 }, t, "vca.envMode", { "ENV 2", "GATE", "GATE+REL" }, true);    // p.45
}

void MainPanel::buildEnvelopes (R r, Target t, int layer)
{
    auto& s = section (*face, r, "ENVELOPES");
    s.addDarkBox ({ 6.0f, 56.0f, 178.0f, 222.0f });
    strips[static_cast<size_t> (layer)].rowEnvTabs = &make<TabBar> (s, R { 8, 28, 174, 22 }, juce::StringArray { "ENV 1", "ENV 2" });
    strips[static_cast<size_t> (layer)].envDisplay = &make<WaveDisplay> (s, R { 12, 61, 166, 52 });
    strips[static_cast<size_t> (layer)].envDisplay->drawer = [this, layer, t] (juce::Graphics& g, RF area) { drawEnvelopeTrace (g, area, layer); };
    for (int i = 0; i < 6; ++i)
    {
        strips[static_cast<size_t> (layer)].rowEnvFaders[i] = &fader (s, { 8 + i * 29, 118, 28, 156 }, t, {}, kEnvTitles[i]);
        strips[static_cast<size_t> (layer)].rowEnvFaders[i]->onDark = true;
    }
    strips[static_cast<size_t> (layer)].rowEnvTabs->onChange = [this, layer] (int t)
    {
        setEnvTab (strips[static_cast<size_t> (layer)].rowEnvFaders, t);
        strips[static_cast<size_t> (layer)].env1ModeSeg->setEnabled (t == 0);
        strips[static_cast<size_t> (layer)].env1KeySeg->setEnabled (t == 0);
    };
    s.addLegend ({ 4.0f, 282.0f, 182.0f, 10.0f }, "AH / DH = ATTACK HOLD / DECAY HOLD", 7.5f);
    s.addLegend ({ 8.0f, 296.0f, 130.0f, 12.0f }, "ENV 1 MODE", 9.5f);
    strips[static_cast<size_t> (layer)].env1ModeSeg = &seg (s, { 8, 310, 130, 22 }, t, "env1.mode", { "NORM", "INV", "LOOP" });    // p.48
    strips[static_cast<size_t> (layer)].loopLed = &make<Led> (s, R { 146, 310, 22, 22 });
    s.addLegend ({ 140.0f, 332.0f, 36.0f, 10.0f }, "LOOP", 8.0f);
    s.addLegend ({ 8.0f, 340.0f, 130.0f, 12.0f }, "ENV 1 KEYTRACK", 9.5f);
    strips[static_cast<size_t> (layer)].env1KeySeg = &seg (s, { 8, 354, 130, 22 }, t, "env1.keytrack", { "OFF", "1/2", "ON" });   // p.49
    setEnvTab (strips[static_cast<size_t> (layer)].rowEnvFaders, 0);
}

void MainPanel::buildChorus (R r, Target t, int layer)
{
    auto& s = section (*face, r, "CHORUS");
    strips[static_cast<size_t> (layer)].chorusSection = &s;
    auto& cb = make<ChorusButtons> (s, R { 8, 40, 64, 66 }, true);
    bindControl (cb, t, "fx.chorus");
    cb.setTooltip ("CHORUS I, II, or both lit = I+II [p.64]");
    strips[static_cast<size_t> (layer)].chorusLed = &make<Led> (s, R { 10, 118, 16, 16 });
    s.addLegend ({ 28.0f, 120.0f, 40.0f, 12.0f }, "ON", 9.5f, juce::Justification::centredLeft);
    s.extraPaint = [this, layer, t] (juce::Graphics& g)
    {
        static const char* names[] { "OFF", "MODE I", "MODE II", "MODE I+II" };
        drawText (g, names[juce::jlimit (0, 3, paramIndex (layerPrefix (layer) + ".fx.chorus"))], { 4.0f, 140.0f, 72.0f, 14.0f }, 9.5f,
                  juce::Justification::centred, col::tick);
    };
}

void MainPanel::buildDelay (R r, Target t, int layer)
{
    auto& s = section (*face, r, "DELAY");
    strips[static_cast<size_t> (layer)].delaySection = &s;
    knob (s, 4, 30, Knob::Size::Medium, t, "fx.delayTime", "TIME");          // pp.65–66
    strips[static_cast<size_t> (layer)].delayLed = &make<Led> (s, R { 48, 28, 14, 14 });
    knob (s, 62, 30, Knob::Size::Medium, t, "fx.delayFeedback", "FEEDBACK");   // p.66
    knob (s, 4, 102, Knob::Size::Medium, t, "fx.delaySend", "SEND");          // p.67
    toggle (s, { 64, 116, 50, 22 }, t, "arp.sync", "SYNC")
        .setTooltip ("SYNC: delay time and LFO 1 rate in note values of the clock [p.92]");
    s.extraPaint = [this, layer, t] (juce::Graphics& g)
    {
        if (auto* p = resolve (t, "fx.delayTime"))
        {
            const auto text = p->getCurrentValueAsText();
            const bool sync = paramValue (layerPrefix (layer) + ".arp.sync") >= 0.5f;
            const RF screen { 8.0f, 172.0f, 104.0f, 20.0f };
            drawLcd (g, screen);
            drawLcdText (g, sync ? text.fromFirstOccurrenceOf ("sync ", false, false) : text.upToFirstOccurrenceOf (" |", false, false),
                         screen, 11.5f, juce::Justification::centred);
        }
    };
    auto& fr = toggle (s, { 8, 202, 104, 22 }, t, "fx.freeze", "FREEZE", Toggle::Style::LedButton);
    fr.momentary = true;
    fr.setTooltip ("FREEZE while held; Alt-click latches. The loop repeats forever and new notes stay out [p.67].");
}

// One full hardware row for a layer, in the section order of the design canvas
// (design/gen.mjs). Both rows run this same code; nothing routes through an edit layer.
void MainPanel::buildLayerRow (int layer, int y)
{
    const Target T = layer == 0 ? Target::Upper : Target::Lower;

    // MASTER / PERFORMANCE is global, so it is built once, beside the UPPER row.
    if (layer == 0)
        buildMaster ({ 20, y, 170, kRowH });

    buildDdsModulator ({ 210,  y, 330, kRowH }, T, layer);
    buildOscillators  ({ 610,  y, 400, kRowH }, T, layer);
    buildMixer        ({ 1080, y, 100, kRowH }, T, layer);
    buildVcf          ({ 1250, y, 170, kRowH }, T, layer);
    buildVca          ({ 1490, y, 110, kRowH }, T, layer);
    buildEnvelopes    ({ 1670, y, 190, kRowH }, T, layer);
    buildChorus       ({ 1930, y, 80,  kRowH }, T, layer);
    buildDelay        ({ 2080, y, 120, kRowH }, T, layer);
}

void MainPanel::buildStrip (int layer, int y)
{
    auto& st = strips[static_cast<size_t> (layer)];
    const Target T = layer == 0 ? Target::Upper : Target::Lower;
    constexpr int h = 150;
    int x = 22;
    auto next = [&] (int w) { R r { x, y, w, h }; x += w + 6; return r; };

    // LAYER
    auto& ls = section (*face, next (168), layerName (layer) + " LAYER", kStripTitle);
    ls.hasLed = true;
    ls.ledColour = layer == 0 ? col::upper : col::lower;
    st.layerSection = &ls;
    ls.addLegend ({ 8.0f, 22.0f, 60.0f, 10.0f }, "PATCH", 8.5f, juce::Justification::centredLeft);
    st.patch = &make<MenuBox> (ls, R { 8, 34, 152, 22 });
    st.patch->text = [this, layer] { return paramValue (layerPrefix (layer) + ".manual") >= 0.5f ? juce::String ("MANUAL") : juce::String ("Init Patch"); };
    st.patch->populate = [this, layer] (juce::PopupMenu& m)
    {
        m.addSectionHeader (layerName (layer) + " LAYER");
        m.addItem ("Load Init Patch", [this, layer] { proc.loadInitPatch (layer); });
    };
    knob (ls, 2, 62, Knob::Size::Small, T, "vca.envLevel", "LEVEL");
    knob (ls, 50, 62, Knob::Size::Small, T, "mixer.pan", "PAN", "L", "R").bipolar = true;
    st.editButton = &make<ActionButton> (ls, R { 102, 64, 58, 22 }, "EDIT", [this, layer] { setEditLayer (layer); });
    st.editButton->isLit = [this, layer] { return editLayer == layer; };
    st.editButton->setTooltip ("Show this layer in the top row");
    toggle (ls, { 102, 90, 58, 22 }, T, "manual", "MANUAL");

    // OSCILLATORS (charcoal)
    auto& os = section (*face, next (140), "OSCILLATORS", kStripTitle);
    os.addDarkBox ({ 4.0f, 20.0f, 132.0f, 126.0f });
    os.addLegend ({ 8.0f, 23.0f, 60.0f, 10.0f }, "DDS 1", 8.5f, juce::Justification::centred, true);
    os.addLegend ({ 74.0f, 23.0f, 60.0f, 10.0f }, "DDS 2", 8.5f, juce::Justification::centred, true);
    drop (os, { 8, 34, 60, 20 }, T, "dds1.wave");
    bindControl (make<Stepper> (os, R { 8, 57, 60, 20 }), T, "dds1.range");
    drop (os, { 72, 34, 60, 20 }, T, "dds2.wave");
    bindControl (make<Stepper> (os, R { 72, 57, 60, 20 }), T, "dds2.range");
    os.addLegend ({ 8.0f, 82.0f, 60.0f, 10.0f }, "SUPER", 8.5f, juce::Justification::centred, true);
    seg (os, { 8, 94, 60, 20 }, T, "ddsMod.super", { "OFF", "1/2", "ON" });
    auto& tune = knob (os, 84, 82, Knob::Size::Tiny, T, "dds2.tune", "TUNE", "-7", "+7");
    tune.bipolar = true;
    markDark (tune);

    // MIXER
    auto& mx = section (*face, next (84), "MIXER", kStripTitle);
    fader (mx, { 4, 22, 38, 124 }, T, "mixer.mix", "MIX", "mixer.pan", "PAN", false);
    fader (mx, { 42, 22, 38, 124 }, T, "mixer.pan", "PAN", {}, {}, false).bipolar = true;

    // VCF
    auto& vf = section (*face, next (124), "VCF", kStripTitle);
    vf.addLegend ({ 6.0f, 22.0f, 112.0f, 10.0f }, "DRIVE", 8.5f);
    seg (vf, { 6, 34, 112, 20 }, T, "vcf.drive", { "OFF", "1", "2" });
    knob (vf, 8, 62, Knob::Size::Small, T, "vcf.lpf", "LPF");
    knob (vf, 68, 62, Knob::Size::Small, T, "vcf.res", "RES");

    // VCA
    auto& va = section (*face, next (70), "VCA", kStripTitle);
    knob (va, 12, 40, Knob::Size::Small, T, "vca.envLevel", "LEVEL");

    // ENVELOPES (charcoal)
    auto& en = section (*face, next (176), "ENVELOPES", kStripTitle);
    en.ruleEnd = 72.0f;
    en.addDarkBox ({ 4.0f, 22.0f, 168.0f, 124.0f });
    st.envTabs = &make<TabBar> (en, R { 78, 3, 92, 17 }, juce::StringArray { "ENV 1", "ENV 2" });
    for (int i = 0; i < 6; ++i)
    {
        st.envFaders[i] = &fader (en, { 5 + i * 27, 24, 28, 122 }, T, {}, kEnvTitles[i], {}, {}, false);
        st.envFaders[i]->onDark = true;
    }
    st.envTabs->onChange = [this, layer] (int t) { setEnvTab (strips[static_cast<size_t> (layer)].envFaders, t); };
    setEnvTab (st.envFaders, 0);

    // LFO: LFO 1 page, and the LFO 2 page = the performance section [pp.54–59, 68–77]
    auto& lf = section (*face, next (318), "LFO", kStripTitle);
    lf.ruleEnd = 36.0f;
    st.lfoTabs = &make<TabBar> (lf, R { 40, 3, 110, 17 }, juce::StringArray { "LFO 1", "LFO 2" });
    st.lfoLed = &make<Led> (lf, R { 154, 3, 16, 16 });
    for (int p = 0; p < 2; ++p)
    {
        auto& page = make<Section> (lf, R { 0, 22, 318, 128 }, juce::String(), 10.0f);
        page.frame = false;
        page.setVisible (p == 0);
        st.lfoPages[p] = &page;
    }
    st.lfoTabs->onChange = [this, layer] (int t)
    {
        auto& s = strips[static_cast<size_t> (layer)];
        s.lfoPages[0]->setVisible (t == 0);
        s.lfoPages[1]->setVisible (t == 1);
    };
    {
        auto& p1 = *static_cast<Section*> (st.lfoPages[0]);
        const char* legends[] { "WAVE", "MODE", "SYNC" };
        for (int i = 0; i < 3; ++i) p1.addLegend ({ 6.0f, 2.0f + static_cast<float> (i) * 21.0f, 34.0f, 18.0f }, legends[i], 8.5f, juce::Justification::centredLeft);
        auto& w = seg (p1, { 42, 2, 114, 18 }, T, "lfo1.wave", { "TRI", "RSAW", "S&H", "SQR", "HF", "TRK" });   // p.58–59
        w.glyphs = { Glyph::Triangle, Glyph::RevSaw, Glyph::SampleHold, Glyph::Square };
        w.setTooltip ("TRIANGLE, REVERSE SAW, S&H, SQUARE, HF, HF TRK [pp.58-59]");
        st.lfo1ModeSeg = &seg (p1, { 42, 23, 114, 18 }, T, "lfo1.mode", { "FREE", "ONCE", "RESET" });          // p.59
        toggle (p1, { 42, 44, 56, 18 }, T, "arp.sync", "SYNC");
        auto& scope = make<WaveDisplay> (p1, R { 6, 66, 150, 56 });
        scope.drawer = [this, layer] (juce::Graphics& g, RF area) { drawLfoTrace (g, area, layer, 0); };
        lfoScope[static_cast<size_t> (layer)] = &scope;
        knob (p1, 164, 0, Knob::Size::Tiny, T, "lfo1.rate", "RATE");
        knob (p1, 204, 0, Knob::Size::Tiny, T, "lfo1.delay", "DELAY");
        st.lrPhase[0] = &knob (p1, 244, 0, Knob::Size::Tiny, T, "lfo1.lrPhase", "LR PHASE");
        knob (p1, 164, 58, Knob::Size::Tiny, T, "ddsMod.lfo1Amt", "PITCH");
        knob (p1, 204, 58, Knob::Size::Tiny, T, "vcf.lfo1Amt", "VCF");
        knob (p1, 244, 58, Knob::Size::Tiny, T, "vca.lfo1Amt", "VCA");
    }
    {
        auto& p2 = *static_cast<Section*> (st.lfoPages[1]);
        const char* legends[] { "WAVE", "TRIG", "DEST", "LAYER" };
        for (int i = 0; i < 4; ++i) p2.addLegend ({ 6.0f, 2.0f + static_cast<float> (i) * 20.0f, 34.0f, 17.0f }, legends[i], 8.5f, juce::Justification::centredLeft);
        auto& w = seg (p2, { 42, 2, 114, 17 }, T, "lfo2.wave", { "SINE", "RSAW", "S&H", "SQR", "SAW", "NOISE" });   // p.70
        w.glyphs = { Glyph::Sine, Glyph::RevSaw, Glyph::SampleHold, Glyph::Square, Glyph::Saw, Glyph::Noise };
        seg (p2, { 42, 22, 114, 17 }, T, "lfo2.trigger", { "TRIG", "AT+TRIG", "ON" })                               // p.71
            .setTooltip ("TRIG: bender push. AT+TRIG: aftertouch or push. ON: always on, aftertouch acts as bend [p.71]");
        seg (p2, { 42, 42, 114, 17 }, T, "dest.osc", { "DDS 1", "1+2", "DDS 2" })                                     // p.72
            .setTooltip ("Oscillators that receive bender and LFO 2 pitch modulation [p.72]");
        seg (p2, { 42, 62, 114, 17 }, Target::Global, "perf.modLayer", { "UPPER", "LOWER", "BOTH" }, false, { 2, 1, 0 })
            .setTooltip ("DEST layer selector: BOTH keeps both layers' bender / LFO 2 settings the same [p.73]");
        auto& scope2 = make<WaveDisplay> (p2, R { 6, 84, 150, 40 });
        scope2.drawer = [this, layer] (juce::Graphics& g, RF area) { drawLfoTrace (g, area, layer, 1); };
        knob (p2, 160, 0, Knob::Size::Tiny, T, "lfo2.rate", "RATE");
        knob (p2, 198, 0, Knob::Size::Tiny, T, "lfo2.delay", "DELAY");
        knob (p2, 236, 0, Knob::Size::Tiny, T, "lfo2.rateMod", "RATE MOD");
        knob (p2, 274, 0, Knob::Size::Tiny, T, "lfo2.ddsAmt", "DDS");
        knob (p2, 160, 62, Knob::Size::Tiny, T, "lfo2.vcfAmt", "VCF");
        knob (p2, 198, 62, Knob::Size::Tiny, T, "lfo2.vcaAmt", "VCA");
        knob (p2, 236, 62, Knob::Size::Tiny, T, "bender.ddsAmt", "BEND DDS");
        knob (p2, 274, 62, Knob::Size::Tiny, T, "bender.vcfAmt", "BEND VCF");
    }

    // MODULATION [pp.84–89]
    auto& md = section (*face, next (150), "MODULATION", kStripTitle);
    md.ruleEnd = 96.0f;
    st.modSection = &md;
    make<ActionButton> (md, R { 100, 3, 44, 17 }, "GRID", [this, layer] { openPopover (Popover::Matrix, layer); })
        .setTooltip ("All 8 sources x 32 destinations of this layer");
    md.addLegend ({ 6.0f, 22.0f, 138.0f, 10.0f }, "SOURCE", 8.5f, juce::Justification::centredLeft);
    md.addLegend ({ 6.0f, 56.0f, 138.0f, 10.0f }, "DESTINATION", 8.5f, juce::Justification::centredLeft);
    st.modSource = &make<MenuBox> (md, R { 6, 33, 138, 21 });
    st.modSource->text = [this, layer] { return juce::String (sg::kMatrixSourceNames[static_cast<size_t> (strips[static_cast<size_t> (layer)].modSrc)]); };
    st.modSource->populate = [this, layer] (juce::PopupMenu& m)
    {
        const auto& s = strips[static_cast<size_t> (layer)];
        for (int i = 0; i < sg::kMatrixSources; ++i)
            m.addItem (juce::PopupMenu::Item (juce::String (i + 1) + "  " + sg::kMatrixSourceNames[static_cast<size_t> (i)])
                           .setTicked (i == s.modSrc).setAction ([this, layer, i] { setModSelection (layer, i, strips[static_cast<size_t> (layer)].modDst); }));
    };
    st.modDest = &make<MenuBox> (md, R { 6, 67, 138, 21 });
    st.modDest->text = [this, layer] { return juce::String (sg::kMatrixDestNames[static_cast<size_t> (strips[static_cast<size_t> (layer)].modDst)]).toUpperCase(); };
    st.modDest->populate = [this, layer] (juce::PopupMenu& m)
    {
        const auto& s = strips[static_cast<size_t> (layer)];
        auto item = [this, layer, &s] (int d, const juce::String& prefix)
        {
            return juce::PopupMenu::Item (prefix + sg::kMatrixDestNames[static_cast<size_t> (d)])
                .setTicked (d == s.modDst).setEnabled (! sg::matrixExcluded (s.modSrc, d))
                .setAction ([this, layer, d] { setModSelection (layer, strips[static_cast<size_t> (layer)].modSrc, d); });
        };
        for (int d = 0; d < sg::kFixedDests; ++d) m.addItem (item (d, juce::String::charToString (static_cast<juce::juce_wchar> ('A' + d)) + "  "));
        juce::PopupMenu direct;
        for (int d = sg::kFixedDests; d < sg::kMatrixDests; ++d) direct.addItem (item (d, juce::String (d - sg::kFixedDests + 1) + "  "));
        m.addSubMenu ("Direct (24 controls)", direct);
    };
    st.modAmount = &make<Knob> (md, R { 4, 92, 46, 54 }, "AMOUNT", Knob::Size::Small, "-100", "+100");
    st.modAmount->bipolar = true;
    st.modAmount->tone = layer == 0 ? Tone::White : Tone::Orange;
    st.modAmount->onValueDisplay = [this] (ParamControl& pc, bool active) { showValue (pc, active); };
    make<ActionButton> (md, R { 58, 96, 86, 21 }, "CLEAR DEST", [this, layer]
    {
        const int d = strips[static_cast<size_t> (layer)].modDst;
        for (int s = 0; s < sg::kMatrixSources; ++s)
            if (auto* p = state.getParameter (matrixId (layer, s, d)))
            {
                p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture();
            }
    }).setTooltip ("Clear every mapping to this destination [p.89]");
    md.extraPaint = [this, layer] (juce::Graphics& g)
    {
        int count = 0;
        for (int s = 0; s < sg::kMatrixSources; ++s)
            for (int d = 0; d < sg::kMatrixDests; ++d)
                if (! sg::matrixExcluded (s, d) && std::abs (paramValue (matrixId (layer, s, d))) > 1.0e-4f) ++count;
        drawText (g, juce::String (count) + (count == 1 ? " ACTIVE ROUTE" : " ACTIVE ROUTES"), { 56.0f, 122.0f, 90.0f, 14.0f }, 8.5f,
                  juce::Justification::centred, count > 0 ? col::accent.darker (0.2f) : col::tick);
    };
    setModSelection (layer, 1, 0);

    // EFFECTS
    auto& fx = section (*face, next (112), "EFFECTS", kStripTitle);
    fx.addLegend ({ 6.0f, 22.0f, 100.0f, 10.0f }, "CHORUS", 8.5f);
    bindControl (make<ChorusButtons> (fx, R { 6, 34, 100, 22 }, false), T, "fx.chorus");
    knob (fx, 4, 62, Knob::Size::Small, T, "fx.delaySend", "SEND");
    auto& frz = toggle (fx, { 54, 80, 54, 20 }, T, "fx.freeze", "FREEZE", Toggle::Style::LedButton);
    frz.momentary = true;
    frz.setTooltip ("FREEZE while held; Alt-click latches [p.67]");

    // VOICES [pp.90–91]
    auto& vo = section (*face, next (150), "VOICES", kStripTitle);
    vo.ruleEnd = 100.0f;
    st.voiceSection = &vo;
    vo.addLegend ({ 6.0f, 22.0f, 84.0f, 10.0f }, "MODE", 8.5f, juce::Justification::centredLeft);
    drop (vo, { 6, 33, 84, 21 }, T, "voice.mode");
    vo.addLegend ({ 6.0f, 56.0f, 84.0f, 10.0f }, "U. SIZE", 8.5f, juce::Justification::centredLeft);
    drop (vo, { 6, 67, 84, 21 }, T, "voice.unisonSize");
    toggle (vo, { 96, 33, 48, 21 }, T, "voice.unison", "UNISON");
    toggle (vo, { 96, 67, 48, 21 }, T, "voice.binaural", "BINAURAL");
    knob (vo, 4, 96, Knob::Size::Tiny, T, "ddsMod.drift", "DRIFT");
    st.lrPhase[1] = &knob (vo, 44, 96, Knob::Size::Tiny, T, "lfo1.lrPhase", "LR PHASE");
    knob (vo, 84, 96, Knob::Size::Tiny, T, "porta.time", "GLIDE");
    vo.extraPaint = [this, layer] (juce::Graphics& g)
    {
        const int v = bridge().layer[static_cast<size_t> (layer)].voices.load();
        drawText (g, juce::String (v) + " V", { 104.0f, 4.0f, 40.0f, 12.0f }, 9.0f, juce::Justification::centredRight,
                  v > 0 ? col::text : col::tick);
    };

    // ARPEGGIATOR / SEQUENCER [pp.92–98]
    auto& ar = section (*face, next (178), "ARPEGGIATOR / SEQUENCER", kStripTitle);
    st.arpSection = &ar;
    toggle (ar, { 6, 22, 40, 20 }, T, "arp.on", "ON");
    drop (ar, { 50, 22, 72, 20 }, T, "arp.mode");
    toggle (ar, { 126, 22, 46, 20 }, T, "hold", "HOLD").setTooltip ("HOLD: notes (and the arpeggio) keep playing after release [p.81]");
    ar.addLegend ({ 6.0f, 45.0f, 54.0f, 10.0f }, "CLK DIV", 8.5f);
    ar.addLegend ({ 64.0f, 45.0f, 52.0f, 10.0f }, "RANGE", 8.5f);
    ar.addLegend ({ 120.0f, 45.0f, 52.0f, 10.0f }, "SWING", 8.5f);
    drop (ar, { 6, 56, 54, 20 }, T, "arp.clockDiv");
    drop (ar, { 64, 56, 52, 20 }, T, "arp.range");
    drop (ar, { 120, 56, 52, 20 }, T, "arp.swing");
    ar.addLegend ({ 6.0f, 79.0f, 54.0f, 10.0f }, "SEQUENCE", 8.5f);
    ar.addLegend ({ 64.0f, 79.0f, 52.0f, 10.0f }, "SEQ REC", 8.5f);
    ar.addLegend ({ 120.0f, 79.0f, 52.0f, 10.0f }, "STEPS", 8.5f);
    auto& slot = drop (ar, { 6, 90, 54, 20 }, T, "seq.slot");
    slot.itemName = [] (int i) { return "SEQ " + juce::String (i + 1); };
    st.recButton = &make<ActionButton> (ar, R { 64, 90, 52, 20 }, "REC", [this, layer]
    {
        UiCommand c;
        c.type = UiCommand::Type::SeqRecord;
        c.layer = layer;
        c.a = bridge().layer[static_cast<size_t> (layer)].recording.load() ? 0 : 1;
        c.b = -1;
        bridge().push (c);
    });
    st.recButton->isLit = [this, layer] { return bridge().layer[static_cast<size_t> (layer)].recording.load() && blinkPhase < 0.6; };
    st.recButton->setTooltip ("SEQ REC: play notes / chords; each is stored when all keys are released [p.96]");
    make<ActionButton> (ar, R { 120, 90, 52, 20 }, "EDIT", [this, layer] { openPopover (Popover::Sequencer, layer); })
        .setTooltip ("Step editor: notes, SLIDE, ACCENT, REST, LENGTH, load / store [pp.95-98]");
    toggle (ar, { 6, 116, 54, 20 }, T, "arp.sync", "SYNC");
    ar.extraPaint = [this, layer] (juce::Graphics& g)
    {
        const auto& f = bridge().layer[static_cast<size_t> (layer)];
        juce::String text = "STOPPED";
        if (f.recording.load())    text = "REC STEP " + juce::String (f.recStep.load() + 1);
        else if (f.running.load()) text = "STEP " + juce::String (juce::jmax (1, f.step.load()));
        const RF screen { 66.0f, 116.0f, 104.0f, 20.0f };
        drawLcd (g, screen);
        drawLcdText (g, text, screen, 10.5f, juce::Justification::centred, f.running.load() || f.recording.load() ? 1.0f : 0.45f);
    };
}

void MainPanel::buildKeyboardArea()
{
    // Bender on the shelf, at the left of the keys [p.69].
    auto& bender = make<BenderPad> (*this, R { kCheek + 22, kShelfY + 16, 176, kShelfH - 32 });
    bender.onMove = [this] (float bend, float push)
    {
        UiCommand c;
        c.type = UiCommand::Type::Bend; c.f = bend; bridge().push (c);
        c.type = UiCommand::Type::Push; c.f = push; bridge().push (c);
    };
    bender.setTooltip ("BENDER: sideways bends pitch and cutoff, push up drives LFO 2 [p.69]");

    // Ribbon controller above the keys [pp.77-78].
    auto& ribbon = make<RibbonStrip> (*this, R { kCheek + 248, kShelfY + 32, 1180, 30 });
    ribbon.onTouch = [this] (float pos) { UiCommand c; c.type = UiCommand::Type::RibbonTouch; c.f = pos; bridge().push (c); };
    ribbon.onMove = [this] (float pos) { UiCommand c; c.type = UiCommand::Type::RibbonMove; c.f = pos; bridge().push (c); };
    ribbon.onRelease = [this] { UiCommand c; c.type = UiCommand::Type::RibbonRelease; bridge().push (c); };
    ribbon.setTooltip ("RIBBON: slide from where you touch to bend the held notes [p.77]");

    // 61 keys, C1 … C6, played with the mouse.
    auto& keys = make<Keyboard> (*this, R { kCheek, kKeysY, kFaceW, kKeysH }, 36, 96);
    keys.onNoteOn = [this] (int note, float velocity)
    {
        UiCommand c; c.type = UiCommand::Type::NoteOn; c.a = note; c.f = velocity; bridge().push (c);
    };
    keys.onNoteOff = [this] (int note) { UiCommand c; c.type = UiCommand::Type::NoteOff; c.a = note; bridge().push (c); };
}

void MainPanel::buildStatusBar()
{
    auto& bar = make<Painted> (*this, R { 0, kStatusY, kWidth, kHeight - kStatusY });
    bar.setInterceptsMouseClicks (false, false);
    bar.painter = [this] (juce::Graphics& g, RF r)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff232426), 0.0f, r.getY(), col::footer, 0.0f, r.getBottom(), false));
        g.fillRect (r);
        g.setColour (juce::Colours::black);
        g.drawHorizontalLine (juce::roundToInt (r.getY()), r.getX(), r.getRight());
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine (juce::roundToInt (r.getY() + 1.0f), r.getX(), r.getRight());
        drawText (g, u8 ("RIGHT-CLICK: MODULATE / MIDI CC   \xc2\xb7   DOUBLE-CLICK: DEFAULT   \xc2\xb7   CTRL-DRAG: FINE   \xc2\xb7   SHIFT OR ALT-DRAG: INVERSE LEGEND"),
                  r.reduced (24.0f, 0.0f), 10.5f, juce::Justification::centredLeft, juce::Colour (0xff96989b), false);
        const int voices = bridge().layer[0].voices.load() + bridge().layer[1].voices.load();
        drawText (g, "VOICES " + juce::String (voices) + u8 (" / 20   \xc2\xb7   ") + juce::String (bridge().bpm.load(), 1)
                     + u8 (" BPM   \xc2\xb7   EDITING ") + layerName (editLayer),
                  r.reduced (64.0f, 0.0f), 10.5f, juce::Justification::centredRight, juce::Colour (0xffc6c8cb));
    };
    statusBar = &bar;

    auto& menu = make<HamburgerButton> (*this, R { kWidth - 44, kStatusY + 6, 26, 20 });
    menu.onClick = [this] { openPopover (Popover::Settings, editLayer); };
}

// ── screens ─────────────────────────────────────────────────────────────────────────────────

void MainPanel::drawScopeTrace (juce::Graphics& g, RF r)
{
    // Output waveform, triggered on a rising zero crossing so it stands still.
    auto& b = bridge();
    const uint32_t pos = b.scopePos.load (std::memory_order_acquire);
    constexpr int window = 700;
    const int size = UiBridge::kScopeSize;
    auto at = [&] (int i) { return b.scope[static_cast<size_t> (((pos + size) - window + i) % size)].load (std::memory_order_relaxed); };

    int trigger = 0;
    for (int i = 1; i < window / 2; ++i)
        if (at (i - 1) <= 0.0f && at (i) > 0.0f) { trigger = i; break; }

    const int span = window / 2;
    juce::Path path;
    float peak = 0.0f;
    for (int i = 0; i <= span; ++i)
    {
        const float v = at (trigger + i);
        peak = juce::jmax (peak, std::abs (v));
        const float x = r.getX() + r.getWidth() * static_cast<float> (i) / static_cast<float> (span);
        const float y = r.getCentreY() - juce::jlimit (-1.0f, 1.0f, v * 1.6f) * r.getHeight() * 0.46f;
        if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
    }
    strokeTrace (g, path, peak > 0.0005f ? 1.0f : 0.35f, 1.6f);
    drawLcdText (g, "OUTPUT", r.withHeight (11.0f), 9.0f, juce::Justification::topLeft, 0.5f);
}

void MainPanel::drawEnvelopeTrace (juce::Graphics& g, RF r, int layer)
{
    // The edited layer's envelope, drawn from its times [pp.46-53], with the live level.
    const auto L = layerPrefix (layer) + ".";
    const bool env1 = strips[static_cast<size_t> (layer)].rowEnvTabs == nullptr || strips[static_cast<size_t> (layer)].rowEnvTabs->current == 0;
    const float ah = env1 ? sg::taper::holdTime (paramValue (L + "env1.attackHold")) : 0.0f;
    const float a  = sg::taper::envTime (paramValue (L + (env1 ? "env1.attack" : "env2.attack")));
    const float dh = sg::taper::holdTime (paramValue (L + (env1 ? "env1.decayHold" : "env2.decayHold")));
    const float d  = sg::taper::envTime (paramValue (L + (env1 ? "env1.decay" : "env2.decay")));
    const float s  = paramValue (L + (env1 ? "env1.sustain" : "env2.sustain"));
    const float rel = sg::taper::envTime (paramValue (L + (env1 ? "env1.release" : "env2.release")));
    const bool inverted = env1 && paramIndex (L + "env1.mode") == 1;

    auto width = [] (float seconds) { return std::sqrt (juce::jlimit (0.0f, 10.0f, seconds)); };   // compressed time axis
    const float sustainW = 0.9f;
    const float total = juce::jmax (0.001f, width (ah) + width (a) + width (dh) + width (d) + sustainW + width (rel));
    const float top = r.getY() + 2.0f, bottom = r.getBottom() - 2.0f, h = bottom - top;
    auto level = [&] (float v) { return bottom - (inverted ? 1.0f - v : v) * h; };

    juce::Path p;
    float x = r.getX();
    p.startNewSubPath (x, level (0.0f));
    x += r.getWidth() * width (ah) / total;                     p.lineTo (x, level (0.0f));
    const float ax = r.getWidth() * width (a) / total;
    for (int i = 1; i <= 10; ++i)                                // exponential-ish rise
    {
        const float t = static_cast<float> (i) / 10.0f;
        p.lineTo (x + ax * t, level (1.0f - std::pow (1.0f - t, 2.2f)));
    }
    x += ax;
    x += r.getWidth() * width (dh) / total;                      p.lineTo (x, level (1.0f));
    const float dx = r.getWidth() * width (d) / total;
    for (int i = 1; i <= 10; ++i)
    {
        const float t = static_cast<float> (i) / 10.0f;
        p.lineTo (x + dx * t, level (s + (1.0f - s) * std::pow (1.0f - t, 2.2f)));
    }
    x += dx;
    x += r.getWidth() * sustainW / total;                        p.lineTo (x, level (s));
    const float rx = r.getWidth() * width (rel) / total;
    for (int i = 1; i <= 10; ++i)
    {
        const float t = static_cast<float> (i) / 10.0f;
        p.lineTo (x + rx * t, level (s * std::pow (1.0f - t, 2.2f)));
    }

    // filled area under the curve
    juce::Path fill (p);
    fill.lineTo (x + rx, bottom);
    fill.lineTo (r.getX(), bottom);
    fill.closeSubPath();
    g.setColour (col::lcdText.withAlpha (0.12f));
    g.fillPath (fill);
    strokeTrace (g, p, 1.0f, 1.5f);

    // live level of the newest voice
    const float lv = bridge().layer[static_cast<size_t> (layer)].env1.load();
    const float shown = env1 ? lv : bridge().layer[static_cast<size_t> (layer)].env2.load();
    if (shown > 0.002f)
    {
        g.setColour (col::lcdText.withAlpha (0.5f));
        g.drawHorizontalLine (juce::roundToInt (level (shown)), r.getX(), r.getRight());
    }
    drawLcdText (g, env1 ? "ENV 1" : "ENV 2", r.withHeight (11.0f), 9.0f, juce::Justification::topRight, 0.5f);
}

void MainPanel::drawFilterTrace (juce::Graphics& g, RF r, int layer)
{
    // 1-pole HPF + 4-pole resonant LPF response [pp.41-43], with the modulated cutoff as a ghost.
    const auto L = layerPrefix (layer) + ".";
    const float res = paramValue (L + "vcf.res");
    const float k = 4.25f * res * (0.35f + 0.65f * res);
    const float hpfHz = sg::taper::hpfHz (paramValue (L + "vcf.hpf"));
    const float octaves = 11.5f - 2.0f * (paramIndex (L + "vcf.keytrack") * 0.5f);

    auto curve = [&] (float cutoffNorm, juce::Path& path)
    {
        const float fc = 20.0f * std::pow (2.0f, juce::jlimit (0.0f, 1.2f, cutoffNorm) * octaves);
        constexpr int n = 96;
        for (int i = 0; i <= n; ++i)
        {
            const float t = static_cast<float> (i) / static_cast<float> (n);
            const float f = 20.0f * std::pow (1000.0f, t);                  // 20 Hz … 20 kHz
            const float wl = f / juce::jmax (10.0f, fc);
            const float lp = 1.0f / std::sqrt (std::pow (1.0f + wl * wl, 4.0f) + k * k - 2.0f * k * (1.0f - wl * wl) * 0.9f);
            const float wh = f / juce::jmax (5.0f, hpfHz);
            const float hp = wh / std::sqrt (1.0f + wh * wh);
            const float db = juce::Decibels::gainToDecibels (juce::jmax (1.0e-4f, lp * hp));
            const float y = r.getBottom() - (juce::jlimit (-48.0f, 18.0f, db) + 48.0f) / 66.0f * r.getHeight();
            const float x = r.getX() + t * r.getWidth();
            if (i == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }
    };

    const float modCutoff = bridge().layer[static_cast<size_t> (layer)].cutoffNorm.load();
    if (modCutoff >= 0.0f && std::abs (modCutoff - paramValue (L + "vcf.lpf")) > 0.01f)
    {
        juce::Path ghost;
        curve (modCutoff, ghost);
        g.setColour (col::lcdText.withAlpha (0.35f));
        g.strokePath (ghost, juce::PathStrokeType (1.0f));
    }
    juce::Path p;
    const float lpf = paramValue (L + "vcf.lpf");
    curve (lpf, p);
    strokeTrace (g, p, 1.0f, 1.5f);

    // where the corner sits, and what it is in Hz
    const float fc = 20.0f * std::pow (2.0f, juce::jlimit (0.0f, 1.2f, modCutoff >= 0.0f ? modCutoff : lpf) * octaves);
    const float tx = juce::jlimit (0.0f, 1.0f, std::log (fc / 20.0f) / std::log (1000.0f));
    g.setColour (col::lcdText.withAlpha (0.3f));
    g.drawVerticalLine (juce::roundToInt (r.getX() + tx * r.getWidth()), r.getY() + 2.0f, r.getBottom() - 2.0f);
    drawLcdText (g, "VCF", r.withHeight (11.0f).translated (2.0f, 1.0f), 9.0f, juce::Justification::topLeft, 0.5f);
    const juce::String readout = fc > 20000.0f ? juce::String (u8 ("\xe2\x80\xba 20 kHz"))       // open past the audio band
                               : fc >= 1000.0f ? juce::String (fc / 1000.0f, 1) + " kHz"
                                               : juce::String (juce::roundToInt (fc)) + " Hz";
    drawLcdText (g, readout, r.withHeight (11.0f).translated (-3.0f, 1.0f), 9.0f, juce::Justification::topRight, 0.75f);
}

void MainPanel::drawLfoTrace (juce::Graphics& g, RF r, int layer, int which)
{
    const auto P = layerPrefix (layer) + ".";
    const int wave = paramIndex (P + (which == 0 ? "lfo1.wave" : "lfo2.wave"));
    auto shape = [which, wave] (float ph)
    {
        ph -= std::floor (ph);
        if (which == 0)
            switch (wave)   // TRIANGLE, REV SAW, S&H, SQUARE, HF, HF TRK
            {
                case 0:  return 1.0f - 4.0f * std::abs (ph - 0.5f);
                case 1:  return 1.0f - 2.0f * ph;
                case 2:  { const auto h = static_cast<uint32_t> (ph * 8.0f) * 2654435761u; return static_cast<float> ((h >> 9) & 0xffff) / 32767.5f - 1.0f; }
                case 3:  return ph < 0.5f ? 1.0f : -1.0f;
                default: return std::sin (juce::MathConstants<float>::twoPi * ph * 6.0f);
            }
        switch (wave)       // SINE, REV SAW, S&H, SQUARE, SAW, NOISE
        {
            case 0:  return std::sin (juce::MathConstants<float>::twoPi * ph);
            case 1:  return 1.0f - 2.0f * ph;
            case 2:  { const auto h = static_cast<uint32_t> (ph * 8.0f) * 2654435761u; return static_cast<float> ((h >> 9) & 0xffff) / 32767.5f - 1.0f; }
            case 3:  return ph < 0.5f ? 1.0f : -1.0f;
            case 4:  return 2.0f * ph - 1.0f;
            default: { const auto h = static_cast<uint32_t> (ph * 97.0f) * 2246822519u; return static_cast<float> ((h >> 9) & 0xffff) / 32767.5f - 1.0f; }
        }
    };

    juce::Path p;
    constexpr int n = 120;
    for (int i = 0; i <= n; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (n);
        const float x = r.getX() + t * r.getWidth();
        const float y = r.getCentreY() - juce::jlimit (-1.0f, 1.0f, shape (t * 2.0f)) * r.getHeight() * 0.44f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    strokeTrace (g, p, 1.0f, 1.4f);

    // the dot runs at the LFO's rate
    const double phase = lfoScopePhase[static_cast<size_t> (layer)];
    const float t = static_cast<float> (std::fmod (phase, 1.0)) * 0.5f;
    const float x = r.getX() + t * r.getWidth();
    const float y = r.getCentreY() - juce::jlimit (-1.0f, 1.0f, shape (t * 2.0f)) * r.getHeight() * 0.44f;
    g.setColour (col::lcdText.withAlpha (0.35f));
    g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ x, y }));
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.fillEllipse (juce::Rectangle<float> (3.6f, 3.6f).withCentre ({ x, y }));
    drawLcdText (g, which == 0 ? "LFO 1" : "LFO 2", r.withHeight (10.0f), 8.5f, juce::Justification::topLeft, 0.5f);
}

void MainPanel::updateModulationRings()
{
    // Show, on the control itself, where modulation has pushed the parameter.
    for (auto& b : bindings)
    {
        auto* p = b.control->getParam();
        if (p == nullptr) { b.control->setModNorm (-1.0f); continue; }
        const auto id = p->getParameterID();
        if (! (id.startsWith ("upper.") || id.startsWith ("lower."))) { b.control->setModNorm (-1.0f); continue; }
        const int layer = id.startsWith ("lower.") ? 1 : 0;
        const auto suffix = id.fromFirstOccurrenceOf (".", false, false);
        const auto& feed = bridge().layer[static_cast<size_t> (layer)];

        float mod = -1.0f;
        if (suffix == "vcf.lpf")
        {
            const float cutoff = feed.cutoffNorm.load();          // includes envelope, LFO, keytrack
            if (cutoff >= 0.0f) mod = juce::jlimit (0.0f, 1.0f, cutoff);
        }
        else
        {
            const int dest = matrixDestForSuffix (suffix);
            if (dest >= 0)
            {
                const float offset = feed.mod[static_cast<size_t> (dest)].load();
                if (std::abs (offset) > 0.002f) mod = juce::jlimit (0.0f, 1.0f, b.control->getNorm() + offset);
            }
        }
        b.control->setModNorm (mod);
    }
}

// ── bindings ────────────────────────────────────────────────────────────────────────────────

float MainPanel::paramValue (const juce::String& id) const
{
    if (auto* v = state.getRawParameterValue (id)) return v->load();
    return 0.0f;
}

int MainPanel::paramIndex (const juce::String& id) const
{
    if (auto* p = state.getParameter (id)) return juce::roundToInt (p->convertFrom0to1 (p->getValue()));
    return 0;
}

void MainPanel::setParamIndex (const juce::String& id, int index)
{
    if (auto* p = state.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (index)));
        p->endChangeGesture();
    }
}

juce::RangedAudioParameter* MainPanel::resolve (Target t, const juce::String& id) const
{
    if (id.isEmpty()) return nullptr;
    juce::String full;
    switch (t)
    {
        case Target::Global:     full = id; break;
        case Target::Upper:      full = "upper." + id; break;
        case Target::Lower:      full = "lower." + id; break;
        case Target::Edit:       full = layerPrefix (editLayer) + "." + id; break;
        case Target::ModLayer:   full = (paramIndex ("perf.modLayer") == 1 ? "lower." : "upper.") + id; break;     // BOTH edits upper, mirrored
        case Target::PortaLayer: full = (paramIndex ("perf.portaLayer") == 1 ? "lower." : "upper.") + id; break;
    }
    auto* p = state.getParameter (full);
    jassert (p != nullptr);
    return p;
}

Tone MainPanel::toneFor (Target t) const
{
    switch (t)
    {
        case Target::Upper: return Tone::White;
        case Target::Lower: return Tone::Orange;
        case Target::Edit:  return editLayer == 0 ? Tone::White : Tone::Orange;
        case Target::Global:
        case Target::ModLayer:
        case Target::PortaLayer: return Tone::Black;
    }
    return Tone::White;
}

void MainPanel::applyTones()
{
    for (auto& b : bindings)
    {
        const auto t = toneFor (b.target);
        if (b.control->tone != t) { b.control->tone = t; b.control->repaint(); }
    }
}

void MainPanel::bindControl (ParamControl& c, Target t, const juce::String& id, const juce::String& secondaryId)
{
    bindings.push_back ({ &c, t, id, secondaryId });
    c.tone = toneFor (t);
    c.setBindings (resolve (t, id), resolve (t, secondaryId));
    c.setShift (shiftActive);
    c.onValueDisplay = [this] (ParamControl& pc, bool active) { showValue (pc, active); };
    c.onPopulateMenu = [this] (ParamControl& pc, juce::PopupMenu& m) { populateModulateMenu (pc, m); };
}

MainPanel::Binding* MainPanel::findBinding (const ParamControl* c)
{
    for (auto& b : bindings)
        if (b.control == c) return &b;
    return nullptr;
}

std::vector<ParamControl*> MainPanel::controlsFor (const juce::String& paramId, bool byPrimary) const
{
    std::vector<ParamControl*> out;
    for (const auto& b : bindings)
        if (auto* p = byPrimary ? b.control->getPrimary() : b.control->getParam())
            if (p->getParameterID() == paramId) out.push_back (b.control);
    return out;
}

juce::String MainPanel::getModulationParamId (int layer) const
{
    const auto* p = strips[static_cast<size_t> (layer)].modAmount->getParam();
    return p != nullptr ? p->getParameterID() : juce::String();
}

void MainPanel::setStripTabs (int envTab, int lfoTab)
{
    for (auto& s : strips)
    {
        if (s.rowEnvTabs != nullptr) s.rowEnvTabs->setCurrent (envTab);
        s.envTabs->setCurrent (envTab);
        s.lfoTabs->setCurrent (lfoTab);
    }
}

void MainPanel::unbindControlsIn (juce::Component& container)
{
    bindings.erase (std::remove_if (bindings.begin(), bindings.end(),
                                    [&container] (const Binding& b) { return container.isParentOf (b.control); }),
                    bindings.end());
    for (int i = static_cast<int> (owned.size()) - 1; i >= 0; --i)
        if (container.isParentOf (owned[static_cast<size_t> (i)].get()))
            owned.erase (owned.begin() + i);
}

void MainPanel::rebind (Target t)
{
    for (auto& b : bindings)
        if (b.target == t)
            b.control->setBindings (resolve (b.target, b.id), resolve (b.target, b.secondaryId));
}

void MainPanel::setEnvTab (Fader* const* faders, int tab)
{
    for (int i = 0; i < 6; ++i)
    {
        auto* b = findBinding (faders[i]);
        if (b == nullptr) continue;
        b->id = tab == 0 ? kEnv1Ids[i] : kEnv2Ids[i];
        b->control->setBindings (resolve (b->target, b->id), nullptr);
        b->control->setEnabled (b->id.isNotEmpty());
    }
}

void MainPanel::setEditLayer (int layer)
{
    layer = juce::jlimit (0, 1, layer);
    // In SINGLE mode the LAYER buttons also choose the playing layer [p.82].
    if (paramIndex ("perf.keyboardMode") == 0 && paramIndex ("perf.singleLayer") != layer)
        setParamIndex ("perf.singleLayer", layer);
    if (layer == editLayer) return;
    editLayer = layer;
    state.state.setProperty ("uiEditLayer", layer, nullptr);
    // No layer tabs any more: both rows are always live. editLayer only still steers the
    // header's LAYER buttons and which layer a pop-over opens on.
    if (layerTabs != nullptr) { layerTabs->current = layer; layerTabs->repaint(); }
    rebind (Target::Edit);
    applyTones();
    updateDynamicState();
    repaint();
}

void MainPanel::showValue (ParamControl& c, bool active)
{
    if (bubble == nullptr) return;
    if (active) bubble->show (c, c.getValueText());
    else        bubble->setVisible (false);
}

int MainPanel::layerOf (const ParamControl& c) const
{
    if (auto* p = c.getParam()) return p->getParameterID().startsWith ("lower.") ? 1 : 0;
    return editLayer;
}

void MainPanel::populateModulateMenu (ParamControl& c, juce::PopupMenu& m)
{
    auto* p = c.getParam();
    if (p == nullptr) return;
    const auto id = p->getParameterID();
    if (id.startsWith ("upper.") || id.startsWith ("lower."))
    {
        const int layer = layerOf (c);
        const int dest = matrixDestForSuffix (id.fromFirstOccurrenceOf (".", false, false));
        if (dest >= 0)
        {
            // Direct parameter mapping: pick a source, then set the amount in MODULATION [p.87].
            juce::PopupMenu sources;
            for (int s = 0; s < sg::kMatrixSources; ++s)
            {
                const bool excluded = sg::matrixExcluded (s, dest);
                const float amount = excluded ? 0.0f : paramValue (matrixId (layer, s, dest));
                juce::String text = sg::kMatrixSourceNames[static_cast<size_t> (s)];
                if (std::abs (amount) > 1.0e-4f) text << "   (" << (amount > 0 ? "+" : "") << juce::roundToInt (amount * 100.0f) << " %)";
                sources.addItem (juce::PopupMenu::Item (text).setEnabled (! excluded).setTicked (std::abs (amount) > 1.0e-4f)
                                     .setAction ([this, layer, s, dest] { setModSelection (layer, s, dest); }));
            }
            m.addSubMenu ("Modulate by (" + layerName (layer) + ")", sources);
        }
    }
    const int cc = ccForParameter (id);
    if (cc >= 0) m.addItem (juce::PopupMenu::Item ("MIDI: CC " + juce::String (cc)).setEnabled (false));
}

void MainPanel::setModSelection (int layer, int src, int dest)
{
    auto& s = strips[static_cast<size_t> (layer)];
    s.modSrc = juce::jlimit (0, sg::kMatrixSources - 1, src);
    s.modDst = juce::jlimit (0, sg::kMatrixDests - 1, dest);
    const bool excluded = sg::matrixExcluded (s.modSrc, s.modDst);
    s.modAmount->setBindings (excluded ? nullptr : state.getParameter (matrixId (layer, s.modSrc, s.modDst)));
    s.modAmount->setEnabled (! excluded);
    s.modAmount->setTooltip (excluded ? "Already wired to its own control (p.87, DD-12)" : juce::String());
    if (s.modSource != nullptr) s.modSource->repaint();
    if (s.modDest != nullptr) s.modDest->repaint();
    if (s.modSection != nullptr) s.modSection->repaint();
}

// ── state that follows other parameters ─────────────────────────────────────────────────────

void MainPanel::updateDynamicState()
{
    // Each row follows its own layer, so this runs once per row.
    for (int rl = 0; rl < 2; ++rl)
    {
        auto& rs = strips[static_cast<size_t> (rl)];
        const auto L = layerPrefix (rl) + ".";
        const bool lfoRange = paramIndex (L + "dds2.range") == 0;
        const int mode = paramIndex (L + "dds2.mode");
        rs.dds2ModeSeg->setEnabled (! lfoRange);   // NORM / RING / SYNC need DDS 2 in the audio path [p.36]
        rs.dds2SubSeg->setEnabled (lfoRange);      // sub-oscillator only with RANGE = LFO [p.39]
        rs.mixerSubSeg->setEnabled (lfoRange);
        const juce::String tuneTitle = lfoRange ? "RATE" : "TUNE";
        if (rs.dds2Tune->title != tuneTitle) { rs.dds2Tune->title = tuneTitle; rs.dds2Tune->repaint(); }

        rs.hfRouteSeg->setEnabled (paramIndex (L + "lfo1.wave") >= 4);
        rs.xmodLeds[0]->setLevel (lfoRange || mode != 2 ? 1.0f : 0.0f);
        rs.xmodLeds[1]->setLevel (! lfoRange && mode == 2 ? 1.0f : 0.0f);
        rs.ringLed->setLevel (! lfoRange && mode == 1 ? 1.0f : 0.0f);
    }

    for (int l = 0; l < 2; ++l)
    {
        auto& s = strips[static_cast<size_t> (l)];
        const auto P = layerPrefix (l) + ".";
        const bool hf = paramIndex (P + "lfo1.wave") >= 4;
        const juce::StringArray labels = hf ? juce::StringArray { "NORM", "DDS 1", "DDS 2" } : juce::StringArray { "FREE", "ONCE", "RESET" };
        if (s.lfo1ModeSeg->labels != labels) { s.lfo1ModeSeg->labels = labels; s.lfo1ModeSeg->repaint(); }
        const juce::String lr = paramValue (P + "voice.binaural") >= 0.5f ? "LR PHASE" : "SPREAD";   // p.91
        for (auto* k : s.lrPhase)
            if (k != nullptr && k->title != lr) { k->title = lr; k->repaint(); }
    }
}

void MainPanel::updateLeds (double dt)
{
    auto& b = bridge();
    const float bpm = juce::jmax (1.0f, b.bpm.load());
    blinkPhase = std::fmod (blinkPhase + dt * 2.0, 1.0);

    const float pl = b.peakL.exchange (0.0f), pr = b.peakR.exchange (0.0f);
    const float fall = static_cast<float> (std::exp (-dt * 6.0));
    meterL = juce::jmax (pl, meterL * fall);
    meterR = juce::jmax (pr, meterR * fall);
    meter->setLevels (meterL, meterR);

    // TEMPO LED flashes on the beat and decays like a real lamp [p.79]
    const double lastTempoPhase = tempoPhase;
    tempoPhase = std::fmod (tempoPhase + dt * bpm / 60.0, 1.0);
    if (tempoPhase < lastTempoPhase) tempoGlow = 1.0f;
    tempoGlow = juce::jmax (0.0f, tempoGlow - static_cast<float> (dt) * 5.0f);
    tempoLed->setLevel (tempoGlow);

    // MASTER: portamento status and the TRANSPOSED flash [pp.74–75]
    if (auto* p = resolve (Target::PortaLayer, "porta.time")) portaLed->setLevel (p->getValue() > 0.0f ? 1.0f : 0.0f);
    const bool flash = paramIndex ("global.transpose") != 0 && blinkPhase < 0.5;
    if (octaveSeg->flashOn != flash) { octaveSeg->flashOn = flash; octaveSeg->repaint(); }

    // Chorus, delay and loop indicators, once per row against that row's own layer.
    for (int rl = 0; rl < 2; ++rl)
    {
        auto& rs = strips[static_cast<size_t> (rl)];
        const auto sz = static_cast<size_t> (rl);
        const auto L = layerPrefix (rl) + ".";
        rs.chorusLed->setLevel (paramIndex (L + "fx.chorus") != 0 ? 1.0f : 0.0f);
        rs.chorusSection->repaint (0, 136, 80, 20);

        // DELAY TIME LED blinks at the delay time (or its synced note value) [p.65]
        const bool sync = paramValue (L + "arp.sync") >= 0.5f;
        const float dtv = paramValue (L + "fx.delayTime");
        const double period = sync ? sg::clockdiv::kDelayBeats[static_cast<size_t> (sg::clockdiv::index (dtv, 16))] * 60.0 / bpm
                                   : static_cast<double> (sg::taper::delayTime (dtv));
        const double lastDelayPhase = delayPhase[sz];
        delayPhase[sz] = std::fmod (delayPhase[sz] + dt / juce::jmax (0.001, period), 1.0);
        if (delayPhase[sz] < lastDelayPhase) delayGlow[sz] = 1.0f;
        delayGlow[sz] = juce::jmax (0.0f, delayGlow[sz] - static_cast<float> (dt) * 4.5f);
        rs.delayLed->setLevel (period < 0.08 ? 0.85f : delayGlow[sz]);
        rs.delaySection->repaint (0, 168, 120, 28);

        // LOOP LED: flashes on every ENV 1 loop cycle of the newest voice [p.48]
        const unsigned cycles = b.layer[sz].loopCycles.load();
        if (cycles != lastLoopCycles[sz] && paramIndex (L + "env1.mode") == 2) loopFlash[sz] = 1.0f;
        lastLoopCycles[sz] = cycles;
        loopFlash[sz] = juce::jmax (0.0f, loopFlash[sz] - static_cast<float> (dt) * 8.0f);
        rs.loopLed->setLevel (loopFlash[sz]);
    }

    for (int l = 0; l < 2; ++l)
    {
        auto& s = strips[static_cast<size_t> (l)];
        const auto P = layerPrefix (l) + ".";
        auto& f = b.layer[static_cast<size_t> (l)];
        // the LAYER lamp lights with the layer's own output and falls back down with it
        layerGlow[static_cast<size_t> (l)] = juce::jmax (juce::jmin (1.0f, f.peak.exchange (0.0f) * 3.0f),
                                                         layerGlow[static_cast<size_t> (l)] * static_cast<float> (std::exp (-dt * 3.5)));
        s.layerSection->setLedLevel (juce::jmax (f.voices.load() > 0 ? 0.35f : 0.08f, layerGlow[static_cast<size_t> (l)]));

        // LFO rate LED of the visible tab [pp.54, 70]
        double hz;
        if (s.lfoTabs->current == 0)
        {
            const int wave = paramIndex (P + "lfo1.wave");
            const float rate = paramValue (P + "lfo1.rate");
            if (wave >= 4) hz = 1000.0;   // HF: steady
            else if (paramValue (P + "arp.sync") >= 0.5f)
                hz = bpm / 60.0 / sg::clockdiv::kLfo1Beats[static_cast<size_t> (sg::clockdiv::index (rate, 16))];
            else hz = sg::taper::lfoLowHz (rate);
        }
        else
        {
            hz = sg::taper::lfoLowHz (paramValue (P + "lfo2.rate"));
        }
        s.lfoPhase = std::fmod (s.lfoPhase + dt * hz, 1.0);
        s.lfoLed->setLevel (hz > 12.0 ? 0.8f : juce::jlimit (0.0f, 1.0f, 0.5f + 0.5f * static_cast<float> (std::cos (juce::MathConstants<double>::twoPi * s.lfoPhase))));
        lfoScopePhase[static_cast<size_t> (l)] = std::fmod (lfoScopePhase[static_cast<size_t> (l)] + dt * juce::jmin (8.0, hz), 1.0);
        if (lfoScope[static_cast<size_t> (l)] != nullptr) lfoScope[static_cast<size_t> (l)]->repaint();

        s.recButton->repaint();
        s.editButton->repaint();
        s.arpSection->repaint (60, 110, 116, 30);
        s.voiceSection->repaint (100, 0, 50, 20);
        s.patch->repaint();
        s.modSection->repaint (54, 118, 94, 20);
    }
    patchBox->repaint();
    learnButton->repaint();
    for (auto* lb : layerButtons) if (lb != nullptr) lb->repaint();
    if (shiftButton != nullptr) shiftButton->repaint();
    for (auto& s : strips)
        for (juce::Component* d : std::initializer_list<juce::Component*> { s.dds1Display, s.dds2Display,
                                                                            s.envDisplay, s.filterDisplay })
            if (d != nullptr) d->repaint();

    // live screens and the modulation shown on the controls themselves
    for (juce::Component* d : std::initializer_list<juce::Component*> { scopeDisplay, statusBar })
        if (d != nullptr) d->repaint();
    updateModulationRings();
}

void MainPanel::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = juce::jlimit (0.0, 0.25, (now - lastTime) * 0.001);
    lastTime = now;

    // SHIFT: latched button, or the Shift key while the pointer is over the panel
    const bool shift = shiftLatched || (juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown() && isMouseOver (true));
    if (shift != shiftActive)
    {
        shiftActive = shift;
        for (auto& bd : bindings) bd.control->setShift (shift);
    }

    const int ml = paramIndex ("perf.modLayer"), pl = paramIndex ("perf.portaLayer");
    if (ml != lastModLayer) { lastModLayer = ml; rebind (Target::ModLayer); }
    if (pl != lastPortaLayer) { lastPortaLayer = pl; rebind (Target::PortaLayer); }

    const int learned = bridge().learnedNote.exchange (-1);
    if (learned >= 0) setParamIndex ("perf.splitPoint", learned);

    updateDynamicState();
    updateLeds (dt);
}

// ── waveform displays ───────────────────────────────────────────────────────────────────────

namespace
{
float classicShape (int shape, float p, float pw = 0.5f)   // 0 sine, 1 saw, 2 square, 3 triangle, 4 noise, 5 pulse
{
    p -= std::floor (p);
    switch (shape)
    {
        case 0: return std::sin (juce::MathConstants<float>::twoPi * p);
        case 1: return 1.0f - 2.0f * p;
        case 2: return p < 0.5f ? 1.0f : -1.0f;
        case 3: return p < 0.25f ? 4.0f * p : (p < 0.75f ? 2.0f - 4.0f * p : 4.0f * p - 4.0f);
        case 4:
        {
            const auto h = static_cast<uint32_t> (p * 97.0f) * 2654435761u;
            return static_cast<float> ((h >> 8) & 0xffff) / 32767.5f - 1.0f;
        }
        case 5: return p < pw ? 1.0f : -1.0f;
        default: return 0.0f;
    }
}

void strokeWave (juce::Graphics& g, RF r, const std::function<float (float)>& f, float alpha, float thickness)
{
    juce::Path path;
    constexpr int n = 160;
    for (int i = 0; i <= n; ++i)
    {
        const float t = static_cast<float> (i) / static_cast<float> (n);
        const float y = r.getCentreY() - r.getHeight() * 0.5f * juce::jlimit (-1.1f, 1.1f, f (t * 2.0f));   // two cycles
        if (i == 0) path.startNewSubPath (r.getX(), y); else path.lineTo (r.getX() + t * r.getWidth(), y);
    }
    g.setColour (col::lcdText.withAlpha (alpha * 0.22f));                // phosphor glow
    g.strokePath (path, juce::PathStrokeType (thickness * 2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col::lcdText.withAlpha (alpha));
    g.strokePath (path, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace

void MainPanel::drawDds1 (juce::Graphics& g, RF r, int layer)
{
    // Waveform, PWM/WAVE morph toward the next waveform (ALT: A → B) and SUPER detune smear [pp.32, 61–62].
    const auto L = layerPrefix (layer) + ".";
    const int wave = paramIndex (L + "dds1.wave");
    const float m = paramValue (L + "ddsMod.pwmWave") * (paramIndex (L + "ddsMod.pwmSource") == 0 ? 1.0f : 0.5f);
    const auto& bank = sg::AltWaveBank::factory();
    const auto a = bank.get (paramIndex (L + "dds1.altA")), b = bank.get (paramIndex (L + "dds1.altB"));
    auto source = [&] (bool second) -> std::function<float (float)>
    {
        const int idx = juce::jmin (5, wave + (second ? 1 : 0));
        if (idx == 5)
        {
            const auto t = (wave == 5 && second) ? b : a;
            return [t] (float p) { p -= std::floor (p); return t->getCycle()[static_cast<size_t> (p * sg::WaveTable::kSize)]; };
        }
        return [idx] (float p) { return classicShape (idx, p); };
    };
    const auto fa = source (false), fb = source (true);
    const auto shape = [&] (float p) { return (1.0f - m) * fa (p) + m * fb (p); };

    const int superMode = paramIndex (L + "ddsMod.super");
    if (superMode > 0)
    {
        const float spread = 0.04f * paramValue (L + "ddsMod.pwDetune") * (superMode == 2 ? 1.0f : 0.5f) + 0.004f;
        for (float k : { -3.0f, -2.0f, -1.0f, 1.0f, 2.0f, 3.0f })
            strokeWave (g, r, [&, k] (float p) { return shape (p * (1.0f + k * spread) + 0.07f * k); }, 0.22f, 0.9f);
    }
    strokeWave (g, r, shape, 1.0f, 1.5f);
}

void MainPanel::drawDds2 (juce::Graphics& g, RF r, int layer)
{
    const auto L = layerPrefix (layer) + ".";
    const int wave = paramIndex (L + "dds2.wave");
    const bool manual = paramIndex (L + "ddsMod.pwmSource") == 0;
    const float pw = 0.5f - 0.45f * juce::jlimit (0.0f, 1.0f, paramValue (L + "ddsMod.pwDetune") + (manual ? paramValue (L + "ddsMod.pwmWave") : 0.0f));   // DD-25
    strokeWave (g, r, [wave, pw] (float p) { return classicShape (wave, p, pw); }, 1.0f, 1.5f);
    const bool lfoRange = paramIndex (L + "dds2.range") == 0;
    const int mode = paramIndex (L + "dds2.mode");
    const juce::String tag = lfoRange ? "LFO" : (mode == 1 ? "RING" : (mode == 2 ? "SYNC" : ""));
    if (tag.isNotEmpty())
        drawLcdText (g, tag, r.withTrimmedTop (-5.0f).removeFromTop (11.0f).withTrimmedRight (-3.0f), 8.5f, juce::Justification::topRight, 0.6f);
}

// ── paint ───────────────────────────────────────────────────────────────────────────────────

void MainPanel::paint (juce::Graphics& g)
{
    // The instrument is drawn as an object: end cheeks, the tilted panel face, the shelf that
    // curves forward over the rolled lip, then the keybed (DD-73).
    const float fx = static_cast<float> (kCheek);
    const float fw = static_cast<float> (kFaceW);
    const float bodyBottom = static_cast<float> (kKeysY + kKeysH);

    g.fillAll (juce::Colour (0xff0c0d0e));                                  // the surface it stands on

    // contact shadow under the body
    {
        juce::ColourGradient sh (juce::Colours::black.withAlpha (0.75f), 0.0f, bodyBottom,
                                 juce::Colours::transparentBlack, 0.0f, bodyBottom + 26.0f, false);
        g.setGradientFill (sh);
        g.fillRect (RF (0.0f, bodyBottom, static_cast<float> (kWidth), 26.0f));
    }

    drawCheek (g, { 0.0f, 0.0f, fx, bodyBottom }, true);
    drawCheek (g, { static_cast<float> (kWidth - kCheek), 0.0f, fx, bodyBottom }, false);

    // ── panel face ───────────────────────────────────────────────────────────────────────
    // The chassis the printed trays are set into (DD-77).
    const RF faceR (fx, 0.0f, fw, static_cast<float> (kFaceH));
    drawMetal (g, faceR, juce::Colour (0xff1c1d1f), 0.0f);
    {
        // One soft key light across the whole face: warm where it lands at the top left, cool in
        // the shadow it leaves toward the bottom right (DD-75).
        juce::ColourGradient key (juce::Colour (0xfffff6e8).withAlpha (0.30f), faceR.getX(), 0.0f,
                                  juce::Colour (0xff2c3340).withAlpha (0.16f), faceR.getRight(), faceR.getBottom(), false);
        key.addColour (0.40, juce::Colours::transparentBlack);
        g.setGradientFill (key);
        g.fillRect (faceR);

        // the face leans back, so it loses a little more light toward the player
        juce::ColourGradient tilt (juce::Colours::transparentBlack, faceR.getCentreX(), faceR.getY(),
                                   juce::Colour (0xff2c3340).withAlpha (0.10f), faceR.getCentreX(), faceR.getBottom(), false);
        tilt.addColour (0.55, juce::Colours::transparentBlack);
        g.setGradientFill (tilt);
        g.fillRect (faceR);
    }
    // the cheeks throw a soft shadow onto the face
    for (int side = 0; side < 2; ++side)
    {
        const float edge = side == 0 ? fx : fx + fw;
        juce::ColourGradient sh (juce::Colours::black.withAlpha (0.16f), edge, 0.0f,
                                 juce::Colours::transparentBlack, edge + (side == 0 ? 22.0f : -22.0f), 0.0f, false);
        g.setGradientFill (sh);
        g.fillRect (RF (side == 0 ? edge : edge - 22.0f, 0.0f, 22.0f, faceR.getHeight()));
    }

    // ── header: a dark anodised band carrying the wordmark and the global controls ───────
    auto emboss = [&g] (const juce::String& text, RF area, float height, bool bold, juce::Colour colour,
                        juce::Justification just = juce::Justification::centredLeft)
    {
        drawText (g, text, area.translated (0.0f, 1.2f), height, just, juce::Colours::white.withAlpha (0.65f), bold);
        drawText (g, text, area.translated (0.0f, -0.4f), height, just, juce::Colours::black.withAlpha (0.10f), bold);
        drawText (g, text, area, height, just, colour, bold);
    };
    auto engraveDark = [&g] (const juce::String& text, RF area, float height, bool bold, juce::Colour colour,
                             juce::Justification just = juce::Justification::centredLeft)
    {
        drawText (g, text, area.translated (0.0f, 1.2f), height, just, juce::Colours::black.withAlpha (0.55f), bold);
        drawText (g, text, area, height, just, colour, bold);
    };

    const RF header (fx, 0.0f, fw, 86.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff34363a), header.getX(), header.getY(),
                                             juce::Colour (0xff17181a), header.getX(), header.getBottom(), false));
    g.fillRect (header);
    drawMetal (g, header.withTrimmedBottom (4.0f), juce::Colour (0xff2b2d30), 0.0f);
    {
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.10f), header.getX(), header.getY(),
                                    juce::Colours::transparentBlack, header.getX() + fw * 0.55f, header.getBottom(), false);
        g.setGradientFill (sheen);
        g.fillRect (header);
    }
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawHorizontalLine (0, header.getX(), header.getRight());
    g.setColour (col::accent);                                               // orange rule under the band
    g.fillRect (RF (fx, 82.0f, fw, 2.0f));
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRect (RF (fx, 84.0f, fw, 2.0f));

    const auto maker = u8 (brand::kMaker);
    engraveDark (maker, { fx + 26.0f, 8.0f, 300.0f, 22.0f }, 19.0f, true, juce::Colour (0xffd8d9db));
    const float productW = juce::GlyphArrangement::getStringWidth (uiFont (40.0f, true), brand::kProduct);
    engraveDark (brand::kProduct, { fx + 24.0f, 28.0f, productW + 12.0f, 44.0f }, 40.0f, true, juce::Colours::white);
    {
        const float rx = fx + 30.0f + productW + 14.0f;
        g.setColour (col::accent);
        g.fillRect (RF (rx, 34.0f, 2.4f, 32.0f));
        engraveDark (brand::kSubtitle, { rx + 12.0f, 34.0f, 300.0f, 16.0f }, 10.5f, true, juce::Colour (0xffc7c8cb));
        engraveDark (u8 ("20 VOICES  \xc2\xb7  DUAL LAYER  \xc2\xb7  DDS + ANALOGUE"), { rx + 12.0f, 50.0f, 300.0f, 16.0f }, 9.0f, false,
                     juce::Colour (0xff8e9094));
    }

    // ── printed rows (DD-77) ─────────────────────────────────────────────────────────────
    // The panel is banded, as it is on the instrument: each row is a shallow tray with a fine
    // dark rule around it, and the sections inside are divided by vermillion lines, not by gaps.
    auto band = [&g, fx, fw] (float y, float h)
    {
        const RF r (fx + 20.0f, y, fw - 40.0f, h);
        g.setColour (juce::Colours::black.withAlpha (0.55f));                       // set into the chassis
        g.fillRoundedRectangle (r.expanded (1.6f), 5.5f);
        {
            juce::Graphics::ScopedSaveState state (g);
            juce::Path clip;
            clip.addRoundedRectangle (r, 4.5f);
            g.reduceClipRegion (clip);
            drawMetal (g, r, col::panel, 0.0f);
        }
        g.setColour (juce::Colours::white.withAlpha (0.8f));                        // top catch-light
        g.drawHorizontalLine (juce::roundToInt (r.getY() + 1.0f), r.getX() + 5.0f, r.getRight() - 5.0f);
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.5f, 1.0f);
    };
    // One tray per layer row, each with its own band beneath it (UPPER over LOWER).
    band (static_cast<float> (kRowY)   - 6.0f, static_cast<float> (kRowH)  + 12.0f);
    band (static_cast<float> (kBand1Y) - 6.0f, static_cast<float> (kBandH) + 12.0f);
    band (static_cast<float> (kRow2Y)  - 6.0f, static_cast<float> (kRowH)  + 12.0f);
    band (static_cast<float> (kBand2Y) - 6.0f, static_cast<float> (kBandH) + 12.0f);

    // Both rows are always live now, so the old "top row edits the selected layer" note is
    // gone; the status bar already carries the right-click / shift guidance.

    // panel fixing screws, at the four corners only
    for (float y : { 16.0f, static_cast<float> (kFaceH) - 16.0f })
        for (float x : { fx + 10.0f, fx + fw - 10.0f })
            drawScrew (g, { x, y }, 4.0f);

    // ── shelf: the panel curves forward toward the player ────────────────────────────────
    const RF shelf (fx, static_cast<float> (kShelfY), fw, static_cast<float> (kShelfH));
    drawMetal (g, shelf, juce::Colour (0xff1c1d1f), 0.0f);                  // chassis under the tray
    band (shelf.getY() + 6.0f, shelf.getHeight() - 12.0f);
    {
        juce::ColourGradient curve (juce::Colours::white.withAlpha (0.16f), shelf.getCentreX(), shelf.getY(),
                                    juce::Colours::black.withAlpha (0.16f), shelf.getCentreX(), shelf.getBottom(), false);
        curve.addColour (0.46, juce::Colours::transparentBlack);
        g.setGradientFill (curve);
        g.fillRect (shelf);
    }

    // nameplate, engraved into the shelf at the right
    {
        const RF plate (fx + fw - 276.0f, shelf.getY() + 22.0f, 250.0f, 52.0f);
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.fillRoundedRectangle (plate.translated (0.0f, 1.6f), 4.0f);
        drawMetal (g, plate, col::panel.brighter (0.05f), 0.6f);
        drawEngravedFrame (g, plate, 4.0f);
        emboss (u8 (brand::kMaker), plate.withHeight (18.0f).translated (0.0f, 5.0f), 12.5f, true, col::text.withAlpha (0.85f),
                juce::Justification::centred);
        emboss (brand::kProduct, plate.withTrimmedTop (20.0f), 21.0f, true, col::text, juce::Justification::centred);
        g.setColour (col::accent);
        g.fillRect (RF (plate.getCentreX() - 30.0f, plate.getBottom() - 8.0f, 60.0f, 1.6f));
    }

    // shelf screws, at the cheeks
    for (float x : { fx + 10.0f, fx + fw - 10.0f })
        drawScrew (g, { x, shelf.getCentreY() }, 4.2f);

    // ── rolled front lip, then the keybed ────────────────────────────────────────────────
    drawLip (g, { fx, static_cast<float> (kLipY), fw, static_cast<float> (kLipH) });
    drawKeybed (g, { fx, static_cast<float> (kKeysY), fw, static_cast<float> (kKeysH) });

    // ── the photographed key light over the whole instrument (DD-75) ─────────────────────
    if (const auto& light = art::shading(); light.isValid())
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (0.15f);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (light, RF (fx, 86.0f, fw, static_cast<float> (kKeysY + kKeysH) - 86.0f));
    }

    // ── vignette: one light source, so the far corners fall away ─────────────────────────
    {
        juce::ColourGradient v (juce::Colours::transparentBlack, fx + fw * 0.26f, 110.0f,
                                juce::Colour (0xff2b2620).withAlpha (0.13f), fx + fw * 0.26f, 110.0f, true);
        v.point2 = { fx + fw * 1.12f, static_cast<float> (kLipY) };
        v.addColour (0.60, juce::Colours::transparentBlack);
        g.setGradientFill (v);
        g.fillRect (RF (fx, 0.0f, fw, static_cast<float> (kLipY)));
    }
}

// ── pop-overs ───────────────────────────────────────────────────────────────────────────────

void MainPanel::openPopover (Popover which, int layer)
{
    closePopover();
    switch (which)
    {
        case Popover::Matrix:    popover = std::make_unique<MatrixPopover> (*this, layer); break;
        case Popover::Sequencer: popover = std::make_unique<SequencerPopover> (*this, layer); break;
        case Popover::AltWaves:  popover = std::make_unique<AltWavePopover> (*this, layer); break;
        case Popover::Settings:  popover = std::make_unique<SettingsPopover> (*this); break;
        case Popover::None:      return;
    }
    popoverKind = which;
    addAndMakeVisible (*popover);
    popover->setBounds (getLocalBounds());
    if (bubble != nullptr) bubble->toFront (false);
}

void MainPanel::closePopover()
{
    if (popover == nullptr) return;
    unbindControlsIn (*popover);
    popover.reset();
    popoverKind = Popover::None;
}

juce::StringArray MainPanel::getReachableParameterIds()
{
    juce::StringArray ids;
    auto collect = [&]
    {
        for (const auto& b : bindings)
            for (const auto& id : { b.id, b.secondaryId })
            {
                if (id.isEmpty()) continue;
                if (b.target == Target::Edit || b.target == Target::ModLayer || b.target == Target::PortaLayer)
                {
                    ids.addIfNotAlreadyThere ("upper." + id);   // these follow a layer selector: either layer
                    ids.addIfNotAlreadyThere ("lower." + id);
                }
                else if (auto* p = resolve (b.target, id))
                {
                    ids.addIfNotAlreadyThere (p->getParameterID());
                }
            }
    };

    for (int tab = 0; tab < 2; ++tab)   // every envelope tab
    {
        setStripTabs (tab, 0);
        collect();
    }
    setStripTabs (0, 0);

    for (auto kind : { Popover::Matrix, Popover::Sequencer, Popover::AltWaves, Popover::Settings })
        for (int layer = 0; layer < 2; ++layer)
        {
            openPopover (kind, layer);
            collect();
            closePopover();
        }

    // The LAYER buttons choose the playing layer in SINGLE mode [p.82].
    ids.addIfNotAlreadyThere ("perf.singleLayer");
    return ids;
}

} // namespace sgui
