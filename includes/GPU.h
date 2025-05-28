#pragma once
#include "Common.h"

class GPU {
public:
    static constexpr uint16_t VRAM_BEGIN = 0x8000;
    static constexpr uint16_t VRAM_END = 0x9FFF;
    static constexpr uint16_t VRAM_SIZE = 0x4000;
    static constexpr uint16_t GPU_REGS_BEGIN = 0xFF40;
    static constexpr uint16_t GPU_REGS_END = 0xFF4B;
    static constexpr uint16_t OAM_BEGIN = 0xFE00;
    static constexpr uint16_t OAM_END = 0xFE9F;

    struct Attributes {
        bool priority;
        bool yflip;
        bool xflip;
        bool paletteNumberDMG;
        bool vramBank;
        uint8_t paletteNumberCGB;
    };

    struct Sprite {
        uint16_t spriteNum;
        uint8_t x;
        uint8_t y;

        explicit Sprite(const uint16_t _num = 0, const int _x = 0, const int _y = 0) : spriteNum(_num), x(_x), y(_y) {
        }

        bool operator<(const Sprite &s) const {
            return x < s.x || spriteNum < s.spriteNum;
        }
    };

    struct Gpi {
        uint8_t index;
        bool autoIncrement;
    };

    struct Stat {
        bool enableLYInterrupt;
        bool enableM2Interrupt;
        bool enableM1Interrupt;
        bool enableM0Interrupt;
        uint8_t mode;
    };

    std::vector<uint8_t> vram = std::vector<uint8_t>(VRAM_SIZE);
    std::vector<std::vector<std::vector<uint32_t> > > screenData = std::vector(
        144, std::vector(160, std::vector<uint32_t>(3)));
    std::vector<uint8_t> oam = std::vector<uint8_t>(0xA0);
    uint8_t lyc = 0; // 0xFF45

    std::pair<bool, uint8_t> priority_[160];
    uint8_t lcdc = 0;
    Stat stat{
        .enableLYInterrupt = false,
        .enableM2Interrupt = false,
        .enableM1Interrupt = false,
        .enableM0Interrupt = false,
        .mode = 0
    }; // 0xFF41
    uint8_t currentLine = 0; // 0xFF44
    uint8_t windowX = 0; // 0xFF4B
    uint8_t windowY = 0; // 0xFF4A
    uint8_t backgroundPalette = 0; // 0xFF47
    uint8_t obp0Palette = 0; // 0xFF48
    uint8_t obp1Palette = 0; // 0xFF49

    uint8_t scrollX = 0; // 0xFF43
    uint8_t scrollY = 0; // 0xFF42
    uint32_t scanlineCounter = 456;

    bool vblank = false;

    // GBC
    bool hblank = false;
    Gpi bgpi{.index = 0x00, .autoIncrement = false}; // 0xFF68
    Gpi obpi{.index = 0x00, .autoIncrement = false}; // 0xFF6A
    uint8_t vramBank = 0;
    std::array<std::array<std::array<uint8_t, 3>, 4>, 8> bgpd = {}; // 0xFF69
    std::array<std::array<std::array<uint8_t, 3>, 4>, 8> obpd = {}; // 0xFF6B

    GPU() = default;

    void DrawScanline();

    void RenderSprites();

    void RenderTiles();

    void SetColor(uint8_t pixel, uint32_t red, uint32_t green, uint32_t blue);

    static Attributes GetAttrsFrom(uint8_t byte);

    static uint8_t ReadGpi(const Gpi &gpi);

    static void WriteGpi(Gpi &gpi, uint8_t value);

    uint8_t ReadVRAM(uint16_t address) const;

    void WriteVRAM(uint16_t address, uint8_t value);

    uint8_t ReadRegisters(uint16_t address) const;

    void WriteRegisters(uint16_t address, uint8_t value);

    enum class HDMAMode {
        GDMA, HDMA
    };

    enum class Hardware {
        DMG, CGB
    };

    struct HDMA {
        uint16_t source;
        uint16_t destination;
        bool active;
        HDMAMode mode;
        uint8_t remain;
    };

    HDMAMode hdmaMode = HDMAMode::GDMA;
    Hardware hardware = Hardware::DMG;
};
