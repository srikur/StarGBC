#ifndef STARGBC_MEMORY_H
#define STARGBC_MEMORY_H

#include <array>
#include <cstdint>
#include <fstream>

struct Memory {
    static constexpr uint16_t WRAM_BEGIN = 0xA000;
    static constexpr uint16_t WRAM_END = 0xBFFF;
    static constexpr uint16_t HRAM_BEGIN = 0xFF80;
    static constexpr uint16_t HRAM_END = 0xFFFE;

    std::array<uint8_t, 0x8000> wram_{};
    std::array<uint8_t, 0x80> hram_{};
    uint8_t wramBank_{0x01};

    bool SaveState(std::ofstream &stateFile) const;

    bool LoadState(std::ifstream &stateFile);
};

#endif //STARGBC_MEMORY_H
