#pragma once
#include <iostream>
#include <deque>

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

    static constexpr uint32_t DMG_SHADE[4] = {
        0xFFFFFFFFu, // FF FF FF FF
        0xFFC0C0C0u, // C0 C0 C0 FF
        0xFF606060u, // 60 60 60 FF
        0xFF000000u // 00 00 00 FF
    };

    enum class Hardware {
        DMG, CGB
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
        uint8_t spriteNum{0x00};
        int16_t x{0x00};
        int16_t y{0x00};
        uint8_t tileIndex{0x00};
        Attributes attributes{};
        bool processed{false};

        bool operator<(const Sprite &s) const {
            return x < s.x || spriteNum < s.spriteNum;
        }
    };

    struct Pixel {
        uint8_t color{0x00};
        uint8_t dmgPalette{0x00};
        uint8_t cgbPalette{0x00};
        bool priority{false};
        bool isSprite{false};
        bool isPlaceholder{true};
        uint8_t spriteNum{0x00};
    };

    std::deque<Pixel> backgroundQueue;
    std::deque<Sprite> spriteFetchQueue;
    std::array<Pixel, 8> spriteArray;
    uint8_t mode2counter{0x00};

    bool windowTriggeredThisFrame{false};
    bool spriteFetchPending_ = false;
    Sprite spriteToFetch_{};
    Attributes backgroundTileAttributes_{};

    // State for the pixel fetcher
    enum class FetcherState {
        GetTile,
        GetTileDataLow,
        GetTileDataHigh,
        Sleep,
        PushToFIFO,
    } fetcherState_ = FetcherState::GetTile;

    bool firstScanlineDataHigh{false};
    std::size_t spritesToFetchIndex_{0};
    uint16_t lastAddress_{0x0000};
    bool spriteFetchActive_{false};
    bool isFetchingWindow_{false};
    uint8_t fetcherDelay_{0};
    uint8_t fetcherTileX_ = 0; // Current tile X-coordinate in the BG/Win map (0-31).
    uint8_t fetcherTileNum_ = 0; // The tile ID read from VRAM.
    uint8_t fetcherTileDataLow_ = 0; // The low byte of tile pixel data.
    uint8_t fetcherTileDataHigh_ = 0; // The high byte of tile pixel data.

    uint8_t windowLineCounter_{0x00};

    std::vector<Sprite> spriteBuffer{};
    uint8_t initialScrollXDiscard_{0x00};
    uint8_t pixelsDrawn{0x00};
    uint32_t spritePenaltyBgTileMask_ = 0;
    bool objectPriority{false};
    bool initialSCXSet{false};

    struct Gpi {
        uint8_t index;
        bool autoIncrement;
    };

    enum class Mode {
        MODE_0,
        MODE_1,
        MODE_2,
        MODE_3
    };

    struct Stat {
        bool enableLYInterrupt{false};
        bool enableM2Interrupt{false};
        bool enableM1Interrupt{false};
        bool enableM0Interrupt{false};
        bool coincidenceFlag{false};
        Mode mode{Mode::MODE_2};

        [[nodiscard]] uint8_t value() const {
            return 0x80 | (enableLYInterrupt << 6) | (enableM2Interrupt << 5) |
                   (enableM1Interrupt << 4) | (enableM0Interrupt << 3) |
                   (coincidenceFlag << 2) | static_cast<uint8_t>(mode);
        }
    };

    std::vector<uint8_t> vram = std::vector<uint8_t>(VRAM_SIZE);
    std::array<uint32_t, SCREEN_HEIGHT * SCREEN_WIDTH * 3> screenData{};
    std::vector<uint8_t> oam = std::vector<uint8_t>(0xA0);
    uint8_t lyc = 0; // 0xFF45

    std::pair<bool, uint8_t> priority_[160];
    uint8_t lcdc = 0;
    Stat stat{}; // 0xFF41
    uint8_t currentLine = 0; // 0xFF44
    uint8_t windowX = 0; // 0xFF4B
    uint8_t windowY = 0; // 0xFF4A
    uint8_t backgroundPalette = 0; // 0xFF47
    uint8_t obp0Palette = 0; // 0xFF48
    uint8_t obp1Palette = 0; // 0xFF49

    uint8_t scrollX = 0; // 0xFF43
    uint8_t scrollY = 0; // 0xFF42
    uint32_t scanlineCounter = 0; // current dot in scanline

    bool vblank = false;
    bool statTriggered{false};

    // GBC
    bool hblank = false;
    Gpi bgpi{.index = 0x00, .autoIncrement = false}; // 0xFF68
    Gpi obpi{.index = 0x00, .autoIncrement = false}; // 0xFF6A
    uint8_t vramBank = 0;
    std::array<std::array<std::array<uint8_t, 3>, 4>, 8> bgpd = {}; // 0xFF69
    std::array<std::array<std::array<uint8_t, 3>, 4>, 8> obpd = {}; // 0xFF6B

    GPU() = default;

    void TickOAMScan();

    void TickMode3();

    void OutputPixel();

    void Fetcher_StepSpriteFetch();

    void Fetcher_StepBackgroundFetch();

    [[nodiscard]] uint16_t CalculateBGTileMapAddress() const;

    uint16_t CalculateTileDataAddress();

    uint16_t CalculateSpriteDataAddress(const Sprite &sprite);

    void ResetScanlineState(bool clearBuffer);

    [[nodiscard]] uint32_t GetBackgroundColor(uint8_t color, uint8_t palette = 0) const;

    [[nodiscard]] uint32_t GetSpriteColor(uint8_t color, uint8_t palette) const;

    void CheckForSpriteTrigger();

    void CheckForWindowTrigger();

    uint8_t GetOAMScanRow() const;

    static Attributes GetAttrsFrom(uint8_t byte);

    static uint8_t ReadGpi(const Gpi &gpi);

    static void WriteGpi(Gpi &gpi, uint8_t value);

    [[nodiscard]] uint8_t ReadVRAM(uint16_t address) const;

    void WriteVRAM(uint16_t address, uint8_t value);

    [[nodiscard]] uint8_t ReadRegisters(uint16_t address) const;

    void WriteRegisters(uint16_t address, uint8_t value);

    [[nodiscard]] bool LCDDisabled() const;

    enum class HDMAMode {
        GDMA, HDMA
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
