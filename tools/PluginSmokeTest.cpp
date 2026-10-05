// Headless host smoke test: loads the built plugin through JUCE's VST3 hosting, plays notes,
// and checks the output is audible, finite and silent again after release.
// Usage: SGPluginSmoke "<path to Geminus.vst3>"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

#include <cmath>
#include <iostream>

namespace
{
int failures = 0;

void check (bool ok, const juce::String& what)
{
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << what.toStdString() << std::endl;
    if (! ok) ++failures;
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File file (argc > 1 ? juce::String (argv[1]) : juce::String());
    std::cout << "Plugin: " << file.getFullPathName().toStdString() << std::endl;

    juce::AudioPluginFormatManager formats;
    formats.addFormat (std::make_unique<juce::VST3PluginFormat>());

    juce::OwnedArray<juce::PluginDescription> types;
    formats.getFormat (0)->findAllTypesForFile (types, file.getFullPathName());
    check (types.size() == 1, "VST3 scan finds exactly one plugin");
    if (types.isEmpty()) return 1;

    const auto& desc = *types[0];
    std::cout << "  name=" << desc.name << "  vendor=" << desc.manufacturerName
              << "  instrument=" << (desc.isInstrument ? "yes" : "no") << std::endl;
    check (desc.isInstrument, "registers as an instrument");

    juce::String error;
    auto plugin = formats.createPluginInstance (desc, 48000.0, 512, error);
    check (plugin != nullptr, "instantiates" + (error.isEmpty() ? juce::String() : " (" + error + ")"));
    if (plugin == nullptr) return 1;

    std::cout << "  parameters=" << plugin->getParameters().size()
              << "  output buses=" << plugin->getBusCount (false) << std::endl;
    check (plugin->getParameters().size() > 50, "exposes its parameters");

    plugin->enableAllBuses();
    plugin->prepareToPlay (48000.0, 512);

    const int channels = plugin->getTotalNumOutputChannels();
    juce::AudioBuffer<float> buffer (channels, 512);
    juce::MidiBuffer midi;

    double heldEnergy = 0.0, tailPeak = 0.0;
    bool finite = true;
    const int blocks = static_cast<int> (2.0 * 48000 / 512);
    for (int b = 0; b < blocks; ++b)
    {
        midi.clear();
        if (b == 0)
            for (int note : { 48, 55, 60, 64 })
                midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);
        if (b == blocks / 2)
            for (int note : { 48, 55, 60, 64 })
                midi.addEvent (juce::MidiMessage::noteOff (1, note), 0);

        buffer.clear();
        plugin->processBlock (buffer, midi);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
            {
                const float v = buffer.getSample (ch, i);
                finite = finite && std::isfinite (v);
                if (b > 4 && b < blocks / 2) heldEnergy += static_cast<double> (v) * v;
                if (b > blocks / 2 + 20)     tailPeak = std::max (tailPeak, static_cast<double> (std::abs (v)));
            }
    }
    const double heldRms = std::sqrt (heldEnergy / ((blocks / 2 - 5) * 512.0 * 2.0));
    std::cout << "  chord RMS=" << heldRms << "  peak after release=" << tailPeak << std::endl;
    check (finite, "output is finite");
    check (heldRms > 0.02, "chord is audible on the main output");
    check (tailPeak < 1.0e-3, "silent after release");

    // State round trip.
    juce::MemoryBlock state;
    plugin->getStateInformation (state);
    auto* lpf = plugin->getParameters()[0];
    for (auto* p : plugin->getParameters())
        if (p->getName (64).containsIgnoreCase ("LPF")) { lpf = p; break; }
    lpf->setValueNotifyingHost (0.25f);
    plugin->setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    check (std::abs (lpf->getValue() - 1.0f) < 1.0e-4f, "state save / restore (" + lpf->getName (64) + ")");

    // MIDI CC 74 on the base channel (upper layer) drives VCF cutoff [manual p.119].
    midi.clear();
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 32), 0);
    for (int b = 0; b < 4; ++b)
    {
        buffer.clear();
        plugin->processBlock (buffer, midi);
        midi.clear();
    }
    std::cout << "  " << lpf->getName (64) << " after CC 74 = 32: " << lpf->getValue() << std::endl;
    check (std::abs (lpf->getValue() - 32.0f / 127.0f) < 0.01f, "CC 74 reaches the upper LPF parameter");

    // Cutoff changes must affect a voice that is already held, without a new note-on.
    lpf->setValueNotifyingHost (1.0f);
    midi.clear();
    midi.addEvent (juce::MidiMessage::noteOn (1, 69, 0.8f), 0);
    double openEnergy = 0.0, closedEnergy = 0.0;
    for (int b = 0; b < 120; ++b)
    {
        if (b == 60) lpf->setValueNotifyingHost (0.0f);
        buffer.clear();
        plugin->processBlock (buffer, midi);
        midi.clear();
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const double sample = buffer.getSample (ch, i);
                if (b >= 30 && b < 60) openEnergy += sample * sample;
                if (b >= 90) closedEnergy += sample * sample;
            }
    }
    check (openEnergy > 0.001 && closedEnergy < openEnergy * 0.25,
           "filter edits affect an already-playing note without retriggering");
    midi.addEvent (juce::MidiMessage::allNotesOff (1), 0);
    plugin->processBlock (buffer, midi);

    // FX rack: ECHO DELAY in FX 1 must reach the audio - a short note keeps echoing after release.
    {
        juce::AudioProcessorParameter* fxType = nullptr;
        for (auto* p : plugin->getParameters())
            if (p->getName (64) == "FX 1 Type") fxType = p;
        check (fxType != nullptr, "FX 1 Type parameter exists");
        lpf->setValueNotifyingHost (1.0f);   // the filter test above left the LPF closed
        // no message loop here, so the processor's timer never applies the type's defaults: set them
        for (auto* p : plugin->getParameters())
        {
            const auto n = p->getName (64);
            if (n == "FX 1 Param 1") p->setValueNotifyingHost (0.5f);        // TIME
            if (n == "FX 1 Param 2") p->setValueNotifyingHost (0.5f);        // FEEDBACK 50 %
            if (n == "FX 1 Param 9") p->setValueNotifyingHost (0.0f);        // MODE: STEREO
            if (n == "FX 1 Dry/Wet") p->setValueNotifyingHost (0.5f);
        }
        auto echoTail = [&] (float typeValue)
        {
            fxType->setValueNotifyingHost (typeValue);
            buffer.clear(); midi.clear();
            plugin->processBlock (buffer, midi);    // hosted VST3: parameter changes arrive with a block
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);   // the processor timer builds the effect
            double tail = 0.0, body = 0.0;
            for (int b = 0; b < 140; ++b)
            {
                midi.clear();
                if (b == 2) midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.9f), 0);
                if (b == 6) midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
                buffer.clear();
                plugin->processBlock (buffer, midi);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < buffer.getNumSamples(); ++i)
                    {
                        const double s = buffer.getSample (ch, i);
                        finite = finite && std::isfinite (s);
                        if (b >= 60) tail += s * s;
                        if (b >= 2 && b < 7) body += s * s;
                    }
            }
            std::cout << "  note energy=" << body << std::endl;
            return tail;
        };
        // the type is a choice: index 3 = ECHO DELAY, out of (number of choices - 1) steps
        const float steps = static_cast<float> (fxType->getNumSteps() - 1);
        const double dry = echoTail (0.0f), echo = echoTail (3.0f / steps);   // NONE, then ECHO DELAY
        std::cout << "  tail energy dry=" << dry << "  with ECHO DELAY=" << echo << std::endl;
        check (finite && echo > dry * 10.0 + 1.0e-4, "FX 1 ECHO DELAY is heard after the note ends");
        echoTail (0.0f);
    }

    // DDS 1 CUSTOM: a saved state carrying a silent WAV with CUSTOM on replaces the saw with the
    // sample (the note goes quiet), and the sample survives the next save.
    {
        // a mono 16-bit 44.1 kHz WAV of 'frames' samples from f(i)
        auto makeWav = [] (int frames, auto f)
        {
            juce::MemoryOutputStream wav;
            auto u32 = [&wav] (int v) { wav.writeInt (v); };
            wav.write ("RIFF", 4); u32 (36 + frames * 2); wav.write ("WAVEfmt ", 8); u32 (16);
            wav.writeShort (1); wav.writeShort (1); u32 (44100); u32 (88200); wav.writeShort (2); wav.writeShort (16);
            wav.write ("data", 4); u32 (frames * 2);
            for (int i = 0; i < frames; ++i) wav.writeShort (static_cast<short> (juce::jlimit (-1.0f, 1.0f, f (i)) * 32767.0f));
            return wav.getMemoryBlock();
        };
        const auto silence = makeWav (22050, [] (int) { return 0.0f; });

        auto noteEnergy = [&] (float velocity = 0.8f)
        {
            double e = 0.0;
            midi.clear();
            midi.addEvent (juce::MidiMessage::noteOn (1, 69, velocity), 0);
            for (int b = 0; b < 40; ++b)
            {
                buffer.clear();
                plugin->processBlock (buffer, midi);
                midi.clear();
                if (b >= 10) for (int i = 0; i < buffer.getNumSamples(); ++i) e += buffer.getSample (0, i) * buffer.getSample (0, i);
            }
            midi.addEvent (juce::MidiMessage::noteOff (1, 69), 0);
            for (int b = 0; b < 200; ++b) { buffer.clear(); plugin->processBlock (buffer, midi); midi.clear(); }
            return e;
        };
        // A hosted VST3 hands back JUCE's wrapper; the plugin's own state is base64 in <IComponent>.
        auto inner = [] (const juce::MemoryBlock& hostState)
        {
            auto wrap = juce::AudioProcessor::getXmlFromBinary (hostState.getData(), static_cast<int> (hostState.getSize()));
            juce::MemoryBlock b;
            if (wrap == nullptr || wrap->getChildByName ("IComponent") == nullptr
                || ! b.fromBase64Encoding (wrap->getChildByName ("IComponent")->getAllSubText())) return std::unique_ptr<juce::XmlElement>();
            return juce::AudioProcessor::getXmlFromBinary (b.getData(), static_cast<int> (b.getSize()));
        };
        juce::MemoryBlock base;
        plugin->getStateInformation (base);
        if (inner (base) == nullptr) { check (false, "plugin state readable through the VST3 wrapper"); return 1; }
        // the base state with CUSTOM on for the upper layer, carrying this WAV
        auto stateWith = [&] (const juce::MemoryBlock& wav, const juce::String& name)
        {
            auto xml = inner (base);
            for (auto* p : xml->getChildWithTagNameIterator ("PARAM"))
                if (p->getStringAttribute ("id") == "upper.dds1.smpOn") p->setAttribute ("value", 1.0);
            if (auto* old = xml->getChildByName ("CUSTOM")) xml->removeChildElement (old, true);
            auto* sample = xml->createNewChildElement ("CUSTOM")->createNewChildElement ("SAMPLE");
            sample->setAttribute ("layer", 0);
            sample->setAttribute ("name", name);
            sample->setAttribute ("data", wav.toBase64Encoding());
            juce::MemoryBlock pluginState, out;
            juce::AudioProcessor::copyXmlToBinary (*xml, pluginState);
            juce::XmlElement host ("VST3PluginState");
            host.createNewChildElement ("IComponent")->addTextElement (pluginState.toBase64Encoding());
            juce::AudioProcessor::copyXmlToBinary (host, out);
            return out;
        };
        const auto withSample = stateWith (silence, "SILENCE");

        const double saw = noteEnergy();
        plugin->setStateInformation (withSample.getData(), static_cast<int> (withSample.getSize()));
        const double custom = noteEnergy();
        std::cout << "  note energy saw=" << saw << "  silent CUSTOM sample=" << custom << std::endl;
        check (saw > 1.0 && custom < saw * 1.0e-4, "DDS 1 CUSTOM plays the sample from a loaded state");

        juce::MemoryBlock again;
        plugin->getStateInformation (again);
        auto back = inner (again);
        const auto* kept = back != nullptr && back->getChildByName ("CUSTOM") != nullptr
                         ? back->getChildByName ("CUSTOM")->getChildByAttribute ("layer", "0") : nullptr;
        check (kept != nullptr && kept->getStringAttribute ("name") == "SILENCE"
               && kept->getStringAttribute ("data") == silence.toBase64Encoding(), "the CUSTOM sample is saved with the state");

        // Unity level, like FL's sampler: a sine at half scale (-6 dBFS) in the file comes out of the
        // plugin at -6 dBFS (one note at full velocity - DYNAMICS is on in the init patch - default VCA
        // LEVEL and MASTER, BINAURAL: the left voice feeds L).
        const auto half = makeWav (88200, [] (int i) { return 0.5f * std::sin (6.2831853f * 220.0f * static_cast<float> (i) / 44100.0f); });
        const auto withHalf = stateWith (half, "HALF");
        plugin->setStateInformation (withHalf.getData(), static_cast<int> (withHalf.getSize()));
        const double outRms = std::sqrt (noteEnergy (1.0f) / (30.0 * buffer.getNumSamples()));
        const double db = 20.0 * std::log10 (outRms / (0.5 / std::sqrt (2.0)));
        std::cout << "  -6 dBFS file comes out at " << db << " dB relative to the file" << std::endl;
        check (std::abs (db) < 1.0, "a dropped file plays at its own level");
        plugin->setStateInformation (base.getData(), static_cast<int> (base.getSize()));
    }

    // The panel opens through the VST3 view interface, at the design size or a scaled copy of it.
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (plugin->createEditorIfNeeded());
        check (editor != nullptr, "editor opens in the host");
        if (editor != nullptr)
        {
            const double ratio = static_cast<double> (editor->getWidth()) / juce::jmax (1, editor->getHeight());
            std::cout << "  editor size " << editor->getWidth() << " x " << editor->getHeight() << std::endl;
            const bool knownLayout = std::abs (ratio - 3400.0 / 1330.0) < 0.02
                                  || std::abs (ratio - 2028.0 / 1152.0) < 0.02;
            check (knownLayout && editor->getWidth() >= 1190, "editor keeps the panel or desktop layout aspect ratio");
        }
    }

    // Separate filter banks survive mode changes and real VST3 state round-trips.
    {
        auto named = [&] (const juce::String& name) -> juce::AudioProcessorParameter*
        {
            for (auto* p : plugin->getParameters()) if (p->getName (64) == name) return p;
            return nullptr;
        };
        auto* tw = named ("upper 3W tw.cutoff");
        auto* style = named ("Upper VCF Style");
        check (tw != nullptr && style != nullptr, "independent 3W parameters exposed by VST3");
        if (tw && style)
        {
            auto flush = [&] { midi.clear(); buffer.clear(); plugin->processBlock (buffer, midi); };
            lpf->setValueNotifyingHost (0.23f);
            tw->setValueNotifyingHost (0.78f);
            style->setValueNotifyingHost (1.0f);
            flush();
            juce::MemoryBlock saved;
            plugin->getStateInformation (saved);
            lpf->setValueNotifyingHost (0.9f); tw->setValueNotifyingHost (0.1f); flush();
            plugin->setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
            flush();
            check (std::abs (lpf->getValue() - 0.23f) < 0.001f && std::abs (tw->getValue() - 0.78f) < 0.001f,
                   "SG and 3W cutoffs recall independently");
            style->setValueNotifyingHost (0.0f); flush();
            check (std::abs (lpf->getValue() - 0.23f) < 0.001f && std::abs (tw->getValue() - 0.78f) < 0.001f,
                   "switching back to SG preserves both banks");

            // Remove the new IDs to simulate a patch saved before independent banks existed.
            auto wrap = juce::AudioProcessor::getXmlFromBinary (saved.getData(), static_cast<int> (saved.getSize()));
            auto* component = wrap ? wrap->getChildByName ("IComponent") : nullptr;
            juce::MemoryBlock data;
            if (component && data.fromBase64Encoding (component->getAllSubText()))
            {
                auto xml = juce::AudioProcessor::getXmlFromBinary (data.getData(), static_cast<int> (data.getSize()));
                if (xml)
                {
                    for (int i = xml->getNumChildElements(); --i >= 0;)
                    {
                        auto* child = xml->getChildElement (i);
                        if (child->getStringAttribute ("id").contains (".tw.")) xml->removeChildElement (child, true);
                    }
                    juce::AudioProcessor::copyXmlToBinary (*xml, data);
                    component->deleteAllTextElements(); component->addTextElement (data.toBase64Encoding());
                    juce::AudioProcessor::copyXmlToBinary (*wrap, data);
                    plugin->setStateInformation (data.getData(), static_cast<int> (data.getSize())); flush();
                    check (std::abs (tw->getValue() - 0.23f) < 0.001f, "legacy patch seeds 3W from its stored SG cutoff");
                }
                else check (false, "legacy patch state decodes");
            }
            else check (false, "legacy VST3 state wrapper decodes");
        }
    }
    plugin->releaseResources();
    plugin.reset();

    std::cout << (failures == 0 ? "SMOKE TEST PASSED" : "SMOKE TEST FAILED") << std::endl;
    return failures == 0 ? 0 : 1;
}
