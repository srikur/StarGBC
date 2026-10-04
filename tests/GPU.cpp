#include "GPU.h"
#include <algorithm>
#include <doctest/doctest.h>
#include <string_view>

namespace {
    struct LineResult {
        unsigned statMode0;
        unsigned rendered;
        uint32_t pixel;
    };

    LineResult renderLine(Model model, bool objects, std::initializer_list<uint8_t> positions, uint8_t windowX = 0) {
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(model);
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
        gpu.RebuildColorLuts();
        unsigned index = 0;
        for (auto x : positions) {
            gpu.oam[index++] = 26;
            gpu.oam[index++] = x;
            gpu.oam[index++] = 1;
            gpu.oam[index++] = 0;
        }
        for (unsigned row = 0; row < 8; ++row)
            gpu.vram[16 + row * 2] = 0xFF;

        LineResult result{};
        bool enteredMode3 = false;
        for (unsigned dot = 1; dot < 456; ++dot) {
            gpu.Update();
            const unsigned mode = gpu.ReadRegisters(0xFF41) & 3;
            if (mode == 3)
                enteredMode3 = true;
            if (result.statMode0 != 0)
                CHECK(mode == 0);
            if (enteredMode3 && mode == 0 && result.statMode0 == 0)
                result.statMode0 = dot;
            if (gpu.stat.mode == GPUMode::MODE_0) {
                result.rendered = dot;
                result.pixel = gpu.GetScreenData()[10 * SCREEN_WIDTH + 20];
                break;
            }
        }
        REQUIRE(result.rendered != 0);
        return result;
    }

    struct ObjectPixels {
        uint8_t x;
        std::string_view colors;
        uint8_t flags{0};
    };

    void checkObjectOverlap(Model model, bool coordinatePriority, std::initializer_list<ObjectPixels> objects, unsigned startX,
                            std::string_view expected) {
        CAPTURE(ModelName(model));
        CAPTURE(coordinatePriority);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(model);
        gpu.objectPriority = coordinatePriority;
        gpu.lcdc = 0x93;
        gpu.currentLine = 10;
        gpu.backgroundPalette = gpu.obp0Palette = 0xE4;
        gpu.obpd[0][0] = {31, 0, 31};
        gpu.obpd[0][1] = {31, 31, 31};
        gpu.obpd[0][2] = {16, 16, 16};
        gpu.obpd[0][3] = {8, 8, 8};
        gpu.RebuildColorLuts();
        for (unsigned row = 0; row < 8; ++row) {
            gpu.vram[row * 2] = gpu.vram[row * 2 + 1] = 0xFF;
        }
        unsigned index = 0;
        for (const auto &object : objects) {
            REQUIRE(object.colors.size() == 8);
            const unsigned tile = index + 1;
            gpu.oam[index * 4] = 26;
            gpu.oam[index * 4 + 1] = object.x;
            gpu.oam[index * 4 + 2] = tile;
            gpu.oam[index * 4 + 3] = object.flags;
            for (unsigned row = 0; row < 8; ++row) {
                for (unsigned x = 0; x < 8; ++x) {
                    const unsigned color = object.colors[x] - '0';
                    gpu.vram[tile * 16 + row * 2] |= (color & 1) << (7 - x);
                    gpu.vram[tile * 16 + row * 2 + 1] |= ((color >> 1) & 1) << (7 - x);
                }
            }
            ++index;
        }
        for (unsigned dot = 0; dot < 456 && gpu.pixelsDrawn < SCREEN_WIDTH; ++dot)
            gpu.Update();
        REQUIRE(gpu.pixelsDrawn == SCREEN_WIDTH);
        constexpr uint32_t cgbColors[] = {0xFF000000, 0xFFFFFFFF, 0xFF848484, 0xFF424242};
        for (unsigned x = 0; x < SCREEN_WIDTH; ++x) {
            const unsigned color = x >= startX && x - startX < expected.size() ? expected[x - startX] - '0' : 0;
            const auto pixel = color == 0 ? 0xFF000000 : IsCgb(model) ? cgbColors[color] : GPU::DMG_SHADE[color];
            CAPTURE(x);
            CHECK(gpu.GetScreenData()[10 * SCREEN_WIDTH + x] == pixel);
        }
    }

    unsigned checkWindowLine(Model model, bool doubleSpeed, bool compatibility, uint8_t wx, uint8_t scx) {
        CAPTURE(ModelName(model));
        CAPTURE(doubleSpeed);
        CAPTURE(compatibility);
        CAPTURE(wx);
        CAPTURE(scx);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(model);
        gpu.doubleSpeed = doubleSpeed;
        gpu.dmgCompat = compatibility;
        gpu.lcdc = 0xF1;
        gpu.currentLine = 10;
        gpu.windowX = wx;
        gpu.windowTriggeredThisFrame = true;
        gpu.scrollX = scx;
        gpu.backgroundPalette = 0xE4;
        constexpr uint8_t shades[] = {31, 23, 11, 0};
        constexpr uint32_t cgbColors[] = {0xFFFFFFFF, 0xFFBDBDBD, 0xFF5A5A5A, 0xFF000000};
        for (unsigned color = 0; color < 4; ++color)
            gpu.bgpd[0][color].fill(shades[color]);
        gpu.RebuildColorLuts();
        constexpr std::string_view pattern = "0123321012032130";
        for (unsigned row = 0; row < 8; ++row) {
            gpu.vram[row * 2] = gpu.vram[row * 2 + 1] = 0xFF;
            for (unsigned x = 0; x < pattern.size(); ++x) {
                const unsigned address = (1 + x / 8) * 16 + row * 2;
                const unsigned color = pattern[x] - '0';
                gpu.vram[address] |= (color & 1) << (7 - x % 8);
                gpu.vram[address + 1] |= (color >> 1) << (7 - x % 8);
            }
        }
        for (unsigned tile = 0; tile < 32; ++tile)
            gpu.vram[0x1C00 + tile] = 1 + tile % 2;
        for (unsigned dot = 0; dot < 456; ++dot) {
            gpu.Update();
            if (gpu.stat.mode == GPUMode::MODE_0)
                break;
        }
        REQUIRE(gpu.pixelsDrawn == SCREEN_WIDTH);
        std::array<uint32_t, SCREEN_WIDTH> expected{};
        for (unsigned x = 0; x < SCREEN_WIDTH; ++x) {
            unsigned color = 3;
            if (x + 7 >= wx) {
                const unsigned windowPixel = x + 7 - wx + (wx == 0 ? scx & 7 : 0);
                color = pattern[windowPixel % pattern.size()] - '0';
            }
            expected[x] = IsCgb(model) ? cgbColors[color] : GPU::DMG_SHADE[color];
        }
        CHECK(std::equal(expected.begin(), expected.end(), gpu.GetScreenData() + 10 * SCREEN_WIDTH));
        return gpu.scanlineCounter;
    }
} // namespace

TEST_CASE("ppu: overlapping OBJs share alignment but retain six-dot fetches") {
    const auto one = renderLine(Model::DMGB, true, {0});
    const auto ten = renderLine(Model::DMGB, true, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
    CHECK(ten.rendered - one.rendered == 54);
    CHECK(ten.statMode0 - one.statMode0 == 54);

    const auto samePosition = renderLine(Model::DMGB, true, {0, 0});
    const auto adjacentTiles = renderLine(Model::DMGB, true, {0, 8});
    const auto reversed = renderLine(Model::DMGB, true, {8, 0});
    CHECK(adjacentTiles.rendered - samePosition.rendered == 5);
    CHECK(reversed.rendered == adjacentTiles.rendered);
}

TEST_CASE("ppu: CGB fetches disabled OBJs while its mixer hides them") {
    const auto cgbOn = renderLine(Model::CGBE, true, {28});
    const auto cgbOff = renderLine(Model::CGBE, false, {28});
    CHECK(cgbOn.rendered == cgbOff.rendered);
    CHECK(cgbOn.pixel != cgbOff.pixel);
    CHECK(cgbOff.pixel == 0xFFFFFFFF);

    const auto dmgOn = renderLine(Model::DMGB, true, {28});
    const auto dmgOff = renderLine(Model::DMGB, false, {28});
    CHECK(dmgOn.rendered > dmgOff.rendered);
    CHECK(dmgOn.pixel != dmgOff.pixel);
}

TEST_CASE("ppu: STAT waits for fetches at the right edge") {
    const auto background = renderLine(Model::DMGB, true, {});
    const auto object = renderLine(Model::DMGB, true, {167});
    const auto window = renderLine(Model::DMGB, true, {}, 165);
    CHECK(object.statMode0 > background.statMode0);
    CHECK(window.statMode0 > background.statMode0);
}

TEST_CASE("ppu: transparent higher-priority OBJ pixels preserve overlapping objects") {
    for (const auto model : {Model::CGBB, Model::CGBC, Model::CGBE, Model::AGBB}) {
        checkObjectOverlap(model, false, {{24, "10101010"}, {20, "22222222"}}, 12, "222212121010");
        checkObjectOverlap(model, false, {{24, "10101010", 0x20}, {20, "22222222"}}, 12, "222221210101");
    }
}

TEST_CASE("ppu: OBJ priority is resolved separately for every FIFO pixel") {
    for (const auto model : {Model::CGBB, Model::CGBC, Model::CGBE, Model::AGBB}) {
        checkObjectOverlap(model, false, {{24, "11101010"}, {26, "22222222"}, {20, "33333333"}}, 12, "33331112121222");
        checkObjectOverlap(model, false, {{24, "11010101"}, {26, "22222222"}, {20, "33333333"}}, 12, "33331121212122");
    }
}

TEST_CASE("ppu: OBJ ordering follows OAM or coordinate priority with OAM breaking ties") {
    for (const auto model : {Model::DMGB, Model::CGBB, Model::CGBC, Model::CGBE, Model::AGBB}) {
        for (const bool coordinatePriority : {false, true}) {
            const bool byX = !IsCgb(model) || coordinatePriority;
            checkObjectOverlap(model, coordinatePriority, {{24, "11111111"}, {20, "22222222"}}, 12, byX ? "222222221111" : "222211111111");
            checkObjectOverlap(model, coordinatePriority, {{20, "10101010"}, {20, "22222222"}}, 12, "12121212");
        }
    }
}

TEST_CASE("ppu: BG priority is applied after selecting the first opaque OBJ pixel") {
    for (const auto model : {Model::DMGB, Model::CGBB, Model::CGBC, Model::CGBE, Model::AGBB}) {
        checkObjectOverlap(model, !IsCgb(model), {{20, "10101010", 0x80}, {20, "22222222"}}, 12, "02020202");
    }
}

TEST_CASE("ppu: SCX delays window output without scrolling its pixels") {
    for (const auto model : {Model::DMGC, Model::CGBB, Model::CGBC, Model::CGBE, Model::AGBB}) {
        for (const bool doubleSpeed : {false, true}) {
            if (doubleSpeed && !IsCgb(model))
                continue;
            for (const uint8_t wx : {1, 5, 7, 8, 32, 165}) {
                const auto unscrolled = checkWindowLine(model, doubleSpeed, false, wx, 0);
                for (uint8_t scx = 1; scx < 16; ++scx) {
                    const auto scrolled = checkWindowLine(model, doubleSpeed, false, wx, scx);
                    CAPTURE(scx);
                    CHECK(scrolled == unscrolled + (scx & 7));
                }
            }
        }
    }
}

TEST_CASE("ppu: compatibility windows clip by WX and preserve the WX zero scroll quirk") {
    for (uint8_t scx = 0; scx < 8; ++scx) {
        for (const uint8_t wx : {0, 1, 5, 7, 8}) {
            checkWindowLine(Model::DMGC, false, false, wx, scx);
            checkWindowLine(Model::CGBE, false, true, wx, scx);
        }
    }
}
