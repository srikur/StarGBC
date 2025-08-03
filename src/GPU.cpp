#include <list>

#include "GPU.h"
#include "Common.h"

static constexpr uint8_t expand5(const uint8_t c) noexcept {
    return static_cast<uint8_t>((c << 3) | (c >> 2));
}

bool GPU::LCDDisabled() const {
    return !Bit<LCDC_ENABLE_BIT>(lcdc);
}

void GPU::ResetScanlineState(const bool clearBuffer) {
    backgroundQueue.clear();
    spriteArray.fill({.isPlaceholder = true});
    if (clearBuffer) spriteBuffer.clear();
    isFetchingWindow_ = false;
    fetcherTileX_ = 0;
    spriteFetchActive_ = false;
    fetcherState_ = FetcherState::GetTile;
    initialScrollXDiscard_ = scrollX % 8;
    currentSpriteIndex = 0;
    firstScanlineDataHigh = true;
    pixelsDrawn = 0;
    fetcherDelay_ = 0;
    spriteFetchQueue.clear();
}

void GPU::TickOAMScan() {
    if (!Bit<LCDC_OBJ_ENABLE>(lcdc)) return;
    if (!(scanlineCounter % 2)) return;
    const uint8_t index = scanlineCounter / 2;

    const uint16_t base = index * 4;
    const auto spriteY = static_cast<int16_t>(static_cast<int16_t>(oam[base]) - 16);
    const auto spriteX = static_cast<int16_t>(static_cast<int16_t>(oam[base + 1]) - 8);
    const uint8_t spriteTileIndex = oam[base + 2];
    const Attributes attr = GetAttrsFrom(oam[base + 3]);
    const uint8_t spriteSize = Bit<LCDC_OBJ_SIZE>(lcdc) ? 16 : 8;

    const bool cond1 = spriteX > -8;
    const bool cond2 = currentLine >= spriteY;
    const bool cond3 = currentLine < spriteY + spriteSize;
    const bool cond4 = spriteBuffer.size() < 10;
    if (cond1 && cond2 && cond3 && cond4) {
        spriteBuffer.push_back(Sprite{
            .spriteNum = index, .x = spriteX, .y = spriteY, .tileIndex = spriteTileIndex, .attributes = attr, .processed = false
        });
    }
}

void GPU::OutputPixel() {
    if (backgroundQueue.empty()) return;

    if (initialScrollXDiscard_ > 0) {
        backgroundQueue.pop_front();
        initialScrollXDiscard_--;
        return;
    }

    const auto bgPixel = backgroundQueue.front();
    backgroundQueue.pop_front();

    const auto spritePixel = spriteArray[0];
    for (int i = 0; i < spriteArray.size() - 1; i++) {
        spriteArray[i] = spriteArray[i + 1];
    }
    spriteArray[spriteArray.size() - 1] = {.isPlaceholder = true, .isSprite = true};

    bool backgroundWins = spritePixel.color == 0 || spritePixel.isPlaceholder;
    if (spritePixel.color != 0) {
        if (hardware == Hardware::CGB && !Bit<LCDC_BG_WINDOW_ENABLE>(lcdc)) {
            backgroundWins = false;
        } else if (bgPixel.priority) {
            backgroundWins = bgPixel.color != 0;
        } else {
            backgroundWins = spritePixel.priority && bgPixel.color != 0;
        }
    }

    const Pixel finalPixel = !backgroundWins ? spritePixel : bgPixel;

    const uint8_t palette = hardware == Hardware::CGB ? finalPixel.cgbPalette : finalPixel.dmgPalette;
    const uint32_t finalColor = finalPixel.isSprite
                                    ? GetSpriteColor(finalPixel.color, palette)
                                    : GetBackgroundColor(finalPixel.color, palette);

    screenData[currentLine * SCREEN_WIDTH + pixelsDrawn] = finalColor;
    pixelsDrawn++;
    CheckForWindowTrigger();
}

void GPU::TickMode3() {
    CheckForSpriteTrigger();
    if (spriteFetchActive_) {
        Fetcher_StepSpriteFetch();
    } else {
        Fetcher_StepBackgroundFetch();
    }
    if (!spriteFetchActive_) OutputPixel();
}

void GPU::CheckForSpriteTrigger() {
    if (!Bit<LCDC_OBJ_ENABLE>(lcdc) || spriteFetchActive_) return;
    for (auto &sprite: spriteBuffer) {
        if (!sprite.processed && pixelsDrawn == sprite.x) {
            sprite.processed = true;
            spriteFetchActive_ = true;
            spriteFetchQueue.push_back(sprite);
            fetcherState_ = FetcherState::GetTile;
        }
    }
}

void GPU::CheckForWindowTrigger() {
    if (Bit<LCDC_WINDOW_ENABLE>(lcdc) && !isFetchingWindow_) {
        const uint8_t effectiveWX = std::max(static_cast<uint8_t>(7), windowX);
        if (currentLine >= windowY && pixelsDrawn >= (effectiveWX - 7)) {
            isFetchingWindow_ = true;
            backgroundQueue.clear();
            fetcherState_ = FetcherState::GetTile;
            fetcherTileX_ = 0;
        }
    }
}

void GPU::Fetcher_StepBackgroundFetch() {
    if (spriteFetchActive_) return;
    if (fetcherDelay_ > 0) {
        fetcherDelay_--;
        return;
    }

    using enum FetcherState;
    switch (fetcherState_) {
        case GetTile: {
            const auto tileMapAddress = CalculateBGTileMapAddress();
            if (hardware == Hardware::CGB) backgroundTileAttributes_ = GetAttrsFrom(vram[tileMapAddress - 0x6000]);
            const bool bank1 = (hardware == Hardware::CGB) && backgroundTileAttributes_.vramBank;
            const uint16_t base = bank1 ? 0x6000 : 0x8000;
            fetcherTileNum_ = vram[tileMapAddress - base];
            fetcherState_ = GetTileDataLow;
            fetcherDelay_ = 1;
            break;
        }
        case GetTileDataLow: {
            const auto tileDataAddress = CalculateTileDataAddress();
            const bool bank1 = (hardware == Hardware::CGB) && backgroundTileAttributes_.vramBank;
            const uint16_t base = bank1 ? 0x6000 : 0x8000;
            fetcherTileDataLow_ = vram[tileDataAddress - base];
            fetcherState_ = GetTileDataHigh;
            fetcherDelay_ = 1;
            break;
        }
        case GetTileDataHigh: {
            const bool bank1 = (hardware == Hardware::CGB) && backgroundTileAttributes_.vramBank;
            const uint16_t base = bank1 ? 0x6000 : 0x8000;
            fetcherTileDataHigh_ = vram[(lastAddress_ + 1) - base];
            fetcherState_ = PushToFIFO;
            fetcherDelay_ = 1;
            if (firstScanlineDataHigh) {
                fetcherState_ = GetTile;
                firstScanlineDataHigh = false;
            }
            break;
        }
        case Sleep:
            fetcherState_ = PushToFIFO;
            fetcherDelay_ = 1;
            break;
        case PushToFIFO: {
            if (!backgroundQueue.empty()) break;
            for (int i = 0; i < 8; i++) {
                const uint8_t pixelBit = backgroundTileAttributes_.xflip ? i : 7 - i;
                const uint8_t bitLow = (fetcherTileDataLow_ >> pixelBit) & 1;
                const uint8_t bitHigh = (fetcherTileDataHigh_ >> pixelBit) & 1;
                const uint8_t color = (bitHigh << 1) | bitLow;

                backgroundQueue.push_back(Pixel{
                    .color = color,
                    .dmgPalette = backgroundPalette ? obp1Palette : obp0Palette,
                    .cgbPalette = backgroundTileAttributes_.paletteNumberCGB,
                    .priority = backgroundTileAttributes_.priority,
                    .isSprite = false,
                    .isPlaceholder = false,
                });
            }
            fetcherTileX_++;
            fetcherState_ = GetTile;
            fetcherDelay_ = 0;
            break;
        }
    }
}

void GPU::Fetcher_StepSpriteFetch() {
    if (fetcherDelay_ > 0) {
        fetcherDelay_--;
        return;
    }

    const Sprite &sprite = spriteFetchQueue.front();

    using enum FetcherState;
    switch (fetcherState_) {
        case GetTile: {
            fetcherTileNum_ = sprite.tileIndex;
            fetcherState_ = GetTileDataLow;
            fetcherDelay_ = 1;
            break;
        }
        case GetTileDataLow: {
            const auto tileAddress = CalculateSpriteDataAddress(sprite);
            const bool bank1 = (hardware == Hardware::CGB) && sprite.attributes.vramBank;
            const uint16_t base = bank1 ? 0x6000 : 0x8000;
            fetcherTileDataLow_ = vram[tileAddress - base];
            fetcherState_ = GetTileDataHigh;
            fetcherDelay_ = 1;
            break;
        }
        case GetTileDataHigh: {
            const bool bank1 = (hardware == Hardware::CGB) && sprite.attributes.vramBank;
            const uint16_t base = bank1 ? 0x6000 : 0x8000;
            fetcherTileDataHigh_ = vram[(lastAddress_ + 1) - base];
            fetcherDelay_ = 1;
            fetcherState_ = PushToFIFO;
            const Attributes attrs = sprite.attributes;
            const uint8_t paletteRegister = attrs.paletteNumberDMG ? obp1Palette : obp0Palette;

            const auto xPos = sprite.x;
            for (int i = xPos < 0 ? 8 + xPos : 0; i < 8; i++) {
                if (spriteArray[i].color != 0 && !spriteArray[i].isPlaceholder) continue;
                const auto pixelIndex = attrs.xflip ? i : 7 - i;
                const uint8_t bitLow = (fetcherTileDataLow_ >> pixelIndex) & 1;
                const uint8_t bitHigh = (fetcherTileDataHigh_ >> pixelIndex) & 1;
                const uint8_t color = (bitHigh << 1) | bitLow;

                spriteArray[i] = Pixel{
                    .color = color,
                    .dmgPalette = paletteRegister,
                    .cgbPalette = attrs.paletteNumberCGB,
                    .priority = attrs.priority,
                    .isSprite = true,
                    .isPlaceholder = false,
                };
            }
            spriteFetchQueue.pop_front();
            if (spriteFetchQueue.empty()) spriteFetchActive_ = false;
            fetcherState_ = GetTile;
            fetcherDelay_ = 0;
            break;
        }
        case Sleep: {
            fetcherState_ = PushToFIFO;
            fetcherDelay_ = 1;
            break;
        }
        case PushToFIFO: break;
    }
}

uint16_t GPU::CalculateBGTileMapAddress() const {
    uint16_t tileMapBase = 0;
    if (isFetchingWindow_) {
        tileMapBase = Bit<LCDC_WINDOW_TILE_MAP_AREA>(lcdc) ? 0x9C00 : 0x9800;
        const uint8_t yPos = windowLineCounter_ / 8;
        const uint8_t xPos = fetcherTileX_;
        return tileMapBase + yPos * 32 + xPos;
    } else {
        tileMapBase = Bit<LCDC_BG_TILE_MAP_AREA>(lcdc) ? 0x9C00 : 0x9800;
        const uint8_t yPos = ((scrollY + currentLine) / 8) & 0x1F;
        const uint8_t xPos = ((fetcherTileX_ * 8 + scrollX) / 8) & 0x1F;
        return tileMapBase + yPos * 32 + xPos;
    }
}

uint16_t GPU::CalculateTileDataAddress() {
    const uint8_t lineInTile = isFetchingWindow_ ? windowLineCounter_ % 8 : ((currentLine + scrollY) % 8);
    if (Bit<LCDC_BG_AND_WINDOW_TILE_DATA>(lcdc)) {
        const uint16_t address = 0x8000 + fetcherTileNum_ * 16 + lineInTile * 2;
        lastAddress_ = address;
        return address;
    } else {
        const auto signedTileNum = std::bit_cast<int8_t>(fetcherTileNum_);
        const uint16_t address = 0x9000 + signedTileNum * 16 + lineInTile * 2;
        lastAddress_ = address;
        return address;
    }
}

uint16_t GPU::CalculateSpriteDataAddress(const Sprite& sprite) {
    const uint8_t spriteHeight = Bit<LCDC_OBJ_SIZE>(lcdc) ? 16 : 8;
    if (Bit<LCDC_OBJ_SIZE>(lcdc)) fetcherTileNum_ &= 0xFE;
    const bool yFlip = sprite.attributes.yflip;
    const uint8_t yPos = sprite.y;
    const uint8_t tileY = yFlip
                              ? spriteHeight - 1 - (currentLine - yPos)
                              : currentLine - yPos;
    const uint16_t address = 0x8000 + fetcherTileNum_ * 16 + tileY * 2;
    lastAddress_ = address;
    return address;
}

uint32_t GPU::GetSpriteColor(const uint8_t color, const uint8_t palette) const {
    if (hardware != Hardware::CGB) return DMG_SHADE[(palette >> (color * 2)) & 0x03];

    const uint8_t red = obpd[palette][color][0];
    const uint8_t green = obpd[palette][color][1];
    const uint8_t blue = obpd[palette][color][2];

    const auto r5 = static_cast<uint8_t>(red & 0x1F);
    const auto g5 = static_cast<uint8_t>(green & 0x1F);
    const auto b5 = static_cast<uint8_t>(blue & 0x1F);

    const auto corrR5 = static_cast<uint8_t>((26 * r5 + 4 * g5 + 2 * b5) >> 5);
    const auto corrG5 = static_cast<uint8_t>((6 * r5 + 24 * g5 + 2 * b5) >> 5);
    const auto corrB5 = static_cast<uint8_t>((2 * r5 + 4 * g5 + 26 * b5) >> 5);

    const uint8_t r = expand5(corrR5);
    const uint8_t g = expand5(corrG5);
    const uint8_t b = expand5(corrB5);

    const uint32_t rgba = 0xFF000000u |
                          (static_cast<uint32_t>(b) << 16) |
                          (static_cast<uint32_t>(g) << 8) |
                          r;
    return rgba;
}

uint32_t GPU::GetBackgroundColor(const uint8_t color, const uint8_t palette) const {
    if (hardware != Hardware::CGB) return DMG_SHADE[(backgroundPalette >> (color * 2)) & 0x03];

    const uint8_t red = bgpd[palette][color][0];
    const uint8_t green = bgpd[palette][color][1];
    const uint8_t blue = bgpd[palette][color][2];

    const auto r5 = static_cast<uint8_t>(red & 0x1F);
    const auto g5 = static_cast<uint8_t>(green & 0x1F);
    const auto b5 = static_cast<uint8_t>(blue & 0x1F);

    const auto corrR5 = static_cast<uint8_t>((26 * r5 + 4 * g5 + 2 * b5) >> 5);
    const auto corrG5 = static_cast<uint8_t>((6 * r5 + 24 * g5 + 2 * b5) >> 5);
    const auto corrB5 = static_cast<uint8_t>((2 * r5 + 4 * g5 + 26 * b5) >> 5);

    const uint8_t r = expand5(corrR5);
    const uint8_t g = expand5(corrG5);
    const uint8_t b = expand5(corrB5);

    const uint32_t rgba = 0xFF000000u |
                          (static_cast<uint32_t>(b) << 16) |
                          (static_cast<uint32_t>(g) << 8) |
                          r;
    return rgba;
}

GPU::Attributes GPU::GetAttrsFrom(const uint8_t byte) {
    return {
        .priority = Bit<OAM_PRIORITY_BIT>(byte), .yflip = Bit<OAM_Y_FLIP_BIT>(byte),
        .xflip = Bit<OAM_X_FLIP_BIT>(byte), .paletteNumberDMG = Bit<OAM_PALETTE_NUMBER_DMG_BIT>(byte),
        .vramBank = Bit<OAM_VRAM_BANK_BIT>(byte), .paletteNumberCGB = static_cast<uint8_t>(byte & 0x07)
    };
}

uint8_t GPU::ReadGpi(const Gpi &gpi) {
    return (gpi.autoIncrement ? 0x80 : 0x00) | gpi.index | 0x40;
}

void GPU::WriteGpi(Gpi &gpi, const uint8_t value) {
    gpi.autoIncrement = (value & 0x80) != 0x00;
    gpi.index = value & 0x3F;
}

uint8_t GPU::ReadVRAM(const uint16_t address) const {
    return stat.mode == 3 ? 0x00 : vram[vramBank * 0x2000 + address - 0x8000];
}

void GPU::WriteVRAM(const uint16_t address, const uint8_t value) {
    if (stat.mode == 3) return;
    vram[vramBank * 0x2000 + address - 0x8000] = value;
}

uint8_t GPU::ReadRegisters(const uint16_t address) const {
    switch (address) {
        case 0xFF40: return lcdc;
        case 0xFF41: {
            const uint8_t bit6 = stat.enableLYInterrupt ? 0x40 : 0x00;
            const uint8_t bit5 = stat.enableM2Interrupt ? 0x20 : 0x00;
            const uint8_t bit4 = stat.enableM1Interrupt ? 0x10 : 0x00;
            const uint8_t bit3 = stat.enableM0Interrupt ? 0x08 : 0x00;
            const uint8_t bit2 = (currentLine == lyc) ? 0x04 : 0x00;
            return 0x80 | bit6 | bit5 | bit4 | bit3 | bit2 | stat.mode;
        }
        case 0xFF42: return scrollY;
        case 0xFF43: return scrollX;
        case 0xFF44: return currentLine;
        case 0xFF45: return lyc;
        case 0xFF47: return backgroundPalette;
        case 0xFF48: return obp0Palette;
        case 0xFF49: return obp1Palette;
        case 0xFF4A: return windowY;
        case 0xFF4B: return windowX;
        case 0xFF4F: return hardware == Hardware::CGB ? (0xFE | vramBank) : 0xFF;
        case 0xFF68: return ReadGpi(bgpi);
        case 0xFF69: {
            const uint8_t r = bgpi.index >> 3;
            const uint8_t c = (bgpi.index >> 1) & 3;
            if ((bgpi.index & 0x01) == 0) {
                return bgpd[r][c][0] | (bgpd[r][c][1] << 5);
            }
            return (bgpd[r][c][1] >> 3) | (bgpd[r][c][2] << 2);
        }
        case 0xFF6A: return ReadGpi(obpi);
        case 0xFF6B: {
            const uint8_t r = obpi.index >> 3;
            const uint8_t c = (obpi.index >> 1) & 3;
            if ((obpi.index & 0x01) == 0) {
                return obpd[r][c][0] | (obpd[r][c][1] << 5);
            }
            return (obpd[r][c][1] >> 3) | (obpd[r][c][2] << 2);
        }
        default:
            throw UnreachableCodeException("GPU::ReadRegisters unreachable code at address: " + std::to_string(address));
    }
}

void GPU::WriteRegisters(const uint16_t address, const uint8_t value) {
    switch (address) {
        case 0xFF40:
            lcdc = value;
            if (!(lcdc & 0x80)) {
                scanlineCounter = 0;
                currentLine = 0;
                stat.mode = 0;
                screenData.fill(0);
                vblank = true;
            }
            break;
        case 0xFF41:
            stat.enableLYInterrupt = value & 0x40;
            stat.enableM2Interrupt = value & 0x20;
            stat.enableM1Interrupt = value & 0x10;
            stat.enableM0Interrupt = value & 0x08;
            break;
        case 0xFF42: scrollY = value;
            break;
        case 0xFF43: scrollX = value;
            break;
        case 0xFF44: currentLine = 0;
            break;
        case 0xFF45: lyc = value;
            break;
        case 0xFF47: backgroundPalette = value;
            break;
        case 0xFF48: obp0Palette = value;
            break;
        case 0xFF49: obp1Palette = value;
            break;
        case 0xFF4A: windowY = value;
            break;
        case 0xFF4B: windowX = value;
            break;
        case 0xFF4F: vramBank = value & 0x01;
            break;
        case 0xFF68: WriteGpi(bgpi, value);
            break;
        case 0xFF69: {
            const uint8_t r = bgpi.index >> 3;
            const uint8_t c = (bgpi.index >> 1) & 0x03;
            if ((bgpi.index & 0x01) == 0) {
                bgpd[r][c][0] = value & 0x1F;
                bgpd[r][c][1] = (bgpd[r][c][1] & 0x18) | (value >> 5);
            } else {
                bgpd[r][c][1] = (bgpd[r][c][1] & 0x07) | ((value & 0x03) << 3);
                bgpd[r][c][2] = (value >> 2) & 0x1F;
            }
            if (bgpi.autoIncrement) bgpi.index = (bgpi.index + 1) & 0x3F;
            break;
        }
        case 0xFF6A: WriteGpi(obpi, value);
            break;
        case 0xFF6B: {
            const uint8_t r = obpi.index >> 3;
            const uint8_t c = (obpi.index >> 1) & 0x03;
            if ((obpi.index & 0x01) == 0) {
                obpd[r][c][0] = value & 0x1F;
                obpd[r][c][1] = (obpd[r][c][1] & 0x18) | (value >> 5);
            } else {
                obpd[r][c][1] = (obpd[r][c][1] & 0x07) | ((value & 0x03) << 3);
                obpd[r][c][2] = (value >> 2) & 0x1F;
            }
            if (obpi.autoIncrement) obpi.index = (obpi.index + 1) & 0x3F;
            break;
        }
        default:
            break;
    }
}
