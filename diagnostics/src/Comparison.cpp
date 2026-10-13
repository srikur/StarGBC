#include "diagnostics/Comparison.h"

#include <fstream>
#include <sstream>
#include "diagnostics/Hash.h"
#include "diagnostics/Json.h"
#include "diagnostics/Trace.h"

namespace diagnostics {
    namespace {
        struct ObservationHeader {
            uint32_t schemaVersion{1};
            std::string record{"observations"};
            std::string build{Gameboy::CoreBuildIdentity()};
            InputFingerprint initial;
            std::string replayHash;
            ObservationPlan plan;
            uint64_t masterTicksPerSecond{MasterTicksPerSecond};
            std::string hardwareFormat;
            std::string scheduler;
        };

        struct ObservationSample {
            std::string record{"sample"};
            CanonicalObservation state;
            std::string eventHash;
            std::optional<std::vector<Event>> events;
        };

        struct ObservationEnd {
            std::string record{"complete"};
            uint64_t tick{};
            uint64_t samples{};
        };
    } // namespace

    CanonicalObservation CaptureObservation(Gameboy &machine) {
        if (machine.UsesRealRTC())
            throw std::invalid_argument("Differential observations require virtual RTC");
        CanonicalObservation result{.position = machine.ExecutionPosition(), .machine = machine.Inspect()};
        const auto hardware = machine.SaveState();
        result.hardwareHash = Sha256(hardware);
        std::array<uint8_t, 0x8000> memory;
        for (uint32_t address = 0x8000; address <= 0xFFFF; ++address)
            memory[address - 0x8000] = machine.InspectCpuRead(address);
        result.memoryHash = Sha256(std::as_bytes(std::span(memory)));
        result.framebufferHash = Sha256(std::as_bytes(std::span(machine.GetScreenData(), SCREEN_WIDTH * SCREEN_HEIGHT)));
        std::vector<float> samples(machine.GetAudioSamplesAvailable() * 2);
        result.audioSamples = machine.ReadAudioSamples(samples.data(), samples.size() / 2);
        result.audioHash = Sha256(std::as_bytes(std::span(samples).first(result.audioSamples * 2)));
        return result;
    }

    void WriteObservations(const std::filesystem::path &path, Gameboy &machine, ReplayPlayer *replay, const InputFingerprint &initial,
                           std::string replayHash, const ObservationPlan &plan) {
        if (machine.TimelineTicks() || !plan.sampleTicks || plan.beginTick > plan.endTick || machine.Observations() ||
            (plan.eventDetails && !plan.events))
            throw std::invalid_argument("Observation capture requires initial conditions and an ordered bounded sampling plan");
        if (plan.sampleTicks > 140'448 || plan.endTick - plan.beginTick > plan.sampleTicks * uint64_t{1'000'000})
            throw std::invalid_argument("Observation capture exceeds the sample or interval limit");
        std::ofstream output(path);
        output.exceptions(std::ios::failbit | std::ios::badbit);
        const auto write = [&](const auto &value) {
            WriteJson(output, value);
            output << '\n';
        };
        write(ObservationHeader{.initial = initial,
                                .replayHash = std::move(replayHash),
                                .plan = plan,
                                .hardwareFormat = "reflection-state-" + std::to_string(kGameboyStateSize),
                                .scheduler = plan.events ? "PerTickForObservations" : "RequestedPolicy"});
        const auto advance = [&](const uint64_t ticks) {
            return replay ? replay->AdvanceTicks(ticks, plan.policy) : machine.AdvanceTicks(ticks, plan.policy);
        };
        static_cast<void>(advance(plan.beginTick));
        write(ObservationSample{.state = CaptureObservation(machine)});
        std::unique_ptr<TraceRecorder> trace;
        if (plan.events) {
            trace = std::make_unique<TraceRecorder>(plan.eventCapacity);
            machine.ObserveWith(&trace->Context());
        }
        uint64_t count = 1;
        try {
            while (machine.TimelineTicks() < plan.endTick) {
                static_cast<void>(advance(std::min(plan.sampleTicks, plan.endTick - machine.TimelineTicks())));
                ObservationSample sample{.state = CaptureObservation(machine)};
                if (trace) {
                    if (trace->Metadata().overwritten)
                        throw std::runtime_error("Observation event capture overflowed; reduce the sampling interval or increase capacity");
                    auto events = trace->Events();
                    std::ranges::stable_sort(events, [](const Event &left, const Event &right) {
                        return std::tie(left.position.tick, left.component) < std::tie(right.position.tick, right.component);
                    });
                    for (auto &event : events)
                        event.sequence = 0;
                    std::ostringstream encoded;
                    WriteJson(encoded, events);
                    sample.eventHash = TextHash(encoded.str());
                    if (plan.eventDetails)
                        sample.events = std::move(events);
                    trace->Clear();
                }
                write(sample);
                ++count;
            }
        } catch (...) {
            machine.ObserveWith(nullptr);
            throw;
        }
        machine.ObserveWith(nullptr);
        write(ObservationEnd{.tick = machine.TimelineTicks(), .samples = count});
    }
} // namespace diagnostics
