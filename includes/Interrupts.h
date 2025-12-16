#ifndef STARGBC_INTERRUPTS_H
#define STARGBC_INTERRUPTS_H

#include "Common.h"

struct Interrupts {
    uint8_t interruptEnable{0x00};
    uint8_t interruptFlag{0xE1};
    uint8_t interruptFlagDelayed{0x00};
    uint8_t interruptSetDelay{0x00};
    bool interruptMasterEnable{false};
    bool interruptDelay{false};

    void Set(InterruptType, bool);
};

#endif //STARGBC_INTERRUPTS_H