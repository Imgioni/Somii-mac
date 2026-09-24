#pragma once

// A user sample for DDS 1's CUSTOM wave (SPKR addition, not on the hardware): stereo float
// frames at the file's own level (a dropped file plays as loud as it is), each channel carrying
// one guard frame so read() never branches.

#include <vector>

namespace sg
{

struct Sample
{
    std::vector<float> l, r;   // frames + 1 guard each
    int frames = 0;
    float rate = 48000.0f;

    // Linear read at frame position pos ∈ [0, frames). ch: 0 left, 1 right, 2 mono sum.
    float read (double pos, int ch) const noexcept
    {
        const int i = static_cast<int> (pos);
        const float f = static_cast<float> (pos - i);
        const float a = l[static_cast<size_t> (i)] + f * (l[static_cast<size_t> (i) + 1] - l[static_cast<size_t> (i)]);
        if (ch == 0) return a;
        const float b = r[static_cast<size_t> (i)] + f * (r[static_cast<size_t> (i) + 1] - r[static_cast<size_t> (i)]);
        return ch == 1 ? b : 0.5f * (a + b);
    }
};

} // namespace sg
