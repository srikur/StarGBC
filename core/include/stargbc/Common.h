#pragma once

#include "../Model.h"
#include "../Reflection.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <memory>
#include <meta>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

// TODO: Move whole codebase to Google C++ style guide?

// TODO: Replace all type usages
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

static constexpr u8 SCREEN_WIDTH = 160;
static constexpr u8 SCREEN_HEIGHT = 144;
static constexpr u16 ROM_BANK_SIZE = 0x4000;
static constexpr u16 BOOT_ROM_BANK = 0xFFFF;
static constexpr u16 BOOT_ROM_LOW_END = 0x0100;
static constexpr u16 BOOT_ROM_HIGH_BEGIN = 0x0200;
static constexpr u16 VRAM_BEGIN = 0x8000;
static constexpr u16 VRAM_END = 0x9FFF;
static constexpr u16 VRAM_SIZE = 0x4000;
static constexpr u16 GPU_REGS_BEGIN = 0xFF40;
static constexpr u16 GPU_REGS_END = 0xFF4B;
static constexpr u16 OAM_BEGIN = 0xFE00;
static constexpr u16 OAM_END = 0xFE9F;
static constexpr u16 WRAM_BEGIN = 0xC000;
static constexpr u16 WRAM_END = 0xDFFF;
static constexpr u16 ERAM_BEGIN = 0xE000;
static constexpr u16 ERAM_END = 0xFDFF;
static constexpr u16 HIGH_RAM_BEGIN = 0xFF80;
static constexpr u16 INTERRUPT_ENABLE = 0xFFFF;
static constexpr u16 BACKGROUND_MAP_LOW = 0x9800;
static constexpr u16 BACKGROUND_MAP_HIGH = 0x9C00;

static constexpr u16 SERIAL_TRANSFER_DATA = 0xFF01;
static constexpr u16 SERIAL_TRANSFER_CONTROL = 0xFF02;
static constexpr u16 LCD_CONTROL_BIT = 0xFF40;
static constexpr u16 OAM_DMA_TRANSFER_REG = 0xFF46;
static constexpr u16 BACKGROUND_PALETTE_INDEX = 0xFF68;
static constexpr u16 BACKGROUND_PALETTE_DATA = 0xFF69;
static constexpr u16 OBJ_PALETTE_INDEX = 0xFF6A;
static constexpr u16 OBJ_PALETTE_DATA = 0xFF6B;

static constexpr u16 SOUND_PANNING = 0xFF25;
static constexpr u16 SOUND_CONTROL = 0xFF26;
static constexpr u16 WAVE_RAM_BEGIN = 0xFF30;
static constexpr u16 WAVE_RAM_END = 0xFF3F;

static constexpr u16 PULSE1_ENVELOPE = 0xFF12;
static constexpr u16 PULSE1_FREQUENCY_HIGH = 0xFF14;
static constexpr u16 MASTER_VOLUME = 0xFF24;

static constexpr u16 PULSE1_SWEEP = 0xFF10;
static constexpr u16 PULSE2_REGISTERS_BEGIN = 0xFF15;

static constexpr u16 INTERRUPT_FLAG = 0xFF0F;

static constexpr u16 LCD_STATUS = 0xFF41;
static constexpr u16 WINDOW_Y = 0xFF4A;
static constexpr u16 HDMA_SOURCE_HIGH = 0xFF51;
static constexpr u16 HDMA_SOURCE_LOW = 0xFF52;
static constexpr u16 HDMA_DESTINATION_HIGH = 0xFF53;
static constexpr u16 HDMA_DESTINATION_LOW = 0xFF54;
static constexpr u16 HDMA_CONTROL = 0xFF55;

static constexpr u16 WINDOW_X = 0xFF4B;

static constexpr u16 SCANLINE_COMPARE = 0xFF45;

static constexpr u16 VRAM_BANK = 0xFF4F;

static constexpr u16 CURRENT_SCANLINE = 0xFF44;

static constexpr u16 PULSE1_DUTY_LENGTH = 0xFF11;
static constexpr u16 PULSE2_DUTY_LENGTH = 0xFF16;
static constexpr u16 PULSE2_ENVELOPE = 0xFF17;
static constexpr u16 PULSE2_FREQUENCY_HIGH = 0xFF19;
static constexpr u16 WAVE_DAC_ENABLE = 0xFF1A;
static constexpr u16 WAVE_LENGTH = 0xFF1B;
static constexpr u16 WAVE_FREQUENCY_HIGH = 0xFF1E;
static constexpr u16 NOISE_LENGTH = 0xFF20;
static constexpr u16 NOISE_ENVELOPE = 0xFF21;
static constexpr u16 NOISE_CONTROL = 0xFF23;

static constexpr u16 DMG_BACKGROUND_PALETTE = 0xFF47;
static constexpr u16 DMG_OBJ_PALETTE_0 = 0xFF48;
static constexpr u16 DMG_OBJ_PALETTE_1 = 0xFF49;

static constexpr u16 SPEED_SWITCH = 0xFF4D;

static constexpr u8 OAM_PRIORITY_BIT = 7;
static constexpr u8 OAM_Y_FLIP_BIT = 6;
static constexpr u8 OAM_X_FLIP_BIT = 5;
static constexpr u8 OAM_PALETTE_NUMBER_DMG_BIT = 4;
static constexpr u8 OAM_VRAM_BANK_BIT = 3;

static constexpr u16 SCANLINE_CYCLES = 456;
static constexpr u8 MODE2_CYCLES = 80;

static constexpr u8 OBJ_TOTAL_SPRITES = 40;

// 0xFF40 -- LCD Control
static constexpr u8 LCDC_ENABLE_BIT = 7;
static constexpr u8 LCDC_WINDOW_TILE_MAP_AREA = 6;
static constexpr u8 LCDC_WINDOW_ENABLE = 5;
static constexpr u8 LCDC_BG_AND_WINDOW_TILE_DATA = 4;
static constexpr u8 LCDC_BG_TILE_MAP_AREA = 3;
static constexpr u8 LCDC_OBJ_SIZE = 2;
static constexpr u8 LCDC_OBJ_ENABLE = 1;
static constexpr u8 LCDC_BG_WINDOW_ENABLE = 0;

constexpr u8 operator""_u8(const unsigned long long value) { return static_cast<u8>(value); }

constexpr u16 operator""_u16(const unsigned long long value) { return static_cast<u16>(value); }

constexpr u32 operator""_u32(const unsigned long long value) { return static_cast<u32>(value); }

template<uint8_t bit>
static constexpr bool Bit(const u8 value) {
    static_assert(bit < 8);
    return value >> bit & 0x01;
}

enum class Keys {
    Right = 0x01,
    Left = 0x02,
    Up = 0x04,
    Down = 0x08,
    A = 0x10,
    B = 0x20,
    Select = 0x40,
    Start = 0x80,
};

enum class Speed { Regular = 0x01, Double = 0x02 };

enum class InterruptState { M1, M2, M3, M4, M5, M6 };

enum class InterruptType {
    VBlank,
    LCDStat,
    Timer,
    Serial,
    Joypad,
};

enum class ComponentSource { CPU, DMA, PPU, HDMA };

template<typename T>
concept BusLike = requires(T bus, u16 addr, u8 val, ComponentSource source) {
    { bus.ReadByte(addr, source) } -> std::same_as<uint8_t>;
    { bus.WriteByte(addr, val, source) } -> std::same_as<void>;
};

// Core exceptions carry diagnostics; applications decide how to report them.
class GameboyException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class UnreachableCodeException final : public GameboyException {
public:
    using GameboyException::GameboyException;
};

class FatalErrorException final : public GameboyException {
public:
    using GameboyException::GameboyException;
};
