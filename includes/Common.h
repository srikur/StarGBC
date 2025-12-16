#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <fstream>
#include <memory>
#include <print>
#include <string>
#include <utility>
#include <vector>

static constexpr uint8_t SCREEN_WIDTH = 160;
static constexpr uint8_t SCREEN_HEIGHT = 144;
static constexpr uint16_t VRAM_BEGIN = 0x8000;
static constexpr uint16_t VRAM_END = 0x9FFF;
static constexpr uint16_t VRAM_SIZE = 0x4000;
static constexpr uint16_t GPU_REGS_BEGIN = 0xFF40;
static constexpr uint16_t GPU_REGS_END = 0xFF4B;
static constexpr uint16_t OAM_BEGIN = 0xFE00;
static constexpr uint16_t OAM_END = 0xFE9F;

static constexpr uint8_t OAM_PRIORITY_BIT = 7;
static constexpr uint8_t OAM_Y_FLIP_BIT = 6;
static constexpr uint8_t OAM_X_FLIP_BIT = 5;
static constexpr uint8_t OAM_PALETTE_NUMBER_DMG_BIT = 4;
static constexpr uint8_t OAM_VRAM_BANK_BIT = 3;

static constexpr uint16_t SCANLINE_CYCLES = 456;
static constexpr uint8_t MODE2_CYCLES = 80;

static constexpr uint8_t OBJ_TOTAL_SPRITES = 40;

// 0xFF40 -- LCD Control
static constexpr uint8_t LCDC_ENABLE_BIT = 7;
static constexpr uint8_t LCDC_WINDOW_TILE_MAP_AREA = 6;
static constexpr uint8_t LCDC_WINDOW_ENABLE = 5;
static constexpr uint8_t LCDC_BG_AND_WINDOW_TILE_DATA = 4;
static constexpr uint8_t LCDC_BG_TILE_MAP_AREA = 3;
static constexpr uint8_t LCDC_OBJ_SIZE = 2;
static constexpr uint8_t LCDC_OBJ_ENABLE = 1;
static constexpr uint8_t LCDC_BG_WINDOW_ENABLE = 0;

enum class Hardware {
    DMG, CGB, MGB, SGB, SGB2, GBA, GBS,
};

template<uint8_t bit>
static constexpr bool Bit(const uint8_t value) {
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

enum class Mode {
    None = 0x00,
    DMG = 0x01,
    MBG = 0x02,
    SGB = 0x03,
    SGB2 = 0x04,
    CGB_DMG = 0x05,
    AGB_DMG = 0x06,
    AGS_DMG = 0x07,
    CGB_GBC = 0x08,
    AGB_GBC = 0x09,
    AGS_GBC = 0x0A,
};

enum class Speed {
    Regular = 0x01,
    Double = 0x02
};

enum class InterruptState {
    M1, M2, M3, M4, M5, M6
};

enum class InterruptType {
    VBlank,
    LCDStat,
    Timer,
    Serial,
    Joypad,
};

template<typename T>
concept BusLike = requires(T bus, uint16_t addr, uint8_t val)
{
    { bus.ReadByte(addr) } -> std::same_as<uint8_t>;
    { bus.WriteByte(addr, val) } -> std::same_as<void>;
};

class GameboyException : public std::exception {
    virtual void message() const noexcept {
        std::printf("Gameboy Exception!\n");
        std::printf("------------------\n");
    }
};

class UnreachableCodeException final : public GameboyException {
    void message() const noexcept override {
        std::printf("Type: Unreachable Code\n");
        std::printf("----------------------\n");
        std::printf("Message: %s\n", message_.c_str());
    }

    std::string message_;

    UnreachableCodeException() = default;

public:
    explicit UnreachableCodeException(std::string message) : message_(std::move(message)) {
        this->message();
    }
};

class FatalErrorException final : public GameboyException {
    void message() const noexcept override {
        std::printf("Type: Fatal Error");
        std::printf("-----------------");
        std::printf("Message: %s\n", message_.c_str());
    }

    std::string message_;

    FatalErrorException() = default;

public:
    explicit FatalErrorException(std::string message) : message_(std::move(message)) {
        this->message();
    }
};
