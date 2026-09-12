#include "GPU.h"
#include <doctest/doctest.h>

namespace {
struct LineResult {
    unsigned statMode0;
    unsigned rendered;
    uint32_t pixel;
};

LineResult renderLine(Hardware hardware, bool objects, std::initializer_list<uint8_t> positions,
                      uint8_t windowX = 0) {
    Interrupts interrupts;
    GPU gpu(interrupts);
    gpu.hardware = hardware;
    gpu.lcdc = objects ? 0x93 : 0x91;
    gpu.currentLine = 10;
    if (windowX) {
        gpu.lcdc |= 0x20;
        gpu.windowX = windowX;
        gpu.windowTriggeredThisFrame = true;
    }
    gpu.backgroundPalette = gpu.obp0Palette = 0xE4;
    gpu.bgpd[0][0] = {31, 31, 31};
    gpu.obpd[0][1] = {31, 0, 0};
    unsigned index = 0;
    for (auto x : positions) {
        gpu.oam[index++] = 26;
        gpu.oam[index++] = x;
        gpu.oam[index++] = 1;
        gpu.oam[index++] = 0;
    }
    for (unsigned row = 0; row < 8; ++row) gpu.vram[16 + row * 2] = 0xFF;

    LineResult result{};
    bool enteredMode3 = false;
    for (unsigned dot = 1; dot < 456; ++dot) {
        gpu.Update();
        const unsigned mode = gpu.ReadRegisters(0xFF41) & 3;
        if (mode == 3) enteredMode3 = true;
        if (result.statMode0 != 0) CHECK(mode == 0);
        if (enteredMode3 && mode == 0 && result.statMode0 == 0) result.statMode0 = dot;
        if (gpu.stat.mode == GPUMode::MODE_0) {
            result.rendered = dot;
            result.pixel = gpu.GetScreenData()[10 * SCREEN_WIDTH + 20];
            break;
        }
    }
    REQUIRE(result.rendered != 0);
    return result;
}
}

TEST_CASE("ppu: overlapping OBJs share alignment but retain six-dot fetches") {
    const auto one = renderLine(Hardware::DMG, true, {0});
    const auto ten = renderLine(Hardware::DMG, true, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    CHECK(ten.rendered - one.rendered == 54);
    CHECK(ten.statMode0 - one.statMode0 == 54);

    const auto samePosition = renderLine(Hardware::DMG, true, {0, 0});
    const auto adjacentTiles = renderLine(Hardware::DMG, true, {0, 8});
    const auto reversed = renderLine(Hardware::DMG, true, {8, 0});
    CHECK(adjacentTiles.rendered - samePosition.rendered == 5);
    CHECK(reversed.rendered == adjacentTiles.rendered);
}

TEST_CASE("ppu: CGB fetches disabled OBJs while its mixer hides them") {
    const auto cgbOn = renderLine(Hardware::CGB, true, {28});
    const auto cgbOff = renderLine(Hardware::CGB, false, {28});
    CHECK(cgbOn.rendered == cgbOff.rendered);
    CHECK(cgbOn.pixel != cgbOff.pixel);
    CHECK(cgbOff.pixel == 0xFFFFFFFF);

    const auto dmgOn = renderLine(Hardware::DMG, true, {28});
    const auto dmgOff = renderLine(Hardware::DMG, false, {28});
    CHECK(dmgOn.rendered > dmgOff.rendered);
    CHECK(dmgOn.pixel != dmgOff.pixel);
}

TEST_CASE("ppu: STAT waits for fetches at the right edge") {
    const auto background = renderLine(Hardware::DMG, true, {});
    const auto object = renderLine(Hardware::DMG, true, {167});
    const auto window = renderLine(Hardware::DMG, true, {}, 165);
    CHECK(object.statMode0 > background.statMode0);
    CHECK(window.statMode0 > background.statMode0);
}
