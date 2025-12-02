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
            if (hdmaActive && hdmaMode == HDMAMode::HDMA) {
                if (!Bit<7>(value)) {
                    hdmaActive = false;
                }
                return;
            }
            hdmaActive = true;
            hdmaRemain = value & 0x7F;
            if (Bit<7>(value)) {
                hdmaMode = HDMAMode::HDMA;
            } else {
                hdmaMode = HDMAMode::GDMA;
            }
            break;
        }
        default: throw UnreachableCodeException("HDMA::WriteHDMA unreachable code");
    }
}

uint8_t HDMA::ReadHDMA(const uint16_t address, const bool gbc) const {
    switch (address) {
        case 0xFF50 ... 0xFF54: return 0xFF;
        case 0xFF55: return gbc ? (hdmaRemain | (hdmaActive ? 0x00 : 0x80)) : 0xFF;
        default: throw UnreachableCodeException("Bus::ReadHDMA unreachable code at address: " + std::to_string(address));
    }
}
