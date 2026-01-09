#ifndef STARGBC_TIMER_H
#define STARGBC_TIMER_H

#include <fstream>

#include "Audio.h"
#include "Interrupts.h"

class Timer {
public:
    uint8_t tma{0x00};
    uint8_t tima{0x00};
    uint8_t tac{0x00};
    uint8_t overflowDelay{0x00};
    uint16_t divCounter{0x0000};
    bool overflowPending{false};
    bool reloadActive{false};
    Audio &audio_;
    Interrupts &interrupts_;

    explicit Timer(Audio &audio, Interrupts &interrupts) : audio_(audio), interrupts_(interrupts) {
    }

    void Tick(Speed);

    void WriteByte(uint16_t, uint8_t);

    [[nodiscard]] uint8_t ReadByte(uint16_t) const;

    void WriteDIV();

    void WriteTAC(uint8_t);

    void WriteTIMA(uint8_t);

    void WriteTMA(uint8_t);

    int TimerBit(uint8_t) const;

    void IncrementTIMA();

    bool SaveState(std::ofstream &) const;

    bool LoadState(std::ifstream &);
};

#endif //STARGBC_TIMER_H
