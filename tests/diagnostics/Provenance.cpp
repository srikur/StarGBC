#include "diagnostics/Provenance.h"
#include <doctest/doctest.h>
#include "Gameboy.h"

namespace {
    void tickPixel(GPU &gpu, diagnostics::ProvenanceRecorder &recorder, const uint64_t tick) {
        recorder.Context().position = {
                .tick = tick, .frame = gpu.displayFrames_, .dot = static_cast<uint16_t>(gpu.scanlineCounter), .ly = gpu.currentLine};
        gpu.Update();
    }

    void configurePixelLine(GPU &gpu) {
        gpu.SetModel(Model::CGBC);
        gpu.lcdc = 0x91;
        gpu.backgroundPalette = 0xE4;
        gpu.vram[0x1800] = 1;
        gpu.vram[16] = 0xFF;
        gpu.bgpd[0][0] = {31, 31, 31};
        gpu.bgpd[0][1] = {20, 20, 20};
        gpu.RebuildColorLuts();
    }
} // namespace

TEST_CASE("diagnostics: provenance does not change pixels hardware state or timeline") {
    for (const auto model : {Model::DMGB, Model::CGBC, Model::CGBE}) {
        const GameboySettings settings{.romName = "roms/acid/cgb-acid2.gbc", .model = model, .noBootrom = true};
        Gameboy continuous(settings);
        Gameboy observed(settings);
        diagnostics::ProvenanceRecorder recorder({.y = {10, 10}});
        observed.RecordPixelsWith(&recorder.Context());
        for (unsigned interval = 0; interval < 3; ++interval) {
            continuous.RunFrame();
            observed.RunFrame();
            CHECK(continuous.SaveState() == observed.SaveState());
            CHECK(continuous.TimelineTicks() == observed.TimelineTicks());
            CHECK(std::ranges::equal(std::span(continuous.GetScreenData(), SCREEN_WIDTH * SCREEN_HEIGHT),
                                     std::span(observed.GetScreenData(), SCREEN_WIDTH * SCREEN_HEIGHT)));
        }
        REQUIRE(recorder.Query(20, 10));
        CHECK(recorder.Query(20, 10)->finalColor == observed.GetScreenData()[10 * SCREEN_WIDTH + 20]);
        CHECK(recorder.Pixels().size() <= 160);
    }
}

TEST_CASE("diagnostics: pixel origins retain bitplane reads across mid-fetch SCY and later VRAM changes") {
    Interrupts interrupts;
    GPU gpu(interrupts);
    configurePixelLine(gpu);
    gpu.vram[19] = 0xFF;
    diagnostics::ProvenanceRecorder recorder;
    gpu.provenance = &recorder.Context();
    bool changed{};
    for (uint64_t tick = 0; tick < 900 && !gpu.pixelsDrawn; tick += 2) {
        tickPixel(gpu, recorder, tick);
        if (!changed && !gpu.firstScanlineDataHigh && gpu.fetcherState_ == FetcherState::GetTileDataHigh) {
            gpu.WriteRegisters(0xFF42, 1);
            gpu.vram[16] = 0;
            changed = true;
        }
    }
    REQUIRE(changed);
    REQUIRE(gpu.pixelsDrawn == 1);
    gpu.vram.fill(0);
    gpu.WriteRegisters(0xFF42, 7);
    const auto pixel = recorder.Query(0, 0);
    REQUIRE(pixel);
    REQUIRE(pixel->selected.valid);
    CHECK(pixel->selected.mapAddress == 0x9800);
    CHECK(pixel->selected.lowAddress == 0x8010);
    CHECK(pixel->selected.highAddress == 0x8013);
    CHECK(pixel->selected.lowRow == 0);
    CHECK(pixel->selected.highRow == 1);
    CHECK(pixel->selected.lowRead == 255);
    CHECK(pixel->selected.highRead == 255);
    CHECK(pixel->selected.color == 3);
    CHECK(pixel->selected.lowTick < pixel->selected.highTick);
}

TEST_CASE("diagnostics: post-output palette changes retain both emitted and final pixel colors") {
    Interrupts interrupts;
    GPU gpu(interrupts);
    configurePixelLine(gpu);
    gpu.SetModel(Model::CGBE);
    gpu.dmgCompat = true;
    diagnostics::ProvenanceRecorder recorder;
    gpu.provenance = &recorder.Context();
    for (uint64_t tick = 0; tick < 900 && gpu.pixelsDrawn < 3; tick += 2)
        tickPixel(gpu, recorder, tick);
    REQUIRE(gpu.pixelsDrawn == 3);
    const auto emitted = recorder.Query(0, 0);
    REQUIRE(emitted);
    gpu.WriteRegisters(0xFF47, 0);
    const auto recolored = recorder.Query(0, 0);
    REQUIRE(recolored);
    CHECK(recolored->recolored);
    CHECK(recolored->emittedColor == emitted->emittedColor);
    CHECK(recolored->finalColor != emitted->emittedColor);
    CHECK(recolored->finalColor == gpu.GetScreenData()[0]);
    CHECK(recolored->finalColorTick >= recolored->emission.tick);
    CHECK(recolored->selected.source == emitted->selected.source);
    REQUIRE(recorder.Recolors().size() == 3);
    CHECK(recorder.Recolors()[0].registerAddress == 0xFF47);
}

TEST_CASE("diagnostics: restoring without provenance history marks a new branch with unknown in-flight origins") {
    Gameboy machine({.romName = "roms/acid/cgb-acid2.gbc", .model = Model::CGBC, .noBootrom = true});
    diagnostics::ProvenanceRecorder recorder({.y = {10, 10}});
    machine.RecordPixelsWith(&recorder.Context());
    machine.RunFrame();
    const auto checkpoint = machine.SaveDebugState();
    machine.RunFrame();
    REQUIRE(recorder.Query(20, 10));
    REQUIRE(machine.LoadDebugState(checkpoint));
    CHECK(recorder.Context().branch == 1);
    CHECK_FALSE(recorder.Query(20, 10));
    CHECK_FALSE(recorder.Context().backgroundFetch.valid);
    for (unsigned interval = 0; interval < 60 && !recorder.Query(20, 10); ++interval)
        machine.RunFrame();
    REQUIRE(recorder.Query(20, 10));
    CHECK(recorder.Query(20, 10)->branch == 1);
}
