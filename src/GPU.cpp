#include <list>

#include "GPU.h"
#include "Common.h"

bool GPU::LCDDisabled() const {
    return !Bit<LCDC_ENABLE_BIT>(lcdc);
}

void GPU::TickOAMScan() {
    if (currentLineSpriteIndex >= 10) return; // max 10 sprites per scanline
    const uint8_t spriteSize = Bit<LCDC_OBJ_SIZE>(lcdc) ? 16 : 8;
    const uint8_t yStart = oam[currentScanByte * 4], xStart = oam[currentScanByte * 4 + 1];
    if (currentLine >= yStart && currentLine < (yStart + spriteSize)) {
        lineSprites[currentLineSpriteIndex++] = Sprite{.spriteNum = currentScanByte, .x = xStart, .y = yStart};
    }
    currentScanByte = (currentScanByte + 1) % OBJ_TOTAL_SPRITES;
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
    return stat.mode == 3 ? 0xFF : vram[vramBank * 0x2000 + address - 0x8000];
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
            break;
    }
}
