#include "core/WaveTable.h"
#include <fstream>
#include <iostream>

// Export one full-resolution cycle per factory slot for the UI generator.
int main(int argc, char** argv)
{
    if (argc != 2) return 1;
    std::ofstream out(argv[1]);
    if (!out) return 2;
    out << '[';
    const auto& bank = sg::AltWaveBank::factory();
    for (int slot = 0; slot < 32; ++slot) {
        if (slot) out << ',';
        out << '[';
        const auto& cycle = bank.get(slot)->getCycle();
        for (int i = 0; i < sg::WaveTable::kSize; ++i) {
            if (i) out << ',';
            out << cycle[i];
        }
        out << ']';
    }
    out << ']';
    return out.good() ? 0 : 3;
}
