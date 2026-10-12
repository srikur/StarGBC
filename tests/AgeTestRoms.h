#pragma once
#include <sstream>
#include "TestRoms.h"

struct AgeTestCase {
    std::string rom;
    Model model;
    std::string reference;
};

static const std::vector<AgeTestCase> ageTestCases = {
#define AGE_TEST(INDEX, ROM, MODEL, REFERENCE) {"roms/age-test-roms/" ROM, Model::MODEL, REFERENCE},
#include "AgeTestCases.inc"
#undef AGE_TEST
};

struct AgeTestResult {
    bool passed;
    std::string details;
};

static AgeTestResult runAgeTest(const AgeTestCase &test) {
    ThreadPermit permit;
    try {
        const auto gameboy = Gameboy::init({
                .romName = test.rom,
                .biosPath = IsCgb(test.model) ? bootroms.cgbBootrom : bootroms.dmgBootrom,
                .model = test.model,
                .executionPolicy = testExecutionPolicy,
        });
        const auto expected =
                test.reference.empty() ? std::vector<uint32_t>{} : readBinaryFile("tests/expected/age-test-roms/" + test.reference + ".screen");
        auto &cpu = gameboy->[:testGameboyMember("cpu_"):];
        auto &bus = gameboy->[:testGameboyMember("bus_"):];
        const auto &regs = gameboy->[:testGameboyMember("registers_"):];
        const unsigned limit = romFrames ? romFrames : 600;
        unsigned matchingFrames = 0;
        bool passed = false;
        unsigned frames = 0;
        for (; frames < limit; ++frames) {
            gameboy->RunFrame();
            if (bus.bootromRunning)
                continue;
            if (!expected.empty()) {
                const bool matches = std::ranges::equal(std::span(gameboy->GetScreenData(), expected.size()), expected);
                matchingFrames = matches ? matchingFrames + 1 : 0;
                passed = matchingFrames >= 2;
                if (!romFrames && passed)
                    break;
            } else {
                // AGE's final LD B,B marker enters freeze: IE=0, EI, NOP, HALT, NOP
                const auto pc = cpu.pc();
                const bool finished = gameboy->InspectRegister(0xFFFF).cpuVisible == 0 && pc >= 8 &&
                                      gameboy->InspectCpuRead(pc - 4) == 0xFB && gameboy->InspectCpuRead(pc - 3) == 0x00 &&
                                      gameboy->InspectCpuRead(pc - 2) == 0x76 && gameboy->InspectCpuRead(pc - 1) == 0x00;
                passed = finished && mooneyePassed(*gameboy);
                if (!romFrames && finished)
                    break;
            }
        }
        std::ostringstream details;
        details << test.rom << " [" << ModelName(test.model) << "] after " << std::min(frames + 1, limit)
                << " frames: " << (passed ? "passed" : "failed or unfinished") << std::hex << ", PC=" << cpu.pc() << " BC=" << regs.GetBC()
                << " DE=" << regs.GetDE() << " HL=" << regs.GetHL();
        if (!passed && expected.empty()) {
            details << "; results at C000:";
            for (unsigned i = 0; i < 128; ++i)
                details << ' ' << +gameboy->DebugPeek(0xC000 + i);
        }
        if (!passed && !expected.empty()) {
            unsigned pixels = 0;
            for (unsigned i = 0; i < expected.size(); ++i)
                pixels += expected[i] != gameboy->GetScreenData()[i];
            details << std::dec << "; differing pixels=" << pixels;
        }
        return {passed, details.str()};
    } catch (const std::exception &error) {
        return {false, test.rom + ": " + error.what()};
    }
}

static const std::vector<std::shared_future<AgeTestResult>> &ageTestFutures() {
    static const auto futures = [] {
        std::vector<std::shared_future<AgeTestResult>> result;
        for (const auto &test : ageTestCases) {
            const std::string name = "age: " + test.rom.substr(std::string_view("roms/age-test-roms/").size()) + " [" +
                                     std::string(ModelName(test.model)) + "]";
            result.push_back(selectedTestNames.contains(name) ? testWorkers->Submit([test] { return runAgeTest(test); })
                                                              : std::shared_future<AgeTestResult>{});
        }
        return result;
    }();
    return futures;
}

#define AGE_TEST(INDEX, ROM, MODEL, REFERENCE)                                                                                                       \
    TEST_CASE("age: " ROM " [" #MODEL "]" * doctest::test_suite("age")) {                                                                            \
        const auto &result = ageTestFutures()[INDEX].get();                                                                                          \
        CHECK_MESSAGE(result.passed, result.details);                                                                                                \
    }
#include "AgeTestCases.inc"
#undef AGE_TEST
