#ifndef STARGBC_DMA_H
#define STARGBC_DMA_H

#include <cstdint>

struct DMA {
    static constexpr int STARTUP_CYCLES{1}; // 4 T-cycles
    static constexpr int TOTAL_BYTES{0xA0};

    uint8_t dmaTickCounter{0x00};
    uint8_t writtenValue{0x00};
    uint16_t startAddress{0x0000};
    uint8_t currentByte{0x00};
    bool transferActive{false};
    bool restartPending{false};
    uint16_t pendingStart{0x0000};
    uint16_t restartCountdown{0x0000};
    uint16_t ticks{0x0000};
    bool transferComplete{false};

    void Set(uint8_t);

};

#endif //STARGBC_DMA_H
