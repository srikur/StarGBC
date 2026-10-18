#include "DebugGraphics.h"

uint8_t debug_tile_pixel(const GraphicsInspection &graphics, const unsigned bank, const unsigned tile, const unsigned x, const unsigned y) {
    if (bank > 1 || tile >= 384 || x > 7 || y > 7)
        throw std::out_of_range("invalid tile coordinates");
    const auto offset{bank * 0x2000 + tile * 16 + y * 2};
    return ((graphics.vram[offset] >> (7 - x)) & 1) | (((graphics.vram[offset + 1] >> (7 - x)) & 1) << 1);
}

std::array<uint32_t, 256 * 256> debug_tile_map(const GraphicsInspection &graphics, const bool high_map) {
    std::array<uint32_t, 256 * 256> pixels{};
    for (auto index{0u}; index < pixels.size(); ++index) {
        const auto x{index % 256};
        const auto y{index / 256};
        const auto offset{(high_map ? BACKGROUND_MAP_HIGH - VRAM_BEGIN : BACKGROUND_MAP_LOW - VRAM_BEGIN) + y / 8 * 32 + x / 8};
        const auto attributes{graphics.cgb_mode ? graphics.vram[0x2000 + offset] : 0};
        const auto number{graphics.vram[offset]};
        const auto tile{Bit<LCDC_BG_AND_WINDOW_TILE_DATA>(graphics.lcd_control) ? unsigned{number}
                                                                                : static_cast<unsigned>(256 + static_cast<int8_t>(number))};
        const auto color{debug_tile_pixel(graphics, (attributes >> 3) & 1, tile, attributes & 0x20 ? 7 - x % 8 : x % 8,
                                          attributes & 0x40 ? 7 - y % 8 : y % 8)};
        pixels[index] = graphics.background_colors[(attributes & 7) * 4 + color];
    }
    return pixels;
}

std::array<uint32_t, 8 * 16> debug_sprite(const GraphicsInspection &graphics, const unsigned index) {
    if (index >= OBJ_TOTAL_SPRITES)
        throw std::out_of_range("invalid OAM index");
    std::array<uint32_t, 8 * 16> pixels{};
    const auto attributes{graphics.oam[index * 4 + 3]};
    const auto height{Bit<LCDC_OBJ_SIZE>(graphics.lcd_control) ? 16u : 8u};
    const auto tile{graphics.oam[index * 4 + 2] & (height == 16 ? 0xFE : 0xFF)};
    const auto bank{graphics.cgb_mode ? (attributes >> OAM_VRAM_BANK_BIT) & 1 : 0};
    const auto palette{graphics.cgb_mode ? attributes & 7 : (attributes >> OAM_PALETTE_NUMBER_DMG_BIT) & 1};
    for (auto index{0u}; index < height * 8; ++index) {
        const auto x{Bit<OAM_X_FLIP_BIT>(attributes) ? 7 - index % 8 : index % 8};
        const auto y{Bit<OAM_Y_FLIP_BIT>(attributes) ? height - 1 - index / 8 : index / 8};
        const auto color{debug_tile_pixel(graphics, bank, tile + y / 8, x, y % 8)};
        pixels[index] = color ? graphics.object_colors[palette * 4 + color] : 0;
    }
    return pixels;
}
