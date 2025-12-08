#include "Serial.h"

uint8_t Serial::ReadSerial(const uint16_t address) const {
    switch (address) {
        case 0xFF01: return data_;
        case 0xFF02: return control_ | 0x7C;
        default: throw UnreachableCodeException("Improper read from serial");
    }
}

void Serial::WriteSerial(const uint16_t address, const uint8_t value, const bool doubleSpeed, const bool gbc) {
    switch (address) {
        case 0xFF01:
            data_ = value;
            break;
        case 0xFF02:
            control_ = value | 0x7E;
            if ((control_ & 0x81) == 0x81) {
                ticksPerBit_ = doubleSpeed ? 256 : 512;
                if (gbc && control_ & 0x02) ticksPerBit_ /= 32;
                ticksUntilShift_ = ticksPerBit_ - 47;
                bitsShifted_ = 0;
                active_ = true;
            }
            break;
        default:
            throw UnreachableCodeException("Improper write to serial");
    }
}

void Serial::ShiftOneBit() {
    data_ = static_cast<uint8_t>(data_ << 1 | 0x01);
}

void Serial::Update() {
    if (!active_) return;

    if (--ticksUntilShift_ == 0) {
        ShiftOneBit();
        if (++bitsShifted_ == 8) {
            active_ = false;
            control_ &= 0x7F;
            interrupts_.Set(InterruptType::Serial, false);
            return;
        }
        ticksUntilShift_ = ticksPerBit_;
    }
}

bool Serial::SaveState(std::ofstream &stateFile) const {
    try {
        if (!stateFile.is_open()) return false;
        stateFile.write(reinterpret_cast<const char *>(&data_), sizeof(data_));
        stateFile.write(reinterpret_cast<const char *>(&control_), sizeof(control_));
        stateFile.write(reinterpret_cast<const char *>(&active_), sizeof(active_));
        stateFile.write(reinterpret_cast<const char *>(&bitsShifted_), sizeof(bitsShifted_));
        stateFile.write(reinterpret_cast<const char *>(&ticksUntilShift_), sizeof(ticksUntilShift_));
        return true;
    } catch ([[maybe_unused]] const std::exception &e) {
        return false;
    }
}

bool Serial::LoadState(std::ifstream &stateFile) {
    try {
        if (!stateFile.is_open()) return false;
        stateFile.read(reinterpret_cast<char *>(&data_), sizeof(data_));
        stateFile.read(reinterpret_cast<char *>(&control_), sizeof(control_));
        stateFile.read(reinterpret_cast<char *>(&active_), sizeof(active_));
        stateFile.read(reinterpret_cast<char *>(&bitsShifted_), sizeof(bitsShifted_));
        stateFile.read(reinterpret_cast<char *>(&ticksUntilShift_), sizeof(ticksUntilShift_));
        return true;
    } catch ([[maybe_unused]] const std::exception &e) {
        return false;
    }
}
