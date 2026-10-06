#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "WebEditor.h"

#include "core/Tapers.h"

SuperGeminiProcessor::SuperGeminiProcessor()
    : AudioProcessor (BusesProperties()
                          // MIX / UPPER / LOWER outputs, mirroring the rear panel [manual p.29]
                          .withOutput ("Main",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Upper", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Lower", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "SuperGemini", sgp::createLayout())
{
    layerRefs[0].bind (apvts, "upper");
    layerRefs[1].bind (apvts, "lower");
    perfRefs.bind (apvts);
    router.bind (apvts);
    fxRack.bind (apvts);
    ribbonParam = apvts.getRawParameterValue ("perf.ribbon");
    apvts.state.setProperty ("envCurve", 2, nullptr);   // every state saved from here on uses the gentle envelope curve
    apvts.state.setProperty ("fxTypes", 3, nullptr);    // and the FX type list without UNDERTOW / HALO, CARVE in shapers
    startTimerHz (30);
}

SuperGeminiProcessor::~SuperGeminiProcessor()
{
    stopTimer();
}

bool SuperGeminiProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    for (int i = 1; i < layouts.outputBuses.size(); ++i)
    {
        const auto& set = layouts.outputBuses.getReference (i);
        if (! set.isDisabled() && set != juce::AudioChannelSet::stereo())
            return false;
    }
    return layouts.inputBuses.isEmpty();
}

void SuperGeminiProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock, kOversampling);
    for (auto& s : scratch) s.assign (static_cast<size_t> (std::max (1, samplesPerBlock)), 0.0f);
    holdState = { false, false };
    fxRack.prepare (sampleRate, samplesPerBlock);
    fxRack.messageTick (false);
}

void SuperGeminiProcessor::handleMidi (const juce::MidiMessage& m)
{
    const int ch = m.getChannel();
    if (m.isNoteOn() && ui.learnSplit.load (std::memory_order_relaxed) && ch == perfParams.baseChannel)
    {
        // Hold SPLIT + play a note: that note becomes the first note of the upper layer [p.82].
        ui.learnedNote.store (m.getNoteNumber());
        ui.learnSplit.store (false);
        return;
    }
    if (m.isNoteOn())
    {
        ui.lastNote.store (m.getNoteNumber(), std::memory_order_relaxed);
        engine.noteOn (ch, m.getNoteNumber(), m.getFloatVelocity());
    }
    else if (m.isNoteOff())
        engine.noteOff (ch, m.getNoteNumber());
    else if (m.isPitchWheel())
        engine.pitchBend (ch, static_cast<float> (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isChannelPressure())
        engine.channelPressure (ch, static_cast<float> (m.getChannelPressureValue()) / 127.0f);
    else if (m.isAftertouch())
        engine.polyPressure (ch, m.getNoteNumber(), static_cast<float> (m.getAfterTouchValue()) / 127.0f);
    else if (m.isController())
    {
        // Parameter CCs / NRPN / RPN go to the host parameters; performance controllers
        // (mod wheel, ribbon, pedals, all-notes-off) go to the engine.
        if (! router.handleController (ch, m.getControllerNumber(), m.getControllerValue()))
            engine.controller (ch, m.getControllerNumber(), m.getControllerValue());
    }
}

sg::ClockInfo SuperGeminiProcessor::readClock()
{
    // Host tempo and transport when EXT CLK receive is on (DD-3), otherwise the TEMPO control.
    sg::ClockInfo ci;
    ci.bpm = perfRefs.tempo();
    if (perfRefs.hostClock())
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
            {
                if (auto bpm = pos->getBpm()) ci.bpm = *bpm;
                ci.hostPlaying = pos->getIsPlaying();
                if (auto ppq = pos->getPpqPosition()) ci.ppq = *ppq;
                else ci.hostPlaying = false;
            }
    return ci;
}

void SuperGeminiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();

    if (static_cast<int> (scratch[0].size()) < n)
        for (auto& s : scratch) s.resize (static_cast<size_t> (n));

    layerRefs[0].read (layerParams[0]);
    layerRefs[1].read (layerParams[1]);
    perfRefs.read (perfParams);
    engine.setParams (perfParams, layerParams[0], layerParams[1]);
    const auto clock = readClock();
    engine.setClock (clock);
    ui.bpm.store (static_cast<float> (clock.bpm), std::memory_order_relaxed);
    ui.drain ([this] (const UiCommand& c) { applyUiCommand (c); });
    // the host "Ribbon" parameter (automation, or a MIDI controller linked to it in the DAW)
    if (const float rib = ribbonParam->load (std::memory_order_relaxed); std::abs (rib - lastRibbon) > 1.0e-4f)
    {
        lastRibbon = rib;
        engine.ribbonAbsolute (rib);
    }
    router.setBaseChannel (perfParams.baseChannel);
    router.setReceiveEnabled (perfRefs.ccReceive());
    for (int l = 0; l < 2; ++l)
    {
        const bool h = layerRefs[static_cast<size_t> (l)].hold();
        if (h != holdState[static_cast<size_t> (l)]) { holdState[static_cast<size_t> (l)] = h; engine.setHold (l, h); }
    }

    float* uL = scratch[0].data();
    float* uR = scratch[1].data();
    float* lL = scratch[2].data();
    float* lR = scratch[3].data();

    // Sample-accurate MIDI: render up to each event, then apply it.
    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (0, n, meta.samplePosition);
        if (t > pos) { engine.process (uL + pos, uR + pos, lL + pos, lR + pos, t - pos); pos = t; }
        handleMidi (meta.getMessage());
    }
    if (pos < n) engine.process (uL + pos, uR + pos, lL + pos, lR + pos, n - pos);

    // the FX modulation sources: each layer's LFOs, envelopes, last note and controllers after this block
    static_assert (fx::Rack::kLayerSources == sg::LayerEngine::kFxSources && fxdefs::kModSources == fx::Rack::kLayerSources + 2);
    fx::Rack::LayerIn fxIn[2];
    for (int l = 0; l < 2; ++l)
    {
        engine.layer (l).fxSources (fxIn[l].src, n);
        fxIn[l].noteOns = engine.layer (l).getNoteOns();
    }
    fxRack.process (uL, uR, lL, lR, n, clock.bpm, clock.ppq, clock.hostPlaying, fxIn);

    const float master = sg::taper::levelGain (perfRefs.masterVolume());
    auto mainBus = getBusBuffer (buffer, false, 0);
    for (int i = 0; i < n; ++i)
    {
        const auto k = static_cast<size_t> (i);
        mainBus.setSample (0, i, (scratch[0][k] + scratch[2][k]) * master);
        mainBus.setSample (1, i, (scratch[1][k] + scratch[3][k]) * master);
    }

    // Layer outputs (post-effects, pre-master, DD-18)
    for (int bus = 1; bus <= 2; ++bus)
    {
        if (getBusCount (false) <= bus || ! getBus (false, bus)->isEnabled()) continue;
        auto b = getBusBuffer (buffer, false, bus);
        b.copyFrom (0, 0, scratch[static_cast<size_t> (bus == 1 ? 0 : 2)].data(), n);
        b.copyFrom (1, 0, scratch[static_cast<size_t> (bus == 1 ? 1 : 3)].data(), n);
    }

    publishUiFeed (mainBus, n);
}

void SuperGeminiProcessor::applyUiCommand (const UiCommand& c)
{
    auto& arp = engine.layer (juce::jlimit (0, 1, c.layer)).getArpSeq();
    switch (c.type)
    {
        case UiCommand::Type::SeqSetStep:    arp.setStep (c.a, c.step); break;
        case UiCommand::Type::SeqSetLength:  arp.setLength (c.a); break;
        case UiCommand::Type::SeqRecord:     arp.setRecording (c.a != 0, c.b); break;
        case UiCommand::Type::SeqLoad:       arp.load (c.a); break;
        case UiCommand::Type::SeqStore:      arp.store (c.a); break;
        case UiCommand::Type::SeqClear:      arp.clearSequence(); break;
        case UiCommand::Type::RibbonTouch:   engine.ribbonTouch (c.f); break;
        case UiCommand::Type::RibbonMove:    engine.ribbonMove (c.f); break;
        case UiCommand::Type::RibbonRelease: engine.ribbonRelease(); break;
        case UiCommand::Type::AllNotesOff:   engine.allNotesOff (false); break;
        case UiCommand::Type::NoteOn:        engine.noteOn (perfParams.baseChannel, c.a, c.f); break;
        case UiCommand::Type::NoteOff:       engine.noteOff (perfParams.baseChannel, c.a); break;
        case UiCommand::Type::Bend:          engine.pitchBend (perfParams.baseChannel, c.f); break;
        case UiCommand::Type::Push:          engine.controller (perfParams.baseChannel, 1, juce::roundToInt (juce::jlimit (0.0f, 1.0f, c.f) * 127.0f)); break;
        case UiCommand::Type::Pressure:      engine.channelPressure (perfParams.baseChannel, juce::jlimit (0.0f, 1.0f, c.f)); break;
    }
}

void SuperGeminiProcessor::publishUiFeed (const juce::AudioBuffer<float>& mainBus, int n)
{
    UiBridge::raisePeak (ui.peakL, mainBus.getMagnitude (0, 0, n));
    UiBridge::raisePeak (ui.peakR, mainBus.getMagnitude (1, 0, n));
    ui.writeScope (mainBus.getReadPointer (0), mainBus.getReadPointer (1), n);
    for (int l = 0; l < 2; ++l)
    {
        const auto& layer = engine.layer (l);
        auto& arp = engine.layer (l).getArpSeq();
        auto& f = ui.layer[static_cast<size_t> (l)];
        f.voices.store (layer.getActiveVoiceCount(), std::memory_order_relaxed);
        f.loopCycles.store (layer.getLoopCycles(), std::memory_order_relaxed);
        f.step.store (arp.getStepIndex(), std::memory_order_relaxed);
        f.running.store (arp.isRunning(), std::memory_order_relaxed);
        f.recording.store (arp.isRecording(), std::memory_order_relaxed);
        f.recStep.store (arp.getRecordStep(), std::memory_order_relaxed);
        ui.publish (l, arp.working(), arp.getRevision());

        // Newest voice: matrix offsets, effective cutoff and envelope levels (modulation rings, LEDs)
        const auto* v = layer.getNewestVoice();
        for (int d = 0; d < sg::kMatrixDests; ++d)
            f.mod[static_cast<size_t> (d)].store (v != nullptr ? v->getMod (static_cast<sg::MDest> (d)) : 0.0f, std::memory_order_relaxed);
        const float octaves = juce::jmax (1.0f, layer.getControl().lpfOctaves);
        f.cutoffNorm.store (v != nullptr ? v->getCutoffOctaves() / octaves : -1.0f, std::memory_order_relaxed);
        f.env1.store (v != nullptr ? v->getEnv1Level() : 0.0f, std::memory_order_relaxed);
        f.env2.store (v != nullptr ? v->getLevel() : 0.0f, std::memory_order_relaxed);
        const auto& s = scratch[static_cast<size_t> (l * 2)];
        float pk = 0.0f;
        for (int i = 0; i < n; ++i) pk = juce::jmax (pk, std::abs (s[static_cast<size_t> (i)]));
        UiBridge::raisePeak (f.peak, pk);
    }
}

// ponytail: prev is freed on the load after next; the audio thread drops a pointer within one
// 32-sample chunk, so only two loads inside one chunk could race. Refcounted handoff if that ever matters.
bool SuperGeminiProcessor::loadCustomSample (int layer, const void* data, size_t size, const juce::String& name)
{
    auto& cs = customSamples[static_cast<size_t> (layer & 1)];
    std::shared_ptr<sg::Sample> s;
    if (data != nullptr && size > 0)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (
            std::make_unique<juce::MemoryInputStream> (data, size, false)));
        if (reader == nullptr || reader->sampleRate <= 0.0 || reader->lengthInSamples < 2) return false;
        const int len = static_cast<int> (std::min<juce::int64> (reader->lengthInSamples,
                                                                  static_cast<juce::int64> (kCustomMaxSeconds * reader->sampleRate)));
        juce::AudioBuffer<float> b (2, len);
        reader->read (&b, 0, len, 0, true, true);   // a mono file lands in both channels

        s = std::make_shared<sg::Sample>();
        s->frames = len;
        s->rate = static_cast<float> (reader->sampleRate);
        for (int c = 0; c < 2; ++c)   // the file's own level: the voice plays it at unity
        {
            auto& dst = c == 0 ? s->l : s->r;
            const float* src = b.getReadPointer (c);
            dst.resize (static_cast<size_t> (len) + 1);
            std::copy (src, src + len, dst.begin());
            dst.back() = dst[static_cast<size_t> (len) - 1];                     // guard frame
        }
        s->findOnsets();   // for SLICE
        cs.file.replaceAll (data, size);
        cs.name = name.isNotEmpty() ? name : juce::String ("SAMPLE");
    }
    else
    {
        cs.file.reset();
        cs.name = {};
    }
    cs.prev = std::move (cs.now);
    cs.now = std::move (s);
    engine.layer (layer & 1).setCustomSample (cs.now.get());
    ++cs.version;
    return true;
}

juce::ValueTree SuperGeminiProcessor::customSamplesToTree() const
{
    juce::ValueTree t ("CUSTOM");
    for (int l = 0; l < 2; ++l)
    {
        const auto& cs = customSamples[static_cast<size_t> (l)];
        juce::ValueTree c ("SAMPLE");
        c.setProperty ("layer", l, nullptr);
        if (cs.file.getSize() > 0)
        {
            c.setProperty ("name", cs.name, nullptr);
            c.setProperty ("data", cs.file.toBase64Encoding(), nullptr);
        }
        t.appendChild (c, nullptr);
    }
    return t;
}

void SuperGeminiProcessor::customSamplesFromTree (const juce::ValueTree& t)
{
    for (int l = 0; l < 2; ++l)
    {
        const auto c = t.getChildWithProperty ("layer", l);
        juce::MemoryBlock b;
        if (c.isValid() && b.fromBase64Encoding (c.getProperty ("data").toString()) && b.getSize() > 0
            && loadCustomSample (l, b.getData(), b.getSize(), c.getProperty ("name").toString()))
            continue;
        if (customSamples[static_cast<size_t> (l)].now != nullptr) loadCustomSample (l, nullptr, 0, {});
    }
}

void SuperGeminiProcessor::loadInitPatch (int layer)
{
    if (customSamples[static_cast<size_t> (layer & 1)].now != nullptr) loadCustomSample (layer, nullptr, 0, {});
    const juce::String prefix = layer == 0 ? "upper." : "lower.";
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr) continue;
        const auto id = rp->getParameterID();
        if (! id.startsWith (prefix) || id == prefix + "hold" || id == prefix + "fx.freeze") continue;
        rp->beginChangeGesture();
        rp->setValueNotifyingHost (rp->getDefaultValue());
        rp->endChangeGesture();
    }
    auto set = [this] (const char* id, int index)
    {
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (apvts.getParameter (id)))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (index)));
            p->endChangeGesture();
        }
    };
    set ("perf.keyboardMode", 0);   // SINGLE [p.22]
    set ("perf.singleLayer", layer);
}

// With the DEST (bender / LFO 2) or PORTAMENTO (portamento / octave) layer selector on BOTH, the
// two layers share one setting: switching to BOTH copies the upper layer's values to the lower
// layer [pp.73–75], and while on BOTH an edit to either layer is copied to the other.
void SuperGeminiProcessor::mirrorGroup (const juce::StringArray& suffixes, bool justSwitchedToBoth)
{
    for (const auto& s : suffixes)
    {
        auto* up = apvts.getParameter ("upper." + s);
        auto* lo = apvts.getParameter ("lower." + s);
        if (up == nullptr || lo == nullptr) continue;

        const float u = up->getValue(), l = lo->getValue();
        const float lastU = lastSeen.count ("upper." + s) ? lastSeen["upper." + s] : u;
        const float lastL = lastSeen.count ("lower." + s) ? lastSeen["lower." + s] : l;

        auto copy = [] (juce::AudioProcessorParameter* dst, float v)
        {
            dst->beginChangeGesture();
            dst->setValueNotifyingHost (v);
            dst->endChangeGesture();
        };

        if (std::abs (u - l) > 1.0e-6f)
        {
            if (justSwitchedToBoth || std::abs (u - lastU) > 1.0e-6f) copy (lo, u);
            else if (std::abs (l - lastL) > 1.0e-6f)                  copy (up, l);
        }
        lastSeen["upper." + s] = up->getValue();
        lastSeen["lower." + s] = lo->getValue();
    }
}

void SuperGeminiProcessor::timerCallback()
{
    const int modLayer = perfRefs.modLayer();
    const int portaLayer = perfRefs.portaLayer();
    if (modLayer == 0)   mirrorGroup (sgp::kModLayerGroup, lastModLayer != 0);
    if (portaLayer == 0) mirrorGroup (sgp::kPortaLayerGroup, lastPortaLayer != 0);
    lastModLayer = modLayer;
    lastPortaLayer = portaLayer;
    fxRack.messageTick (true);
}

GeminusWebSession& SuperGeminiProcessor::getWebSession()
{
    if (webSession == nullptr)
        webSession = std::make_unique<GeminusWebSession> (*this);
    return *webSession;
}

juce::AudioProcessorEditor* SuperGeminiProcessor::createEditor()
{
    return new GeminusWebEditor (*this);
}

// Sequences: 16 memories + each layer's working sequence. Step format: "n,n,…/velocity/flags/bend"
// with flags t = tie, a = accent, r = rest, b = has bend. (Phase 6 moves this to the JSON library.)
juce::ValueTree SuperGeminiProcessor::sequencesToTree()
{
    auto encode = [] (const sg::Sequence& s)
    {
        juce::StringArray steps;
        for (const auto& st : s.steps)
        {
            juce::StringArray notes;
            for (int i = 0; i < st.count; ++i) notes.add (juce::String (st.notes[static_cast<size_t> (i)]));
            juce::String flags;
            if (st.tie) flags << "t";
            if (st.accent) flags << "a";
            if (st.rest) flags << "r";
            if (st.hasBend) flags << "b";
            steps.add (notes.joinIntoString (",") + "/" + juce::String (st.velocity, 3) + "/" + flags + "/" + juce::String (st.bend, 3));
        }
        juce::ValueTree t ("SEQ");
        t.setProperty ("length", s.length, nullptr);
        t.setProperty ("steps", steps.joinIntoString (";"), nullptr);
        return t;
    };

    juce::ValueTree root ("SEQUENCES");
    for (const auto& s : engine.getSequenceBank()) root.appendChild (encode (s), nullptr);
    for (int l = 0; l < 2; ++l)
    {
        auto w = encode (engine.layer (l).getArpSeq().working());
        w.setProperty ("working", l, nullptr);
        root.appendChild (w, nullptr);
    }
    return root;
}

void SuperGeminiProcessor::sequencesFromTree (const juce::ValueTree& root)
{
    auto decode = [] (const juce::ValueTree& t)
    {
        sg::Sequence s;
        s.length = juce::jlimit (1, 64, static_cast<int> (t.getProperty ("length", 16)));
        const auto steps = juce::StringArray::fromTokens (t.getProperty ("steps").toString(), ";", "");
        for (int i = 0; i < steps.size() && i < 64; ++i)
        {
            const auto parts = juce::StringArray::fromTokens (steps[i], "/", "");
            auto& st = s.steps[static_cast<size_t> (i)];
            const auto notes = juce::StringArray::fromTokens (parts[0], ",", "");
            st.count = 0;
            for (const auto& n : notes)
                if (n.isNotEmpty() && st.count < 8) st.notes[st.count++] = static_cast<int8_t> (n.getIntValue());
            st.velocity = parts.size() > 1 ? parts[1].getFloatValue() : 0.8f;
            const auto flags = parts.size() > 2 ? parts[2] : juce::String();
            st.tie = flags.containsChar ('t');
            st.accent = flags.containsChar ('a');
            st.rest = flags.containsChar ('r');
            st.hasBend = flags.containsChar ('b');
            st.bend = parts.size() > 3 ? parts[3].getFloatValue() : 0.0f;
        }
        return s;
    };

    int slot = 0;
    for (const auto& child : root)
    {
        if (! child.hasType ("SEQ")) continue;
        if (child.hasProperty ("working"))
        {
            const int l = juce::jlimit (0, 1, static_cast<int> (child.getProperty ("working")));
            engine.layer (l).getArpSeq().working() = decode (child);
        }
        else if (slot < 16)
        {
            engine.getSequenceBank()[static_cast<size_t> (slot++)] = decode (child);
        }
    }
}

void SuperGeminiProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("SEQUENCES"), nullptr);
    state.appendChild (sequencesToTree(), nullptr);
    state.removeChild (state.getChildWithName ("FXIR"), nullptr);
    state.appendChild (fxRack.toTree(), nullptr);
    state.removeChild (state.getChildWithName ("CUSTOM"), nullptr);
    state.appendChild (customSamplesToTree(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SuperGeminiProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            // Older patches shared the two panels' controls. Seed the new bank once on load;
            // subsequent edits and saves have independent IDs. Never inherit a previous patch.
            for (const auto* layer : { "upper", "lower" })
            {
                const juce::String prefix = juce::String (layer) + ".";
                auto oldValue = [&] (const char* id, float fallback)
                {
                    const auto node = tree.getChildWithProperty ("id", prefix + id);
                    return node.isValid() ? static_cast<float> (node.getProperty ("value")) : fallback;
                };
                auto seed = [&] (const char* id, float value)
                {
                    if (tree.getChildWithProperty ("id", prefix + id).isValid()) return;
                    juce::ValueTree node ("PARAM");
                    node.setProperty ("id", prefix + id, nullptr);
                    node.setProperty ("value", value, nullptr);
                    tree.appendChild (node, nullptr);
                };
                seed ("tw.cutoff", oldValue ("vcf.lpf", 1.0f));
                seed ("tw.res", oldValue ("vcf.res", 0.0f));
                seed ("tw.envAmt", 0.5f + 0.5f * oldValue ("vcf.envAmt", 0.0f));
                seed ("tw.keytrack", 0.25f * oldValue ("vcf.keytrack", 0.0f));
                seed ("tw.resComp", 1.0f);
                seed ("svf.envAmt", 0.5f);
                seed ("svf.velocity", 0.0f);
                seed ("svf.keytrack", 0.0f);
            }
            // Envelope times moved from 1 ms·10^(4x) to 10 s·x² (2026-10-05). A state saved before carries no
            // envCurve 2: its faders move to the positions that give the same times, so old sounds are unchanged.
            if (static_cast<int> (tree.getProperty ("envCurve", 1)) < 2)
            {
                for (const auto* layer : { "upper", "lower" })
                    for (const auto* id : { "env1.attackHold", "env1.attack", "env1.decayHold", "env1.decay", "env1.release",
                                            "env2.attack", "env2.decayHold", "env2.decay", "env2.release" })
                    {
                        auto node = tree.getChildWithProperty ("id", juce::String (layer) + "." + id);
                        if (! node.isValid()) continue;
                        const float x = static_cast<float> (node.getProperty ("value"));
                        const bool hold = juce::String (id).contains ("Hold");
                        node.setProperty ("value", hold && x <= 0.0f ? 0.0f : sg::taper::envPosition (0.001f * std::pow (10.0f, 4.0f * juce::jlimit (0.0f, 1.0f, x))), nullptr);
                    }
                tree.setProperty ("envCurve", 2, nullptr);
            }
            // UNDERTOW (type 2) and HALO (type 18) were removed (user, 2026-10-05) and the later types moved up.
            // A state saved before carries no fxTypes 2: its slots get the new numbers, the removed two are emptied.
            if (static_cast<int> (tree.getProperty ("fxTypes", 1)) < 2)
            {
                for (const auto* id : { "fx1.type", "fx2.type", "fx3.type" })
                {
                    auto node = tree.getChildWithProperty ("id", id);
                    if (! node.isValid()) continue;
                    const int t = juce::roundToInt (static_cast<float> (node.getProperty ("value")));
                    node.setProperty ("value", t == 2 || t == 18 ? 0 : t > 18 ? t - 2 : t > 2 ? t - 1 : t, nullptr);
                }
                tree.setProperty ("fxTypes", 2, nullptr);
            }
            // CARVE's controls were rearranged into eleven shapers the same day (fxTypes 3): a CARVE slot saved
            // in the first layout starts again from CARVE's defaults rather than misreading its old values.
            if (static_cast<int> (tree.getProperty ("fxTypes", 1)) < 3)
            {
                for (int s = 1; s <= 3; ++s)
                {
                    const juce::String p = "fx" + juce::String (s) + ".";
                    const auto type = tree.getChildWithProperty ("id", p + "type");
                    if (! type.isValid() || juce::roundToInt (static_cast<float> (type.getProperty ("value"))) != fxdefs::Carve) continue;
                    for (int i = 0; i < fxdefs::kNP; ++i)
                    {
                        auto node = tree.getChildWithProperty ("id", p + "p" + juce::String (i + 1));
                        if (node.isValid()) node.setProperty ("value", fxdefs::kParams[fxdefs::Carve][i].def, nullptr);
                    }
                }
                tree.setProperty ("fxTypes", 3, nullptr);
            }
            const auto seqs = tree.getChildWithName ("SEQUENCES");
            if (seqs.isValid())
            {
                sequencesFromTree (seqs);
                tree.removeChild (seqs, nullptr);
                engine.layer (0).getArpSeq().markChanged();
                engine.layer (1).getArpSeq().markChanged();
            }
            const auto irTree = tree.getChildWithName ("FXIR");
            fxRack.fromTree (irTree);
            tree.removeChild (irTree, nullptr);
            const auto custom = tree.getChildWithName ("CUSTOM");
            customSamplesFromTree (custom);
            tree.removeChild (custom, nullptr);
            apvts.replaceState (tree);
            fxRack.stateLoaded();
            fxRack.messageTick (false);

            // DELAY FREEZE is a performance gesture, never restored from a saved project (DD-9).
            for (auto* id : { "upper.fx.freeze", "lower.fx.freeze" })
                if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (0.0f);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SuperGeminiProcessor();
}
