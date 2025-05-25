#include <list>
#include <algorithm>
#include <ranges>

#include "GPU.h"
#include "Common.h"

void GPU::DrawScanline() {
    if ((lcdc & 0x01) != 0) {
        RenderTiles();
    }

    if ((lcdc & 0x02) != 0) {
        RenderSprites();
    }
}

void GPU::RenderSprites() {
    std::list<Sprite> sprites;
    const uint8_t spriteSize = (lcdc & 0x04) ? 16 : 8;

    for (int i = 0; i < 40; i++) {
        uint8_t yPos = oam[i * 4] - 16;
        uint8_t xPos = oam[i * 4 + 1] - 8;
        if ((yPos <= currentLine) && ((yPos + spriteSize) > currentLine)) {
            sprites.emplace_back(i, xPos, yPos);
        }
    }
    if (hardware != Hardware::CGB) {
        sprites.sort();
    }
    if (sprites.size() > 10) {
        sprites.resize(10);
    }

    sprites.reverse();

    for (const Sprite sprite: sprites) {
        const uint16_t spriteAddress = sprite.spriteNum * 4;
        const uint8_t yPos = oam[spriteAddress] - 16;
        const uint8_t xPos = oam[spriteAddress + 1] - 8;

        uint8_t tileNumber = oam[spriteAddress + 2];
        if (lcdc & 0x04) {
            tileNumber &= 0xFE;
        } else {
            tileNumber &= 0xFF;
        }
        const auto& [priority, xflip, yflip, paletteNumberDMG, vramBank, paletteNumberCGB] = GetAttrsFrom(
            oam[spriteAddress + 3]);

        if (yPos <= (0xFF - spriteSize + 1)) {
            if ((currentLine < yPos) || (currentLine > (yPos + spriteSize - 1))) {
                continue;
            }
        } else {
            if (currentLine > (yPos + spriteSize - 1)) {
                continue;
            }
        }
        if ((xPos >= 160) && (xPos <= (0xFF - 7))) {
            continue;
        }

        const uint8_t tileY = yflip ? (spriteSize - 1 - (currentLine - yPos)) : (currentLine - yPos);
        const uint16_t tileYAddress = 0x8000 + tileNumber * 16 + tileY * 2;

        uint8_t data1, data2;
        if ((hardware == Hardware::CGB) && vramBank) {
            data1 = vram[tileYAddress - 0x6000];
            data2 = vram[tileYAddress + 1 - 0x6000];
        } else {
            data1 = vram[tileYAddress - 0x8000];
            data2 = vram[tileYAddress + 1 - 0x8000];
        }

        for (uint8_t pixel = 0; pixel < 8; pixel++) {
            if (xPos + pixel >= 160) {
                continue;
            }
            const uint8_t tileX = xflip ? 7 - pixel : pixel;

            const uint8_t colorLow = ((data1 & (0x80 >> tileX)) != 0) ? 1 : 0;
            const uint8_t colorHigh = ((data2 & (0x80 >> tileX)) != 0) ? 2 : 0;
            const uint8_t color = colorHigh | colorLow;
            if (color == 0) {
                continue;
            }

            auto [fst, snd] = this->priority_[(xPos + pixel)];
            bool skip = false;
            if (hardware == Hardware::CGB && !(lcdc & 0x01)) {
                skip = snd == 0;
            } else if (fst) {
                skip = snd != 0;
            } else {
                skip = priority && snd != 0;
            }

            if (skip) {
                continue;
            }

            if (hardware == Hardware::CGB) {
                const uint8_t r = obpd[paletteNumberCGB][color][0];
                const uint8_t g = obpd[paletteNumberCGB][color][1];
                const uint8_t b = obpd[paletteNumberCGB][color][2];
                SetColor(xPos + pixel, r, g, b);
            } else {
                uint8_t colorVal = 0;
                if (paletteNumberDMG) {
                    switch ((obp1Palette >> (2 * color)) & 0x03) {
                        case 0x00:
                            colorVal = 255;
                            break;
                        case 0x01:
                            colorVal = 192;
                            break;
                        case 0x02:
                            colorVal = 96;
                            break;
                        default:
                            colorVal = 0;
                            break;
                    }
                } else {
                    switch ((obp0Palette >> (2 * color)) & 0x03) {
                        case 0x00:
                            colorVal = 255;
                            break;
                        case 0x01:
                            colorVal = 192;
                            break;
                        case 0x02:
                            colorVal = 96;
                            break;
                        default:
                            colorVal = 0;
                            break;
                    }
                };
                screenData[currentLine][static_cast<uint8_t>(xPos + pixel)][0] = colorVal;
                screenData[currentLine][static_cast<uint8_t>(xPos + pixel)][1] = colorVal;
                screenData[currentLine][static_cast<uint8_t>(xPos + pixel)][2] = colorVal;
            }
        }
    }
}

void GPU::RenderTiles() {
    const bool usingWindow = (lcdc & 0x20) && (windowY <= currentLine);
    const uint16_t tileData = (lcdc & 0x10) ? 0x8000 : 0x8800;
    const uint8_t wndwX = this->windowX - 7;

    uint8_t yPos = 0;
    if (!usingWindow) {
        yPos = scrollY + currentLine;
    } else {
        yPos = currentLine - windowY;
    }
    uint16_t tileRow = (static_cast<uint16_t>(yPos) >> 3) & 0x1F;

    for (int pixel = 0; pixel < 160; pixel++) {
        uint8_t xPos = 0;
        if (usingWindow && (pixel >= wndwX)) {
            xPos = pixel - wndwX;
        } else {
            xPos = scrollX + pixel;
        }
        uint16_t tileCol = (static_cast<uint16_t>(xPos) >> 3) & 0x1F;

        uint16_t backgroundMemory = 0;
        if (usingWindow && (pixel >= wndwX)) {
            if (lcdc & 0x40) {
                backgroundMemory = 0x9C00;
            } else {
                backgroundMemory = 0x9800;
            }
        } else {
            if (lcdc & 0x08) {
                backgroundMemory = 0x9C00;
            } else {
                backgroundMemory = 0x9800;
            }
        }

        uint16_t tileAddress = backgroundMemory + (tileRow * 32) + tileCol;
        uint8_t tileNumber = vram[tileAddress - 0x8000];
        uint16_t tileOffset = 0;
        if (lcdc & 0x10) {
            tileOffset = static_cast<uint16_t>(static_cast<int16_t>(tileNumber));
        } else {
            tileOffset = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(tileNumber)) + 128);
        }
        tileOffset *= 16;
        uint16_t tileLocation = tileData + tileOffset;
        Attributes tileAttrs = GetAttrsFrom(vram[tileAddress - 0x6000]);

        uint8_t tileY = 0;
        if (tileAttrs.yflip) {
            tileY = 7 - (yPos % 8);
        } else {
            tileY = yPos % 8;
        }

        uint8_t data1, data2;
        if ((hardware == Hardware::CGB) && tileAttrs.vramBank) {
            data1 = vram[(tileLocation + static_cast<uint16_t>(tileY * 2)) - 0x6000];
            data2 = vram[(tileLocation + static_cast<uint16_t>(tileY * 2) + 1) - 0x6000];
        } else {
            data1 = vram[(tileLocation + static_cast<uint16_t>(tileY * 2)) - 0x8000];
            data2 = vram[(tileLocation + static_cast<uint16_t>(tileY * 2) + 1) - 0x8000];
        }

        uint8_t tileX = 0;
        if (tileAttrs.xflip) {
            tileX = 7 - (xPos % 8);
        } else {
            tileX = xPos % 8;
        }

        uint8_t colorLow = (data1 & (0x80 >> tileX)) != 0 ? 1 : 0;
        uint8_t colorHigh = (data2 & (0x80 >> tileX)) != 0 ? 2 : 0;
        uint8_t color = colorHigh | colorLow;

        priority_[pixel] = std::make_pair(tileAttrs.priority, color);

        if (hardware == Hardware::CGB) {
            uint8_t r = bgpd[tileAttrs.paletteNumberCGB][color][0];
            uint8_t g = bgpd[tileAttrs.paletteNumberCGB][color][1];
            uint8_t b = bgpd[tileAttrs.paletteNumberCGB][color][2];
            SetColor(pixel, r, g, b);
        } else {
            switch ((backgroundPalette >> (2 * color)) & 0x03) {
                case 0x00:
                    color = 255;
                    break;
                case 0x01:
                    color = 192;
                    break;
                case 0x02:
                    color = 96;
                    break;
                default:
                    color = 0;
                    break;
            }
            screenData[currentLine][pixel][0] = color;
            screenData[currentLine][pixel][1] = color;
            screenData[currentLine][pixel][2] = color;
        }
    }
}

void GPU::SetColor(uint8_t pixel, uint32_t red, uint32_t green, uint32_t blue) {
    uint8_t newRed = ((red * 13 + green * 2 + blue) >> 1);
    uint8_t newGreen = ((green * 3 + blue) << 1);
    uint8_t newBlue = ((red * 3 + green * 2 + blue * 11) >> 1);
    screenData[currentLine][pixel][0] = newRed;
    screenData[currentLine][pixel][1] = newGreen;
    screenData[currentLine][pixel][2] = newBlue;
}

GPU::Attributes GPU::GetAttrsFrom(const uint8_t byte) {
    Attributes attributes{};
    attributes.priority = byte & (1 << 7) ? true : false;
    attributes.yflip = byte & (1 << 6) ? true : false;
    attributes.xflip = byte & (1 << 5) ? true : false;
    attributes.paletteNumberDMG = byte & (1 << 4) ? true : false;
    attributes.vramBank = byte & (1 << 3) ? true : false;
    attributes.paletteNumberCGB = byte & 0x07;
    return attributes;
}

uint8_t GPU::ReadGpi(const Gpi &gpi) {
    uint8_t a = gpi.autoIncrement ? 0x80 : 0x00;
    return a | gpi.index;
}

void GPU::WriteGpi(Gpi &gpi, uint8_t value) {
    gpi.autoIncrement = (value & 0x80) != 0x00;
    gpi.index = value & 0x3F;
}

uint8_t GPU::ReadVRAM(uint16_t address) const {
    return vram[vramBank * 0x2000 + address - 0x8000];
}

void GPU::WriteVRAM(uint16_t address, uint8_t value) {
    vram[vramBank * 0x2000 + address - 0x8000] = value;
}

uint8_t GPU::ReadRegisters(const uint16_t address) const {
    switch (address) {
        case 0xFF40:
            return lcdc;
        case 0xFF41: {
            const uint8_t bit6 = stat.enableLYInterrupt ? 0x40 : 0x00;
            const uint8_t bit5 = stat.enableM2Interrupt ? 0x40 : 0x00;
            const uint8_t bit4 = stat.enableM1Interrupt ? 0x40 : 0x00;
            const uint8_t bit3 = stat.enableM0Interrupt ? 0x40 : 0x00;
            const uint8_t bit2 = (currentLine == lyc) ? 0x40 : 0x00;
            return bit6 | bit5 | bit4 | bit3 | bit2 | stat.mode;
        }
        case 0xFF42:
            return scrollY;
        case 0xFF43:
            return scrollX;
        case 0xFF44:
            return currentLine;
        case 0xFF45:
            return lyc;
        case 0xFF47:
            return backgroundPalette;
        case 0xFF48:
            return obp0Palette;
        case 0xFF49:
            return obp1Palette;
        case 0xFF4A:
            return windowY;
        case 0xFF4B:
            return windowX;
        case 0xFF4F:
            return 0xFE | vramBank;
        case 0xFF68:
            return ReadGpi(bgpi);
        case 0xFF69: {
            const uint8_t r = bgpi.index >> 3;
            const uint8_t c = (bgpi.index >> 1) & 3;
            if ((bgpi.index & 0x01) == 0x00) {
                const uint8_t a = bgpd[r][c][0];
                const uint8_t b = bgpd[r][c][1] << 5;
                return a | b;
            }
            const uint8_t a = bgpd[r][c][1] >> 3;
            const uint8_t b = bgpd[r][c][2] << 2;
            return a | b;
        }
        case 0xFF6A:
            return ReadGpi(obpi);
        case 0xFF6B: {
            const uint8_t r = obpi.index >> 3;
            const uint8_t c = (obpi.index >> 1) & 3;
            if ((obpi.index & 0x01) == 0x00) {
                const uint8_t a = obpd[r][c][0];
                const uint8_t b = obpd[r][c][1] << 5;
                return a | b;
            }
            const uint8_t a = obpd[r][c][1] >> 3;
            const uint8_t b = obpd[r][c][2] << 2;
            return a | b;
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
                for (auto& row : screenData | std::views::all) {
                    for (auto& pixel : row | std::views::all) {
                        std::ranges::fill(pixel, 0xFF);
                    }
                }
                vblank = true;
            }
            break;
        case 0xFF41:
            stat.enableLYInterrupt = (value & 0x40) != 0x00;
            stat.enableM2Interrupt = (value & 0x20) != 0x00;
            stat.enableM1Interrupt = (value & 0x10) != 0x00;
            stat.enableM0Interrupt = (value & 0x08) != 0x00;
            break;
        case 0xFF42:
            scrollY = value;
            break;
        case 0xFF43:
            scrollX = value;
            break;
        case 0xFF44:
            currentLine = 0;
            break;
        case 0xFF45:
            lyc = 0;
            break;
        case 0xFF47:
            backgroundPalette = value;
            break;
        case 0xFF48:
            obp0Palette = value;
            break;
        case 0xFF49:
            obp1Palette = value;
            break;
        case 0xFF4A:
            windowY = value;
            break;
        case 0xFF4B:
            windowX = value;
            break;
        case 0xFF4F:
            vramBank = value & 0x01;
            break;
        case 0xFF68:
            return WriteGpi(bgpi, value);
        case 0xFF69: {
            const uint8_t r = bgpi.index >> 3;
            const uint8_t c = (bgpi.index >> 1) & 0x03;
            if ((bgpi.index & 0x01) == 0x00) {
                bgpd[r][c][0] = value & 0x1F;
                bgpd[r][c][1] = (bgpd[r][c][1] & 0x18) | (value >> 5);
            } else {
                bgpd[r][c][1] = (bgpd[r][c][1] & 0x07) | ((value & 0x03) << 3);
                bgpd[r][c][2] = (value >> 2) & 0x1F;
            }
            if (bgpi.autoIncrement) {
                bgpi.index += 0x01;
                bgpi.index &= 0x3f;
            }
            break;
        }
        case 0xFF6A:
            return WriteGpi(obpi, value);
        case 0xFF6B: {
            const uint8_t r = obpi.index >> 3;
            const uint8_t c = (obpi.index >> 1) & 0x03;
            if ((obpi.index & 0x01) == 0x00) {
                obpd[r][c][0] = value & 0x1F;
                obpd[r][c][1] = (obpd[r][c][1] & 0x18) | (value >> 5);
            } else {
                obpd[r][c][1] = (obpd[r][c][1] & 0x07) | ((value & 0x03) << 3);
                obpd[r][c][2] = (value >> 2) & 0x1F;
            }
            if (obpi.autoIncrement) {
                obpi.index += 0x01;
                obpi.index &= 0x3f;
            }
            break;
        }
        default:
            throw UnreachableCodeException("GPU::WriteRegisters unreachable code");
    }
}
