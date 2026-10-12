#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace diagnostics {
    inline constexpr uint32_t SchemaVersion = 1;
    inline constexpr uint64_t MasterTicksPerSecond = 8'388'608;
    inline constexpr uint64_t RunFrameTicks = 140'448;

    enum class Outcome { Pass, AssertionFailure, Timeout, UnsupportedConfiguration, InfrastructureError };
    enum class Completion { Assertion, StableResult, CompletionMarker, FixedInterval, Deadline, Unsupported, Exception };
    enum class ExpectationStatus { Established, Disputed, InternalInvariant, ReferenceComparison, Unspecified };

    struct CaseIdentity {
        std::string id;
        std::string suite;
        std::string rom;
        std::string model;
        std::string startup;
        std::string expectationVariant;
    };

    struct InputFingerprint {
        std::string rom;
        std::string bootrom;
        std::string reference;
        std::string configuration;
        std::string cartridgeSave;
    };

    struct RunIdentity {
        uint32_t schemaVersion{SchemaVersion};
        std::string record{"run"};
        std::string executable;
        std::string executableHash;
        std::string compiler;
        std::vector<std::string> arguments;
        uint64_t masterTicksPerSecond{MasterTicksPerSecond};
        uint64_t runFrameTicks{RunFrameTicks};
    };

    struct Position {
        uint64_t tick{};
        uint64_t frame{};
        uint16_t dot{};
        uint8_t ly{};
        uint8_t cpuPhase{};
    };

    struct TraceIdentity {
        uint32_t schemaVersion{SchemaVersion};
        uint64_t sequence{};
        uint64_t branch{};
        uint64_t parentBranch{};
        Position restoredPosition;
        std::string captureConfiguration;
    };

    struct CheckpointIdentity {
        uint32_t schemaVersion{SchemaVersion};
        std::string stateFormat;
        std::string build;
        InputFingerprint inputs;
        Position position;
        uint64_t replayCursor{};
    };

    struct Result {
        uint32_t schemaVersion{SchemaVersion};
        std::string record{"result"};
        CaseIdentity test;
        InputFingerprint inputs;
        Outcome outcome{Outcome::InfrastructureError};
        Completion completion{Completion::Exception};
        ExpectationStatus expectationStatus{ExpectationStatus::Unspecified};
        Position position;
        uint64_t budgetTicks{};
        std::string expected;
        std::string actual;
        std::string details;
        std::vector<std::string> artifacts;

        [[nodiscard]] bool Passed() const { return outcome == Outcome::Pass; }
    };

    [[nodiscard]] std::string StableCaseId(const CaseIdentity &test);
}
