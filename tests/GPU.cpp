#include "GPU.h"
#include <algorithm>
#include <array>
#include <doctest/doctest.h>
#include <iterator>
#include <string_view>
#include "diagnostics/Provenance.h"

namespace {
    void prime_dmg_oam_scan(GPU &gpu) {
        if (gpu.is_cgb)
            return;
        const auto saved_stat = gpu.stat;
        gpu.stat = {};
        gpu.stat.mode = GPUMode::MODE_0;
        --gpu.currentLine;
        gpu.scanlineCounter = 455;
        while (gpu.scanlineCounter != 0)
            gpu.Update();
        gpu.stat = saved_stat;
        gpu.statTriggered = false;
    }

    struct LineResult {
        unsigned statMode0;
        unsigned rendered;
        uint32_t pixel;
    };

    LineResult renderLine(Model model, bool objects, std::initializer_list<uint8_t> positions, uint8_t windowX = 0, uint8_t scroll_x = 0) {
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(model);
        gpu.lcdc = objects ? 0x93 : 0x91;
        gpu.currentLine = 10;
        gpu.scrollX = scroll_x;
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

        prime_dmg_oam_scan(gpu);
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
        prime_dmg_oam_scan(gpu);
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

TEST_CASE("ppu: left-edge sprite fetches preserve pending window activation and row progression") {
    constexpr std::array models{Model::DMGB, Model::CGBC, Model::CGBE, Model::AGBB};
    constexpr std::array speeds{false, true};
    constexpr std::array optimizations{false, true};
    constexpr std::array<uint8_t, 8> scroll_values{0, 1, 2, 3, 4, 5, 6, 7};
    constexpr std::array<uint8_t, 3> window_values{1, 4, 7};
    constexpr std::array<std::array<uint8_t, 2>, 5> object_positions{{{0, 8}, {1, 8}, {2, 8}, {0, 0}, {8, 8}}};
    constexpr std::array<uint32_t, 4> colors{0xFFFFFFFF, 0xFFBDBDBD, 0xFF5A5A5A, 0xFF000000};
    constexpr std::array<uint8_t, 4> shades{31, 23, 11, 0};
    for (const auto [model, double_speed, optimized, scroll_x, window_x, positions] :
         std::views::cartesian_product(models, speeds, optimizations, scroll_values, window_values, object_positions)) {
        if (IsDmg(model) && double_speed)
            continue;
        CAPTURE(model);
        CAPTURE(double_speed);
        CAPTURE(optimized);
        CAPTURE(scroll_x);
        CAPTURE(window_x);
        CAPTURE(positions[0]);
        CAPTURE(positions[1]);
        Interrupts interrupts;
        GPU gpu(interrupts, false);
        gpu.SetModel(model);
        gpu.optimized_execution = optimized;
        gpu.doubleSpeed = double_speed;
        gpu.lcdc = 0xF3;
        gpu.currentLine = 10;
        gpu.windowY = 10;
        gpu.windowX = window_x;
        gpu.scrollX = scroll_x;
        gpu.backgroundPalette = 0xE4;
        for (auto color{0u}; color < shades.size(); ++color)
            gpu.bgpd[0][color].fill(shades[color]);
        gpu.RebuildColorLuts();
        for (auto row{0u}; row < 8; ++row) {
            const auto color{row % 3 + 1};
            gpu.vram[16 + row * 2] = color & 1 ? 0xFF : 0;
            gpu.vram[17 + row * 2] = color & 2 ? 0xFF : 0;
        }
        std::fill(gpu.vram.begin() + 0x1C00, gpu.vram.begin() + 0x2000, 1);
        for (auto index{0u}; index < positions.size(); ++index) {
            gpu.oam[index * 4] = 26;
            gpu.oam[index * 4 + 1] = positions[index];
        }
        prime_dmg_oam_scan(gpu);
        for (auto line{10u}; line <= 18; ++line) {
            while (gpu.currentLine == line && gpu.scanlineCounter < 400)
                gpu.Update();
            CAPTURE(line);
            CHECK(gpu.pixelsDrawn == SCREEN_WIDTH);
            CHECK(gpu.windowLineCounter_ == line - 10);
            const auto color{(line - 10) % 8 % 3 + 1};
            const auto expected{IsCgb(model) ? colors[color] : GPU::DMG_SHADE[color]};
            const auto pixels{std::span(gpu.GetScreenData() + line * SCREEN_WIDTH, SCREEN_WIDTH)};
            CHECK(std::ranges::all_of(pixels, [&](const auto pixel) { return pixel == expected; }));
            while (gpu.currentLine == line)
                gpu.Update();
        }
    }
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

TEST_CASE("ppu: window enable qualifies the retained WY match") {
    constexpr std::array models = {Model::DMGB, Model::CGBC, Model::CGBE};
    constexpr std::array optimizeds = {false, true};
    constexpr std::array speeds = {false, true};
    constexpr std::array<u32, 2> enable_dots = {450, 452};
    for (const auto [model, optimized, speed, enable_dot] : std::views::cartesian_product(models, optimizeds, speeds, enable_dots)) {
        if (IsDmg(model) && speed)
            continue;
        CAPTURE(ModelName(model));
        CAPTURE(optimized);
        CAPTURE(speed);
        CAPTURE(enable_dot);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(model);
        gpu.optimized_execution = optimized;
        gpu.doubleSpeed = speed;
        gpu.lcdc = 0x91;
        gpu.currentLine = 10;
        gpu.windowY = 10;
        gpu.windowX = 7;
        for (auto _ : std::views::indices(enable_dot)) {
            gpu.Update();
        }
        REQUIRE(gpu.scanlineCounter == enable_dot);
        gpu.WriteRegisters(LCD_CONTROL_BIT, 0xB1);
        for (u32 dot{enable_dot}; dot < 456 + 280; ++dot)
            gpu.Update();
        REQUIRE(gpu.currentLine == 11);
        REQUIRE(gpu.pixelsDrawn == SCREEN_WIDTH);
        CHECK(gpu.isFetchingWindow_ == (enable_dot == 450));
    }
}

TEST_CASE("ppu: blocked CGB palette writes preserve RAM and advance the address") {
    constexpr std::array models = {Model::CGBC, Model::CGBE};
    constexpr std::array speeds = {false, true};
    constexpr std::array optimizeds = {false, true};
    constexpr std::array<u16, 2> addresses = {BACKGROUND_PALETTE_INDEX, OBJ_PALETTE_INDEX};
    for (const auto [model, speed, optimized, address] : std::views::cartesian_product(models, speeds, optimizeds, addresses)) {
        CAPTURE(ModelName(model));
        CAPTURE(speed);
        CAPTURE(optimized);
        CAPTURE(address);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(model);
        gpu.doubleSpeed = speed;
        gpu.optimized_execution = optimized;
        gpu.lcdc = 0;
        gpu.WriteRegisters(address, 0x3F);
        gpu.WriteRegisters(address + 1, 0x12);
        gpu.WriteRegisters(address, 0);
        gpu.WriteRegisters(address + 1, 0x34);
        gpu.lcdc = 0x91;
        gpu.currentLine = 10;
        gpu.stat.mode = GPUMode::MODE_2;
        for (auto dot{0uz}; dot < 100 && !gpu.cpu_palette_blocked(); ++dot)
            gpu.Update();
        REQUIRE(gpu.cpu_palette_blocked());
        gpu.WriteRegisters(address, 0xBF);
        CHECK(gpu.ReadRegisters(address + 1) == 0xFF);
        gpu.WriteRegisters(address + 1, 0x56);
        CHECK((gpu.ReadRegisters(address) & 0x3F) == 0);
        gpu.WriteRegisters(address + 1, 0x78);
        CHECK((gpu.ReadRegisters(address) & 0x3F) == 1);
        for (auto dot{0uz}; dot < 300 && gpu.cpu_palette_blocked(); ++dot)
            gpu.Update();
        REQUIRE_FALSE(gpu.cpu_palette_blocked());
        REQUIRE(gpu.stat.mode == GPUMode::MODE_0);
        gpu.WriteRegisters(address, 0x3F);
        CHECK(gpu.ReadRegisters(address + 1) == 0x12);
        gpu.WriteRegisters(address, 0);
        CHECK(gpu.ReadRegisters(address + 1) == 0x34);
        gpu.WriteRegisters(address + 1, 0x56);
        CHECK(gpu.ReadRegisters(address + 1) == 0x56);
    }
}


TEST_CASE("ppu: sprite groups share background alignment without starving a completed tile") {
    constexpr std::array models = {Model::DMGB, Model::CGBC, Model::CGBE};
    for (const auto model : models) {
        CAPTURE(ModelName(model));
        const auto background = renderLine(model, false, {}, 0, 5);
        const auto sprites = renderLine(model, true, {8, 9, 10, 40, 41, 42, 72, 73, 74}, 0, 5);
        CHECK(sprites.rendered - background.rendered == 9 * 6);
    }
}

TEST_CASE("ppu: first-line CGB WY retention samples after the frame boundary") {
    constexpr std::array models = {Model::CGBC, Model::CGBE};
    constexpr std::array optimizations = {false, true};
    constexpr std::array speeds = {false, true};
    constexpr std::array<unsigned, 2> write_dot_values = {2, 4};
    for (const auto [model, optimized, double_speed, write_dot] : std::views::cartesian_product(models, optimizations, speeds, write_dot_values)) {
        CAPTURE(ModelName(model));
        CAPTURE(optimized);
        CAPTURE(double_speed);
        CAPTURE(write_dot);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(model);
        gpu.optimized_execution = optimized;
        gpu.doubleSpeed = double_speed;
        gpu.lcdc = 0xB1;
        gpu.currentLine = 153;
        gpu.scanlineCounter = 455;
        gpu.stat.mode = GPUMode::MODE_1;
        gpu.windowY = 0;
        gpu.windowX = 7;
        gpu.Update();
        REQUIRE(gpu.currentLine == 0);
        REQUIRE_FALSE(gpu.windowTriggeredThisFrame);
        while (gpu.scanlineCounter < write_dot)
            gpu.Update();
        gpu.WriteRegisters(WINDOW_Y, 255);
        while (gpu.scanlineCounter < 280)
            gpu.Update();
        CHECK(gpu.isFetchingWindow_ == (write_dot == 4));
    }
}

TEST_CASE("ppu: a late mode-2 enable can request STAT while blocking the following mode-1 request") {
    constexpr std::array models = {Model::CGBC, Model::CGBE};
    constexpr std::array coincidence_values = {false, true};
    for (const auto [model, coincidence] : std::views::cartesian_product(models, coincidence_values)) {
        CAPTURE(model);
        CAPTURE(coincidence);
        Interrupts interrupts;
        interrupts.interrupt_flag = 0;
        GPU gpu(interrupts);
        gpu.SetModel(model);
        gpu.doubleSpeed = true;
        gpu.lcdc = 0x91;
        gpu.currentLine = 143;
        gpu.scanlineCounter = 455;
        gpu.lyc = 143;
        gpu.stat.mode = GPUMode::MODE_0;
        gpu.stat.enable_m1_interrupt = true;
        gpu.stat.enable_ly_interrupt = coincidence;
        gpu.WriteRegisters(LCD_STATUS, coincidence ? 0x70 : 0x30);
        CHECK(interrupts.is_set(InterruptType::LCDStat) == !coincidence);
        interrupts.interrupt_flag = 0;
        for (auto dot{0u}; dot < 6; ++dot)
            gpu.Update();
        CHECK_FALSE(interrupts.is_set(InterruptType::LCDStat));
        CHECK(interrupts.is_set(InterruptType::VBlank));
    }
}

TEST_CASE("ppu: DMG live WY comparison applies before the horizontal window match") {
    constexpr std::array<unsigned, 3> write_dot_values = {84u, 88u, 92u};
    for (const auto write_dot : write_dot_values) {
        CAPTURE(write_dot);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(Model::DMGB);
        gpu.lcdc = 0xB1;
        gpu.currentLine = 10;
        gpu.windowY = 255;
        gpu.windowX = 7;
        while (gpu.scanlineCounter < write_dot)
            gpu.Update();
        gpu.WriteRegisters(WINDOW_Y, 10);
        while (gpu.scanlineCounter < 300)
            gpu.Update();
        CHECK(gpu.isFetchingWindow_ == (write_dot < 92));
        CHECK_FALSE(gpu.windowTriggeredThisFrame);
        while (gpu.scanlineCounter < 452)
            gpu.Update();
        CHECK(gpu.windowTriggeredThisFrame);
    }
}

TEST_CASE("ppu: disabling a CGB window fetch resumes its retained background pixels") {
    constexpr std::array models = {Model::CGBC, Model::CGBE};
    constexpr std::array observed_values = {false, true};
    constexpr std::array objects_values = {false, true};
    for (const auto [model, observed, objects] : std::views::cartesian_product(models, observed_values, objects_values)) {

        CAPTURE(model);
        CAPTURE(observed);
        CAPTURE(objects);
        Interrupts interrupts;
        GPU gpu(interrupts);
        diagnostics::ProvenanceRecorder recorder({.y = {10, 10}});
        gpu.SetModel(model);
        if (observed)
            gpu.provenance = &recorder.Context();
        gpu.lcdc = 0xF3;
        gpu.currentLine = 10;
        recorder.Context().position.ly = 10;
        gpu.windowX = objects ? 15 : 7;
        if (objects) {
            gpu.oam[0] = 26;
            gpu.oam[1] = 16;
        }
        gpu.windowTriggeredThisFrame = true;
        gpu.bgpd[0][1] = {8, 8, 8};
        gpu.bgpd[0][2] = {16, 16, 16};
        gpu.RebuildColorLuts();
        std::fill(gpu.vram.begin() + 0x1800, gpu.vram.begin() + 0x1C00, 1);
        std::fill(gpu.vram.begin() + 0x1C00, gpu.vram.begin() + 0x2000, 2);
        for (auto row{0u}; row < 8; ++row) {
            gpu.vram[16 + row * 2] = 0xFF;
            gpu.vram[33 + row * 2] = 0xFF;
        }
        while (gpu.scanlineCounter < (objects ? 104u : 96u))
            gpu.Update();
        REQUIRE(gpu.window_fetch_startup);
        REQUIRE(gpu.pixelsDrawn == (objects ? 8u : 0u));
        REQUIRE(gpu.backgroundQueue.size() == 8);
        gpu.WriteRegisters(LCD_CONTROL_BIT, 0xD3);
        CHECK(gpu.pixelsDrawn == (objects ? 8u : 1u));
        while (gpu.stat.mode != GPUMode::MODE_0)
            gpu.Update();
        CHECK(gpu.scanlineCounter == renderLine(model, false, {}).rendered + (objects ? 17u : 3u));
        for (auto x{0u}; x < SCREEN_WIDTH; ++x) {
            CAPTURE(x);
            CHECK(gpu.GetScreenData()[10 * SCREEN_WIDTH + x] == (x >= (objects ? 16u : 8u) && x < (objects ? 24u : 16u) ? 0xFF848484 : 0xFF424242));
        }
        if (observed) {
            const auto pixel = recorder.Query(0, 10);
            REQUIRE(pixel);
            CHECK(pixel->selected.source == diagnostics::PixelSource::Background);
            CHECK(pixel->selected.mapAddress == 0x9820);
        }
    }
}

TEST_CASE("ppu: DMG right-edge windows retain their trigger and count each activation") {
    constexpr std::array models = {Model::DMGB, Model::DMGC};
    constexpr std::array fine_values = {0, 3, 7};
    constexpr std::array reenable_values = {false, true};
    constexpr std::array optimizations = {false, true};
    for (const auto [model, fine, reenable, optimized] : std::views::cartesian_product(models, fine_values, reenable_values, optimizations)) {
        CAPTURE(model);
        CAPTURE(fine);
        CAPTURE(reenable);
        CAPTURE(optimized);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(model);
        gpu.optimized_execution = optimized;
        gpu.lcdc = 0xF1;
        gpu.currentLine = 10;
        gpu.windowY = 10;
        gpu.windowX = 166;
        gpu.scrollX = fine;
        gpu.backgroundPalette = 0xE4;
        for (auto tile{1u}; tile <= 32; ++tile) {
            gpu.vram[0x1C00 + tile - 1] = tile;
            for (auto row{0u}; row < 8; ++row) {
                for (auto x{0u}; x < 8; ++x) {
                    const auto color = (tile * 3 + row + x % 3) % 4;
                    gpu.vram[tile * 16 + row * 2] |= (color & 1) << (7 - x);
                    gpu.vram[tile * 16 + row * 2 + 1] |= (color >> 1) << (7 - x);
                }
            }
        }
        std::array<unsigned, 3> rendered{};
        while (gpu.currentLine < 13) {
            if (reenable && gpu.currentLine > 10) {
                if (gpu.scanlineCounter == 36)
                    gpu.WriteRegisters(LCD_CONTROL_BIT, 0xD1);
                if (gpu.scanlineCounter == 80)
                    gpu.WriteRegisters(LCD_CONTROL_BIT, 0xF1);
            }
            gpu.Update();
            if (gpu.currentLine < 13 && gpu.pixelsDrawn == SCREEN_WIDTH && rendered[gpu.currentLine - 10] == 0) {
                rendered[gpu.currentLine - 10] = gpu.scanlineCounter;
                if (gpu.currentLine > 10)
                    CHECK(gpu.windowLineCounter_ == (gpu.currentLine - 10) * (reenable ? 2 : 1));
            }
        }
        REQUIRE(rendered[0] != 0);
        CHECK(rendered[1] == rendered[0]);
        CHECK(rendered[2] == rendered[0]);
        for (auto line{10u}; line < 13; ++line) {
            for (auto x{0u}; x < SCREEN_WIDTH; ++x) {
                const auto window_x = x + 8 + fine;
                const auto row = (line - 10) * (reenable ? 2 : 1);
                const auto color = line == 10 ? 0 : ((window_x / 8 + 1) * 3 + row + window_x % 8 % 3) % 4;
                CAPTURE(line);
                CAPTURE(x);
                CHECK(gpu.GetScreenData()[line * SCREEN_WIDTH + x] == GPU::DMG_SHADE[color]);
            }
        }
    }
}

TEST_CASE("ppu: CGB STAT readback separates line comparisons from rendering modes") {
    constexpr std::array speeds = {false, true};
    constexpr std::array optimizations = {false, true};
    for (const auto [double_speed, optimized] : std::views::cartesian_product(speeds, optimizations)) {
        CAPTURE(double_speed);
        CAPTURE(optimized);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(Model::CGBC);
        gpu.optimized_execution = optimized;
        gpu.doubleSpeed = double_speed;
        gpu.lcdc = 0x91;
        gpu.currentLine = 152;
        gpu.scanlineCounter = 450;
        gpu.stat.mode = GPUMode::MODE_1;
        gpu.lyc = 153;
        interrupts.interrupt_flag = 0;
        while (gpu.currentLine == 152)
            gpu.Update();
        const unsigned comparison_delay = double_speed ? 1 : 2;
        for (auto dot{0u}; dot <= 12; ++dot) {
            REQUIRE(gpu.currentLine == 153);
            REQUIRE(gpu.scanlineCounter == dot);
            CHECK((gpu.ReadRegisters(LCD_STATUS) & 7) == (dot >= comparison_delay && dot < 8 ? 5 : 1));
            gpu.WriteRegisters(SCANLINE_COMPARE, 0);
            CHECK((gpu.ReadRegisters(LCD_STATUS) & 7) == (dot >= 8 + comparison_delay ? 5 : 1));
            gpu.WriteRegisters(SCANLINE_COMPARE, 153);
            CHECK(interrupts.interrupt_flag == 0);
            gpu.Update();
        }
        while (gpu.currentLine == 153)
            gpu.Update();
        gpu.WriteRegisters(SCANLINE_COMPARE, 0);
        CHECK((gpu.ReadRegisters(LCD_STATUS) & 7) == (double_speed ? 6 : 4));
        while (gpu.scanlineCounter < 82) {
            gpu.Update();
            const auto dot = gpu.scanlineCounter;
            const unsigned mode = dot < (double_speed ? 80u : 81u) ? 2 : 3;
            CHECK((gpu.ReadRegisters(LCD_STATUS) & 7) == (4 | mode));
            if (dot == 80)
                CHECK(gpu.stat.mode == GPUMode::MODE_3);
        }
    }
}

TEST_CASE("ppu: CGB LYC writes preserve a continuous match across the next comparison") {
    constexpr std::array speeds = {false, true};
    constexpr std::array write_dot_values = {450u, 452u};
    for (const auto [double_speed, write_dot] : std::views::cartesian_product(speeds, write_dot_values)) {
        CAPTURE(double_speed);
        CAPTURE(write_dot);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(Model::CGBC);
        gpu.doubleSpeed = double_speed;
        gpu.lcdc = 0x91;
        gpu.currentLine = 5;
        gpu.scanlineCounter = write_dot;
        gpu.stat.mode = GPUMode::MODE_0;
        gpu.lyc = 5;
        gpu.lycInterruptLine_ = true;
        gpu.stat.enable_ly_interrupt = true;
        gpu.statTriggered = true;
        interrupts.interrupt_flag = 0;
        gpu.WriteRegisters(SCANLINE_COMPARE, 6);
        CHECK(gpu.ReadRegisters(SCANLINE_COMPARE) == 6);
        while (gpu.currentLine == 5 || gpu.scanlineCounter < 10)
            gpu.Update();
        REQUIRE(gpu.currentLine == 6);
        CHECK(interrupts.is_set(InterruptType::LCDStat) == (write_dot == 450));
    }
}

TEST_CASE("ppu: HBlank interrupts retain STAT and LYC values captured before late writes") {
    constexpr std::array speeds = {false, true};
    constexpr std::array fine_values = {0u, 4u};
    constexpr std::array disable_lyc_values = {false, true};
    constexpr std::array early_values = {false, true};
    for (const auto [double_speed, fine, disable_lyc, early] : std::views::cartesian_product(speeds, fine_values, disable_lyc_values, early_values)) {
        CAPTURE(double_speed);
        CAPTURE(fine);
        CAPTURE(disable_lyc);
        CAPTURE(early);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(Model::CGBC);
        gpu.doubleSpeed = double_speed;
        gpu.lcdc = 0x91;
        gpu.currentLine = 10;
        gpu.scrollX = fine;
        gpu.lyc = disable_lyc ? 10 : 255;
        gpu.stat.enable_m0_interrupt = true;
        gpu.stat.enable_ly_interrupt = disable_lyc;
        const unsigned lead = disable_lyc ? (early ? (double_speed ? 2 : 5) : (double_speed ? 1 : 4)) : (early ? 1 : 0);
        const unsigned write_dot = 252 + fine - lead;
        while (gpu.scanlineCounter < write_dot)
            gpu.Update();
        interrupts.interrupt_flag = 0;
        gpu.WriteRegisters(disable_lyc ? SCANLINE_COMPARE : LCD_STATUS, disable_lyc ? 255 : 0);
        while (gpu.scanlineCounter < 300)
            gpu.Update();
        CHECK(interrupts.is_set(InterruptType::LCDStat) == (disable_lyc ? early : !early));
    }
}

TEST_CASE("ppu: right-edge fetches can delay STAT mode 0 after the HBlank interrupt") {
    constexpr std::array models = {Model::DMGB, Model::CGBC};
    constexpr std::array speeds = {false, true};
    constexpr std::array optimizations = {false, true};
    for (const auto [model, double_speed, optimized] : std::views::cartesian_product(models, speeds, optimizations)) {
        if (double_speed && model == Model::DMGB)
            continue;

        CAPTURE(model);
        CAPTURE(double_speed);
        CAPTURE(optimized);
        const auto measure = [&](const unsigned object_x, const unsigned window_x) {
            Interrupts interrupts;
            GPU gpu(interrupts);
            gpu.SetModel(model);
            gpu.optimized_execution = optimized;
            gpu.doubleSpeed = double_speed;
            gpu.lcdc = window_x ? 0xB3 : 0x93;
            gpu.currentLine = 10;
            gpu.lyc = 255;
            gpu.windowTriggeredThisFrame = true;
            gpu.windowX = window_x;
            gpu.stat.enable_m0_interrupt = true;
            gpu.oam[0] = 26;
            gpu.oam[1] = object_x;
            prime_dmg_oam_scan(gpu);
            interrupts.interrupt_flag = 0;
            std::array<unsigned, 3> times{};
            while (gpu.scanlineCounter < 400) {
                gpu.Update();
                if (!times[0] && ((interrupts.interrupt_flag | interrupts.interrupt_visible_pending) & 2))
                    times[0] = gpu.scanlineCounter;
                if (!times[1] && gpu.scanlineCounter > 100 && (gpu.ReadRegisters(LCD_STATUS) & 3) == 0)
                    times[1] = gpu.scanlineCounter;
                if (!times[2] && gpu.pixelsDrawn == SCREEN_WIDTH)
                    times[2] = gpu.scanlineCounter;
            }
            REQUIRE(times[0] != 0);
            REQUIRE(times[1] != 0);
            REQUIRE(times[2] != 0);
            return times;
        };
        const auto baseline = measure(200, 0);
        const auto last_object = measure(167, 0);
        const auto last_window = measure(167, 166);
        const auto earlier_object = measure(166, 0);
        CHECK(last_object[0] == baseline[0]);
        CHECK(last_object[1] > last_object[0]);
        CHECK(last_object[2] > last_object[0]);
        CHECK(last_window[0] == baseline[0]);
        CHECK(last_window[1] > last_window[0]);
        CHECK(last_window[2] > last_window[0]);
        CHECK(earlier_object[0] > baseline[0]);
    }
}

TEST_CASE("ppu: sprite fetch alignment follows the window tile origin independently of SCX") {
    constexpr std::array models = {Model::DMGB, Model::CGBC};
    constexpr std::array<uint8_t, 6> window_x_values = {1, 4, 7, 8, 15, 166};
    constexpr std::array<uint8_t, 3> scroll_x_values = {0, 3, 7};
    constexpr std::array<uint8_t, 8> offset_values = {0, 1, 2, 3, 4, 5, 6, 7};
    for (const auto [model, window_x, scroll_x, offset] : std::views::cartesian_product(models, window_x_values, scroll_x_values, offset_values)) {
        if (model == Model::DMGB && window_x == 166)
            continue;

        const auto baseline = renderLine(model, true, {}, window_x, scroll_x);
        const uint8_t object_x = window_x == 166 ? 167 : 24 + offset;
        CAPTURE(model);
        CAPTURE(window_x);
        CAPTURE(scroll_x);
        CAPTURE(object_x);
        const auto object = renderLine(model, true, {object_x}, window_x, scroll_x);
        const unsigned phase = (object_x - window_x - 1) & 7;
        const unsigned stall = 6 + (phase < 5 ? 5 - phase : 0);
        CHECK(object.rendered - baseline.rendered == stall);
        if (window_x == 166)
            break;

    }
}

TEST_CASE("ppu: DMG scans objects while disabled so a later enable can fetch them") {
    constexpr std::array optimizations = {false, true};
    constexpr std::array<unsigned, 3> enable_dot_values = {0, 100, 120};
    for (const auto [optimized, enable_dot] : std::views::cartesian_product(optimizations, enable_dot_values)) {
        CAPTURE(optimized);
        CAPTURE(enable_dot);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(Model::DMGB);
        gpu.optimized_execution = optimized;
        gpu.lcdc = 0x91;
        gpu.currentLine = 10;
        gpu.backgroundPalette = gpu.obp0Palette = 0xE4;
        gpu.oam[0] = 26;
        gpu.oam[1] = 24;
        gpu.oam[2] = 1;
        for (auto row{0u}; row < 8; ++row)
            gpu.vram[16 + row * 2] = 255;
        prime_dmg_oam_scan(gpu);
        while (gpu.scanlineCounter < 400) {
            if (enable_dot && gpu.scanlineCounter == enable_dot)
                gpu.WriteRegisters(LCD_CONTROL_BIT, 0x93);
            gpu.Update();
        }
        const bool visible = enable_dot == 100;
        CHECK(gpu.GetScreenData()[10 * SCREEN_WIDTH + 16] == GPU::DMG_SHADE[visible ? 1 : 0]);
        CHECK(gpu.mode3EndDot_ == (visible ? 263 : 252));
    }
}

TEST_CASE("ppu: DMG object disable releases alignment and active fetch stalls") {
    constexpr std::array models = {Model::DMGB, Model::CGBC};
    constexpr std::array optimizations = {false, true};
    constexpr std::array<unsigned, 3> disable_dot_values = {108, 112, 116};
    for (const auto [model, optimized, disable_dot] : std::views::cartesian_product(models, optimizations, disable_dot_values)) {
        CAPTURE(model);
        CAPTURE(optimized);
        CAPTURE(disable_dot);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(model);
        gpu.optimized_execution = optimized;
        gpu.lcdc = 0x93;
        gpu.currentLine = 10;
        gpu.oam[0] = 26;
        gpu.oam[1] = 25;
        prime_dmg_oam_scan(gpu);
        while (gpu.scanlineCounter < 400) {
            if (gpu.scanlineCounter == disable_dot)
                gpu.WriteRegisters(LCD_CONTROL_BIT, 0x91);
            gpu.Update();
        }
        CHECK(gpu.pixelsDrawn == SCREEN_WIDTH);
        CHECK(gpu.mode3EndDot_ == (model == Model::DMGB ? 252 + disable_dot - 108 : 263));
    }
}

TEST_CASE("ppu: CGB OAM selection combines both object-size samples") {
    constexpr std::array speeds = {false, true};
    constexpr std::array optimizations = {false, true};
    constexpr std::array<unsigned, 3> index_values = {0, 1, 39};
    constexpr std::array<int, 3> offset_values = {-2, 0, 2};
    constexpr std::array initially_large_values = {false, true};
    for (const auto [double_speed, optimized, index, offset, initially_large] : std::views::cartesian_product(speeds, optimizations, index_values, offset_values, initially_large_values)) {
        CAPTURE(double_speed);
        CAPTURE(optimized);
        CAPTURE(index);
        CAPTURE(offset);
        CAPTURE(initially_large);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(Model::CGBC);
        gpu.doubleSpeed = double_speed;
        gpu.optimized_execution = optimized;
        gpu.lcdc = initially_large ? 0x97 : 0x93;
        gpu.currentLine = 10;
        gpu.oam[index * 4] = 18;
        gpu.oam[index * 4 + 1] = 24;
        const unsigned write_dot = 2 + index * 2 + offset;
        while (gpu.scanlineCounter < 90) {
            if (gpu.scanlineCounter == write_dot)
                gpu.WriteRegisters(LCD_CONTROL_BIT, initially_large ? 0x93 : 0x97);
            gpu.Update();
        }
        const bool selected = initially_large ? offset >= 0 : offset <= 0;
        REQUIRE(gpu.spriteBuffer.size() == unsigned(selected));
        if (selected)
            CHECK(gpu.spriteBuffer.begin()->sprite_num / 2 == index);
    }
}

TEST_CASE("ppu: DMG OAM selection uses the second object-size sample across the line boundary") {
    constexpr std::array optimizations = {false, true};
    constexpr std::array<unsigned, 3> index_values = {0, 1, 39};
    constexpr std::array<int, 3> offset_values = {-1, 0, 1};
    constexpr std::array initially_large_values = {false, true};
    for (const auto [optimized, index, offset, initially_large] : std::views::cartesian_product(optimizations, index_values, offset_values, initially_large_values)) {
        CAPTURE(optimized);
        CAPTURE(index);
        CAPTURE(offset);
        CAPTURE(initially_large);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(Model::DMGB);
        gpu.optimized_execution = optimized;
        gpu.lcdc = initially_large ? 0x97 : 0x93;
        gpu.currentLine = 9;
        gpu.scanlineCounter = 450;
        gpu.stat.mode = GPUMode::MODE_0;
        gpu.oam[index * 4] = 18;
        gpu.oam[index * 4 + 1] = 24;
        const unsigned write_dot = 456 + index * 2 - 2 + offset;
        for (auto dot{450u}; dot < 456 + 90; ++dot) {
            if (dot == write_dot)
                gpu.WriteRegisters(LCD_CONTROL_BIT, initially_large ? 0x93 : 0x97);
            gpu.Update();
        }
        REQUIRE(gpu.currentLine == 10);
        const bool selected = initially_large ? offset > 0 : offset <= 0;
        REQUIRE(gpu.spriteBuffer.size() == unsigned(selected));
        if (selected)
            CHECK(gpu.spriteBuffer.begin()->sprite_num / 2 == index);
    }
}


TEST_CASE("ppu: disabled LCD retains the readable comparison for STAT write interrupts") {
    constexpr std::array models = {Model::DMGB, Model::CGBC, Model::CGBE};
    constexpr std::array optimizations = {false, true};
    constexpr std::array match_values = {false, true};
    constexpr std::array previously_enabled_values = {false, true};
    constexpr std::array enable_values = {false, true};
    constexpr std::array<uint8_t, 2> new_lyc_values = {0, 0xFF};
    for (const auto [model, optimized, match, previously_enabled, enable, new_lyc] :
         std::views::cartesian_product(models, optimizations, match_values, previously_enabled_values, enable_values, new_lyc_values)) {
        CAPTURE(model);
        CAPTURE(optimized);
        CAPTURE(match);
        CAPTURE(previously_enabled);
        CAPTURE(enable);
        CAPTURE(new_lyc);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(model);
        gpu.optimized_execution = optimized;
        gpu.lcdc = 0x91;
        gpu.currentLine = 10;
        gpu.scanlineCounter = 300;
        gpu.stat.mode = GPUMode::MODE_0;
        gpu.lyc = match ? 10 : 11;
        gpu.Update();
        REQUIRE(((gpu.ReadRegisters(LCD_STATUS) & 4) != 0) == match);
        gpu.WriteRegisters(LCD_CONTROL_BIT, 0x11);
        gpu.WriteRegisters(SCANLINE_COMPARE, new_lyc);
        gpu.WriteRegisters(LCD_STATUS, previously_enabled ? 0x40 : 0);
        interrupts.interrupt_flag = 0;
        gpu.WriteRegisters(LCD_STATUS, enable ? 0x40 : 0);
        for (auto dot{0u}; dot < 16; ++dot)
            gpu.Update();
        CHECK(((gpu.ReadRegisters(LCD_STATUS) & 4) != 0) == match);
        CHECK(gpu.ReadRegisters(CURRENT_SCANLINE) == 0);
        CHECK(interrupts.is_set(InterruptType::LCDStat) == (match && !previously_enabled && (!IsCgb(model) || enable)));
    }
    for (const auto [model, optimized] : std::views::cartesian_product(models, optimizations)) {
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(model);
        gpu.optimized_execution = optimized;
        gpu.lcdc = 0x91;
        gpu.currentLine = 10;
        gpu.scanlineCounter = IsCgb(model) ? 0 : 452;
        gpu.stat.mode = IsCgb(model) ? GPUMode::MODE_2 : GPUMode::MODE_0;
        gpu.stat.coincidence_flag = true;
        gpu.lyc = 10;
        REQUIRE((gpu.ReadRegisters(LCD_STATUS) & 4) == 0);
        gpu.WriteRegisters(LCD_CONTROL_BIT, 0x11);
        CHECK((gpu.ReadRegisters(LCD_STATUS) & 4) == 0);
    }
}


TEST_CASE("ppu: CGB LYC comparisons retain sampled STAT enable and blocking inputs") {
    constexpr std::array speeds = {false, true};
    constexpr std::array optimizations = {false, true};
    constexpr std::array<unsigned, 3> change_values = {0u, 1u, 2u};
    constexpr std::array<unsigned, 4> lead_values = {0u, 1u, 2u, 3u};
    for (const auto [double_speed, optimized, change, lead] : std::views::cartesian_product(speeds, optimizations, change_values, lead_values)) {
        CAPTURE(double_speed);
        CAPTURE(optimized);
        CAPTURE(change);
        CAPTURE(lead);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(Model::CGBC);
        gpu.doubleSpeed = double_speed;
        gpu.optimized_execution = optimized;
        gpu.lcdc = 0x91;
        gpu.currentLine = 153;
        gpu.scanlineCounter = 8 - lead;
        gpu.stat.mode = GPUMode::MODE_1;
        gpu.stat.enable_ly_interrupt = true;
        gpu.stat.enable_m1_interrupt = change == 1;
        gpu.lyc = 0;
        gpu.lycInterruptLine_ = false;
        gpu.WriteRegisters(LCD_STATUS, change == 0 ? 0 : change == 1 ? 0x40 : 0x50);
        interrupts.interrupt_flag = 0;
        interrupts.cancel_pending(InterruptType::LCDStat);
        while (gpu.scanlineCounter < 16)
            gpu.Update();
        const bool retained = lead < (double_speed ? 1u : 2u);
        CHECK(interrupts.is_set(InterruptType::LCDStat) == (change == 1 ? !retained : retained));
        CHECK((gpu.ReadRegisters(LCD_STATUS) & 4) != 0);
    }
}


TEST_CASE("ppu: CGB LYC cancellation and mode-2 blocking use separate capture intervals") {
    constexpr std::array speeds = {false, true};
    constexpr std::array optimizations = {false, true};
    constexpr std::array mode2_values = {false, true};
    for (const auto [double_speed, optimized, mode2] : std::views::cartesian_product(speeds, optimizations, mode2_values)) {
        for (auto write_dot{450u}; write_dot <= 457; ++write_dot) {
            CAPTURE(double_speed);
            CAPTURE(optimized);
            CAPTURE(mode2);
            CAPTURE(write_dot);
            Interrupts interrupts;
            GPU gpu(interrupts);
            gpu.SetModel(Model::CGBC);
            gpu.doubleSpeed = double_speed;
            gpu.optimized_execution = optimized;
            gpu.lcdc = 0x91;
            gpu.currentLine = 1;
            gpu.scanlineCounter = 450;
            gpu.stat.mode = GPUMode::MODE_0;
            gpu.stat.enable_ly_interrupt = true;
            gpu.stat.enable_m2_interrupt = mode2;
            gpu.lyc = mode2 ? 1 : 2;
            gpu.lycInterruptLine_ = mode2;
            interrupts.interrupt_flag = 0;
            for (auto dot{450u}; dot < 468; ++dot) {
                if (dot == write_dot)
                    gpu.WriteRegisters(SCANLINE_COMPARE, 0xFF);
                gpu.Update();
            }
            const bool expected = mode2 ? write_dot < (double_speed ? 453u : 451u) : write_dot >= 456;
            CHECK(interrupts.is_set(InterruptType::LCDStat) == expected);
            CHECK((gpu.ReadRegisters(LCD_STATUS) & 4) == 0);
        }
    }
}


TEST_CASE("ppu: DMG mode-1 entry has independent mode-source blocking") {
    constexpr std::array models = {Model::DMGB, Model::DMGC};
    constexpr std::array optimizations = {false, true};
    constexpr std::array match_values = {false, true};
    constexpr std::array<unsigned, 8> enables_values = {0, 1, 2, 3, 4, 5, 6, 7};
    for (const auto [model, optimized, match, enables] : std::views::cartesian_product(models, optimizations, match_values, enables_values)) {
        CAPTURE(model);
        CAPTURE(optimized);
        CAPTURE(match);
        CAPTURE(enables);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(model);
        gpu.optimized_execution = optimized;
        gpu.lcdc = 0x91;
        gpu.currentLine = 143;
        gpu.scanlineCounter = 450;
        gpu.stat.mode = GPUMode::MODE_0;
        gpu.stat.enable_m0_interrupt = enables & 1;
        gpu.stat.enable_m1_interrupt = true;
        gpu.stat.enable_m2_interrupt = enables & 2;
        gpu.stat.enable_ly_interrupt = enables & 4;
        gpu.lyc = match ? 143 : 0xFF;
        gpu.lycInterruptLine_ = match;
        gpu.stat.coincidence_flag = match;
        gpu.m2IrqRaisedEarly = gpu.stat.enable_m2_interrupt;
        gpu.statTriggered = gpu.stat.enable_m0_interrupt || gpu.m2IrqRaisedEarly || (match && gpu.stat.enable_ly_interrupt);
        interrupts.interrupt_flag = 0;
        while (gpu.currentLine == 143)
            gpu.Update();
        CHECK(interrupts.is_set(InterruptType::VBlank));
        CHECK(interrupts.is_set(InterruptType::LCDStat) == ((enables & 3) == 0));
        interrupts.interrupt_flag = 0;
        while (gpu.scanlineCounter < 20)
            gpu.Update();
        CHECK_FALSE(interrupts.is_set(InterruptType::LCDStat));
    }
}

TEST_CASE("ppu: LCD startup retains the qualified window match before WY changes") {
    constexpr std::array models = {Model::DMGB, Model::CGBC, Model::CGBE};
    constexpr std::array speeds = {false, true};
    constexpr std::array optimizations = {false, true};
    constexpr std::array<uint8_t, 2> initial_wy_values = {0, 1};
    constexpr std::array initially_enabled_values = {false, true};
    constexpr std::array previous_match_values = {false, true};
    for (const auto [model, double_speed, optimized, initial_wy, initially_enabled, previous_match] : std::views::cartesian_product(models, speeds, optimizations, initial_wy_values, initially_enabled_values, previous_match_values)) {
        if (IsDmg(model) && double_speed)
            continue;

        CAPTURE(ModelName(model));
        CAPTURE(double_speed);
        CAPTURE(optimized);
        CAPTURE(initial_wy);
        CAPTURE(initially_enabled);
        CAPTURE(previous_match);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(model);
        gpu.doubleSpeed = double_speed;
        gpu.optimized_execution = optimized;
        gpu.WriteRegisters(LCD_CONTROL_BIT, 0);
        gpu.backgroundPalette = 0xE4;
        gpu.windowLineCounter_ = 0xFF;
        gpu.windowTriggeredThisFrame = previous_match;
        gpu.WriteRegisters(WINDOW_Y, initial_wy);
        gpu.WriteRegisters(WINDOW_X, 7);
        for (auto i{0_u16}; i < 16; ++i)
            gpu.WriteVRAM(0x8010 + i, 0xFF);
        for (auto i{0_u16}; i < 32; ++i)
            gpu.WriteVRAM(0x9C00 + i, 1);
        if (IsCgb(model)) {
            gpu.WriteRegisters(BACKGROUND_PALETTE_INDEX, 0x80);
            gpu.WriteRegisters(BACKGROUND_PALETTE_DATA, 0xFF);
            gpu.WriteRegisters(BACKGROUND_PALETTE_DATA, 0x7F);
            gpu.WriteRegisters(BACKGROUND_PALETTE_INDEX, 0x86);
            gpu.WriteRegisters(BACKGROUND_PALETTE_DATA, 0);
            gpu.WriteRegisters(BACKGROUND_PALETTE_DATA, 0);
        }
        gpu.WriteRegisters(LCD_CONTROL_BIT, initially_enabled ? 0xF1 : 0xD1);
        for (auto dot{0u}; dot < 20; ++dot)
            gpu.Update();
        gpu.WriteRegisters(WINDOW_Y, 0xFF);
        gpu.WriteRegisters(LCD_CONTROL_BIT, 0xF1);
        for (auto dot{20u}; dot < 300; ++dot)
            gpu.Update();
        REQUIRE(gpu.pixelsDrawn == SCREEN_WIDTH);
        CHECK(gpu.GetScreenData()[80] == (initially_enabled && initial_wy == 0 ? 0xFF000000u : 0xFFFFFFFFu));
    }
}

TEST_CASE("ppu: LCD palette sampling changes the transient shade without shifting pixels") {
    constexpr std::array sampling_values = {DmgPaletteSampling::BitwiseOr, DmgPaletteSampling::NewValue};
    constexpr std::array optimizations = {false, true};
    constexpr std::array<uint16_t, 3> addresses = {DMG_BACKGROUND_PALETTE, DMG_OBJ_PALETTE_0, DMG_OBJ_PALETTE_1};
    for (const auto [sampling, optimized, address] : std::views::cartesian_product(sampling_values, optimizations, addresses)) {
        CAPTURE(sampling);
        CAPTURE(optimized);
        CAPTURE(address);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(Model::DMGB);
        gpu.dmg_palette_sampling = sampling;
        gpu.optimized_execution = optimized;
        gpu.lcdc = 0x93;
        gpu.stat.mode = GPUMode::MODE_3;
        gpu.scanlineCounter = 120;
        gpu.pixelsDrawn = 40;
        gpu.initialSCXSet = true;
        gpu.backgroundPalette = gpu.obp0Palette = gpu.obp1Palette = 0x55;
        for (auto pixel{0u}; pixel < 8; ++pixel) {
            gpu.backgroundQueue.push_back(Pixel{.color = 1});
            gpu.spriteArray[pixel] = address == DMG_BACKGROUND_PALETTE
                                            ? Pixel{.is_placeholder = true}
                                            : Pixel{.color = 1, .dmg_palette = static_cast<uint8_t>(address == DMG_OBJ_PALETTE_1),
                                                    .is_sprite = true, .is_placeholder = false};
        }
        gpu.WriteRegisters(address, 0xAA);
        for (auto dot{0u}; dot < 3; ++dot) {
            gpu.Update();
            CHECK(gpu.pixelsDrawn == 41 + dot);
            CHECK(gpu.scanlineCounter == 121 + dot);
        }
        CHECK(gpu.GetScreenData()[40] == 0xFFC0C0C0u);
        CHECK(gpu.GetScreenData()[41] == (sampling == DmgPaletteSampling::BitwiseOr ? 0xFF000000u : 0xFF606060u));
        CHECK(gpu.GetScreenData()[42] == 0xFF606060u);
    }
}


TEST_CASE("ppu: sprite tile reads drive the position bus retained during OAM DMA") {
    constexpr std::array models = {Model::DMGB, Model::CGBC, Model::CGBE};
    constexpr std::array speeds = {false, true};
    constexpr std::array<uint8_t, 2> tile_values = {0, 27};
    for (const auto [model, double_speed, tile] : std::views::cartesian_product(models, speeds, tile_values)) {
        if (model == Model::DMGB && double_speed)
            continue;

        CAPTURE(ModelName(model));
        CAPTURE(double_speed);
        CAPTURE(tile);
        Interrupts interrupts;
        GPU gpu(interrupts);
        gpu.SetModel(model);
        gpu.doubleSpeed = double_speed;
        gpu.lcdc = 0x93;
        gpu.currentLine = 10;
        gpu.oam.fill(0xA0);
        gpu.oam[156] = 26;
        gpu.oam[157] = 24;
        gpu.oam[158] = tile;
        prime_dmg_oam_scan(gpu);
        unsigned elapsed = 0;
        while (gpu.stat.mode != GPUMode::MODE_0 && elapsed++ < 456)
            gpu.Update();
        REQUIRE(elapsed < 456);
        REQUIRE(gpu.spriteFetchedThisLine_);
        gpu.oam_dma_active = true;
        elapsed = 0;
        while ((gpu.currentLine != 11 || gpu.scanlineCounter < 84) && elapsed++ < 456)
            gpu.Update();
        REQUIRE(elapsed < 456);
        CHECK(gpu.spriteBuffer.size() == (tile == 27 ? 10u : 0u));
        for (const auto &sprite : gpu.spriteBuffer)
            CHECK(sprite.y == 11);
    }
}
