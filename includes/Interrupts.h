#ifndef STARGBC_INTERRUPTS_H
#define STARGBC_INTERRUPTS_H

#include "Common.h"
#include <array>

struct Interrupts {
    uint8_t interruptEnable{0x00};
    uint8_t interruptFlag{0xE1};
    uint8_t interruptFlagDelayed{0x00};
    uint8_t interruptVisiblePending{0x00};
    std::array<uint8_t, 5> interruptSetDelays{};
    std::array<uint8_t, 5> interruptVisibleDelays{};
    bool interruptMasterEnable{false};
    bool interruptDelay{false};

    void Set(InterruptType, bool);

    void SetAfter(InterruptType, uint8_t dots, bool visibleEarly = false, uint8_t visibleDelay = 0);
    void Tick();

    [[nodiscard]] bool IsSet(InterruptType) const;
};

#endif //STARGBC_INTERRUPTS_H
