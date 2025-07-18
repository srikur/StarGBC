#pragma once
#include <iostream>

#include "Common.h"

class GPU {
public:
    static constexpr uint8_t SCREEN_WIDTH = 160;
    static constexpr uint8_t SCREEN_HEIGHT = 144;
    static constexpr uint16_t VRAM_BEGIN = 0x8000;
    static constexpr uint16_t VRAM_END = 0x9FFF;
    static constexpr uint16_t VRAM_SIZE = 0x4000;
    static constexpr uint16_t GPU_REGS_BEGIN = 0xFF40;
    static constexpr uint16_t GPU_REGS_END = 0xFF4B;
    static constexpr uint16_t OAM_BEGIN = 0xFE00;
    static constexpr uint16_t OAM_END = 0xFE9F;

    static constexpr uint32_t DMG_SHADE[4] = {
        0xFFFFFFFFu, // FF FF FF FF
        0xFFC0C0C0u, // C0 C0 C0 FF
        0xFF606060u, // 60 60 60 FF
        0xFF000000u // 00 00 00 FF
    };

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

        [[nodiscard]] uint8_t value() const {
            return (enableLYInterrupt << 6) | (enableM2Interrupt << 5) |
                   (enableM1Interrupt << 4) | (enableM0Interrupt << 3) | mode;
        }
    };

    std::vector<uint8_t> vram = std::vector<uint8_t>(VRAM_SIZE);
    std::array<uint32_t, SCREEN_HEIGHT * SCREEN_WIDTH * 3> screenData{};
    std::vector<uint8_t> oam = std::vector<uint8_t>(0xA0);
    uint8_t lyc = 0; // 0xFF45

    std::pair<bool, uint8_t> priority_[160];
    uint8_t lcdc = 0;
    Stat stat{
        .enableLYInterrupt = false,
        .enableM2Interrupt = false,
        .enableM1Interrupt = false,
        .enableM0Interrupt = false,
        .mode = 2
    }; // 0xFF41
    uint8_t currentLine = 0; // 0xFF44
    uint8_t windowX = 0; // 0xFF4B
    uint8_t windowY = 0; // 0xFF4A
    uint8_t backgroundPalette = 0; // 0xFF47
    uint8_t obp0Palette = 0; // 0xFF48
    uint8_t obp1Palette = 0; // 0xFF49

    uint8_t scrollX = 0; // 0xFF43
    uint8_t scrollY = 0; // 0xFF42
    uint32_t scanlineCounter = 0; // 0xFF44 -- LY

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

    [[nodiscard]] uint8_t ReadVRAM(uint16_t address) const;

    void WriteVRAM(uint16_t address, uint8_t value);

    [[nodiscard]] uint8_t ReadRegisters(uint16_t address) const;

    void WriteRegisters(uint16_t address, uint8_t value);

    bool LCDEnabled() const;

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

    bool SaveState(std::ofstream &stateFile) const {
        try {
            stateFile.write(reinterpret_cast<const char *>(&lcdc), sizeof(lcdc));
            stateFile.write(reinterpret_cast<const char *>(&stat), sizeof(stat));

            stateFile.write(reinterpret_cast<const char *>(vram.data()), vram.size());
            stateFile.write(reinterpret_cast<const char *>(oam.data()), oam.size());
            stateFile.write(reinterpret_cast<const char *>(screenData.data()), screenData.size());
            stateFile.write(reinterpret_cast<const char *>(priority_), sizeof(priority_));
            stateFile.write(reinterpret_cast<const char *>(&lyc), sizeof(lyc));
            stateFile.write(reinterpret_cast<const char *>(&currentLine), sizeof(currentLine));
            stateFile.write(reinterpret_cast<const char *>(&windowX), sizeof(windowX));
            stateFile.write(reinterpret_cast<const char *>(&windowY), sizeof(windowY));
            stateFile.write(reinterpret_cast<const char *>(&backgroundPalette), sizeof(backgroundPalette));
            stateFile.write(reinterpret_cast<const char *>(&obp0Palette), sizeof(obp0Palette));
            stateFile.write(reinterpret_cast<const char *>(&obp1Palette), sizeof(obp1Palette));
            stateFile.write(reinterpret_cast<const char *>(&scrollX), sizeof(scrollX));
            stateFile.write(reinterpret_cast<const char *>(&scrollY), sizeof(scrollY));
            stateFile.write(reinterpret_cast<const char *>(&scanlineCounter), sizeof(scanlineCounter));
            stateFile.write(reinterpret_cast<const char *>(&vblank), sizeof(vblank));
            stateFile.write(reinterpret_cast<const char *>(&hblank), sizeof(hblank));
            stateFile.write(reinterpret_cast<const char *>(&bgpi), sizeof(bgpi));
            stateFile.write(reinterpret_cast<const char *>(&obpi), sizeof(obpi));
            stateFile.write(reinterpret_cast<const char *>(&vramBank), sizeof(vramBank));
            stateFile.write(reinterpret_cast<const char *>(&bgpd), sizeof(bgpd));
            stateFile.write(reinterpret_cast<const char *>(&obpd), sizeof(obpd));
        } catch (const std::exception &e) {
            std::cerr << "Error saving GPU state: " << e.what() << '\n';
            return false;
        }
        return true;
    }

    bool LoadState(std::ifstream &stateFile) {
        try {
            stateFile.read(reinterpret_cast<char *>(&lcdc), sizeof(lcdc));
            stateFile.read(reinterpret_cast<char *>(&stat), sizeof(stat));
            stateFile.read(reinterpret_cast<char *>(vram.data()), vram.size());
            stateFile.read(reinterpret_cast<char *>(oam.data()), oam.size());
            stateFile.read(reinterpret_cast<char *>(screenData.data()), screenData.size());
            stateFile.read(reinterpret_cast<char *>(priority_), sizeof(priority_));
            stateFile.read(reinterpret_cast<char *>(&lyc), sizeof(lyc));
            stateFile.read(reinterpret_cast<char *>(&currentLine), sizeof(currentLine));
            stateFile.read(reinterpret_cast<char *>(&windowX), sizeof(windowX));
            stateFile.read(reinterpret_cast<char *>(&windowY), sizeof(windowY));
            stateFile.read(reinterpret_cast<char *>(&backgroundPalette), sizeof(backgroundPalette));
            stateFile.read(reinterpret_cast<char *>(&obp0Palette), sizeof(obp0Palette));
            stateFile.read(reinterpret_cast<char *>(&obp1Palette), sizeof(obp1Palette));
            stateFile.read(reinterpret_cast<char *>(&scrollX), sizeof(scrollX));
            stateFile.read(reinterpret_cast<char *>(&scrollY), sizeof(scrollY));
            stateFile.read(reinterpret_cast<char *>(&scanlineCounter), sizeof(scanlineCounter));
            stateFile.read(reinterpret_cast<char *>(&vblank), sizeof(vblank));
            stateFile.read(reinterpret_cast<char *>(&hblank), sizeof(hblank));
            stateFile.read(reinterpret_cast<char *>(&bgpi), sizeof(bgpi));
            stateFile.read(reinterpret_cast<char *>(&obpi), sizeof(obpi));
            stateFile.read(reinterpret_cast<char *>(&vramBank), sizeof(vramBank));
            stateFile.read(reinterpret_cast<char *>(&bgpd), sizeof(bgpd));
            stateFile.read(reinterpret_cast<char *>(&obpd), sizeof(obpd));
        } catch (const std::exception &e) {
            std::cerr << "Error loading GPU state: " << e.what() << '\n';
            return false;
        }
        return true;
    }
};
