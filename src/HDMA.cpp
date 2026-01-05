#include "HDMA.h"
#include "Common.h"

void HDMA::WriteHDMA(const uint16_t address, const uint8_t value) {
    switch (address) {
        case 0xFF51: hdmaSource = (static_cast<uint16_t>(value) << 8) | (hdmaSource & 0xFF);
            break;
        case 0xFF52: hdmaSource = (hdmaSource & 0xFF00) | static_cast<uint16_t>(value & 0xF0);
            break;
        case 0xFF53: hdmaDestination = 0x8000 | (static_cast<uint16_t>(value & 0x1F) << 8) | (hdmaDestination & 0xFF);
            break;
        case 0xFF54: hdmaDestination = (hdmaDestination & 0xFF00) | static_cast<uint16_t>(value & 0xF0);
            break;
        case 0xFF55: {
            if (hdmaActive) {
                if (!Bit<7>(value)) {
                    hdmaActive = true;
                    hdma5 = 0x80 | value;
                } else {
                    // restart copy
                    hdma5 = hdmaRemain = value & 0x7F;
                }
                return;
            }
            hdmaActive = true;
            hdmaStartDelay = 4;
            bytesThisBlock = 0x10;
            step = HDMAStep::Read;
            hdma5 = hdmaRemain = value & 0x7F;
            hdmaMode = Bit<7>(value) ? HDMAMode::HDMA : HDMAMode::GDMA;
            break;
        }
        default: throw UnreachableCodeException("HDMA::WriteHDMA unreachable code");
    }
}

uint8_t HDMA::ReadHDMA(const uint16_t address, const bool gbc) const {
    switch (address) {
        /* Cycle-Accurate docs pg. 47 -- "Always returns FFh when read" */
        case 0xFF50 ... 0xFF54: return 0xFF;
        /* Cycle-Accurate docs pg. 47 -- "Returns FFh in DMG and GBC in DMG mode" */
        case 0xFF55: return !gbc || !hdmaActive ? 0xFF : hdma5;
        default: throw UnreachableCodeException("HDMA::ReadHDMA unreachable code at address: " + std::to_string(address));
    }
}
