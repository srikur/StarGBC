#include <list>

#include "GPU.h"
#include "Common.h"

void GPU::DrawScanline() {
    if (lcdc & 0x01) RenderTiles();
    if (lcdc & 0x02) RenderSprites();
}

void GPU::RenderSprites() {
    std::list<Sprite> sprites;
    const uint8_t spriteSize = (lcdc & 0x04) ? 16 : 8;

    for (int i = 0; i < 40; ++i) {
        uint8_t yPos = oam[i * 4] - 16;
        uint8_t xPos = oam[i * 4 + 1] - 8;
        if (yPos <= currentLine && (yPos + spriteSize) > currentLine)
            sprites.emplace_back(i, xPos, yPos);
    }
    // if (hardware != Hardware::CGB) sprites.sort();
    if (sprites.size() > 10) sprites.resize(10);
    sprites.reverse();

    for (const Sprite &sprite: sprites) {
        const uint16_t spriteAddress = sprite.spriteNum * 4;
        const uint8_t yPos = oam[spriteAddress] - 16;
        const uint8_t xPos = oam[spriteAddress + 1] - 8;

        uint8_t tileNumber = oam[spriteAddress + 2];
        if (lcdc & 0x04) tileNumber &= 0xFE; // 8×16 OBJ mode

        const auto &[priority, yflip, xflip,
                    paletteNumberDMG, vramBank, paletteNumberCGB] =
                GetAttrsFrom(oam[spriteAddress + 3]);

        // Y range check, including wrap‑around sprites
        if (yPos > (0xFF - spriteSize + 1)) {
            if (currentLine > (yPos + spriteSize - 1)) continue;
        } else if (currentLine < yPos || currentLine > (yPos + spriteSize - 1)) {
            continue;
        }
        if (xPos >= 160 && xPos <= (0xFF - 7)) continue;

        const uint8_t tileY = yflip
                                  ? spriteSize - 1 - (currentLine - yPos)
                                  : currentLine - yPos;
        const uint16_t tileYAddr = 0x8000 + tileNumber * 16 + tileY * 2;

        const bool bank1 = (hardware == Hardware::CGB) && vramBank;
        const uint16_t base = bank1 ? 0x6000 : 0x8000;
        const uint8_t data1 = vram[tileYAddr - base];
        const uint8_t data2 = vram[tileYAddr + 1 - base];

        for (uint8_t pixel = 0; pixel < 8; ++pixel) {
            if (xPos + pixel >= 160) continue;

            const uint8_t tileX = xflip ? 7 - pixel : pixel;
            const uint8_t colorLow = (data1 & (0x80 >> tileX)) ? 1 : 0;
            const uint8_t colorHigh = (data2 & (0x80 >> tileX)) ? 2 : 0;
            const uint8_t color = colorHigh | colorLow;
            if (!color) continue; // colour 0 is transparent

            auto [bgPrio, bgCol] = priority_[xPos + pixel];

            bool skip = false;
            if (hardware == Hardware::CGB && !(lcdc & 0x01)) {
                skip = false; // OBJ always wins when BG disabled
            } else if (bgPrio) {
                skip = bgCol != 0;
            } else {
                skip = priority && bgCol != 0;
            }
            if (skip) continue;

            if (hardware == Hardware::CGB) {
                const uint8_t r = obpd[paletteNumberCGB][color][0];
                const uint8_t g = obpd[paletteNumberCGB][color][1];
                const uint8_t b = obpd[paletteNumberCGB][color][2];
                SetColor(xPos + pixel, r, g, b);
            } else {
                const uint8_t shade = [&] {
                    switch (const uint8_t src = paletteNumberDMG ? obp1Palette : obp0Palette;
                        (src >> (color * 2)) & 0x03) {
                        case 0: return DMG_SHADE[0];
                        case 1: return DMG_SHADE[1];
                        case 2: return DMG_SHADE[2];
                        default: return DMG_SHADE[3];
                    }
                }();
                screenData[currentLine * SCREEN_WIDTH + (xPos + pixel)] = shade;
            }
        }
    }
}

void GPU::RenderTiles() {
    const bool usingWindow = (lcdc & 0x20) && (windowY <= currentLine);
    const uint16_t tileData = (lcdc & 0x10) ? 0x8000 : 0x8800;
    const uint8_t wndwX = windowX - 7;

    const uint8_t yPos = usingWindow
                             ? currentLine - windowY
                             : scrollY + currentLine;
    const uint16_t tileRow = (yPos >> 3) & 0x1F;

    for (int pixel = 0; pixel < 160; ++pixel) {
        const bool windowOnThisPixel = usingWindow && (pixel >= wndwX);
        const uint8_t xPos = windowOnThisPixel
                                 ? pixel - wndwX
                                 : scrollX + pixel;
        const uint16_t tileCol = (xPos >> 3) & 0x1F;

        const uint16_t bgMapBase = windowOnThisPixel
                                       ? ((lcdc & 0x40) ? 0x9C00 : 0x9800)
                                       : ((lcdc & 0x08) ? 0x9C00 : 0x9800);

        const uint16_t tileAddr = bgMapBase + tileRow * 32 + tileCol;
        const uint8_t tileNum = vram[tileAddr - 0x8000];

        Attributes attrs{};
        if (hardware == Hardware::CGB)
            attrs = GetAttrsFrom(vram[tileAddr - 0x6000]);

        const auto signedIndex = static_cast<int16_t>(
            (lcdc & 0x10) ? tileNum : static_cast<int8_t>(tileNum) + 128);
        const uint16_t tileLocation = tileData + (signedIndex * 16);

        const uint8_t tileY = attrs.yflip ? 7 - (yPos & 7) : (yPos & 7);
        const bool bank1 = (hardware == Hardware::CGB) && attrs.vramBank;
        const uint16_t base = bank1 ? 0x6000 : 0x8000;

        const uint8_t data1 = vram[(tileLocation + tileY * 2) - base];
        const uint8_t data2 = vram[(tileLocation + tileY * 2 + 1) - base];

        const uint8_t tileX = attrs.xflip ? 7 - (xPos & 7) : (xPos & 7);

        const uint8_t colorLow = (data1 & (0x80 >> tileX)) ? 1 : 0;
        const uint8_t colorHigh = (data2 & (0x80 >> tileX)) ? 2 : 0;
        const uint8_t color = colorHigh | colorLow;

        priority_[pixel] = {attrs.priority, color};

        if (hardware == Hardware::CGB) {
            const uint8_t r = bgpd[attrs.paletteNumberCGB][color][0];
            const uint8_t g = bgpd[attrs.paletteNumberCGB][color][1];
            const uint8_t b = bgpd[attrs.paletteNumberCGB][color][2];
            SetColor(pixel, r, g, b);
        } else {
            const uint8_t shade = [&] {
                switch (backgroundPalette >> (color * 2) & 0x03) {
                    case 0: return DMG_SHADE[0];
                    case 1: return DMG_SHADE[1];
                    case 2: return DMG_SHADE[2];
                    default: return DMG_SHADE[3];
                }
            }();
            screenData[currentLine * SCREEN_WIDTH + pixel] = shade;
        }
    }
}

static constexpr uint8_t expand5(const uint8_t c) noexcept {
    return static_cast<uint8_t>((c << 3) | (c >> 2));
}

void GPU::SetColor(const uint8_t pixel, const uint32_t red, const uint32_t green, const uint32_t blue) {
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
    screenData[currentLine * SCREEN_WIDTH + pixel] = rgba;
}

GPU::Attributes GPU::GetAttrsFrom(const uint8_t byte) {
    return {
        .priority = (byte & 0x80) ? true : false, .yflip = (byte & 0x40) ? true : false,
        .xflip = (byte & 0x20) ? true : false,
        .paletteNumberDMG = (byte & 0x10) ? true : false, .vramBank = (byte & 0x08) ? true : false,
        .paletteNumberCGB = static_cast<uint8_t>(byte & 0x07)
    };
}

uint8_t GPU::ReadGpi(const Gpi &gpi) {
    return (gpi.autoIncrement ? 0x80 : 0x00) | gpi.index;
}

void GPU::WriteGpi(Gpi &gpi, const uint8_t value) {
    gpi.autoIncrement = (value & 0x80) != 0x00;
    gpi.index = value & 0x3F;
}

uint8_t GPU::ReadVRAM(const uint16_t address) const {
    return vram[vramBank * 0x2000 + address - 0x8000];
}

void GPU::WriteVRAM(const uint16_t address, const uint8_t value) {
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
            return bit6 | bit5 | bit4 | bit3 | bit2 | stat.mode;
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
        case 0xFF4F: return static_cast<uint8_t>(0xFE | vramBank);
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
            throw UnreachableCodeException("GPU::ReadRegisters unreachable code");
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
            break; // writing resets LY
        case 0xFF45: lyc = value;
            break; // fixed: store provided value
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
            throw UnreachableCodeException("GPU::WriteRegisters unreachable code");
    }
}
