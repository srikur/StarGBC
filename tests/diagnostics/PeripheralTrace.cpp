#include "GameboyFiles.h"
#include <doctest/doctest.h>
#include "stargbc/Gameboy.h"
#include "diagnostics/Trace.h"

namespace {
    template<class T>
    consteval std::meta::info peripheralMember(const std::string_view name) {
        for (const auto member : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
            if (std::meta::identifier_of(member) == name)
                return member;
        throw "Unknown peripheral member";
    }

    auto only(const diagnostics::Component component, const diagnostics::EventKind kind) {
        return diagnostics::EventFilter{.components = uint64_t{1} << static_cast<unsigned>(component),
                                        .kinds = uint64_t{1} << static_cast<unsigned>(kind)};
    }
} // namespace

TEST_CASE(

        "diagnostics: trigger retains bounded history and following events including a filtered trigger") {
    auto capture = only(diagnostics::Component::Bus, diagnostics::EventKind::BusRead);
    auto match = only(diagnostics::Component::Bus, diagnostics::EventKind::BusWrite);
    diagnostics::TraceRecorder recorder(4, capture);
    recorder.Arm({match, 2, 1});
    for (uint64_t tick = 0; tick < 10; ++tick) {
        recorder.Context().position.tick = tick;
        recorder.Context().Emit(diagnostics::Component::Bus, tick == 5 ? diagnostics::EventKind::BusWrite : diagnostics::EventKind::BusRead);
    }
    REQUIRE(recorder.CaptureComplete());
    const auto metadata = recorder.Metadata();
    REQUIRE(metadata.triggerEvent);
    CHECK(metadata.triggerEvent->position.tick == 5);
    CHECK(metadata.observed == 10);
    CHECK(metadata.overwritten == 3);
    const auto events = recorder.Events();
    REQUIRE(events.size() == 5);
    CHECK(events[0].kind == diagnostics::EventKind::Overflow);
    for (size_t i = 1; i < events.size(); ++i)
        CHECK(events[i].position.tick == i + 2);
    diagnostics::TraceRecorder invalid(4);
    CHECK_THROWS_AS(invalid.Arm({match, 2, 2}), std::invalid_argument);
    diagnostics::TraceRecorder immediate(1, capture);
    immediate.Arm({match, 0, 0});
    immediate.Context().Emit(diagnostics::Component::Bus, diagnostics::EventKind::BusWrite);
    CHECK(immediate.CaptureComplete());
    CHECK(immediate.Events().size() == 1);
}

TEST_CASE(

        "diagnostics: timer overflow and reload retain their distinct modeled ticks") {
    Audio audio;
    audio.SetModel(Model::DMGB);
    Interrupts interrupts;
    Timer timer(audio, interrupts);
    diagnostics::TraceRecorder recorder(32);
    timer.observations = &recorder.Context();
    timer.tima = 255;
    timer.tma = 37;
    timer.tac = 5;
    timer.divCounter = 15;
    for (uint64_t tick = 0; tick <= 8; tick += 2) {
        recorder.Context().position.tick = tick;
        timer.Tick(Speed::Regular);
    }
    const auto events = recorder.Events();
    REQUIRE(events.size() == 3);
    CHECK(events[0].kind == diagnostics::EventKind::TimerEdge);
    CHECK(events[1].kind == diagnostics::EventKind::TimerOverflow);
    CHECK(events[1].position.tick == 0);
    CHECK(events[2].kind == diagnostics::EventKind::TimerReload);
    CHECK(events[2].position.tick == 8);
    CHECK(events[2].value == 37);
    CHECK(timer.tima == 37);
}

TEST_CASE(

        "diagnostics: delayed PPU writes become visible to the fetcher at their apply tick") {
    Interrupts interrupts;
    GPU gpu(interrupts);
    gpu.SetModel(Model::DMGB);
    gpu.lcdc = 0x91;
    gpu.stat.mode = GPUMode::MODE_3;
    gpu.scanlineCounter = 120;
    gpu.initialSCXSet = true;
    gpu.scrollX = 3;
    diagnostics::TraceRecorder recorder(32, only(diagnostics::Component::PPU, diagnostics::EventKind::RegisterApply));
    gpu.observations = &recorder.Context();
    gpu.WriteRegisters(0xFF43, 9);
    CHECK(gpu.scxFetcherOld == 3);
    CHECK(gpu.scrollX == 9);
    for (uint64_t tick = 20; tick <= 24; tick += 2) {
        recorder.Context().position.tick = tick;
        gpu.Update();
    }
    const auto events = recorder.Events();
    REQUIRE(events.size() == 1);
    CHECK(events[0].address == 0xFF43);
    CHECK(events[0].value == 9);
    CHECK(events[0].position.tick == 24);
}

TEST_CASE(

        "diagnostics: OAM DMA transfer records identify source destination and ownership") {
    auto gameboy = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = Model::CGBC, .no_bootrom = true}});
    auto &bus = gameboy.[:peripheralMember<Gameboy>("bus_"):];
    auto &cpu = gameboy.[:peripheralMember<Gameboy>("cpu_"):];
    cpu.Lock();
    bus.gpu_.hdma.hdmaActive = false;
    for (uint16_t i = 0; i < 160; ++i)
        bus.DebugPoke(0xC000 + i, i ^ 0x5A);
    diagnostics::TraceRecorder recorder(512, {.components = uint64_t{1} << static_cast<unsigned>(diagnostics::Component::DMA)});
    gameboy.ObserveWith(&recorder.Context());
    bus.WriteByte(0xFF46, 0xC0, ComponentSource::CPU);
    static_cast<void>(gameboy.AdvanceTicks(1300));
    const auto events = recorder.Events();
    REQUIRE(events.size() == 162);
    CHECK(events.front().kind == diagnostics::EventKind::DmaOwnership);
    CHECK(events.front().value == 1);
    CHECK(events.back().kind == diagnostics::EventKind::DmaOwnership);
    CHECK(events.back().value == 0);
    for (unsigned i = 0; i < 160; ++i) {
        const auto &event = events[i + 1];
        CHECK(event.address == 0xFE00 + i);
        CHECK(event.detail == 0xC000 + i);
        CHECK(event.value == (i ^ 0x5A));
        CHECK(event.initiator == diagnostics::Initiator::DMA);
    }
}

TEST_CASE(

        "diagnostics: deferred audio retains modeled mixer ticks and inspection emits nothing") {
    std::array<std::vector<diagnostics::Event>, 2> results;
    for (const bool defer : {false, true}) {
        Audio audio;
        audio.SetModel(Model::CGBC);
        diagnostics::TraceRecorder recorder(4096, only(diagnostics::Component::APU, diagnostics::EventKind::AudioMix));
        audio.ObserveWith(&recorder.Context());
        for (const auto [address, value] : std::array<std::pair<uint16_t, uint8_t>, 7>{
                     {{0xFF26, 0x80}, {0xFF24, 0x77}, {0xFF25, 0x11}, {0xFF11, 0x80}, {0xFF12, 0xF0}, {0xFF13, 0xF8}, {0xFF14, 0x87}}})
            audio.WriteByte(address, value, false);
        for (uint64_t tick = 0; tick < 4000; tick += 2) {
            recorder.Context().position.tick = tick;
            audio.Tick();
            if (!defer)
                audio.CatchUp();
        }
        const auto before = recorder.Metadata().observed;
        static_cast<void>(audio.Inspect());
        static_cast<void>(audio.InspectRegister(0xFF76));
        CHECK(recorder.Metadata().observed == before);
        recorder.Context().position.tick = 9000;
        audio.CatchUp();
        results[defer] = recorder.Events();
    }
    REQUIRE(results[0].size() > 10);
    REQUIRE(results[0].size() == results[1].size());
    for (size_t i = 0; i < results[0].size(); ++i) {
        CHECK(results[0][i].position.tick == results[1][i].position.tick);
        CHECK(results[0][i].value == results[1][i].value);
        CHECK(results[0][i].detail == results[1][i].detail);
        CHECK(results[1][i].position.tick < 4000);
    }
}

TEST_CASE(

        "diagnostics: general DMA records both bus ownership edges and all sixteen destination writes") {
    auto gameboy = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = Model::CGBC, .no_bootrom = true}});
    auto &bus = gameboy.[:peripheralMember<Gameboy>("bus_"):];
    gameboy.[:peripheralMember<Gameboy>("cpu_"):].Lock();
    bus.gpu_.hdma = {};
    bus.gpu_.WriteRegisters(0xFF40, 0);
    for (uint16_t i = 0; i < 16; ++i)
        bus.DebugPoke(0xC000 + i, i + 1);
    diagnostics::TraceRecorder recorder(32, {.components = uint64_t{1} << static_cast<unsigned>(diagnostics::Component::DMA)});
    gameboy.ObserveWith(&recorder.Context());
    for (const auto [address, value] :
         std::array<std::pair<uint16_t, uint8_t>, 5>{{{0xFF51, 0xC0}, {0xFF52, 0}, {0xFF53, 0}, {0xFF54, 0}, {0xFF55, 0}}})
        bus.WriteByte(address, value, ComponentSource::CPU);
    static_cast<void>(gameboy.AdvanceTicks(100));
    const auto events = recorder.Events();
    REQUIRE(events.size() == 18);
    CHECK(events.front().value == 1);
    CHECK(events.back().value == 0);
    for (unsigned i = 0; i < 16; ++i) {
        CHECK(events[i + 1].kind == diagnostics::EventKind::DmaTransfer);
        CHECK(events[i + 1].address == 0x8000 + i);
        CHECK(events[i + 1].detail == 0xC000 + i);
        CHECK(events[i + 1].value == i + 1);
        CHECK(bus.DebugPeek(0x8000 + i) == i + 1);
    }
}
