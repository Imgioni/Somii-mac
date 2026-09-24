#pragma once

// Arpeggiator and 64-step sequencer, one per layer [manual pp.92–98]. When ON, the layer's keys go
// here instead of straight to the voices; notes are emitted on the clock grid (CLK DIV, SWING,
// host tempo / TEMPO).

#include "Clock.h"
#include "LayerParams.h"

#include <algorithm>
#include <array>

namespace sg
{

// Fixed-capacity list: the arpeggiator runs on the audio thread, so it must never allocate.
// Pushes beyond the capacity are dropped (capacities cover every MIDI note).
template <typename T, size_t N>
class FixedList
{
public:
    void clear() noexcept                { count = 0; }
    bool empty() const noexcept          { return count == 0; }
    size_t size() const noexcept         { return count; }
    void push_back (const T& v) noexcept { if (count < N) items[count++] = v; }
    const T& back() const noexcept       { return items[count - 1]; }
    T& operator[] (size_t i) noexcept             { return items[i]; }
    const T& operator[] (size_t i) const noexcept { return items[i]; }
    T* begin() noexcept             { return items.data(); }
    T* end() noexcept               { return items.data() + count; }
    const T* begin() const noexcept { return items.data(); }
    const T* end() const noexcept   { return items.data() + count; }

    template <typename Pred>
    void eraseIf (Pred p) noexcept { count = static_cast<size_t> (std::remove_if (begin(), end(), p) - begin()); }

    template <size_t M>
    void assign (const FixedList<T, M>& other) noexcept { clear(); for (const auto& v : other) push_back (v); }

private:
    std::array<T, N> items {};
    size_t count = 0;
};

// One sequencer step [pp.95–97]: note or chord, SLIDE (tie to the next step), ACCENT, REST and an
// optional recorded bender position.
struct SeqStep
{
    std::array<int8_t, 8> notes {};
    uint8_t count = 0;
    float velocity = 0.8f;
    bool tie = false, accent = false, rest = false, hasBend = false;
    float bend = 0.0f;
};

struct Sequence
{
    std::array<SeqStep, 64> steps {};
    int length = 16;   // LENGTH track: last step [p.97]
};

using SequenceBank = std::array<Sequence, 16>;   // 16 sequence memories [p.95]

struct ArpSeqSettings
{
    bool on = false;
    int clockDiv = 4;        // index into clockdiv::kClockDivBeats, 1/16 default [p.92]
    int range = 1;           // 1–4 octaves [p.94]
    int swing = 0;           // 0–4 [p.93]
    ArpMode mode = ArpMode::Up;
    int seqSlot = 0;         // linked sequence 0–15 [p.98]
    bool sync = true;        // step follows CLK DIV on the clock grid; off = free running
    double freeBeats = 0.5;  // step length with sync off, already converted to beats
};

class ArpSeq
{
public:
    struct Sink
    {
        virtual ~Sink() = default;
        virtual void arpNoteOn (int note, float velocity) = 0;
        virtual void arpNoteOff (int note) = 0;
        virtual void arpBend (float bend) = 0;
    };

    static constexpr double kGate = 0.5;   // gate length as a fraction of the step (DD-52)

    void setBank (SequenceBank* b) noexcept { bank = b; }
    void reset() noexcept;
    void setSettings (const ArpSeqSettings& s, Sink& sink);
    const ArpSeqSettings& getSettings() const noexcept { return settings; }
    bool isOn() const noexcept { return settings.on; }
    bool isRunning() const noexcept { return running; }

    void setHold (bool on, Sink& sink);            // HOLD / sustain latches the chord [pp.81, 94]
    void keyDown (int note, float velocity, bool hostSynced, double beatPos, Sink& sink);
    void keyUp (int note, Sink& sink);
    void stop (Sink& sink);

    // HOLD hand-over between the arp and plain playing: the layer gives its held notes to the
    // arp when it is switched on, and takes the arp's held notes back when it is switched off.
    void adoptHeld (int note, float velocity, bool keyIsDown, bool hostSynced, double beatPos);
    template <typename Fn>
    void forEachHeld (Fn&& fn) const
    {
        for (const auto& [n, v] : chord)
            fn (n, v, std::any_of (physical.begin(), physical.end(), [n = n] (const auto& e) { return e.first == n; }));
    }
    void clearKeys() noexcept { physical.clear(); chord.clear(); chordReleased = true; }

    // Samples until the next event (≤ maxSamples); 0 means events are due now — call fire().
    int samplesUntilNext (double beatPos, double beatsPerSample, int maxSamples) const noexcept;
    void fire (double beatPos, Sink& sink);

    // Working sequence (LOAD copies a memory into it, STORE copies it back) [p.98]
    Sequence& working() noexcept { return seq; }
    const Sequence& working() const noexcept { return seq; }
    void load (int slot) noexcept;
    void store (int slot) noexcept;
    void clearSequence() noexcept { seq = Sequence(); ++revision; }   // SEQ REC + MOD ASSIGN [p.98]
    // Step editor [pp.96–97]: TRACK edits (slide / accent / rest / notes) and LENGTH.
    void setStep (int index, const SeqStep& s) noexcept;
    void setLength (int length) noexcept;
    // Bumped whenever the working sequence changes, so the editor knows when to redraw.
    uint32_t getRevision() const noexcept { return revision; }
    void markChanged() noexcept { ++revision; }

    // Step recording [p.96]: notes are stored when all keys are released, then the step advances.
    // startStep < 0 keeps the current record position (the step LED that is flashing).
    void setRecording (bool on, int startStep = -1) noexcept;
    bool isRecording() const noexcept { return recording; }
    int getRecordStep() const noexcept { return recStep; }
    void recordBend (float bend) noexcept { recBend = bend; recHasBend = true; }

    int getStepIndex() const noexcept { return static_cast<int> (stepCounter); }

private:
    double stepBeats() const noexcept
    {
        return settings.sync ? clockdiv::kClockDivBeats[static_cast<size_t> (settings.clockDiv & 7)] : settings.freeBeats;
    }
    double stepOffset (int64_t k) const noexcept;
    double stepStart (int64_t k) const noexcept { return gridOrigin + stepOffset (k); }
    void startRunning (bool hostSynced, double beatPos);
    void releaseSounding (Sink& sink);
    void playArpStep (Sink& sink);
    void playSeqStep (Sink& sink);
    int transposeKey() const noexcept;

    ArpSeqSettings settings;
    SequenceBank* bank = nullptr;
    Sequence seq;
    uint32_t revision = 0;

    using KeyList = FixedList<std::pair<int, float>, 128>;

    // keys
    KeyList physical;                               // held keys, press order
    KeyList chord;                                  // notes the arpeggio / sequence plays from
    FixedList<std::pair<int, float>, 512> arpNotes; // chord over RANGE octaves (scratch)
    bool hold = false;
    bool chordReleased = true;                      // all keys up since the chord was latched

    // clock state
    bool running = false;
    double gridOrigin = 0.0;
    int64_t stepCounter = 0;
    double nextStepBeat = 0.0, gateOffBeat = -1.0;
    FixedList<int, 128> sounding;
    bool tieIntoNext = false;
    uint32_t rng = 0x12345678u;

    // recording
    bool recording = false;
    int recStep = 0;
    KeyList recChord;
    float recBend = 0.0f;
    bool recHasBend = false;
};

} // namespace sg
