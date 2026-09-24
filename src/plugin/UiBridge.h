#pragma once

// Traffic between the editor (message thread) and the audio thread, without locks on the audio
// side: the editor pushes commands into a FIFO that processBlock drains, and processBlock
// publishes meters, LEDs and the sequencer state through atomics and a try-locked snapshot.

#include <juce_core/juce_core.h>

#include "core/ArpSeq.h"
#include "core/Matrix.h"

#include <array>
#include <atomic>

struct UiCommand
{
    enum class Type : uint8_t
    {
        SeqSetStep,      // layer, a = step index, step
        SeqSetLength,    // layer, a = length
        SeqRecord,       // layer, a = on, b = start step (−1 keeps the position)
        SeqLoad,         // layer, a = memory 0–15
        SeqStore,        // layer, a = memory 0–15
        SeqClear,        // layer
        RibbonTouch,     // f = position 0…1
        RibbonMove,      // f = position 0…1
        RibbonRelease,
        AllNotesOff,
        NoteOn,          // a = note, f = velocity (on-screen keyboard, base channel)
        NoteOff,         // a = note
        Bend,            // f = −1 … +1 (on-screen bender, sideways)
        Push,            // f = 0 … 1 (on-screen bender, push = mod wheel)
        Pressure         // f = 0 … 1 (on-screen ribbon, vertical = channel aftertouch)
    };

    Type type = Type::AllNotesOff;
    int layer = 0;
    int a = 0, b = 0;
    float f = 0.0f;
    sg::SeqStep step {};
};

class UiBridge
{
public:
    // ── editor → audio ──
    bool push (const UiCommand& c) noexcept
    {
        const auto w = fifo.write (1);
        if (w.blockSize1 + w.blockSize2 == 0) return false;
        buffer[static_cast<size_t> (w.blockSize1 > 0 ? w.startIndex1 : w.startIndex2)] = c;
        return true;
    }

    template <typename Fn>
    void drain (Fn&& fn) noexcept
    {
        const auto rd = fifo.read (fifo.getNumReady());
        for (int i = 0; i < rd.blockSize1; ++i) fn (buffer[static_cast<size_t> (rd.startIndex1 + i)]);
        for (int i = 0; i < rd.blockSize2; ++i) fn (buffer[static_cast<size_t> (rd.startIndex2 + i)]);
    }

    // ── audio → editor ──
    struct LayerFeed
    {
        std::atomic<int> voices { 0 };
        std::atomic<unsigned> loopCycles { 0 };
        std::atomic<int> step { 0 };
        std::atomic<bool> running { false };
        std::atomic<bool> recording { false };
        std::atomic<int> recStep { 0 };
        // newest voice, for the modulation rings and the glowing LEDs
        std::array<std::atomic<float>, sg::kMatrixDests> mod {};   // matrix offset per destination (normalised)
        std::atomic<float> cutoffNorm { -1.0f };                   // effective LPF position 0…1, −1 = silent
        std::atomic<float> env1 { 0.0f }, env2 { 0.0f };
        std::atomic<float> peak { 0.0f };                          // layer output, max since the editor last read
    };
    std::array<LayerFeed, 2> layer;
    std::atomic<float> peakL { 0.0f }, peakR { 0.0f };   // main output, max since the editor last read
    std::atomic<float> bpm { 120.0f };                    // tempo in force (host or TEMPO)
    std::atomic<int> lastNote { -1 };                    // last MIDI note-on, for the pages' LEARN keys

    // Output oscilloscope: the most recent main-mix samples (mono), written by the audio thread.
    static constexpr int kScopeSize = 2048;
    std::array<std::atomic<float>, kScopeSize> scope {};
    std::atomic<uint32_t> scopePos { 0 };
    void writeScope (const float* l, const float* r, int n) noexcept
    {
        uint32_t pos = scopePos.load (std::memory_order_relaxed);
        for (int i = 0; i < n; ++i)
            scope[(pos++) % kScopeSize].store (0.5f * (l[i] + r[i]), std::memory_order_relaxed);
        scopePos.store (pos, std::memory_order_release);
    }

    // Split point learn: the next base-channel note sets it instead of playing [p.82].
    std::atomic<bool> learnSplit { false };
    std::atomic<int> learnedNote { -1 };

    static void raisePeak (std::atomic<float>& a, float v) noexcept
    {
        float cur = a.load (std::memory_order_relaxed);
        while (v > cur && ! a.compare_exchange_weak (cur, v, std::memory_order_relaxed)) {}
    }

    // Working-sequence snapshots. The audio thread publishes with a try-lock (never waits) and
    // retries on the next block if the editor happened to be reading.
    void publish (int l, const sg::Sequence& s, uint32_t revision) noexcept
    {
        const auto i = static_cast<size_t> (l);
        if (published[i] == revision) return;
        const juce::SpinLock::ScopedTryLockType lock (seqLock);
        if (! lock.isLocked()) return;
        seqs[i] = s;
        published[i] = revision;
        version.fetch_add (1, std::memory_order_release);
    }

    // Message thread. Returns the snapshot version (changes whenever either layer republishes).
    uint32_t readSequence (int l, sg::Sequence& out)
    {
        const juce::SpinLock::ScopedLockType lock (seqLock);
        out = seqs[static_cast<size_t> (l)];
        return version.load (std::memory_order_acquire);
    }
    uint32_t getSequenceVersion() const noexcept { return version.load (std::memory_order_acquire); }

private:
    juce::AbstractFifo fifo { 256 };
    std::array<UiCommand, 256> buffer {};
    juce::SpinLock seqLock;
    std::array<sg::Sequence, 2> seqs {};
    std::array<uint32_t, 2> published { ~0u, ~0u };
    std::atomic<uint32_t> version { 0 };
};
