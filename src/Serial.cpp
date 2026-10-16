#include "Serial.h"

uint8_t Serial::read_serial(const uint16_t address) const {
    switch (address) {
        case SERIAL_TRANSFER_DATA:
            return data;
        case SERIAL_TRANSFER_CONTROL:
            return control | 0x7C;
        default:
            throw UnreachableCodeException("Improper read from serial");
    }
}

void Serial::write_serial(const uint16_t address, const uint8_t value, const bool gbc) {
    switch (address) {
        case SERIAL_TRANSFER_DATA:
            data = value;
            break;
        case SERIAL_TRANSFER_CONTROL:
            control = value & 0x83;
            ticks_per_bit = gbc && (control & 0x02) ? 16 : 512;
            clock_high = false;
            bits_shifted = 0;
            active = (control & 0x81) == 0x81;
            break;
        default:
            // TODO: completely remove exceptions from core; mark functions noexcept
            throw UnreachableCodeException("Improper write to serial");
    }
}

void Serial::shift_one_bit() { data = static_cast<uint8_t>(data << 1 | 0x01); }

void Serial::clock_edge() {
    clock_high = !clock_high;
    if (clock_high)
        return;
    shift_one_bit();
    if (++bits_shifted == 8) {
        active = false;
        control &= 0x7F;
        interrupts.set(InterruptType::Serial, false);
    }
}

bool Serial::save_state(std::ofstream &stateFile) const {
    try {
        if (!stateFile.is_open())
            return false;
        stateFile.write(reinterpret_cast<const char *>(&data), sizeof(data));
        stateFile.write(reinterpret_cast<const char *>(&control), sizeof(control));
        stateFile.write(reinterpret_cast<const char *>(&active), sizeof(active));
        stateFile.write(reinterpret_cast<const char *>(&bits_shifted), sizeof(bits_shifted));
        stateFile.write(reinterpret_cast<const char *>(&ticks_per_bit), sizeof(ticks_per_bit));
        stateFile.write(reinterpret_cast<const char *>(&clock_high), sizeof(clock_high));
        return true;
    } catch ([[maybe_unused]] const std::exception &e) {
        return false;
    }
}

bool Serial::load_state(std::ifstream &stateFile) {
    try {
        if (!stateFile.is_open())
            return false;
        stateFile.read(reinterpret_cast<char *>(&data), sizeof(data));
        stateFile.read(reinterpret_cast<char *>(&control), sizeof(control));
        stateFile.read(reinterpret_cast<char *>(&active), sizeof(active));
        stateFile.read(reinterpret_cast<char *>(&bits_shifted), sizeof(bits_shifted));
        stateFile.read(reinterpret_cast<char *>(&ticks_per_bit), sizeof(ticks_per_bit));
        stateFile.read(reinterpret_cast<char *>(&clock_high), sizeof(clock_high));
        return true;
    } catch ([[maybe_unused]] const std::exception &e) {
        return false;
    }
}
