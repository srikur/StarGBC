#ifndef STARGBC_HDMA_H
#define STARGBC_HDMA_H

#include <cstdint>

enum class HDMAMode {
    GDMA, HDMA
};

enum class HDMAStep {
    Read, Write
};

struct HDMA {
    uint16_t hdmaSource{};
    uint16_t hdmaDestination{};
    uint8_t hdmaRemain{};
    uint8_t hdmaStartDelay{};
    uint8_t hdma5{};
    uint8_t bytesThisBlock{};
    uint8_t byte{};
    HDMAStep step{HDMAStep::Read};
    HDMAMode hdmaMode{HDMAMode::GDMA};
    bool hdmaActive{};
    bool hblankBlockFinished{};

    void WriteHDMA(uint16_t, uint8_t);
    [[nodiscard]] uint8_t ReadHDMA(uint16_t, bool) const;
};

#endif //STARGBC_HDMA_H
