// UI check without a host: builds the editor, renders the panel and each pop-over to PNG, drives
// controls with synthesised mouse events, and fails if any host parameter can't be reached from
// a control ("no dead or missing controls").
// Usage: SGUiSnapshot <output folder> [scale]

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "ui/Popovers.h"
#include "ui/Artwork.h"

#include <iostream>

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << what << std::endl;
    if (! ok) ++failures;
}

juce::MouseEvent mouse (juce::Component& c, juce::Point<float> pos, juce::Point<float> down,
                        juce::ModifierKeys mods = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier))
{
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), pos, mods,
                             juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation,
                             juce::MouseInputSource::defaultRotation, juce::MouseInputSource::defaultTiltX,
                             juce::MouseInputSource::defaultTiltY, &c, &c, now, down, now, 1, pos != down);
}

void drag (juce::Component& c, juce::Point<float> from, juce::Point<float> to)
{
    c.mouseDown (mouse (c, from, from));
    c.mouseDrag (mouse (c, to, from));
    c.mouseUp (mouse (c, to, from));
}

void click (juce::Component& c, juce::Point<float> at) { drag (c, at, at); }

float value (SuperGeminiProcessor& p, const juce::String& id) { return p.apvts.getParameter (id)->getValue(); }
int index (SuperGeminiProcessor& p, const juce::String& id)
{
    auto* prm = p.apvts.getParameter (id);
    return juce::roundToInt (prm->convertFrom0to1 (prm->getValue()));
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File out (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                   : juce::File::getCurrentWorkingDirectory());
    out.createDirectory();
    const float scale = argc > 2 ? static_cast<float> (std::atof (argv[2])) : 1.0f;

    // Panel artwork: report what was cut out of each sheet, and write the parts next to the
    // screenshots so a bad cut is obvious.
    for (int i = 0; i < 3; ++i)
    {
        const auto& knob = sgui::art::knob (static_cast<sgui::Tone> (i));
        std::cout << "  art  knob " << i << (knob.isValid() ? " valid " : " INVALID ")
                  << knob.getWidth() << " x " << knob.getHeight() << std::endl;
        if (! knob.isValid()) continue;
        const auto file = out.getChildFile ("art_knob" + juce::String (i) + ".png");
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat().writeImageToStream (knob, stream);
    }

    SuperGeminiProcessor proc;
    proc.prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (proc.getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;
    auto runAudio = [&] { buffer.clear(); proc.processBlock (buffer, midi); };

    {
        std::unique_ptr<juce::AudioProcessorEditor> base (proc.createEditor());
        auto* editor = dynamic_cast<SuperGeminiEditor*> (base.get());
        check (editor != nullptr, "the plugin opens the Geminus panel");
        if (editor == nullptr) return 1;
        editor->setUiScale (1.0f);
        auto& panel = editor->getPanel();
        using P = sgui::MainPanel::Popover;

        // ── pictures ──
        auto save = [&] (const juce::String& name)
        {
            const auto image = panel.createComponentSnapshot (panel.getLocalBounds(), true, scale);
            const auto file = out.getChildFile (name);
            file.deleteFile();
            juce::FileOutputStream stream (file);
            juce::PNGImageFormat png;
            png.writeImageToStream (image, stream);
        };
        save ("panel.png");
        panel.setEditLayer (1);
        save ("panel_lower.png");
        panel.setEditLayer (0);
        panel.setStripTabs (1, 1);
        save ("panel_env2_lfo2.png");
        panel.setStripTabs (0, 0);
        const std::pair<P, const char*> pops[] { { P::Matrix, "matrix.png" }, { P::Sequencer, "sequencer.png" },
                                                 { P::AltWaves, "altwaves.png" }, { P::Settings, "settings.png" } };
        for (const auto& [kind, name] : pops)
        {
            panel.openPopover (kind, 0);
            save (name);
            panel.closePopover();
        }
        std::cout << "  pictures in " << out.getFullPathName() << std::endl;

        // ── every parameter has a control ──
        const auto reachable = panel.getReachableParameterIds();
        int total = 0, missing = 0;
        for (auto* p : proc.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                ++total;
                if (! reachable.contains (rp->getParameterID())) { std::cout << "        no control: " << rp->getParameterID() << std::endl; ++missing; }
            }
        check (missing == 0, "all " + juce::String (total) + " parameters reachable from a control (" + juce::String (missing) + " missing)");

        auto first = [&] (const juce::String& id) -> sgui::ParamControl* { auto v = panel.controlsFor (id); return v.empty() ? nullptr : v.front(); };

        // ── interaction ──
        if (auto* lpf = first ("upper.vcf.lpf"))
        {
            const auto c = lpf->getLocalBounds().getCentre().toFloat();
            drag (*lpf, c, c.translated (0.0f, 100.0f));
            check (std::abs (value (proc, "upper.vcf.lpf") - 0.5f) < 0.02f, "dragging LPF down 100 px halves it: " + juce::String (value (proc, "upper.vcf.lpf"), 3));
        }
        else check (false, "LPF knob found");

        panel.setEditLayer (1);
        check (panel.controlsFor ("lower.vcf.lpf").size() >= 2, "LOWER selected: the top-row LPF follows the lower layer");
        check (first ("lower.vcf.lpf") != nullptr && first ("lower.vcf.lpf")->tone == sgui::Tone::Orange, "LOWER selected: the top row turns to orange knobs");
        check (index (proc, "perf.singleLayer") == 1, "in SINGLE, selecting LOWER makes the lower layer play [p.82]");
        panel.setEditLayer (0);
        check (first ("upper.vcf.lpf")->tone == sgui::Tone::White, "UPPER selected: white knobs");

        if (auto* kb = first ("perf.keyboardMode"))
        {
            click (*kb, { static_cast<float> (kb->getWidth()) * 0.5f, static_cast<float> (kb->getHeight()) * 0.5f });
            check (index (proc, "perf.keyboardMode") == 1, "KEYBOARD MODE: clicking DUAL selects it");
            click (*kb, { 6.0f, 6.0f });
        }

        if (auto* ch = first ("upper.fx.chorus"))
        {
            click (*ch, { static_cast<float> (ch->getWidth()) * 0.5f, 8.0f });                                          // CHORUS I
            check (index (proc, "upper.fx.chorus") == 1, "CHORUS I lit = mode I");
            click (*ch, { static_cast<float> (ch->getWidth()) * 0.5f, static_cast<float> (ch->getHeight()) - 8.0f });   // + CHORUS II
            check (index (proc, "upper.fx.chorus") == 3, "CHORUS I + II lit = mode I+II [p.64]");
        }

        if (auto* uni = first ("upper.voice.unison"))
        {
            click (*uni, { 8.0f, static_cast<float> (uni->getHeight()) * 0.5f });
            check (value (proc, "upper.voice.unison") > 0.5f, "UNISON button toggles unison");
        }

        // SHIFT: PW/DETUNE (inverse legend DRIFT) edits DRIFT [p.112]
        panel.setShiftLatched (true);
        {
            sgui::ParamControl* pw = nullptr;
            for (auto* c : panel.controlsFor ("upper.ddsMod.pwDetune", true))
                if (c->hasSecondary()) { pw = c; break; }
            const float before = value (proc, "upper.ddsMod.pwDetune");
            if (pw != nullptr)
            {
                const auto c = pw->getLocalBounds().getCentre().toFloat();
                drag (*pw, c, c.translated (0.0f, -60.0f));
            }
            check (pw != nullptr && value (proc, "upper.ddsMod.drift") > 0.25f && value (proc, "upper.ddsMod.pwDetune") == before,
                   "with SHIFT, the PW/DETUNE knob sets DRIFT and leaves PW/DETUNE alone");
        }
        panel.setShiftLatched (false);

        proc.apvts.getParameter ("upper.vcf.res")->setValueNotifyingHost (0.7f);
        auto* res = first ("upper.vcf.res");
        check (res != nullptr && std::abs (res->getNorm() - 0.7f) < 1.0e-4f, "automation moves the RES knob");

        proc.apvts.getParameter ("perf.portaLayer")->setValueNotifyingHost (proc.apvts.getParameter ("perf.portaLayer")->convertTo0to1 (1.0f));
        panel.refresh();
        check (panel.controlsFor ("lower.porta.time").size() >= 2 && ! panel.controlsFor ("lower.octave").empty(),
               "PORTAMENTO layer = LOWER: GLIDE and OCTAVE act on the lower layer");

        panel.selectModulation (0, 2, 4);
        check (panel.getModulationParamId (0) == "upper.mtx.env1.hpf", "MODULATION ENV 1 > HPF edits " + panel.getModulationParamId (0));
        panel.selectModulation (0, 1, 9);
        check (panel.getModulationParamId (0).isEmpty(), "LFO 2 > LPF cutoff is not offered (hard-wired, p.87)");

        {
            panel.openPopover (P::Sequencer, 0);
            if (auto* seqPop = dynamic_cast<sgui::SequencerPopover*> (panel.getOpenPopover()))
            {
                seqPop->editStep (3, [] (sg::SeqStep& s) { s.count = 2; s.notes[0] = 60; s.notes[1] = 64; s.accent = true; });
                seqPop->setLength (8);
                runAudio();
                const auto& w = proc.getEngine().layer (0).getArpSeq().working();
                check (w.steps[3].count == 2 && w.steps[3].notes[1] == 64 && w.steps[3].accent && w.length == 8,
                       "step editor: notes, ACCENT and LENGTH reach the engine");
                sg::Sequence snap;
                proc.getUiBridge().readSequence (0, snap);
                check (snap.steps[3].count == 2 && snap.length == 8, "the engine publishes the edited sequence back to the editor");
            }
            else check (false, "sequencer pop-over found");
            panel.closePopover();
        }

        {
            UiCommand c;
            c.type = UiCommand::Type::RibbonTouch; c.f = 0.2f; proc.getUiBridge().push (c);
            c.type = UiCommand::Type::RibbonMove;  c.f = 0.7f; proc.getUiBridge().push (c);
            runAudio();
            check (std::abs (proc.getEngine().layer (0).getMods().ribbonSemis) > 0.1f, "dragging the on-screen ribbon bends pitch");
            c.type = UiCommand::Type::RibbonRelease; proc.getUiBridge().push (c);
            runAudio();
        }

        proc.getUiBridge().learnSplit.store (true);
        midi.addEvent (juce::MidiMessage::noteOn (1, 67, 0.8f), 0);
        runAudio();
        midi.clear();
        panel.refresh();
        check (index (proc, "perf.splitPoint") == 67, "LEARN: the next note (G4) becomes the split point");

        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);
        runAudio();
        midi.clear();
        for (int i = 0; i < 8; ++i) runAudio();
        check (proc.getUiBridge().peakL.load() > 0.01f, "output meter receives the level");

        // Derived from the panel's own size so a layout change does not need this edited.
        const int baseW = sgui::MainPanel::kWidth, baseH = sgui::MainPanel::kHeight;
        editor->setUiScale (0.5f);
        check (editor->getWidth() == baseW / 2 && editor->getHeight() == baseH / 2,
               "resize to 50 %: " + juce::String (editor->getWidth()) + " x " + juce::String (editor->getHeight()));
        editor->setUiScale (2.0f);
        check (editor->getWidth() == baseW * 2 && editor->getHeight() == baseH * 2,
               "resize to 200 %: " + juce::String (editor->getWidth()) + " x " + juce::String (editor->getHeight()));
    }
    proc.releaseResources();
    std::cout << (failures == 0 ? "UI CHECK PASSED" : "UI CHECK FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
