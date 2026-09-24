#pragma once

// A "super voice" [manual p.xiii, p.91]: two complete voices sharing one LFO 1.
//  • Binaural: one note plays on both; left and right are independent voices whose LFO 1 phase
//    differs by LR PHASE [p.56] and whose DDS 1 sisters are mirrored for stereo width [p.61].
//  • Non-binaural: the two voices play different notes, share the LFO [p.54], and are panned by
//    SPREAD (alternating hard left/right at maximum) [p.58]; in SOLO/LEGATO they aren't panned and
//    SPREAD offsets their LFO 1 phase instead [p.91].

#include "Voice.h"

namespace sg
{

class SuperVoice
{
public:
    void prepare (float hostRate, int oversampling, uint32_t seed, bool analogTolerance);
    void reset();

    void setBinaural (bool b) noexcept { binaural = b; }
    bool isBinaural() const noexcept   { return binaural; }
    // Non-binaural mono modes: no panning; per-voice LFO 1 phase offsets instead.
    void setMonoSpread (bool mono, float offset0, float offset1) noexcept { monoMode = mono; monoOff[0] = offset0; monoOff[1] = offset1; }

    // Binaural: slot is ignored and both voices play the note. Non-binaural: slot 0 or 1.
    void startNote (int slot, int note, float velocity, float unisonSemis, const LayerControl& c, bool declick, bool glide);
    void changeNote (int slot, int note, float unisonSemis) noexcept;
    void releaseSlot (int slot) noexcept;
    void releaseNote (int note) noexcept;
    void releaseAll() noexcept;
    void killAll() noexcept;
    void setPolyAftertouch (int note, float v) noexcept;
    void resyncLfo2() noexcept { voices[0].resyncLfo2(); voices[1].resyncLfo2(); }

    bool isActive() const noexcept           { return voices[0].isActive() || voices[1].isActive(); }
    bool isSlotActive (int s) const noexcept { return voices[s].isActive(); }
    const Voice& getVoice (int s) const noexcept { return voices[s]; }

    void setupBlock (const LayerControl& c, float blockSeconds) noexcept;

    // Adds numHost × os samples into the oversampled stereo bus.
    void render (const LayerControl& c, const LayerMods& m, float* osL, float* osR,
                 int numHost, int os, float spread) noexcept;

    uint64_t age = 0;   // allocation order, for voice stealing

private:
    Voice voices[2];
    Lfo1 lfo;
    bool binaural = true, monoMode = false;
    float monoOff[2] { 0.0f, 0.0f };
    float lrPhase = 0.0f;
    float tmp[2][Voice::kMaxGroup * 16] {};
};

} // namespace sg
