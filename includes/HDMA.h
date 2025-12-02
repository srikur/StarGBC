#ifndef STARGBC_HDMA_H
#define STARGBC_HDMA_H

#include <cstdint>

enum class HDMAMode {
    GDMA, HDMA
};

struct HDMA {
    uint16_t hdmaSource{0x0000};
    uint16_t hdmaDestination{0x8000};
    uint8_t hdmaRemain{0x00};
    HDMAMode hdmaMode{HDMAMode::GDMA};
    bool hdmaActive{false};

    void WriteHDMA(uint16_t, uint8_t);
    [[nodiscard]] uint8_t ReadHDMA(uint16_t, bool) const;
};

#endif //STARGBC_HDMA_H