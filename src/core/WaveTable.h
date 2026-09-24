#pragma once

// Single-cycle wavetables for the DDS 1 ALT position [manual pp.33–34, 109].
//
// Format matches the hardware's .ws6 files: 4096 points per cycle, band-limited to 512
// harmonics (fs/8). Each table is stored as a mip pyramid (512, 256, … 1 harmonics) so the
// oscillator can pick the richest level that cannot alias audibly.

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace sg
{

class WaveTable
{
public:
    static constexpr int kSize    = 4096;
    static constexpr int kLevels  = 10;     // 512 >> level harmonics
    static constexpr int kMaxHarm = 512;

    // Builds from one cycle of kSize samples (any scale; normalised to peak 1).
    static std::shared_ptr<const WaveTable> fromCycle (const float* cycle, std::string name);
    // Builds from harmonic amplitudes/phases (index 1..kMaxHarm; index 0 ignored).
    static std::shared_ptr<const WaveTable> fromHarmonics (const std::vector<float>& amp,
                                                           const std::vector<float>& phase,
                                                           std::string name);

    // Linear-interpolated read, phase in [0,1).
    float read (float phase, int level) const noexcept
    {
        const float* t = levels[static_cast<size_t> (level)].data();
        const float x = phase * static_cast<float> (kSize);
        const int i = static_cast<int> (x);
        const float f = x - static_cast<float> (i);
        return t[i] + f * (t[i + 1] - t[i]);
    }

    // Richest level whose top harmonic stays below maxHarmonics.
    static int levelFor (float maxHarmonics) noexcept
    {
        int level = 0;
        float h = static_cast<float> (kMaxHarm);
        while (level < kLevels - 1 && h > maxHarmonics) { h *= 0.5f; ++level; }
        return level;
    }

    const std::string& getName() const noexcept { return name; }
    const std::vector<float>& getCycle() const noexcept { return levels[0]; }   // kSize + 1 guard

private:
    std::array<std::vector<float>, kLevels> levels;   // each kSize + 1 (guard point)
    std::string name;
};

using WaveTablePtr = std::shared_ptr<const WaveTable>;

// The 32 alternative-waveform slots (two groups of 16, "waveforms" and "alt_waveforms" [p.102]).
// UDO's factory set isn't distributable, so the bank starts with 32 procedurally generated
// waves; users replace them with their own .ws6 / WAV files (Phase 6).
class AltWaveBank
{
public:
    static constexpr int kSlots = 32;

    static const AltWaveBank& factory();     // thread-safe, built once

    WaveTablePtr get (int slot) const noexcept { return slots[static_cast<size_t> (slot < 0 ? 0 : (slot >= kSlots ? kSlots - 1 : slot))]; }

private:
    AltWaveBank();
    std::array<WaveTablePtr, kSlots> slots;
};

} // namespace sg
