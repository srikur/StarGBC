#include <doctest/doctest.h>
#include "Gameboy.h"
#include "diagnostics/Trace.h"

namespace {
    template<class T>
    consteval std::meta::info traceTestMember(const std::string_view name) {
        for (const auto member : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
            if (std::meta::identifier_of(member) == name)
                return member;
        throw "Unknown trace test member";
    }
}

TEST_CASE (
"diagnostics: tracing disabled observed and captured preserves hardware and pixels"
)
 {
    for (const auto model : {Model::DMGB, Model::CGBC}) {
        const GameboySettings settings{.romName = "roms/acid/cgb-acid2.gbc", .model = model, .noBootrom = true};
        Gameboy disabled(settings);
        Gameboy observed(settings);
        Gameboy captured(settings);
        diagnostics::TraceRecorder observer(32);
        diagnostics::TraceRecorder recorder(1024);
        observer.SetCaptureEnabled(false);
        observed.ObserveWith(&observer.Context());
        captured.ObserveWith(&recorder.Context());
        for (unsigned interval = 0; interval < 3; ++interval) {
            disabled.RunFrame();
            observed.RunFrame();
            captured.RunFrame();
            CHECK(disabled.SaveState() == observed.SaveState());
            CHECK(disabled.SaveState() == captured.SaveState());
            CHECK(std::ranges::equal(std::span(disabled.GetScreenData(), SCREEN_WIDTH * SCREEN_HEIGHT),
                                     std::span(captured.GetScreenData(), SCREEN_WIDTH * SCREEN_HEIGHT)));
        }
        CHECK(observer.Metadata().observed > 0);
        CHECK(observer.Metadata().retained == 0);
        CHECK(recorder.Metadata().overwritten > 0);
        const auto events = recorder.Events();
        REQUIRE(events.size() == 1025);
        CHECK(events.front().kind == diagnostics::EventKind::Overflow);
        for (size_t i = 1; i < events.size(); ++i) {
            CHECK(events[i - 1].sequence < events[i].sequence);
            if (events[i - 1].kind != diagnostics::EventKind::AudioMix && events[i].kind != diagnostics::EventKind::AudioMix)
                CHECK(events[i - 1].position.tick <= events[i].position.tick);
        }
    }
}

TEST_CASE (
"diagnostics: bus records distinguish blocked reads from stored FF and retain banks"
)
 {
    Gameboy gameboy({.romName = "roms/acid/cgb-acid2.gbc", .model = Model::CGBC, .noBootrom = true});
    diagnostics::TraceRecorder recorder(64);
    gameboy.ObserveWith(&recorder.Context());
    auto &gpu = gameboy.[:traceTestMember<Gameboy>("gpu_"):];
    auto &bus = gameboy.[:traceTestMember<Gameboy>("bus_"):];
    gpu.vramBank = 1;
    gpu.vram[0x2000] = 0xFF;
    gpu.lcdc = 0;
    CHECK(bus.ReadByte(0x8000, ComponentSource::CPU) == 0xFF);
    gpu.lcdc = 0x80;
    gpu.stat.mode = GPUMode::MODE_3;
    gpu.scanlineCounter = 120;
    CHECK(bus.ReadByte(0x8000, ComponentSource::CPU) == 0xFF);
    const auto events = recorder.Events();
    REQUIRE(events.size() == 2);
    CHECK_FALSE(events[0].blocked);
    CHECK(events[1].blocked);
    CHECK(events[0].bank == 1);
    CHECK(events[1].bank == 1);
    const auto count = recorder.Metadata().observed;
    static_cast<void>(gameboy.InspectCpuRead(0x8000));
    CHECK(recorder.Metadata().observed == count);
}

TEST_CASE (
"diagnostics: interrupt sampling uses CPU phases at both speeds and restore branches retain time"
)
 {
    for (const bool doubleSpeed : {false, true}) {
        Gameboy gameboy({.romName = "roms/acid/cgb-acid2.gbc", .model = Model::CGBC, .noBootrom = true});
        if (doubleSpeed) {
            auto &bus = gameboy.[:traceTestMember<Gameboy>("bus_"):];
            bus.prepareSpeedShift = true;
            bus.ChangeSpeed();
            REQUIRE(gameboy.Inspect().speed == Speed::Double);
        }
        diagnostics::TraceRecorder recorder(128);
        gameboy.ObserveWith(&recorder.Context());
        const auto checkpoint = gameboy.SaveState();
        static_cast<void>(gameboy.AdvanceTicks(8));
        std::vector<diagnostics::Event> samples;
        for (const auto &event : recorder.Events())
            if (event.kind == diagnostics::EventKind::InterruptSample)
                samples.push_back(event);
        REQUIRE(samples.size() == (doubleSpeed ? 4u : 2u));
        CHECK(samples[0].position.tick == (doubleSpeed ? 1u : 2u));
        CHECK(samples[1].position.tick == (doubleSpeed ? 3u : 6u));
        CHECK(samples[0].position.cpuPhase == 1);
        CHECK(samples[1].position.cpuPhase == 3);
        REQUIRE(gameboy.LoadState(checkpoint));
        const auto branch = recorder.Events().back();
        CHECK(branch.kind == diagnostics::EventKind::Branch);
        CHECK(branch.branch == 1);
        CHECK(branch.parentBranch == 0);
        CHECK(branch.position.tick == 0);
        CHECK(branch.relatedTick == 8);
    }
}

TEST_CASE (
"diagnostics: trace filters combine component kind address PC bank line dot and time"
)
 {
    diagnostics::EventFilter filter{
            .components = uint64_t{1} << static_cast<unsigned>(diagnostics::Component::Bus),
            .kinds = uint64_t{1} << static_cast<unsigned>(diagnostics::EventKind::BusRead),
            .ticks = {10, 20}, .addresses = {{0xFF40, 0xFF45}}, .pcs = {{0x4000, 0x4100}}, .pcBank = 3,
            .lines = {{5, 6}}, .dots = {{80, 120}}};
    diagnostics::Event event{.position = {.tick = 10, .dot = 80, .ly = 5}, .pc = 0x4000, .pcBank = 3, .address = 0xFF41,
                             .component = diagnostics::Component::Bus, .kind = diagnostics::EventKind::BusRead};
    CHECK(filter.Matches(event));
    event.position.tick = 21;
    CHECK_FALSE(filter.Matches(event));
    event.position.tick = 20;
    event.pcBank = 2;
    CHECK_FALSE(filter.Matches(event));
    event.pcBank = 3;
    event.blocked = true;
    CHECK(filter.Matches(event));
}
