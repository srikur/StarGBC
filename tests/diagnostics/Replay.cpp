#include "../../frontends/sdl/GameboyFiles.h"
#include "diagnostics/Replay.h"
#include <doctest/doctest.h>
#include <fstream>
#include "EmulationSession.h"
#include "diagnostics/Hash.h"
#include "diagnostics/Trace.h"
#include "diagnostics/Watchpoints.h"

namespace {
    template<class T>
    consteval std::meta::info replayMember(const std::string_view name) {
        for (const auto member : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
            if (std::meta::identifier_of(member) == name)
                return member;
        throw "Unknown replay member";
    }

    const diagnostics::InputFingerprint replayInputs{
            .rom = "fixture-ROM", .bootrom = "skip", .configuration = "CGBC-virtual-RTC", .cartridgeSave = "absent"};
    const diagnostics::CheckpointIdentity checkpointIdentity{.build = "test-executable", .inputs = replayInputs};

    std::filesystem::path replayDirectory() {
        const auto path = std::filesystem::temp_directory_path() /
                          ("stargbc-replay-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(path);
        return path;
    }
} // namespace

TEST_CASE("diagnostics: mid-frame debug checkpoints preserve pixels deferred audio and future output") {
    for (const auto model : {Model::DMGB, Model::CGBC, Model::CGBE}) {
        for (const bool doubleSpeed : {false, true}) {
            if (doubleSpeed && IsDmg(model))
                continue;
            const host::LaunchSettings settings{.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = model, .no_bootrom = true}};
            auto continuous = host::load_gameboy(settings);
            auto restored = host::load_gameboy(settings);
            if (doubleSpeed) {
                for (auto *machine : {&continuous, &restored}) {
                    auto &bus = machine->[:replayMember<Gameboy>("bus_"):];
                    bus.prepareSpeedShift = true;
                    bus.ChangeSpeed();
                }
            }
            for (const uint64_t ticks : {1, 158, 9, 71, 671, 140001}) {
                for (auto *machine : {&continuous, &restored}) {
                    static_cast<void>(machine->AdvanceTicks(ticks));
                    auto &audio = machine->[:replayMember<Gameboy>("audio_"):];
                    for (unsigned i = 0; i < 7; ++i)
                        audio.Tick();
                }
                const auto snapshot = restored.SaveDebugState();
                static_cast<void>(restored.AdvanceTicks(1301));
                REQUIRE(restored.LoadDebugState(snapshot));
                CHECK(restored.SaveDebugState() == continuous.SaveDebugState());
                for (auto *machine : {&continuous, &restored})
                    static_cast<void>(machine->AdvanceTicks(2003));
                CHECK(restored.SaveState() == continuous.SaveState());
                CHECK(std::ranges::equal(std::span(continuous.GetScreenData(), SCREEN_WIDTH * SCREEN_HEIGHT),
                                         std::span(restored.GetScreenData(), SCREEN_WIDTH * SCREEN_HEIGHT)));
                std::array<float, 128> left{}, right{};
                CHECK(continuous.ReadAudioSamples(left.data(), 64) == restored.ReadAudioSamples(right.data(), 64));
                CHECK(left == right);
            }
        }
    }
}

TEST_CASE("diagnostics: checkpoint files reject incompatible builds and corruption without changing the machine") {
    auto machine = host::load_gameboy({.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = Model::CGBC, .no_bootrom = true}});
    diagnostics::TraceRecorder recorder(32);
    machine.ObserveWith(&recorder.Context());
    const auto directory = replayDirectory();
    const auto path = directory / "state.checkpoint";
    static_cast<void>(machine.AdvanceTicks(173));
    const auto snapshot = machine.SaveDebugState();
    diagnostics::WriteCheckpoint(path, machine, checkpointIdentity);
    static_cast<void>(machine.AdvanceTicks(203));
    const auto before = machine.SaveDebugState();
    auto incompatible = checkpointIdentity;
    incompatible.build = "another-build";
    CHECK_THROWS_AS(static_cast<void>(diagnostics::RestoreCheckpoint(path, machine, incompatible)), std::invalid_argument);
    CHECK(machine.SaveDebugState() == before);
    const auto restored = diagnostics::RestoreCheckpoint(path, machine, checkpointIdentity);
    CHECK(restored.position.tick == 173);
    CHECK(machine.SaveDebugState() == snapshot);
    REQUIRE(recorder.Events().back().kind == diagnostics::EventKind::Branch);
    CHECK(recorder.Events().back().position.tick == 173);
    {
        std::fstream file(path, std::ios::in | std::ios::out | std::ios::binary);
        file.seekp(20);
        file.put('!');
    }
    CHECK_THROWS_AS(static_cast<void>(diagnostics::RestoreCheckpoint(path, machine, checkpointIdentity)), std::invalid_argument);
    CHECK(machine.SaveDebugState() == snapshot);
    std::filesystem::remove_all(directory);
}

TEST_CASE("diagnostics: replay records buttons after session overrides and restores the input cursor") {
    const host::LaunchSettings settings{.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = Model::CGBC, .no_bootrom = true}};
    auto recorded = host::load_gameboy(settings);
    diagnostics::InputRecorder recorder(recorded, replayInputs);
    EmulationSession session(recorded);
    session.SetEventHandler([&](const SessionEvent event, const uint64_t frame) {
        if (event == SessionEvent::BeforeFrame)
            session.SetOverrides(0xFF, frame == 1 ? 0x11 : 0x80);
    });
    REQUIRE(session.RunFrame());
    REQUIRE(session.RunFrame());
    REQUIRE(recorder.Log().events.size() == 5);
    CHECK(recorder.Log().events.front().tick == 0);
    CHECK(recorder.Log().events.back().tick == 140448);
    CHECK(recorder.Log().events.back().buttons == 0x80);
    const auto directory = replayDirectory();
    const auto logPath = directory / "inputs.replay";
    diagnostics::WriteInputLog(logPath, recorder.Log());
    const auto log = diagnostics::ReadInputLog(logPath);
    CHECK(diagnostics::InputLogHash(log) == diagnostics::InputLogHash(recorder.Log()));
    auto replayed = host::load_gameboy(settings);
    diagnostics::ReplayPlayer player(replayed, log, replayInputs);
    static_cast<void>(player.AdvanceTicks(12345));
    player.SaveCheckpoint(directory / "checkpoint", checkpointIdentity);
    static_cast<void>(player.AdvanceTicks(280896 - 12345));
    CHECK(recorded.SaveState() == replayed.SaveState());
    CHECK(recorded.PressedButtons() == replayed.PressedButtons());
    const auto finalState = replayed.SaveDebugState();
    player.RestoreCheckpoint(directory / "checkpoint", checkpointIdentity);
    CHECK(player.Cursor() == 2);
    static_cast<void>(player.AdvanceTicks(280896 - 12345));
    CHECK(replayed.SaveDebugState() == finalState);
    std::filesystem::remove_all(directory);
}

TEST_CASE("diagnostics: replay inputs can trigger safe stops and incomplete input recordings are rejected") {
    const host::LaunchSettings settings{.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = Model::CGBC, .no_bootrom = true}};
    auto machine = host::load_gameboy(settings);
    diagnostics::InputLog log{.initial = replayInputs, .events = {{5, 0, 1}, {5, 1, 3}, {11, 2, 0}}};
    diagnostics::ReplayPlayer player(machine, log, replayInputs);
    diagnostics::TraceRecorder recorder(32);
    diagnostics::Watchpoints conditions(recorder.Context());
    conditions.Add({.events = {.kinds = uint64_t{1} << static_cast<unsigned>(diagnostics::EventKind::InputApplied)}});
    machine.ObserveWith(&recorder.Context());
    const auto first = player.RunUntil({}, 100);
    REQUIRE(first.trigger);
    CHECK(first.stopped.tick == 5);
    CHECK(machine.PressedButtons() == 1);
    CHECK(player.Cursor() == 1);
    const auto second = player.RunUntil({}, 100);
    REQUIRE(second.trigger);
    CHECK(second.stopped.tick == 5);
    CHECK(machine.PressedButtons() == 3);
    CHECK(player.Cursor() == 2);
    auto incomplete = host::load_gameboy(settings);
    diagnostics::InputRecorder bounded(incomplete, replayInputs, 1);
    incomplete.ApplyButtons(3);
    CHECK_THROWS_AS(static_cast<void>(bounded.Log()), std::runtime_error);
}

TEST_CASE("diagnostics: checkpoint resumes HALT STOP speed switching DMA and pending display writes") {
    for (unsigned scenario = 0; scenario < 6; ++scenario) {
        const host::LaunchSettings settings{.rom_path = "roms/acid/cgb-acid2.gbc", .emulation = {.model = Model::CGBC, .no_bootrom = true}};
        auto continuous = host::load_gameboy(settings);
        auto restored = host::load_gameboy(settings);
        for (auto *machine : {&continuous, &restored}) {
            auto &bus = machine->[:replayMember<Gameboy>("bus_"):];
            auto &cpu = machine->[:replayMember<Gameboy>("cpu_"):];
            if (scenario == 0)
                cpu.[:replayMember<CPU<Bus>>("halted_"):] = true;
            if (scenario == 1)
                cpu.[:replayMember<CPU<Bus>>("stopped_"):] = true;
            if (scenario == 2) {
                bus.prepareSpeedShift = true;
                bus.ChangeSpeed();
                bus.speedSwitchHalt = 20;
                bus.speedShiftActive = true;
            }
            if (scenario == 3)
                bus.WriteByte(0xFF46, 0xC0, ComponentSource::CPU);
            if (scenario == 4) {
                bus.gpu_.stat.mode = GPUMode::MODE_3;
                bus.gpu_.scanlineCounter = 120;
                bus.gpu_.scrollX = 3;
                bus.gpu_.scxFetcherOld = 1;
                bus.gpu_.scxWriteStage = 3;
            }
            if (scenario == 5) {
                bus.gpu_.hdma = {.hdmaSource = 0xC000, .hdmaDestination = 0x8000, .hdmaRemain = 2, .hdmaActive = true};
            }
            static_cast<void>(machine->AdvanceTicks(scenario == 3 ? 21 : 1));
        }
        const auto checkpoint = restored.SaveDebugState();
        static_cast<void>(restored.AdvanceTicks(2000));
        REQUIRE(restored.LoadDebugState(checkpoint));
        CHECK(restored.SaveState() == continuous.SaveState());
        static_cast<void>(restored.AdvanceTicks(2049));
        static_cast<void>(continuous.AdvanceTicks(2049));
        CHECK(restored.SaveState() == continuous.SaveState());
        CHECK(restored.SaveDebugState() == continuous.SaveDebugState());
    }
}

TEST_CASE("diagnostics: LCD palette sampling survives replay and distinguishes initial inputs") {
    const auto directory = replayDirectory();
    constexpr std::array sampling_values = {DmgPaletteSampling::BitwiseOr, DmgPaletteSampling::NewValue};
    for (const auto sampling : sampling_values) {
        const host::LaunchSettings settings{.rom_path = "roms/acid/dmg-acid2.gb",
                                            .emulation = {.model = Model::DMGB, .no_bootrom = true, .dmg_palette_sampling = sampling}};
        auto original = host::load_gameboy(settings);
        const auto inputs = diagnostics::IdentifyInitialInputs(original, settings);
        auto alternate_settings = settings;
        alternate_settings.emulation.dmg_palette_sampling =
                sampling == DmgPaletteSampling::BitwiseOr ? DmgPaletteSampling::NewValue : DmgPaletteSampling::BitwiseOr;
        auto alternate = host::load_gameboy(alternate_settings);
        CHECK(inputs.configuration != diagnostics::IdentifyInitialInputs(alternate, alternate_settings).configuration);
        diagnostics::InputRecorder recorder(original, inputs, 1000, settings);
        static_cast<void>(original.AdvanceTicks(1000));
        const auto state = original.SaveDebugState();
        REQUIRE(alternate.LoadDebugState(state));
        CHECK(alternate.SaveDebugState() == state);
        const auto path = directory / "palette.replay";
        diagnostics::WriteInputLog(path, recorder.Log());
        const auto log = diagnostics::ReadInputLog(path);
        REQUIRE(log.settings.has_value());
        CHECK(log.settings->emulation.dmg_palette_sampling == sampling);
        auto replayed = host::load_gameboy(*log.settings);
        const auto replay_inputs = diagnostics::IdentifyInitialInputs(replayed, *log.settings);
        diagnostics::ReplayPlayer player(replayed, log, replay_inputs);
        static_cast<void>(player.AdvanceTicks(1000));
        CHECK(replayed.SaveDebugState() == state);
    }
    std::filesystem::remove_all(directory);
}

TEST_CASE("diagnostics: forced CGB startup is retained by recorded inputs") {
    const auto directory = replayDirectory();
    constexpr std::array forced_values = {false, true};
    for (const auto forced : forced_values) {
        const host::LaunchSettings settings{.rom_path = "roms/acid/dmg-acid2.gb",
                                            .emulation = {.model = Model::CGBC, .no_bootrom = true, .force_cgb_mode = forced}};
        auto original = host::load_gameboy(settings);
        const auto inputs = diagnostics::IdentifyInitialInputs(original, settings);
        auto alternate_settings = settings;
        alternate_settings.emulation.force_cgb_mode = !forced;
        auto alternate = host::load_gameboy(alternate_settings);
        CHECK(inputs.configuration != diagnostics::IdentifyInitialInputs(alternate, alternate_settings).configuration);
        diagnostics::InputRecorder recorder(original, inputs, 1000, settings);
        static_cast<void>(original.AdvanceTicks(1000));
        const auto path = directory / "cgb-startup.replay";
        diagnostics::WriteInputLog(path, recorder.Log());
        const auto log = diagnostics::ReadInputLog(path);
        REQUIRE(log.settings.has_value());
        CHECK(log.settings->emulation.force_cgb_mode == forced);
        auto replayed = host::load_gameboy(*log.settings);
        CHECK(replayed.IsInCgbMode() == forced);
        diagnostics::ReplayPlayer player(replayed, log, diagnostics::IdentifyInitialInputs(replayed, *log.settings));
        static_cast<void>(player.AdvanceTicks(1000));
        CHECK(replayed.SaveDebugState() == original.SaveDebugState());
    }
    std::filesystem::remove_all(directory);
}
