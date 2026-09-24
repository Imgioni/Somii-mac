#include "Performance.h"

namespace sg
{

void PerformanceEngine::prepare (double hostRate, int maxBlockSize, int oversampling)
{
    for (auto& l : layers)
    {
        l.setSequenceBank (&sequenceBank);
        l.prepare (hostRate, maxBlockSize, oversampling);
    }
    reset();
}

void PerformanceEngine::reset()
{
    for (auto& l : layers) l.reset();
    noteRoute.fill (0);
    ribbonDown = false;
    ribbonBend = 0.0f;
    applyVoiceLimits();
}

void PerformanceEngine::applyVoiceLimits() noexcept
{
    // SINGLE: one layer with all 20 voices; DUAL / SPLIT: 10 each [p.82].
    // A layer left behind with HOLD on keeps its voices, so what it is holding (notes or an
    // arpeggio) keeps sounding after switching away; it is released when its HOLD goes off.
    if (pp.mode == KeyboardMode::Single)
    {
        layers[static_cast<size_t> (pp.singleLayer)].setVoiceLimit (20);
        auto& other = layers[static_cast<size_t> (1 - pp.singleLayer)];
        if (! other.isHolding()) other.setVoiceLimit (0);
    }
    else
    {
        layers[0].setVoiceLimit (10);
        layers[1].setVoiceLimit (10);
    }
}

void PerformanceEngine::setParams (const PerformanceParams& p, const LayerParams& upper, const LayerParams& lower)
{
    const bool limitsChanged = p.mode != pp.mode || p.singleLayer != pp.singleLayer;
    const int sustainedBefore = pp.mode == KeyboardMode::Single ? pp.singleLayer : 0;
    pp = p;
    pp.singleLayer = pp.singleLayer ? 1 : 0;
    pp.baseChannel = std::clamp (pp.baseChannel, 1, 16);
    if (limitsChanged)
    {
        // The pedal follows the playing layer; the layer it leaves must not stay sustained forever.
        const int sustainedNow = pp.mode == KeyboardMode::Single ? pp.singleLayer : 0;
        if (sustainedNow != sustainedBefore && sustainPedal)
        {
            layers[static_cast<size_t> (sustainedBefore)].setSustain (false);
            layers[static_cast<size_t> (sustainedNow)].setSustain (true);
        }
        applyVoiceLimits();
    }

    layers[0].setParams (upper);
    layers[1].setParams (lower);

    // Pitch offsets: octave [p.75], global transpose [p.75], fine tune [p.76], PERF DETUNE (both
    // layers) and LOWER DETUNE (lower only) [p.80].
    const float common = pp.transpose + pp.fineTuneCents * 0.01f + pp.perfDetune;
    const float upOct = 12.0f * static_cast<float> (upper.octave);
    const float loOct = 12.0f * static_cast<float> (lower.octave);
    layers[0].setPitchOffsets (upOct + common, upOct + pp.transpose);
    layers[1].setPitchOffsets (loOct + common + pp.lowerDetune, loOct + pp.transpose);
}

void PerformanceEngine::setHold (int layer, bool on) noexcept
{
    layers[static_cast<size_t> (layer)].setHold (on);
    if (! on) applyVoiceLimits();   // an unselected layer gives its voices back once HOLD is off
}

int PerformanceEngine::layerMaskFor (int note) const noexcept
{
    switch (pp.mode)
    {
        case KeyboardMode::Single: return 1 << pp.singleLayer;
        case KeyboardMode::Dual:   return 3;
        case KeyboardMode::Split:  return note >= pp.splitPoint ? 1 : 2;   // split point = first upper note
    }
    return 1;
}

template <typename Fn>
void PerformanceEngine::forChannel (int channel, Fn&& fn)
{
    if (channel == pp.baseChannel)          { fn (layers[0]); fn (layers[1]); }
    else if (channel == pp.baseChannel + 1) { fn (layers[1]); }
}

void PerformanceEngine::noteOn (int channel, int note, float velocity)
{
    if (note < 0 || note > 127) return;
    if (velocity <= 0.0f) { noteOff (channel, note); return; }

    if (channel == pp.baseChannel)
    {
        const int mask = layerMaskFor (note);
        noteRoute[static_cast<size_t> (note)] = static_cast<uint8_t> (mask);
        if (mask & 1) layers[0].noteOn (note, velocity);
        if (mask & 2) layers[1].noteOn (note, velocity);
    }
    else if (channel == pp.baseChannel + 1)
    {
        layers[1].noteOn (note, velocity);
    }
}

void PerformanceEngine::noteOff (int channel, int note)
{
    if (note < 0 || note > 127) return;
    if (channel == pp.baseChannel)
    {
        // Route the release to wherever the note went, even if the keyboard mode changed since.
        const int mask = noteRoute[static_cast<size_t> (note)] != 0 ? noteRoute[static_cast<size_t> (note)] : layerMaskFor (note);
        noteRoute[static_cast<size_t> (note)] = 0;
        if (mask & 1) layers[0].noteOff (note);
        if (mask & 2) layers[1].noteOff (note);
    }
    else if (channel == pp.baseChannel + 1)
    {
        layers[1].noteOff (note);
    }
}

void PerformanceEngine::pitchBend (int channel, float value)
{
    forChannel (channel, [value] (LayerEngine& l) { l.setBend (value); });
}

void PerformanceEngine::channelPressure (int channel, float value)
{
    forChannel (channel, [value] (LayerEngine& l) { l.setChannelAftertouch (value); });
}

void PerformanceEngine::polyPressure (int channel, int note, float value)
{
    if (note < 0 || note > 127) return;
    if (channel == pp.baseChannel)
    {
        const int mask = noteRoute[static_cast<size_t> (note)];
        if (mask & 1) layers[0].setPolyAftertouch (note, value);
        if (mask & 2) layers[1].setPolyAftertouch (note, value);
    }
    else if (channel == pp.baseChannel + 1)
    {
        layers[1].setPolyAftertouch (note, value);
    }
}

void PerformanceEngine::controller (int channel, int cc, int value)
{
    const float v = static_cast<float> (value) / 127.0f;
    switch (cc)
    {
        case 1:   forChannel (channel, [v] (LayerEngine& l) { l.setPush (v); }); break;          // Modulation Lever [p.116]
        case 11:  forChannel (channel, [v] (LayerEngine& l) { l.setExpression (v); }); break;    // Expression → matrix source 6
        case 4:   if (channel == pp.baseChannel) volumePedal = v; break;                         // Foot Controller = volume pedal (DD-20)

        case 2:   ribbonMsb = value; updateRibbon(); break;                                      // Ribbon coarse / fine [p.78]
        case 34:  ribbonLsb = value; updateRibbon(); break;

        case 64:                                                                                 // Sustain [p.29, DD-19]
        {
            const bool on = value >= 64;
            if (channel == pp.baseChannel)
            {
                sustainPedal = on;
                if (pp.mode == KeyboardMode::Single) layers[static_cast<size_t> (pp.singleLayer)].setSustain (on);
                else                                 layers[0].setSustain (on);
            }
            else if (channel == pp.baseChannel + 1)
            {
                layers[1].setSustain (on);
            }
            break;
        }

        case 120: forChannel (channel, [] (LayerEngine& l) { l.allNotesOff (true); }); break;   // All Sound Off
        case 123: forChannel (channel, [] (LayerEngine& l) { l.allNotesOff (false); }); break;  // All Notes Off
        case 121:                                                                                // Reset All Controllers
            forChannel (channel, [] (LayerEngine& l)
            {
                l.setBend (0.0f); l.setPush (0.0f); l.setChannelAftertouch (0.0f); l.setExpression (0.0f); l.setSustain (false);
            });
            break;
        default: break;
    }
}

// Deviation (user, 2026-09-20): the manual has the ribbon ignore CC 2 / 34 in its pitch role
// [p.78], which left no way to play it from a MIDI controller. It now always receives them.
void PerformanceEngine::updateRibbon() noexcept
{
    ribbonAbsolute (static_cast<float> (ribbonMsb * 128 + ribbonLsb) / 16383.0f);
}

// MIDI CC 2 / 34 and the host's "Ribbon" parameter set the ribbon absolutely: the middle is no
// bend and each end is the full range, like a centred bend wheel. A finger on the on-screen ribbon
// wins while it is down. Layers using the ribbon in the matrix take the position, not the bend.
void PerformanceEngine::ribbonAbsolute (float position) noexcept
{
    if (ribbonDown) return;
    ribbonPos = clampf (position, 0.0f, 1.0f);
    const float bend = (ribbonPos - 0.5f) * 2.0f * taper::kRibbonSemis;
    for (auto& l : layers) l.setRibbon (ribbonPos, bend);
}

void PerformanceEngine::ribbonTouch (float position)
{
    ribbonDown = true;
    ribbonAnchor = ribbonPos = clampf (position, 0.0f, 1.0f);
    ribbonBend = 0.0f;
    for (auto& l : layers) l.setRibbon (ribbonPos, 0.0f);
}

void PerformanceEngine::ribbonMove (float position)
{
    if (! ribbonDown) { ribbonTouch (position); return; }
    ribbonPos = clampf (position, 0.0f, 1.0f);
    // Bend depends on the finger's travel from where it landed: bottom → top = full range up [p.77].
    ribbonBend = (ribbonPos - ribbonAnchor) * taper::kRibbonSemis;
    for (auto& l : layers) l.setRibbon (ribbonPos, ribbonBend);
}

void PerformanceEngine::ribbonRelease()
{
    ribbonDown = false;
    ribbonBend = 0.0f;
    for (auto& l : layers) l.setRibbon (ribbonPos, 0.0f);
}

void PerformanceEngine::allNotesOff (bool immediate)
{
    for (auto& l : layers) l.allNotesOff (immediate);
    noteRoute.fill (0);
}

void PerformanceEngine::process (float* upperL, float* upperR, float* lowerL, float* lowerR, int n)
{
    layers[0].process (upperL, upperR, n);
    layers[1].process (lowerL, lowerR, n);
    if (volumePedal < 1.0f)
    {
        const float g = volumePedal * volumePedal;
        for (int i = 0; i < n; ++i) { upperL[i] *= g; upperR[i] *= g; lowerL[i] *= g; lowerR[i] *= g; }
    }
}

} // namespace sg
