#pragma once

#include "TestRoms.h"
#include <array>
#include <optional>
#include <sstream>

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
    {"000-oam_lock", MicrotestOutput::Byte, 0x8000, 0xFF, 0x019A, 0x01A0},
    {"000-write_to_x8000", MicrotestOutput::Byte, 0x8000, 0x55, 0x0150, 0x0158},
    {"001-vram_unlocked", MicrotestOutput::Byte, 0x8000, 0x55, 0x0169, 0x016D},
    {"002-vram_locked", MicrotestOutput::Byte, 0x8000, 0x84, 0x019B, 0x01A1},
    {"004-tima_boot_phase", MicrotestOutput::Byte, 0x8000, 0x55, 0x0165, 0x016B},
    {"004-tima_cycle_timer", MicrotestOutput::Byte, 0x8000, 0x55, 0x01BE, 0x01C4},
    {"007-lcd_on_stat", MicrotestOutput::Byte, 0x8000, 0x00, 0x004D, 0x004F},
    {"400-dma", MicrotestOutput::Screen},
    {"500-scx-timing", MicrotestOutput::Byte, 0x8000, 0x49, 0x018A, 0x018F},
    {"800-ppu-latch-scx", MicrotestOutput::Screen},
    {"801-ppu-latch-scy", MicrotestOutput::Screen},
    {"802-ppu-latch-tileselect", MicrotestOutput::Screen},
    {"803-ppu-latch-bgdisplay", MicrotestOutput::Screen},
    {"audio_testbench", MicrotestOutput::Audio},
    {"cpu_bus_1", MicrotestOutput::Byte, 0xFF80, 0x55, 0x0155, 0x0158},
    {"dma_basic", MicrotestOutput::Dma},
    {"flood_vram", MicrotestOutput::Vram},
    {"lcdon_write_timing", MicrotestOutput::Byte, 0x8000, 0x00, 0x01EC, 0x01F2},
    {"ly_while_lcd_off", MicrotestOutput::Byte, 0x8000, 0x00, 0x0160, 0x0166},
    {"minimal", MicrotestOutput::Byte, 0x8000, 0x49, 0x018A, 0x018F},
    {"mode2_stat_int_to_oam_unlock", MicrotestOutput::Byte, 0x8000, 0xFF, 0x0083, 0x0089},
    {"oam_sprite_trashing", MicrotestOutput::Screen},
    {"poweron", MicrotestOutput::Byte, 0x8000, 0x80, 0x0150, 0x0160},
    {"ppu_scx_vs_bgp", MicrotestOutput::Screen},
    {"ppu_sprite_testbench", MicrotestOutput::Screen},
    {"ppu_spritex_vs_scx", MicrotestOutput::Byte, 0x8000, 0x55, 0x376A, 0x3771},
    {"ppu_win_vs_wx", MicrotestOutput::Screen},
    {"ppu_wx_early", MicrotestOutput::Screen},
    {"toggle_lcdc", MicrotestOutput::LcdOff},
    {"temp", MicrotestOutput::Lockup},
    {"wave_write_to_0xC003", MicrotestOutput::Byte, 0xC003, 0x55, 0x0155, 0x0158},
};

class MicrotestDiagnosticCheck {
    const MicrotestDiagnostic &test_;
    std::vector<uint32_t> screen_;
    std::array<uint8_t, 0x2000> vram_{};

public:
    explicit MicrotestDiagnosticCheck(const MicrotestDiagnostic &test) : test_(test) {
        const auto path = "tests/expected/gbmicrotest/" + std::string(test.name);
        if (test.output == MicrotestOutput::Screen) screen_ = readBinaryFile(path + ".screen");
        if (test.output == MicrotestOutput::Vram) {
            std::ifstream file(path + ".vram", std::ios::binary);
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
        if (bus.bootromRunning) return false;
        switch (test_.output) {
            case MicrotestOutput::Byte: {
                // Inspect VRAM directly: the CPU can be in mode 3 when the
                // harness samples a value that the ROM already stored.
                const auto actual = test_.address == 0x8000 ? gpu.vram[0]
                    : bus.ReadByte(test_.address, ComponentSource::CPU);
                details << " [" << test_.address << "]=" << +actual << " expected=" << +test_.expected;
                return actual == test_.expected && cpu.pc() >= test_.pcBegin && cpu.pc() <= test_.pcEnd;
            }
            case MicrotestOutput::Screen:
                details << " screen=" << test_.name;
                return std::ranges::equal(std::span(gameboy.GetScreenData(), screen_.size()), screen_);
            case MicrotestOutput::Dma:
                details << " OAM copy from 8000-809F";
                return cpu.pc() >= 0xFF82 && cpu.pc() <= 0xFF84 && !gpu.oamDmaActive &&
                    gpu.vram[0] == 0xFF && gpu.vram[0x9F] == 0xFF &&
                    std::ranges::equal(gpu.oam, std::span(gpu.vram.data(), gpu.oam.size()));
            case MicrotestOutput::Vram:
                details << " BC=" << regs.GetBC() << " HL=" << regs.GetHL() << " VRAM contents";
                return regs.GetBC() == 0x2000 && regs.GetHL() == 0xA000 &&
                    std::ranges::equal(vram_, std::span(gpu.vram.data(), vram_.size()));
            case MicrotestOutput::Audio: {
                constexpr std::array<uint8_t, 6> expected{0xF0, 0x5F, 0x77, 0xFF, 0xF9, 0xBF};
                constexpr std::array<uint16_t, 6> addresses{0xFF21, 0xFF22, 0xFF24, 0xFF25, 0xFF26, 0xFF23};
                bool match = cpu.pc() >= 0x0178 && cpu.pc() <= 0x017A;
                for (size_t i = 0; i < addresses.size(); ++i) {
                    const auto actual = bus.ReadByte(addresses[i], ComponentSource::CPU);
                    details << " [" << addresses[i] << "]=" << +actual;
                    match &= actual == expected[i];
                }
                return match;
            }
            case MicrotestOutput::LcdOff:
                details << " LCDC=" << +gpu.lcdc << " LY=" << +gpu.currentLine << " dot=" << gpu.scanlineCounter;
                return cpu.pc() >= 0x01A0 && cpu.pc() <= 0x01A2 && gpu.LCDDisabled() &&
                    gpu.currentLine == 0 && gpu.scanlineCounter == 0;
            case MicrotestOutput::Lockup:
                // temp has one NOP and no terminator. Execution falls through
                // the ROM into the boot logo tile data at 8014 (an illegal opcode).
                details << " locked=" << cpu.locked();
                return cpu.locked() && cpu.pc() == 0x8015;
        }
        return false;
    }
};
