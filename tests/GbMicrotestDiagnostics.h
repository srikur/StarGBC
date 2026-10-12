#pragma once

#include <array>
#include <optional>
#include <sstream>
#include "TestRoms.h"

// Early GBMicrotest experiments predate the FF80/FF81/FF82 protocol.
// Their result locations and completion loops are taken from the ROM source
enum class MicrotestOutput { Byte, Screen, Dma, Vram, Audio, LcdOff, Lockup };

struct MicrotestDiagnostic {
    std::string_view name;
    MicrotestOutput output;
    uint16_t address{};
    uint8_t expected{};
    uint16_t pcBegin{};
    uint16_t pcEnd{};
};

static constexpr MicrotestDiagnostic microtestDiagnostics[] = {
#include "MicrotestCases.inc"
};

class MicrotestDiagnosticCheck {
    const MicrotestDiagnostic &test_;
    std::vector<uint32_t> screen_;
    std::array<uint8_t, 0x2000> vram_{};

public:
    explicit MicrotestDiagnosticCheck(const MicrotestDiagnostic &test, const std::string &path) : test_(test) {
        if (test.output == MicrotestOutput::Screen)
            screen_ = readBinaryFile(path);
        if (test.output == MicrotestOutput::Vram) {
            std::ifstream file(path, std::ios::binary);
            if (!file.read(reinterpret_cast<char *>(vram_.data()), vram_.size())) {
                throw std::runtime_error("Could not read complete VRAM reference: " + path);
            }
        }
    }

    bool passed(Gameboy &gameboy, std::ostream &details) const {
        auto &bus = gameboy.[:testGameboyMember("bus_"):];
        const auto &gpu = gameboy.[:testGameboyMember("gpu_"):];
        auto &cpu = gameboy.[:testGameboyMember("cpu_"):];
        const auto &regs = gameboy.[:testGameboyMember("registers_"):];
        details << std::hex << " PC=" << cpu.pc();
        if (bus.bootromRunning)
            return false;
        switch (test_.output) {
            case MicrotestOutput::Byte: {
                // Inspect VRAM directly: the CPU can be in mode 3 when the
                // harness samples a value that the ROM already stored.
                const auto actual = test_.address == 0x8000 ? gpu.vram[0] : gameboy.DebugPeek(test_.address);
                details << " [" << test_.address << "]=" << +actual << " expected=" << +test_.expected;
                return actual == test_.expected && cpu.pc() >= test_.pcBegin && cpu.pc() <= test_.pcEnd;
            }
            case MicrotestOutput::Screen:
                details << " screen=" << test_.name;
                return std::ranges::equal(std::span(gameboy.GetScreenData(), screen_.size()), screen_);
            case MicrotestOutput::Dma:
                details << " OAM copy from 8000-809F";
                return cpu.pc() >= 0xFF82 && cpu.pc() <= 0xFF84 && !gpu.oamDmaActive && gpu.vram[0] == 0xFF && gpu.vram[0x9F] == 0xFF &&
                       std::ranges::equal(gpu.oam, std::span(gpu.vram.data(), gpu.oam.size()));
            case MicrotestOutput::Vram:
                details << " BC=" << regs.GetBC() << " HL=" << regs.GetHL() << " VRAM contents";
                return regs.GetBC() == 0x2000 && regs.GetHL() == 0xA000 && std::ranges::equal(vram_, std::span(gpu.vram.data(), vram_.size()));
            case MicrotestOutput::Audio: {
                constexpr std::array<uint8_t, 6> expected{0xF0, 0x5F, 0x77, 0xFF, 0xF9, 0xBF};
                constexpr std::array<uint16_t, 6> addresses{0xFF21, 0xFF22, 0xFF24, 0xFF25, 0xFF26, 0xFF23};
                bool match = cpu.pc() >= 0x0178 && cpu.pc() <= 0x017A;
                for (size_t i = 0; i < addresses.size(); ++i) {
                    const auto actual = gameboy.InspectRegister(addresses[i]).cpuVisible;
                    details << " [" << addresses[i] << "]=" << +actual;
                    match &= actual == expected[i];
                }
                return match;
            }
            case MicrotestOutput::LcdOff:
                details << " LCDC=" << +gpu.lcdc << " LY=" << +gpu.currentLine << " dot=" << gpu.scanlineCounter;
                return cpu.pc() >= 0x01A0 && cpu.pc() <= 0x01A2 && gpu.LCDDisabled() && gpu.currentLine == 0 && gpu.scanlineCounter == 0;
            case MicrotestOutput::Lockup:
                // temp has one NOP and no terminator. Execution falls through
                // the ROM into the boot logo tile data at 8014 (an illegal opcode).
                details << " locked=" << cpu.locked();
                return cpu.locked() && cpu.pc() == 0x8015;
        }
        return false;
    }
};
