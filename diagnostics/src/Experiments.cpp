#include "diagnostics/Experiments.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <format>
#include <sstream>
#include "Gameboy.h"
#include "diagnostics/Artifacts.h"
#include "diagnostics/Hash.h"
#include "diagnostics/Json.h"
#include "diagnostics/Provenance.h"
#include "diagnostics/Watchpoints.h"

namespace diagnostics {
    namespace {
        template<class T>
        consteval std::meta::info member(const std::string_view name) {
            for (const auto field : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
                if (std::meta::identifier_of(field) == name)
                    return field;
            throw "Unknown experiment fixture field";
        }

        template<class T>
        void writeJson(const std::filesystem::path &path, const T &value) {
            std::ofstream stream;
            stream.exceptions(std::ios::badbit | std::ios::failbit);
            stream.open(path);
            WriteJson(stream, value);
            stream << '\n';
        }

        uint8_t seededByte(const uint64_t seed) {
            auto value = seed + 0x9E3779B97F4A7C15ULL;
            value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
            value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
            return (value ^ (value >> 31)) & 255;
        }

        struct Program {
            uint16_t origin{0x150};
            std::vector<uint8_t> bytes;
            [[nodiscard]] uint16_t Address() const { return origin + bytes.size(); }
            void Emit(const std::initializer_list<uint8_t> values) { bytes.insert(bytes.end(), values); }
            void Write(const uint16_t address, const uint8_t value) {
                Emit({0x3E, value});
                if (address >= 0xFF00)
                    Emit({0xE0, static_cast<uint8_t>(address)});
                else
                    Emit({0xEA, static_cast<uint8_t>(address), static_cast<uint8_t>(address >> 8)});
            }
            void Delay(unsigned cycles) {
                while (cycles >= 5) {
                    const auto count = std::min(255u, (cycles - 1) / 4);
                    Emit({0x06, static_cast<uint8_t>(count), 0x05, 0x20, 0xFD});
                    cycles -= count * 4 + 1;
                }
                while (cycles--)
                    Emit({0});
            }
            void WaitLine(const uint8_t line) { Emit({0xF0, 0x44, 0xFE, line, 0x20, 0xFA}); }
        };

        std::vector<uint8_t> makeRom(const Scenario &scenario, ExperimentResult &result) {
            const uint64_t cycle = scenario.doubleSpeed ? 4 : 8;
            Program program;
            program.Emit({0xF3, 0x31, 0xFE, 0xFF});
            for (const auto address : {0xFF40, 0xFF26, 0xFF07, 0xFFFF, 0xFF0F})
                program.Write(address, 0);
            if (scenario.doubleSpeed) {
                program.Write(0xFF4D, 1);
                program.Emit({0x10, 0});
            }
            for (const auto value : scenario.noise)
                program.Write(0xFF84, value);
            const auto anchor = [&] { program.Write(0xFF80, 0xA5); };
            const auto action = [&](const unsigned delay, const uint16_t address, const uint8_t value) {
                program.Delay(delay);
                const auto pc = static_cast<uint16_t>(program.Address() + 2);
                program.Write(address, value);
                result.actions.push_back({"CpuWrite", (delay + 5) * cycle, address, value, pc});
            };
            if (scenario.family == ExperimentFamily::TimerReload) {
                program.Write(0xFF06, 37);
                program.Write(0xFF05, 255);
                program.Write(0xFF04, 0);
                program.Write(0xFF07, 5);
                anchor();
                action(4 + scenario.phase, 0xFF05, seededByte(scenario.seed));
                program.Emit({0xF0, 0x05, 0xE0, 0x82});
                program.Delay(16);
            } else if (scenario.family == ExperimentFamily::Dma) {
                program.Emit({0x21, 0x00, 0xC0, 0x06, 0xA0, 0x3E, seededByte(scenario.seed), 0x22, 0x3C, 0x05, 0x20, 0xFB});
                Program routine{.origin = 0xFF90};
                routine.Write(0xFF80, 0xA5);
                routine.Write(0xFF46, 0xC0);
                routine.Delay(156 + scenario.phase);
                const auto readPc = routine.Address();
                routine.Emit({0xFA, 0x00, 0xC0, 0xE0, 0x82});
                routine.Delay(170);
                routine.Write(0xFF81, 0x5A);
                routine.Emit({0x18, 0xFE});
                if (routine.bytes.size() > 0x6E)
                    throw std::logic_error("DMA experiment exceeds HRAM");
                for (size_t i = 0; i < routine.bytes.size(); ++i)
                    program.Write(routine.origin + i, routine.bytes[i]);
                program.Emit({0xC3, 0x90, 0xFF});
                result.actions.push_back({"CpuWrite", 5 * cycle, 0xFF46, 0xC0, 0xFF96});
                result.actions.push_back({"CpuRead", (5 + 156 + scenario.phase + 4) * cycle, 0xC000, 0, readPc});
            } else {
                program.Write(0xFF47, 0xE4);
                program.Write(0xFF48, 0xE4);
                program.Write(0xFF43, scenario.scx);
                program.Write(0xFF4A, 1);
                program.Write(0xFF4B, scenario.wx);
                program.Write(0xFF6C, scenario.coordinatePriority);
                program.Write(0xFF68, 0x80);
                program.Write(0xFF6A, 0x80);
                for (const auto color : {0x7FFF, 0x56B5, 0x294A, 0x0000})
                    for (const auto address : {0xFF69, 0xFF6B}) {
                        program.Write(address, color & 255);
                        program.Write(address, color >> 8);
                    }
                const auto pattern = (seededByte(scenario.seed) & 0x7F) | 1;
                for (unsigned row = 0; row < 8; ++row) {
                    program.Write(0x8010 + row * 2, pattern);
                    program.Write(0x8021 + row * 2, 255);
                }
                for (unsigned tile = 0; tile < 32; ++tile)
                    program.Write(0x9C00 + tile, 1);
                for (unsigned index = 0; index < 2; ++index) {
                    program.Write(0xFE00 + index * 4, 17);
                    program.Write(0xFE01 + index * 4, scenario.spriteX + (index ? scenario.spriteOffset : 0));
                    program.Write(0xFE02 + index * 4, index + 1);
                    program.Write(0xFE03 + index * 4, 0);
                }
                if (scenario.family == ExperimentFamily::Stat) {
                    program.Write(0xFF83, 0);
                    program.Write(0xFF41, 0x20);
                    program.Write(0xFFFF, 2);
                    anchor();
                    action(4 + scenario.phase, 0xFF40, 0x91);
                    program.Emit({0xFB, 0});
                    program.Emit({0xF0, 0x83, 0xFE, 1, 0x20, 0xFA});
                    program.Emit({0xF0, 0x83, 0xE0, 0x82});
                } else {
                    program.Write(0xFF40, scenario.family == ExperimentFamily::Window ? 0xF1 : 0x93);
                    program.WaitLine(1);
                    anchor();
                    action(12 + scenario.phase, scenario.family == ExperimentFamily::Window ? 0xFF4B : 0xFF43,
                           scenario.family == ExperimentFamily::Window ? scenario.wx : scenario.scx);
                    program.WaitLine(2);
                    program.Emit({0xF0, 0x41, 0xE0, 0x82});
                }
            }
            program.Write(0xFF81, 0x5A);
            program.Emit({0x18, 0xFE});
            std::vector<uint8_t> rom(32768);
            rom[0x100] = 0xC3;
            rom[0x101] = 0x50;
            rom[0x102] = 1;
            rom[0x143] = 0x80;
            Program handler{.origin = 0x48};
            handler.Write(0xFF83, 1);
            handler.Write(0xFF41, 0);
            handler.Emit({0xD9});
            std::ranges::copy(handler.bytes, rom.begin() + handler.origin);
            if (program.bytes.size() > 0x7EB0)
                throw std::logic_error("Experiment ROM is too large");
            std::ranges::copy(program.bytes, rom.begin() + program.origin);
            for (size_t i = 0x134; i < 0x14D; ++i)
                rom[0x14D] -= rom[i] + 1;
            return rom;
        }

        void configureGpu(GPU &gpu, const Scenario &scenario) {
            gpu.SetModel(scenario.model);
            gpu.doubleSpeed = scenario.doubleSpeed;
            gpu.optimizedExecution = false;
            gpu.currentLine = 1;
            gpu.lcdc = scenario.family == ExperimentFamily::Window ? 0xF1 : 0x93;
            gpu.windowX = scenario.wx;
            gpu.windowTriggeredThisFrame = true;
            gpu.scrollX = scenario.scx;
            gpu.objectPriority = scenario.coordinatePriority;
            gpu.backgroundPalette = gpu.obp0Palette = 0xE4;
            constexpr std::array<uint8_t, 4> shades{31, 23, 11, 0};
            for (unsigned color = 0; color < shades.size(); ++color) {
                gpu.bgpd[0][color].fill(shades[color]);
                gpu.obpd[0][color].fill(shades[color]);
            }
            for (unsigned row = 0; row < 8; ++row) {
                gpu.vram[16 + row * 2] = (seededByte(scenario.seed) & 0x7F) | 1;
                gpu.vram[33 + row * 2] = 255;
            }
            for (unsigned tile = 0; tile < 32; ++tile)
                gpu.vram[0x1C00 + tile] = 1;
            for (unsigned index = 0; index < 2; ++index) {
                gpu.oam[index * 4] = 17;
                gpu.oam[index * 4 + 1] = scenario.spriteX + (index ? scenario.spriteOffset : 0);
                gpu.oam[index * 4 + 2] = index + 1;
            }
            gpu.RebuildColorLuts();
        }
    }

    ExperimentResult RunExperiment(Scenario scenario, const std::filesystem::path &directory) {
        if (scenario.schemaVersion != 1 || scenario.model == Model::Auto || scenario.phase < -4 || scenario.phase > 64 ||
            scenario.spriteOffset < -7 || scenario.spriteOffset > 7 || scenario.spriteX + scenario.spriteOffset < 0 || scenario.spriteX + scenario.spriteOffset > 255 ||
            scenario.noise.size() > 64 || scenario.budgetTicks > 1'000'000 || (scenario.doubleSpeed && !IsCgb(scenario.model)) ||
            ((scenario.basis == ExperimentBasis::HardwareMeasurement || scenario.basis == ExperimentBasis::ReferenceComparison) && scenario.basisEvidence.empty()))
            throw std::invalid_argument("Unsupported scenario version, model, phase, budget or expectation evidence");
        const uint64_t tcycle = scenario.doubleSpeed ? 1 : 2;
        if (!scenario.budgetTicks)
            scenario.budgetTicks = scenario.mode == ExperimentMode::WholeMachine ? 140448
                                    : scenario.family == ExperimentFamily::TimerReload ? 16 * tcycle
                                    : scenario.family == ExperimentFamily::Dma ? 680 * tcycle : 912;
        const auto componentStep = scenario.family == ExperimentFamily::TimerReload || scenario.family == ExperimentFamily::Dma ? tcycle : 2;
        if (scenario.mode == ExperimentMode::Component && scenario.budgetTicks % componentStep)
            throw std::invalid_argument("Component budget must end on a component tick boundary");
        if (!std::filesystem::create_directories(directory))
            throw std::invalid_argument("Experiment output directory already exists");
        ExperimentResult result{.scenario = scenario, .build = std::string(Gameboy::CoreBuildIdentity()),
                .completionContract = scenario.mode == ExperimentMode::WholeMachine ? "CpuCompletionMarker:FF81=5A"
                                      : "ComponentBudgetAndRequiredObservations",
                .phaseUnit = scenario.mode == ExperimentMode::WholeMachine ? "CpuMachineCycles"
                             : scenario.family == ExperimentFamily::TimerReload || scenario.family == ExperimentFamily::Dma ? "CpuTCycles" : "PpuDots"};
        TraceRecorder trace(262144);
        if (scenario.mode == ExperimentMode::Component) {
            trace.Context().pc = trace.Context().pcBank = UINT16_MAX;
            trace.Context().position = {.frame = UINT64_MAX, .dot = UINT16_MAX, .ly = UINT8_MAX, .cpuPhase = UINT8_MAX};
        }
        ProvenanceRecorder provenance({.y = {1, 1}}, 160, 8192);
        std::vector<uint32_t> pixels(160 * 144);
        const auto check = [&](const std::string &name, const bool passed, const std::string &evidence) {
            result.checks.push_back({name, passed, evidence});
        };
        bool completed{};
        if (scenario.mode == ExperimentMode::WholeMachine) {
            const auto rom = makeRom(scenario, result);
            const auto path = directory / "scenario.gb";
            std::ofstream file(path, std::ios::binary);
            file.write(reinterpret_cast<const char *>(rom.data()), rom.size());
            file.close();
            result.romHash = FileHash(path);
            Gameboy machine({.romName = path.string(), .model = scenario.model, .noBootrom = true,
                             .colorCorrection = false, .executionPolicy = ExecutionPolicy::Ordinary});
            writeJson(directory / "initial.json", machine.Inspect());
            machine.ObserveWith(&trace.Context());
            machine.RecordPixelsWith(&provenance.Context());
            Watchpoints watchpoints(trace.Context());
            watchpoints.Add({.events = {.kinds = uint64_t{1} << static_cast<unsigned>(EventKind::BusWrite), .addresses = {{0xFF81, 0xFF81}}}, .value = 0x5A});
            const auto stopped = machine.RunUntil({}, scenario.budgetTicks);
            completed = stopped.trigger.has_value();
            result.completedTick = machine.TimelineTicks();
            result.probeValue = machine.DebugPeek(0xFF82);
            std::ranges::copy(std::span(machine.GetScreenData(), pixels.size()), pixels.begin());
            writeJson(directory / "terminal.json", machine.Inspect());
        } else if (scenario.family == ExperimentFamily::TimerReload) {
            Audio audio;
            audio.SetModel(scenario.model);
            Interrupts interrupts;
            Timer timer(audio, interrupts);
            timer.tima = 255;
            timer.tma = 37;
            timer.tac = 5;
            timer.divCounter = 15;
            timer.observations = &trace.Context();
            for (const auto value : scenario.noise) {
                trace.Context().Emit(Component::Control, EventKind::RegisterWrite, 0xFF06, value, 0, Initiator::Debugger);
                timer.WriteByte(0xFF06, value, scenario.doubleSpeed ? Speed::Double : Speed::Regular);
            }
            timer.tma = 37;
            struct TimerSetup {
                uint16_t divCounter;
                uint8_t tima, tma, tac;
                uint64_t ticksPerTCycle;
            };
            writeJson(directory / "initial.json", TimerSetup{timer.divCounter, timer.tima, timer.tma, timer.tac, tcycle});
            interrupts.observations = &trace.Context();
            const auto actionTick = (4 + scenario.phase) * tcycle;
            result.actions.push_back({"ComponentWriteBeforeTick", actionTick, 0xFF05, seededByte(scenario.seed)});
            for (uint64_t tick = 0; tick < scenario.budgetTicks; tick += tcycle) {
                trace.Context().position.tick = tick;
                if (tick == actionTick) {
                    trace.Context().Emit(Component::Control, EventKind::RegisterWrite, 0xFF05, seededByte(scenario.seed), 0, Initiator::Debugger);
                    timer.WriteByte(0xFF05, seededByte(scenario.seed), scenario.doubleSpeed ? Speed::Double : Speed::Regular);
                    result.probeValue = timer.tima;
                }
                timer.Tick(scenario.doubleSpeed ? Speed::Double : Speed::Regular);
                if (tick % 2 == 0)
                    interrupts.Tick();
            }
            result.completedTick = scenario.budgetTicks;
            completed = actionTick < scenario.budgetTicks;
            const uint32_t expected = actionTick == 5 * tcycle ? 37 : seededByte(scenario.seed);
            check("reload-latch-write", result.probeValue == expected, std::format("Immediate TIMA sample {} expected {}", result.probeValue, expected));
        } else if (scenario.family == ExperimentFamily::Dma) {
            const auto rom = makeRom(scenario, result);
            result.actions.clear();
            const auto path = directory / "fixture.gb";
            std::ofstream file(path, std::ios::binary);
            file.write(reinterpret_cast<const char *>(rom.data()), rom.size());
            file.close();
            result.romHash = FileHash(path);
            Gameboy machine({.romName = path.string(), .model = scenario.model, .noBootrom = true});
            auto &bus = machine.[:member<Gameboy>("bus_"):];
            bus.speed = scenario.doubleSpeed ? Speed::Double : Speed::Regular;
            bus.gpu_.doubleSpeed = scenario.doubleSpeed;
            for (unsigned i = 0; i < 160; ++i)
                bus.DebugPoke(0xC000 + i, seededByte(scenario.seed) + i);
            writeJson(directory / "initial.json", machine.Inspect());
            bus.observations = &trace.Context();
            for (const auto value : scenario.noise) {
                bus.DebugPoke(0xC100, value);
                trace.Context().Emit(Component::Control, EventKind::BusWrite, 0xC100, value, 0, Initiator::Debugger);
            }
            bus.dma_.Set(0xC0);
            const auto probeTick = (644 + scenario.phase) * tcycle;
            result.actions.push_back({"ComponentCpuVisibilitySample", probeTick, 0xC000});
            for (uint64_t tick = 0; tick < scenario.budgetTicks; tick += tcycle) {
                trace.Context().position.tick = tick;
                if (tick == probeTick) {
                    result.probeValue = bus.InspectRead(0xC000, ComponentSource::CPU);
                    trace.Context().Emit(Component::Control, EventKind::BusRead, 0xC000, result.probeValue, 0, Initiator::Debugger,
                                         bus.AccessBlocked(0xC000, ComponentSource::CPU, false));
                }
                bus.UpdateDMA();
            }
            result.completedTick = scenario.budgetTicks;
            completed = probeTick < scenario.budgetTicks && !bus.dma_.transferActive;
            check("dma-copy", std::ranges::equal(bus.gpu_.oam, std::views::iota(0, 160), [&](const auto value, const auto index) {
                return value == static_cast<uint8_t>(seededByte(scenario.seed) + index);
            }), "All 160 OAM bytes equal the seeded WRAM source");
        } else {
            Interrupts interrupts;
            GPU gpu(interrupts);
            configureGpu(gpu, scenario);
            struct PpuSetup {
                uint8_t lcdc, ly, scx, wx;
                bool doubleSpeed;
                std::string vramHash, oamHash;
            };
            writeJson(directory / "initial.json", PpuSetup{gpu.lcdc, gpu.currentLine, gpu.scrollX, gpu.windowX, gpu.doubleSpeed,
                      Sha256(std::as_bytes(std::span(gpu.vram))), Sha256(std::as_bytes(std::span(gpu.oam)))});
            gpu.observations = &trace.Context();
            gpu.provenance = &provenance.Context();
            interrupts.observations = &trace.Context();
            for (const auto value : scenario.noise) {
                trace.Context().Emit(Component::Control, EventKind::RegisterWrite, 0xFF42, value, 0, Initiator::Debugger);
                gpu.WriteRegisters(0xFF42, value);
            }
            if (scenario.family == ExperimentFamily::Stat) {
                gpu.WriteRegisters(0xFF41, 0x20);
                interrupts.interruptFlag = interrupts.interruptFlagDelayed = 0;
                interrupts.interruptEnable = 2;
            }
            const uint64_t actionTick = (78 + scenario.phase) * 2;
            const uint16_t address = scenario.family == ExperimentFamily::Window ? 0xFF4B : scenario.family == ExperimentFamily::Stat ? 0xFF41 : 0xFF43;
            const uint8_t value = scenario.family == ExperimentFamily::Window ? scenario.wx : scenario.family == ExperimentFamily::Stat ? 0x28 : scenario.scx;
            result.actions.push_back({"ComponentWriteBeforeTick", actionTick, address, value});
            for (uint64_t tick = 0; tick < scenario.budgetTicks; tick += 2) {
                trace.Context().position = {.tick = tick, .frame = gpu.displayFrames_, .dot = static_cast<uint16_t>(gpu.scanlineCounter), .ly = gpu.currentLine, .cpuPhase = UINT8_MAX};
                provenance.Context().position = trace.Context().position;
                if (tick == actionTick) {
                    trace.Context().Emit(Component::Control, EventKind::RegisterWrite, address, value, 0, Initiator::Debugger);
                    gpu.WriteRegisters(address, value);
                }
                gpu.Update();
            }
            completed = provenance.Pixels().size() == 160 && actionTick < scenario.budgetTicks;
            result.completedTick = scenario.budgetTicks;
            result.probeValue = scenario.family == ExperimentFamily::Stat ? interrupts.interruptFlag & 2 : gpu.ReadRegisters(address);
            std::ranges::copy(std::span(gpu.GetScreenData(), pixels.size()), pixels.begin());
        }
        const auto events = trace.Events();
        const auto captured = provenance.Pixels();
        if (scenario.mode == ExperimentMode::WholeMachine) {
            const auto anchor = std::ranges::find_if(events, [](const Event &event) {
                return event.kind == EventKind::BusWrite && event.address == 0xFF80 && event.value == 0xA5;
            });
            if (anchor != events.end())
                result.anchorTick = anchor->position.tick;
            check("cpu-anchor", anchor != events.end(), "CPU wrote the experiment anchor to FF80");
            for (const auto &action : result.actions) {
                const auto event = std::ranges::find_if(events, [&](const Event &candidate) {
                    return candidate.kind == (action.operation == "CpuRead" ? EventKind::BusRead : EventKind::BusWrite) &&
                           candidate.address == action.address && candidate.pc == action.instructionAddress &&
                           candidate.position.tick >= result.anchorTick;
                });
                check("cpu-action-timing", event != events.end() && event->position.tick == result.anchorTick + action.offsetTicks,
                      std::format("Address {:04X}: expected tick {}, observed {}", action.address, result.anchorTick + action.offsetTicks,
                                  event == events.end() ? UINT64_MAX : event->position.tick));
                check("cpu-action-value", event != events.end() && event->initiator == Initiator::CPU &&
                      (action.operation == "CpuRead" || event->value == action.value),
                      "The planned operation is a CPU bus event with the requested write value");
            }
        } else {
            for (const auto &action : result.actions)
                check("component-action-timing", std::ranges::any_of(events, [&](const Event &event) {
                    return event.initiator == Initiator::Debugger && event.address == action.address && event.position.tick == action.offsetTicks;
                }), "Component stimulus uses the declared before-tick boundary");
        }
        if (scenario.family == ExperimentFamily::Window || scenario.family == ExperimentFamily::Sprites) {
            check("rendered-line", captured.size() == 160, "Every pixel of line 1 has a captured emission");
            check("opaque-object-selection", std::ranges::all_of(captured, [](const PixelExplanation &pixel) {
                return pixel.selected.source != PixelSource::Sprite || pixel.selected.color != 0;
            }), "A transparent object cannot be the selected source");
            check("pixel-output-order", std::ranges::is_sorted(captured, {}, [](const PixelExplanation &pixel) { return pixel.emission.tick; }),
                  "Captured pixel emission times are ordered");
        }
        if (scenario.family == ExperimentFamily::Window && scenario.wx < 166)
            check("window-activation", std::ranges::any_of(captured, [](const PixelExplanation &pixel) {
                return pixel.background.source == PixelSource::Window || pixel.background.source == PixelSource::WindowGlitch;
            }), "Visible window pixels have captured window origins");
        if (scenario.family == ExperimentFamily::Sprites && scenario.spriteX >= 8 && scenario.spriteX <= 160 && scenario.spriteOffset == 0) {
            check("transparent-sprite-fallback", std::ranges::any_of(captured, [](const PixelExplanation &pixel) {
                return pixel.selected.source == PixelSource::Sprite && pixel.selected.oamIndex == 1 &&
                       std::ranges::any_of(std::span(pixel.sprites.candidates).first(pixel.sprites.count), [](const SpriteCandidate &candidate) {
                           return candidate.origin.oamIndex == 0 && candidate.origin.color == 0 && candidate.decision == SpriteDecision::Transparent;
                       });
            }), "A transparent earlier sprite exposes the opaque overlapping sprite");
        }
        if (scenario.family == ExperimentFamily::Stat) {
            const auto request = std::ranges::find_if(events, [](const Event &event) { return event.kind == EventKind::InterruptRequest && event.value == 2; });
            check("stat-request", request != events.end(), "STAT generated a recorded interrupt request");
            if (scenario.mode == ExperimentMode::WholeMachine) {
                const auto accept = std::ranges::find_if(events, [](const Event &event) { return event.kind == EventKind::InterruptAccept && event.value == 2; });
                check("stat-request-before-acceptance", request != events.end() && accept != events.end() && request->position.tick <= accept->position.tick,
                      "The CPU accepts the recorded STAT request after it exists");
            }
        }
        if (scenario.family == ExperimentFamily::Dma) {
            check("dma-transfers", std::ranges::count(events, EventKind::DmaTransfer, &Event::kind) == 160, "Exactly 160 OAM bytes transferred");
            unsigned index{};
            bool valuesMatch{true};
            for (const auto &event : events)
                if (event.kind == EventKind::DmaTransfer) {
                    valuesMatch &= event.address == 0xFE00 + index && event.detail == 0xC000 + index &&
                                   event.value == static_cast<uint8_t>(seededByte(scenario.seed) + index);
                    ++index;
                }
            check("dma-transfer-values", valuesMatch, "Transfer order, source addresses and seeded values match the complete OAM copy");
        }
        if (scenario.expectedValue)
            check("probe-value", result.probeValue == *scenario.expectedValue, std::format("Sample {} expected {}", result.probeValue, *scenario.expectedValue));
        result.outcome = completed ? Outcome::Pass : Outcome::Timeout;
        for (const auto &invariant : result.checks)
            if (!invariant.passed && completed) {
                result.outcome = Outcome::AssertionFailure;
                if (result.failureSignature.empty()) {
                    result.failureSignature = std::string(model_name(scenario.family)) + ":" + invariant.invariant;
                    if (invariant.invariant == "probe-value")
                        result.failureSignature += std::format(":{}!={}", result.probeValue, *scenario.expectedValue);
                }
            }
        if (trace.Metadata().overwritten || provenance.Metadata().overwrittenEvents || provenance.Metadata().overwrittenPixels) {
            result.outcome = Outcome::InfrastructureError;
            result.details = "Experiment capture overflowed";
        }
        result.events = events.size();
        std::ostringstream eventRecords;
        WriteJson(eventRecords, events);
        result.eventHash = TextHash(eventRecords.str());
        result.framebufferHash = Sha256(std::as_bytes(std::span(pixels)));
        trace.Identify(result.build, result.romHash, "TimingScenario-v1");
        trace.WriteJsonLines(directory / "events.jsonl");
        trace.WriteBinary(directory / "events.trace");
        provenance.Identify(result.build, result.romHash);
        provenance.WriteJson(directory / "provenance.json");
        WriteBitmap(directory / "actual.bmp", pixels, 160, 144);
        writeJson(directory / "scenario.json", result.scenario);
        writeJson(directory / "result.json", result);
        return result;
    }
}
