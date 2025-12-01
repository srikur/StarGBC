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

    static constexpr int TimerBit(uint8_t);

    void IncrementTIMA();

    bool SaveState(std::ofstream &) const;

    bool LoadState(std::ifstream &);
};

#endif //STARGBC_TIMER_H
