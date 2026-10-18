#pragma once

#include "Inspection.h"

[[nodiscard]] uint8_t debug_tile_pixel(const GraphicsInspection &graphics, unsigned bank, unsigned tile, unsigned x, unsigned y);
[[nodiscard]] std::array<uint32_t, 256 * 256> debug_tile_map(const GraphicsInspection &graphics, bool high_map);
[[nodiscard]] std::array<uint32_t, 8 * 16> debug_sprite(const GraphicsInspection &graphics, unsigned index);
