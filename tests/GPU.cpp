#include "GPU.h"
#include <algorithm>
#include <doctest/doctest.h>
#include <string_view>
#include "diagnostics/Provenance.h"

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
        auto index{0u};
        for (auto x : positions) {
            gpu.oam[index++] = 26;
            gpu.oam[index++] = x;
            gpu.oam[index++] = 1;
            gpu.oam[index++] = 0;
        }
        for (auto row{0u}; row < 8; ++row)
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
                            std::string_view expected, diagnostics::PixelProvenanceContext *provenance = nullptr) {
        CAPTURE(ModelName(model));
        CAPTURE(coordinatePriority);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(model);
        gpu.provenance = provenance;
        gpu.objectPriority = coordinatePriority;
        gpu.lcdc = 0x93;
        gpu.currentLine = 10;
        gpu.backgroundPalette = gpu.obp0Palette = 0xE4;
        gpu.obpd[0][0] = {31, 0, 31};
        gpu.obpd[0][1] = {31, 31, 31};
        gpu.obpd[0][2] = {16, 16, 16};
        gpu.obpd[0][3] = {8, 8, 8};
        gpu.RebuildColorLuts();
        for (auto row{0u}; row < 8; ++row) {
            gpu.vram[row * 2] = gpu.vram[row * 2 + 1] = 0xFF;
        }
        auto index{0u};
        for (const auto &object : objects) {
            REQUIRE(object.colors.size() == 8);
            const unsigned tile = index + 1;
            gpu.oam[index * 4] = 26;
            gpu.oam[index * 4 + 1] = object.x;
            gpu.oam[index * 4 + 2] = tile;
            gpu.oam[index * 4 + 3] = object.flags;
            for (auto row{0u}; row < 8; ++row) {
                for (auto x{0u}; x < 8; ++x) {
                    const unsigned color = object.colors[x] - '0';
                    gpu.vram[tile * 16 + row * 2] |= (color & 1) << (7 - x);
                    gpu.vram[tile * 16 + row * 2 + 1] |= ((color >> 1) & 1) << (7 - x);
                }
            }
            ++index;
        }
        for (auto dot{0u}; dot < 456 && gpu.pixelsDrawn < SCREEN_WIDTH; ++dot) {
            if (provenance)
                provenance->position = {.tick = dot * 2, .dot = static_cast<uint16_t>(gpu.scanlineCounter), .ly = gpu.currentLine};
            gpu.Update();
        }
        REQUIRE(gpu.pixelsDrawn == SCREEN_WIDTH);
        constexpr uint32_t cgbColors[] = {0xFF000000, 0xFFFFFFFF, 0xFF848484, 0xFF424242};
        for (auto x{0u}; x < SCREEN_WIDTH; ++x) {
            const unsigned color = x >= startX && x - startX < expected.size() ? expected[x - startX] - '0' : 0;
            const auto pixel = color == 0 ? 0xFF000000 : IsCgb(model) ? cgbColors[color] : GPU::DMG_SHADE[color];
            CAPTURE(x);
            CHECK(gpu.GetScreenData()[10 * SCREEN_WIDTH + x] == pixel);
        }
    }

    unsigned checkWindowLine(Model model, bool doubleSpeed, bool compatibility, uint8_t wx, uint8_t scx,
                             diagnostics::PixelProvenanceContext *provenance = nullptr) {
        CAPTURE(ModelName(model));
        CAPTURE(doubleSpeed);
        CAPTURE(compatibility);
        CAPTURE(wx);
        CAPTURE(scx);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(model);
        gpu.provenance = provenance;
        gpu.doubleSpeed = doubleSpeed;
        gpu.dmg_compat = compatibility;
        gpu.lcdc = 0xF1;
        gpu.currentLine = 10;
        gpu.windowX = wx;
        gpu.windowTriggeredThisFrame = true;
        gpu.scrollX = scx;
        gpu.backgroundPalette = 0xE4;
        constexpr uint8_t shades[] = {31, 23, 11, 0};
        constexpr uint32_t cgbColors[] = {0xFFFFFFFF, 0xFFBDBDBD, 0xFF5A5A5A, 0xFF000000};
        for (auto color{0u}; color < 4; ++color)
            gpu.bgpd[0][color].fill(shades[color]);
        gpu.RebuildColorLuts();
        constexpr std::string_view pattern = "0123321012032130";
        for (auto row{0u}; row < 8; ++row) {
            gpu.vram[row * 2] = gpu.vram[row * 2 + 1] = 0xFF;
            for (auto x{0u}; x < pattern.size(); ++x) {
                const unsigned address = (1 + x / 8) * 16 + row * 2;
                const unsigned color = pattern[x] - '0';
                gpu.vram[address] |= (color & 1) << (7 - x % 8);
                gpu.vram[address + 1] |= (color >> 1) << (7 - x % 8);
            }
        }
        for (auto tile{0u}; tile < 32; ++tile)
            gpu.vram[0x1C00 + tile] = 1 + tile % 2;
        for (auto dot{0u}; dot < 456; ++dot) {
            if (provenance)
                provenance->position = {.tick = dot * 2, .dot = static_cast<uint16_t>(gpu.scanlineCounter), .ly = gpu.currentLine};
            gpu.Update();
            if (gpu.stat.mode == GPUMode::MODE_0)
                break;
        }
        REQUIRE(gpu.pixelsDrawn == SCREEN_WIDTH);
        std::array<uint32_t, SCREEN_WIDTH> expected{};
        for (auto x{0u}; x < SCREEN_WIDTH; ++x) {
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

TEST_CASE("ppu: writes to LY preserve scanline progression and STAT coincidence") {
    for (const auto model : {Model::DMGB, Model::CGBC, Model::CGBE}) {
        for (const auto mode : {GPUMode::MODE_0, GPUMode::MODE_1, GPUMode::MODE_2, GPUMode::MODE_3}) {
            for (const uint8_t value : {0x00, 0x77, 0xFF}) {
                CAPTURE(ModelName(model));
                CAPTURE(mode);
                CAPTURE(value);
                Interrupts writtenInterrupts, controlInterrupts;
                GPU written(writtenInterrupts), control(controlInterrupts);
                const uint8_t line = mode == GPUMode::MODE_1 ? 0x97 : 0x44;
                const unsigned dot = mode == GPUMode::MODE_0 ? 400 : mode == GPUMode::MODE_3 ? 184 : 32;
                for (auto *gpu : {&written, &control}) {
                    gpu->SetModel(model);
                    gpu->lcdc = 0x91;
                    gpu->currentLine = gpu->lyc = line;
                    gpu->stat.mode = mode == GPUMode::MODE_1 ? GPUMode::MODE_1 : GPUMode::MODE_2;
                    gpu->stat.enable_ly_interrupt = true;
                    for (auto i{0u}; i < dot; ++i)
                        gpu->Update();
                    REQUIRE(gpu->stat.mode == mode);
                    REQUIRE(gpu->ReadRegisters(0xFF44) == line);
                    REQUIRE((gpu->ReadRegisters(0xFF41) & 4) != 0);
                }

                written.WriteRegisters(0xFF44, value);
                REQUIRE(written.ReadRegisters(0xFF44) == line);

                for (auto i{0u}; i < 456; ++i) {
                    written.Update();
                    control.Update();
                    CHECK(written.ReadRegisters(0xFF44) == control.ReadRegisters(0xFF44));
                    CHECK(written.ReadRegisters(0xFF41) == control.ReadRegisters(0xFF41));
                    CHECK(writtenInterrupts.interrupt_flag == controlInterrupts.interrupt_flag);
                }
                CHECK(std::equal(written.GetScreenData(), written.GetScreenData() + SCREEN_WIDTH * SCREEN_HEIGHT, control.GetScreenData()));
            }
        }
    }
}

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

TEST_CASE("diagnostics: Crystal sprite overlap provenance identifies transparent priority candidates") {
    diagnostics::ProvenanceRecorder recorder({.y = {10, 10}});
    checkObjectOverlap(Model::CGBC, false, {{24, "10101010"}, {20, "22222222"}}, 12, "222212121010", &recorder.Context());
    const auto pixel = recorder.Query(17, 10);
    REQUIRE(pixel);
    REQUIRE(pixel->selected.valid);
    CHECK(pixel->selected.source == diagnostics::PixelSource::Sprite);
    CHECK(pixel->selected.oamIndex == 1);
    CHECK(pixel->selected.color == 2);
    REQUIRE(pixel->sprites.count == 2);
    CHECK(std::ranges::any_of(std::span(pixel->sprites.candidates).first(pixel->sprites.count), [](const auto &candidate) {
        return candidate.origin.oamIndex == 0 && candidate.origin.color == 0 && candidate.decision == diagnostics::SpriteDecision::Transparent;
    }));
    CHECK(pixel->selected.lowTick < pixel->selected.highTick);
    CHECK(pixel->selected.highTick <= pixel->emission.tick);
    CHECK(pixel->selected.lowRead == 0);
    CHECK(pixel->selected.highRead == 255);
    CHECK(pixel->finalColor == 0xFF848484);
}

TEST_CASE("diagnostics: Crystal window provenance explains clipped window pixels independently of SCX delay") {
    diagnostics::ProvenanceRecorder recorder({.y = {10, 10}});
    const auto observedTiming = checkWindowLine(Model::CGBC, false, false, 5, 3, &recorder.Context());
    CHECK(observedTiming == checkWindowLine(Model::CGBC, false, false, 5, 3));
    const auto pixel = recorder.Query(0, 10);
    REQUIRE(pixel);
    REQUIRE(pixel->selected.valid);
    CHECK(pixel->selected.source == diagnostics::PixelSource::Window);
    CHECK(pixel->selected.mapAddress == 0x9C00);
    CHECK(pixel->selected.tile == 1);
    CHECK(pixel->selected.bit == 5);
    CHECK(pixel->selected.color == 2);
    const auto events = recorder.PipelineEvents();
    CHECK(std::ranges::count_if(events, [](const auto &event) { return event.reason == diagnostics::PipelineReason::WindowClipping; }) == 2);
    CHECK(pixel->selected.lowTick < pixel->selected.highTick);
    CHECK(pixel->selected.highTick < pixel->emission.tick);
}

TEST_CASE("ppu: VBlank idle optimization processes each new line before skipping dots") {
    for (const auto model : {Model::DMGB, Model::CGBC, Model::CGBE}) {
        Interrupts optimizedInterrupts, ordinaryInterrupts;
        GPU optimized(optimizedInterrupts), ordinary(ordinaryInterrupts);
        for (auto *gpu : {&optimized, &ordinary}) {
            gpu->SetModel(model);
            gpu->lcdc = 0x91;
            gpu->currentLine = 150;
            gpu->scanlineCounter = 455;
            gpu->stat.mode = GPUMode::MODE_1;
            gpu->stat.enable_ly_interrupt = true;
            gpu->lyc = 151;
        }
        ordinary.optimized_execution = false;
        for (auto dot{0u}; dot < 460; ++dot) {
            CAPTURE(model);
            CAPTURE(dot);
            optimized.Update();
            ordinary.Update();
            CHECK(optimized.ReadRegisters(0xFF41) == ordinary.ReadRegisters(0xFF41));
            CHECK(optimizedInterrupts.interrupt_flag == ordinaryInterrupts.interrupt_flag);
            CHECK(optimizedInterrupts.interrupt_visible_pending == ordinaryInterrupts.interrupt_visible_pending);
            CHECK(optimizedInterrupts.interrupt_set_delays == ordinaryInterrupts.interrupt_set_delays);
        }
        CHECK((optimizedInterrupts.interrupt_flag & 2) != 0);
    }
}

TEST_CASE("ppu: OAM scan shortcut preserves the falling STAT request edge") {
    for (const auto model : {Model::DMGB, Model::CGBC, Model::CGBE}) {
        Interrupts optimizedInterrupts, ordinaryInterrupts;
        GPU optimized(optimizedInterrupts), ordinary(ordinaryInterrupts);
        for (auto *gpu : {&optimized, &ordinary}) {
            gpu->SetModel(model);
            gpu->lcdc = 0x91;
            gpu->currentLine = 10;
            gpu->stat.mode = GPUMode::MODE_2;
            gpu->stat.enable_m2_interrupt = true;
        }
        ordinary.optimized_execution = false;
        for (auto dot{0u}; dot < 80; ++dot) {
            CAPTURE(model);
            CAPTURE(dot);
            optimized.Update();
            ordinary.Update();
            CHECK(optimized.statTriggered == ordinary.statTriggered);
            CHECK(optimizedInterrupts.interrupt_flag == ordinaryInterrupts.interrupt_flag);
            if (dot == 7) {
                for (auto *gpu : {&optimized, &ordinary}) {
                    gpu->WriteRegisters(0xFF45, 10);
                    gpu->WriteRegisters(0xFF41, 0x60);
                }
            }
        }
    }
}
