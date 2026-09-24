#pragma once

// One layer's sound engine: voice assign (SOLO / LEGATO / POLY 1 / POLY 2, UNISON, BINAURAL),
// HOLD and sustain, performance modulation state, parameter smoothing, oversampled rendering,
// decimation and layer pan [manual pp.40, 81, 90–91].

#include "ArpSeq.h"
#include "Decimator.h"
#include "Effects.h"
#include "LayerControl.h"
#include "SuperVoice.h"

#include <array>
#include <atomic>
#include <vector>

namespace sg
{

class LayerEngine : private ArpSeq::Sink
{
public:
    static constexpr int kSuperVoices = 10;   // 20 voices = 10 super voices [p.xiii]
    static constexpr int kMaxUnits    = 20;
    static constexpr int kChunk = 32;         // control block (≤ 64 per the spec)

    void setAnalogTolerance (bool on) noexcept { analog = on; }
    void prepare (double hostRate, int maxBlockSize, int oversampling);
    void reset();

    void setParams (const LayerParams& p) noexcept { target = p; }
    // DDS 1 CUSTOM sample (any thread). The caller keeps the previous one alive until the next swap.
    void setCustomSample (const Sample* s) noexcept { customSample.store (s, std::memory_order_release); }

    // Voices this layer may use: 20 in SINGLE, 10 in DUAL / SPLIT, 0 for the silent layer [p.82].
    void setVoiceLimit (int voices) noexcept;
    int getVoiceLimit() const noexcept { return voiceLimit; }

    // Performance inputs
    void setPitchOffsets (float pitchSemis, float keySemis) noexcept { mods.pitchOffset = pitchSemis; mods.keyOffset = keySemis; }
    // Played bender; while SEQ REC is armed its position is stored with the next step [p.97].
    void setBend (float b) noexcept
    {
        mods.bend = clampf (b, -1.0f, 1.0f);
        if (arp.isRecording()) arp.recordBend (mods.bend);
    }
    void setPush (float v) noexcept               { mods.push = clampf (v, 0.0f, 1.0f); }
    void setChannelAftertouch (float v) noexcept  { mods.channelAT = clampf (v, 0.0f, 1.0f); }
    void setExpression (float v) noexcept         { mods.expression = clampf (v, 0.0f, 1.0f); }
    void setRibbon (float pos, float bendSemis) noexcept { mods.ribbonPos = clampf (pos, 0.0f, 1.0f); mods.ribbonSemis = bendSemis; }
    void setPolyAftertouch (int note, float v) noexcept;
    void setSustain (bool on) noexcept;
    void setHold (bool on) noexcept;
    bool isHolding() const noexcept { return hold; }
    bool usesRibbonInMatrix() const noexcept { return control.routes.uses (MSrc::Ribbon); }

    void noteOn (int note, float velocity);    // goes to the arpeggiator / sequencer when it is ON
    void noteOff (int note);
    void allNotesOff (bool immediate);

    // Clock for one host block: tempo, and the host position when following its transport.
    void setClock (const ClockInfo& info) noexcept;
    void setSequenceBank (SequenceBank* b) noexcept { arp.setBank (b); }
    ArpSeq& getArpSeq() noexcept { return arp; }
    float getDelayTimeSeconds() const noexcept { return delay.getTimeSeconds(); }
    double getBeatPosition() const noexcept { return beatPos; }

    // Renders n host samples (writes, does not add), including the layer PAN. Call noteOn/noteOff
    // between calls to place events sample-accurately.
    void process (float* outL, float* outR, int n);

    int getActiveVoiceCount() const noexcept;
    float getLatencySamples() const noexcept { return decL.latencyHostSamples(); }
    int getOversampling() const noexcept { return os; }
    const LayerControl& getControl() const noexcept { return control; }
    const LayerMods& getMods() const noexcept { return mods; }
    // Pitch of the first sounding voice, and voices held on a note (tests / UI).
    float getSoundingPitch() const noexcept;
    int countVoicesOnNote (int note) const noexcept;
    // ENV 1 cycles of the most recent voice, for the LOOP LED [p.48].
    unsigned getLoopCycles() const noexcept;
    // The most recently started active voice (editor read-outs), or null.
    const Voice* getNewestVoice() const noexcept;

private:
    void beginBlock (int len);
    void handHeldToArp();
    void takeHeldFromArp();
    void applyStructuralChanges();
    void updateEffects() noexcept;

    // Keyboard-level note handling (the arpeggiator's output also lands here).
    void keyOn (int note, float velocity);
    void keyOff (int note, bool fromArp);
    bool arpTakesKeys() const noexcept { return arp.isOn() || arp.isRecording(); }

    // ArpSeq::Sink
    void arpNoteOn (int note, float velocity) override { keyOn (note, velocity); }
    void arpNoteOff (int note) override                { keyOff (note, true); }
    void arpBend (float b) override                    { mods.bend = clampf (b, -1.0f, 1.0f); }   // playback, never re-recorded

    // voice units: binaural → super voice index; non-binaural → super voice × 2 + slot
    int poolUnits() const noexcept          { return binaural ? voiceLimit / 2 : voiceLimit; }
    bool unitActive (int u) const noexcept;
    bool unitGateOn (int u) const noexcept;
    float unitLevel (int u) const noexcept;
    void startUnit (int u, int note, float velocity, float unisonSemis, bool declick, bool glide);
    void releaseUnit (int u) noexcept;
    float unisonOffset (int index, int count) const noexcept;

    void polyNoteOn (int note, float velocity);
    void monoNoteOn (int note, float velocity);
    void keyReleased (int note);
    void releaseLatched();
    bool isMono() const noexcept { return voiceMode == VoiceMode::Solo || voiceMode == VoiceMode::Legato; }

    double fs = 48000.0;
    int os = 2;
    bool analog = true;
    bool firstBlock = true;
    int voiceLimit = 20;
    uint64_t ageCounter = 0;
    // allocation order per voice unit. Per unit, not per super voice: a super voice's two
    // non-binaural slots are L and R, and sharing one age made every tie go to slot 0 (left).
    std::array<uint64_t, kMaxUnits> unitAge {};

    // voice-assign configuration currently in force
    bool binaural = true;
    VoiceMode voiceMode = VoiceMode::Poly1;
    Lfo2Trigger lastTrigger = Lfo2Trigger::Trig;

    // key / note state
    std::array<bool, 128> keyDown {}, latched {};
    std::array<float, 128> keyVelocity {};
    std::vector<int> keyStack;            // held keys in press order (mono note priority)
    std::array<int, kMaxUnits> unitNote {};
    int monoNote = -1;
    bool sustain = false, hold = false;

    LayerParams target, smoothed;
    LayerControl control;
    std::atomic<const Sample*> customSample { nullptr };
    LayerMods mods;
    float panL = 1.0f, panR = 1.0f;

    std::array<SuperVoice, kSuperVoices> voices;
    Decimator decL, decR;
    std::vector<float> osL, osR;

    ArpSeq arp;
    Chorus chorus;
    StereoDelay delay;
    double bpm = 120.0, beatPos = 0.0;
    bool hostSynced = false;
};

} // namespace sg
