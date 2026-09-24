#pragma once

// A performance: two layers (UPPER, LOWER) plus the overarching settings [manual pp.17–19, 79–83]
// and the performance controllers (bender, mod wheel, aftertouch, ribbon, pedals) [pp.68–78].
// MIDI: the base channel plays like the built-in keyboard (keyboard mode applies); base + 1 plays
// the lower layer [p.99, DD-19].

#include "LayerEngine.h"

#include <array>

namespace sg
{

enum class KeyboardMode { Single, Dual, Split };   // p.82

struct PerformanceParams
{
    KeyboardMode mode = KeyboardMode::Single;
    int singleLayer = 0;          // 0 = UPPER (default [p.82]), 1 = LOWER
    int splitPoint = 60;          // first note of the upper layer, default C4 [p.82]
    float lowerDetune = 0.0f;     // ±7 st [p.80]
    float perfDetune = 0.0f;      // ±7 st [p.80]
    float transpose = 0.0f;       // ±12 st [p.75]
    float fineTuneCents = 0.0f;   // ±100 ct [p.76]
    int baseChannel = 1;          // MIDI CH [p.99]
};

class PerformanceEngine
{
public:
    static constexpr int kUpper = 0, kLower = 1;

    void setAnalogTolerance (bool on) noexcept { for (auto& l : layers) l.setAnalogTolerance (on); }
    void prepare (double hostRate, int maxBlockSize, int oversampling);
    void reset();

    void setParams (const PerformanceParams& pp, const LayerParams& upper, const LayerParams& lower);

    // MIDI, channels 1–16.
    void noteOn (int channel, int note, float velocity);
    void noteOff (int channel, int note);
    void pitchBend (int channel, float value);          // −1 … +1
    void channelPressure (int channel, float value);    // 0 … 1
    void polyPressure (int channel, int note, float value);
    void controller (int channel, int cc, int value);   // performance CCs: 1, 2/34, 4, 11, 64, 120, 121, 123
    void allNotesOff (bool immediate);

    // On-screen ribbon (relative: bend follows the distance from the touch point [p.77]).
    void ribbonTouch (float position);
    void ribbonMove (float position);
    void ribbonRelease();
    // MIDI / host control: 0…1, 0.5 = no bend (see Performance.cpp)
    void ribbonAbsolute (float position) noexcept;

    void setHold (int layer, bool on) noexcept;
    void setClock (const ClockInfo& info) noexcept { layers[0].setClock (info); layers[1].setClock (info); }

    // The 16 sequence memories, shared by both layers [p.95].
    SequenceBank& getSequenceBank() noexcept { return sequenceBank; }

    // Renders both layers (each already panned) into separate stereo buffers.
    void process (float* upperL, float* upperR, float* lowerL, float* lowerR, int n);

    LayerEngine& layer (int i) noexcept             { return layers[static_cast<size_t> (i)]; }
    const LayerEngine& layer (int i) const noexcept { return layers[static_cast<size_t> (i)]; }
    int getActiveVoiceCount() const noexcept { return layers[0].getActiveVoiceCount() + layers[1].getActiveVoiceCount(); }
    float getVolumePedal() const noexcept { return volumePedal; }
    const PerformanceParams& getParams() const noexcept { return pp; }

private:
    int layerMaskFor (int note) const noexcept;
    void applyVoiceLimits() noexcept;
    template <typename Fn> void forChannel (int channel, Fn&& fn);
    void updateRibbon() noexcept;

    std::array<LayerEngine, 2> layers;
    SequenceBank sequenceBank {};
    PerformanceParams pp;
    bool sustainPedal = false;              // base-channel CC 64, handed over when SINGLE changes layer
    std::array<uint8_t, 128> noteRoute {};   // layers each base-channel note went to (bit 0 upper, bit 1 lower)

    float ribbonAnchor = 0.0f, ribbonPos = 0.0f, ribbonBend = 0.0f;
    bool ribbonDown = false;
    int ribbonMsb = 0, ribbonLsb = 0;
    float volumePedal = 1.0f;
};

} // namespace sg
