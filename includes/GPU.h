#pragma once
#include <deque>

#include "Common.h"
#include "HDMA.h"
#include "Interrupts.h"

struct Pixel {
    uint8_t color{0x00};
    uint8_t dmgPalette{0x00};
    uint8_t cgbPalette{0x00};
    bool priority{false};
    bool isSprite{false};
    bool isPlaceholder{true};
    uint8_t spriteNum{0x00};
};

struct Attributes {
    bool priority{false};
    bool yflip{false};
    bool xflip{false};
    bool paletteNumberDMG{false};
    bool vramBank{false};
    uint8_t paletteNumberCGB{0x00};
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

// State for the pixel fetcher
enum class FetcherState {
    GetTile,
    GetTileDataLow,
    GetTileDataHigh,
    Sleep,
    PushToFIFO,
};

struct Gpi {
    uint8_t index;
    bool autoIncrement;
};

enum class GPUMode {
    MODE_0,
    MODE_1,
    MODE_2,
    MODE_3
};

enum class CorruptionType {
    Write,
    Read,
    ReadWrite,
};

struct Stat {
    bool enableLYInterrupt{false};
    bool enableM2Interrupt{false};
    bool enableM1Interrupt{false};
    bool enableM0Interrupt{false};
    bool coincidenceFlag{false};
    GPUMode mode{GPUMode::MODE_2};

    [[nodiscard]] uint8_t value() const {
        return 0x80 | enableLYInterrupt << 6 | enableM2Interrupt << 5 |
               enableM1Interrupt << 4 | enableM0Interrupt << 3 |
               coincidenceFlag << 2 | static_cast<uint8_t>(mode);
    }
};

class GPU {
public:
    explicit GPU(Interrupts &interrupts) : interrupts_(interrupts) {
    }

    static constexpr uint32_t DMG_SHADE[4] = {
        0xFFFFFFFFu, // FF FF FF FF
        0xFFC0C0C0u, // C0 C0 C0 FF
        0xFF606060u, // 60 60 60 FF
        0xFF000000u // 00 00 00 FF
    };

    std::deque<Pixel> backgroundQueue;
    std::deque<Sprite> spriteFetchQueue;
    std::array<Pixel, 8> spriteArray;

    bool windowTriggeredThisFrame{false};
    Sprite spriteToFetch_{};
    Attributes backgroundTileAttributes_{};

    FetcherState fetcherState_{FetcherState::GetTile};

    bool firstScanlineDataHigh{false};
    uint16_t lastAddress_{0x0000};
    bool spriteFetchActive_{false};
    bool isFetchingWindow_{false};
    uint8_t fetcherDelay_{0};
    uint8_t fetcherTileX_ = 0; // Current tile X-coordinate in the BG/Win map (0-31).
    uint8_t fetcherTileNum_ = 0; // The tile ID read from VRAM.
    uint8_t fetcherTileDataLow_ = 0; // The low byte of tile pixel data.
    uint8_t fetcherTileDataHigh_ = 0; // The high byte of tile pixel data.

    uint8_t windowLineCounter_{0x00};

    std::vector<Sprite> spriteBuffer{10};
    uint8_t initialScrollXDiscard_{0x00};
    uint8_t pixelsDrawn{0x00};
    bool objectPriority{false};
    bool initialSCXSet{false};

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

    HDMA hdma{};
    Hardware hardware = Hardware::DMG;

    void Update();

    void TickOAMScan();

    void TickMode3();

    void OutputPixel();

    void ResetScanlineState(bool clearBuffer);

    uint8_t GetOAMScanRow() const;

    [[nodiscard]] uint8_t ReadVRAM(uint16_t address) const;

    void WriteVRAM(uint16_t address, uint8_t value);

    [[nodiscard]] uint8_t ReadRegisters(uint16_t address) const;

    void WriteRegisters(uint16_t address, uint8_t value);

    const uint32_t *GetScreenData() const;

    bool SaveState(std::ofstream &stateFile) const;

    bool LoadState(std::ifstream &stateFile);

    [[nodiscard]] bool LCDDisabled() const;

private:
    Interrupts &interrupts_;

    void Fetcher_StepSpriteFetch();

    void Fetcher_StepBackgroundFetch();

    [[nodiscard]] uint16_t CalculateBGTileMapAddress() const;

    uint16_t CalculateTileDataAddress();

    uint16_t CalculateSpriteDataAddress(const Sprite &sprite);

    [[nodiscard]] uint32_t GetBackgroundColor(uint8_t color, uint8_t palette = 0) const;

    [[nodiscard]] uint32_t GetSpriteColor(uint8_t color, uint8_t palette) const;

    void CheckForSpriteTrigger();

    void CheckForWindowTrigger();

    static Attributes GetAttrsFrom(uint8_t byte);

    static uint8_t ReadGpi(const Gpi &gpi);

    static void WriteGpi(Gpi &gpi, uint8_t value);
};
