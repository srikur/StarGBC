#define DOCTEST_CONFIG_IMPLEMENT
#include "doctest.h"
#include <Gameboy.h>

#include <fstream>
#include <future>
#include <iostream>
#include <semaphore>
#include <span>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

struct Bootroms {
    std::string dmg0Bootrom = "roms/dmg0_boot.bin";
    std::string dmgBootrom = "roms/dmg_boot.bin";
    std::string cgb0Bootrom = "roms/cgb0_boot.bin";
    std::string cgbEBootrom = "roms/cgbE_boot.bin";
    std::string cgbBootrom = "roms/cgb_boot.bin";
    std::string cgbAGBBootrom = "roms/cgb_agb_boot.bin";
    std::string cgbAGB0Bootrom = "roms/cgb_agb0_boot.bin";
    std::string sgbBootrom = "roms/sgb_boot.bin";
    std::string sgb2Bootrom = "roms/sgb2_boot.bin";
};

constexpr Bootroms bootroms{};
static size_t maxThreads = std::thread::hardware_concurrency();
static std::shared_ptr<std::counting_semaphore<> > threadSemaphore;

struct ThreadPermit {
    ThreadPermit() { threadSemaphore->acquire(); }
    ~ThreadPermit() { threadSemaphore->release(); }
};

static std::vector<uint32_t> readBinaryFile(const std::string &path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) {
        throw std::runtime_error("Could not open file " + path);
    }
    constexpr int SCREEN_SIZE = 160 * 144;
    std::vector<uint32_t> result(SCREEN_SIZE);
    ifs.read(reinterpret_cast<char *>(result.data()), SCREEN_SIZE * sizeof(uint32_t));
    return result;
}

static bool runBlarggTest(const std::string &rom,
                          const std::string &expected_screen,
                          const std::string &bios,
                          const Mode mode) {
    ThreadPermit _permit;
    const std::vector<uint32_t> expectedResult = readBinaryFile(expected_screen);

    const auto gameboy = Gameboy::init({
        .romName = rom,
        .biosPath = bios,
        .mode = mode,
        .runBootrom = true
    });
    gameboy->SetThrottle(false);

    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < 5s) {
        gameboy->UpdateEmulator();
    }
    gameboy->SetPaused(true);

    const uint32_t *fb = gameboy->GetScreenData();
    if (!std::ranges::equal(std::span(fb, expectedResult.size()), expectedResult)) {
        std::cerr << "Failed " << rom << std::endl;
        return false;
    }
    return true;
}

struct BlarggCase {
    std::string rom;
    std::string expected;
    std::string bios;
    Mode mode;
};

static const std::vector<BlarggCase> blarggTestcases = {
    {"roms/blargg/halt_bug.gb", "tests/expected/blargg/halt_bug.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/instr_timing/instr_timing.gb", "tests/expected/blargg/instr_timing.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/cpu_instrs/individual/01-special.gb", "tests/expected/blargg/cpu_instrs/01-special.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/cpu_instrs/individual/02-interrupts.gb", "tests/expected/blargg/cpu_instrs/02-interrupts.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/cpu_instrs/individual/03-op sp,hl.gb", "tests/expected/blargg/cpu_instrs/03-op sp,hl.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/cpu_instrs/individual/04-op r,imm.gb", "tests/expected/blargg/cpu_instrs/04-op r,imm.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/cpu_instrs/individual/05-op rp.gb", "tests/expected/blargg/cpu_instrs/05-op rp.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/cpu_instrs/individual/06-ld r,r.gb", "tests/expected/blargg/cpu_instrs/06-ld r,r.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/cpu_instrs/individual/07-jr,jp,call,ret,rst.gb", "tests/expected/blargg/cpu_instrs/07-jr,jp,call,ret,rst.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/cpu_instrs/individual/08-misc instrs.gb", "tests/expected/blargg/cpu_instrs/08-misc instrs.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/cpu_instrs/individual/09-op r,r.gb", "tests/expected/blargg/cpu_instrs/09-op r,r.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/cpu_instrs/individual/10-bit ops.gb", "tests/expected/blargg/cpu_instrs/10-bit ops.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/cpu_instrs/individual/11-op a,(hl).gb", "tests/expected/blargg/cpu_instrs/11-op a,(hl).gb.screen", bootroms.dmgBootrom, Mode::DMG},
};

static auto &blarggFutures() {
    using SF = std::shared_future<bool>;
    static std::vector<SF> futures = [] {
        std::vector<SF> tmp;
        tmp.reserve(blarggTestcases.size());

        for (const auto &tc: blarggTestcases) {
            tmp.emplace_back(
                std::async(std::launch::async, [tc] {
                    return runBlarggTest(tc.rom, tc.expected, tc.bios, tc.mode);
                }).share()
            );
        }
        return tmp;
    }();
    return futures;
}

#define BLARGG_TEST(IDX, ROM_STR)                                                 \
TEST_CASE("blargg: " ROM_STR) {                                              \
auto& futures = blarggFutures();                                    \
CHECK_MESSAGE(futures[IDX].get(), "failed: " ROM_STR);                   \
}

BLARGG_TEST(0, "roms/blargg/halt_bug.gb")
BLARGG_TEST(1, "roms/blargg/instr_timing/instr_timing.gb")
BLARGG_TEST(2, "roms/blargg/cpu_instrs/individual/01-special.gb")
BLARGG_TEST(3, "roms/blargg/cpu_instrs/individual/02-interrupts.gb")
BLARGG_TEST(4, "roms/blargg/cpu_instrs/individual/03-op sp,hl.gb")
BLARGG_TEST(5, "roms/blargg/cpu_instrs/individual/04-op r,imm.gb")
BLARGG_TEST(6, "roms/blargg/cpu_instrs/individual/05-op rp.gb")
BLARGG_TEST(7, "roms/blargg/cpu_instrs/individual/06-ld r,r.gb")
BLARGG_TEST(8, "roms/blargg/cpu_instrs/individual/07-jr,jp,call,ret,rst.gb")
BLARGG_TEST(9, "roms/blargg/cpu_instrs/individual/08-misc instrs.gb")
BLARGG_TEST(10, "roms/blargg/cpu_instrs/individual/09-op r,r.gb")
BLARGG_TEST(11, "roms/blargg/cpu_instrs/individual/10-bit ops.gb")
BLARGG_TEST(12, "roms/blargg/cpu_instrs/individual/11-op a,(hl).gb")

int main(const int argc, char **argv) {
    std::vector<char *> doctest_args;
    doctest_args.reserve(argc);
    doctest_args.push_back(argv[0]);

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];
        if (constexpr std::string_view kPrefix = "--max-threads="; arg.rfind(kPrefix, 0) == 0) {
            try {
                maxThreads = std::stoul(std::string(arg.substr(kPrefix.size())));
            } catch (...) {
                std::cerr << "Invalid value for --max-threads" << std::endl;
                return EXIT_FAILURE;
            }
        } else {
            doctest_args.push_back(argv[i]);
        }
    }

    if (maxThreads == 0) { maxThreads = 1; }
    threadSemaphore = std::make_shared<std::counting_semaphore<> >(maxThreads);

    doctest::Context ctx;
    ctx.applyCommandLine(static_cast<int>(doctest_args.size()), doctest_args.data());
    return ctx.run();
}