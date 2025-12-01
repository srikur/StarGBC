#ifndef STARGBC_TIMER_H
#define STARGBC_TIMER_H

#include <fstream>

struct Timer {
    uint8_t tma{0x00};
    uint8_t tima{0x00};
    uint8_t tac{0x00};
    uint8_t overflowDelay{0x00};
    uint16_t divCounter{0x0000};
    bool overflowPending{false};
    bool reloadActive{false};

    void WriteByte(uint16_t, uint8_t);

    [[nodiscard]] uint8_t ReadByte(uint16_t) const;

    void WriteDIV();

    void WriteTAC(uint8_t);

    void WriteTIMA(uint8_t);

    void WriteTMA(uint8_t);

    constexpr int TimerBit(uint8_t tacMode) const {
        switch (tacMode & 0x03) {
            case 0x00: return 9; // 4096 Hz
            case 0x01: return 3; // 262144 Hz
            case 0x02: return 5; // 65536 Hz
            case 0x03: return 7; // 16384 Hz
            default: ;
        }
        return 9;
    }

    void IncrementTIMA();

    bool SaveState(std::ofstream &) const;

    bool LoadState(std::ifstream &);
};

#endif //STARGBC_TIMER_H
