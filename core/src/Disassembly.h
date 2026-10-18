#pragma once

#include <array>
#include <cstdint>
#include <string>

struct DisassembledInstruction {
    uint16_t address{};
    uint8_t size{1};
    std::array<uint8_t, 3> bytes{};
    std::string text;
    bool call{};
};

[[nodiscard]] DisassembledInstruction disassemble(uint16_t address, std::array<uint8_t, 3> bytes);
