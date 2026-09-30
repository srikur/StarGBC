#pragma once
#include "TestRoms.h"
#include <sstream>

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
        });
        const auto expected = test.reference.empty() ? std::vector<uint32_t>{}
            : readBinaryFile("tests/expected/age-test-roms/" + test.reference + ".screen");
        auto &cpu = gameboy->[:testGameboyMember("cpu_"):];
        auto &bus = gameboy->[:testGameboyMember("bus_"):];
        const auto &regs = gameboy->[:testGameboyMember("registers_"):];
        const unsigned limit = romFrames ? romFrames : 600;
        unsigned matchingFrames = 0;
        bool passed = false;
        unsigned frames = 0;
        for (; frames < limit; ++frames) {
            gameboy->RunFrame();
            if (bus.bootromRunning) continue;
            if (!expected.empty()) {
                const bool matches = std::ranges::equal(std::span(gameboy->GetScreenData(), expected.size()), expected);
                matchingFrames = matches ? matchingFrames + 1 : 0;
                passed = matchingFrames >= 2;
                if (!romFrames && passed) break;
            } else {
                // AGE's final LD B,B marker enters freeze: IE=0, EI, NOP, HALT, NOP
                const auto pc = cpu.pc();
                const bool finished = bus.ReadByte(0xFFFF, ComponentSource::CPU) == 0 && pc >= 8 &&
                    bus.ReadByte(pc - 4, ComponentSource::CPU) == 0xFB &&
                    bus.ReadByte(pc - 3, ComponentSource::CPU) == 0x00 &&
                    bus.ReadByte(pc - 2, ComponentSource::CPU) == 0x76 &&
                    bus.ReadByte(pc - 1, ComponentSource::CPU) == 0x00;
                passed = finished && mooneyePassed(*gameboy);
                if (!romFrames && finished) break;
            }
        }
        std::ostringstream details;
        details << test.rom << " [" << ModelName(test.model) << "] after " << std::min(frames + 1, limit)
            << " frames: " << (passed ? "passed" : "failed or unfinished") << std::hex
            << ", PC=" << cpu.pc() << " BC=" << regs.GetBC() << " DE=" << regs.GetDE() << " HL=" << regs.GetHL();
        if (!passed && expected.empty()) {
            details << "; results at C000:";
            for (unsigned i = 0; i < 128; ++i) details << ' ' << +bus.ReadByte(0xC000 + i, ComponentSource::CPU);
        }
        if (!passed && !expected.empty()) {
            unsigned pixels = 0;
            for (unsigned i = 0; i < expected.size(); ++i) pixels += expected[i] != gameboy->GetScreenData()[i];
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
            result.emplace_back(std::async(parallelRomTests ? std::launch::async : std::launch::deferred,
                [test] { return runAgeTest(test); }).share());
        }
        return result;
    }();
    return futures;
}

#define AGE_TEST(INDEX, ROM, MODEL, REFERENCE) \
TEST_CASE("age: " ROM " [" #MODEL "]") { \
    const auto &result = ageTestFutures()[INDEX].get(); \
    CHECK_MESSAGE(result.passed, result.details); \
}
#include "AgeTestCases.inc"
#undef AGE_TEST
