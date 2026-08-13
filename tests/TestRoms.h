#ifndef STARGBC_TESTROMS_H
#define STARGBC_TESTROMS_H

#include <fstream>
#include <future>
#include <Gameboy.h>
#include <iostream>
#include <semaphore>
#include <span>
#include <string>
#include <vector>

#include "doctest.h"
#include "ThreadContext.h"

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

static bool runRomTest(const std::string &rom,
                          const std::string &expected_screen,
                          const std::string &bios,
                          const Mode mode) {
    ThreadPermit _permit;
    try {
        const std::vector<uint32_t> expectedResult = readBinaryFile(expected_screen);

        const auto gameboy = Gameboy::init({
            .romName = rom,
            .biosPath = bios,
            .mode = mode,
        });
        gameboy->SetThrottle(false);

        const auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < 10s) {
            gameboy->UpdateEmulator();
        }
        gameboy->SetPaused(true);

        if (!std::ranges::equal(std::span(gameboy->GetScreenData(), expectedResult.size()), expectedResult)) {
            std::cerr << "Failed " << rom << std::endl;
            return false;
        }
        return true;
    } catch ([[maybe_unused]] const std::exception &e) {
        return false;
    }
}

struct Case {
    std::string rom;
    std::string expected;
    std::string bios;
    Mode mode;
};

static const std::vector<Case> romTestcases = {
    {"roms/blargg/halt_bug.gb", "tests/expected/blargg/halt_bug.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/instr_timing/instr_timing.gb", "tests/expected/blargg/instr_timing.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/interrupt_time/interrupt_time.gb", "tests/expected/blargg/interrupt_time.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
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
    {"roms/blargg/mem_timing/individual/01-read_timing.gb", "tests/expected/blargg/mem_timing/01-read_timing.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/mem_timing/individual/02-write_timing.gb", "tests/expected/blargg/mem_timing/02-write_timing.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/mem_timing/individual/03-modify_timing.gb", "tests/expected/blargg/mem_timing/03-modify_timing.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/mem_timing-2/rom_singles/01-read_timing.gb", "tests/expected/blargg/mem_timing/01-read_timing.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/mem_timing-2/rom_singles/02-write_timing.gb", "tests/expected/blargg/mem_timing/02-write_timing.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/mem_timing-2/rom_singles/03-modify_timing.gb", "tests/expected/blargg/mem_timing/03-modify_timing.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/dmg_sound/rom_singles/01-registers.gb", "tests/expected/blargg/dmg_sound/01-registers.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/dmg_sound/rom_singles/02-len ctr.gb", "tests/expected/blargg/dmg_sound/02-len ctr.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/dmg_sound/rom_singles/03-trigger.gb", "tests/expected/blargg/dmg_sound/03-trigger.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/dmg_sound/rom_singles/04-sweep.gb", "tests/expected/blargg/dmg_sound/04-sweep.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/dmg_sound/rom_singles/05-sweep details.gb", "tests/expected/blargg/dmg_sound/05-sweep details.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/dmg_sound/rom_singles/06-overflow on trigger.gb", "tests/expected/blargg/dmg_sound/06-overflow on trigger.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/dmg_sound/rom_singles/07-len sweep period sync.gb", "tests/expected/blargg/dmg_sound/07-len sweep period sync.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/dmg_sound/rom_singles/08-len ctr during power.gb", "tests/expected/blargg/dmg_sound/08-len ctr during power.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/dmg_sound/rom_singles/09-wave read while on.gb", "tests/expected/blargg/dmg_sound/09-wave read while on.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/dmg_sound/rom_singles/10-wave trigger while on.gb", "tests/expected/blargg/dmg_sound/10-wave trigger while on.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/dmg_sound/rom_singles/11-regs after power.gb", "tests/expected/blargg/dmg_sound/11-regs after power.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/dmg_sound/rom_singles/12-wave write while on.gb", "tests/expected/blargg/dmg_sound/12-wave write while on.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/cgb_sound/rom_singles/01-registers.gb", "tests/expected/blargg/cgb_sound/01-registers.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/blargg/cgb_sound/rom_singles/02-len ctr.gb", "tests/expected/blargg/cgb_sound/02-len ctr.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/blargg/cgb_sound/rom_singles/03-trigger.gb", "tests/expected/blargg/cgb_sound/03-trigger.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/blargg/cgb_sound/rom_singles/04-sweep.gb", "tests/expected/blargg/cgb_sound/04-sweep.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/blargg/cgb_sound/rom_singles/05-sweep details.gb", "tests/expected/blargg/cgb_sound/05-sweep details.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/blargg/cgb_sound/rom_singles/06-overflow on trigger.gb", "tests/expected/blargg/cgb_sound/06-overflow on trigger.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/blargg/cgb_sound/rom_singles/07-len sweep period sync.gb", "tests/expected/blargg/cgb_sound/07-len sweep period sync.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/blargg/cgb_sound/rom_singles/08-len ctr during power.gb", "tests/expected/blargg/cgb_sound/08-len ctr during power.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/blargg/cgb_sound/rom_singles/09-wave read while on.gb", "tests/expected/blargg/cgb_sound/09-wave read while on.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/blargg/cgb_sound/rom_singles/10-wave trigger while on.gb", "tests/expected/blargg/cgb_sound/10-wave trigger while on.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/blargg/cgb_sound/rom_singles/11-regs after power.gb", "tests/expected/blargg/cgb_sound/11-regs after power.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/blargg/cgb_sound/rom_singles/12-wave.gb", "tests/expected/blargg/cgb_sound/12-wave.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/blargg/oam_bug/rom_singles/1-lcd_sync.gb", "tests/expected/blargg/oam_bug/1-lcd_sync.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/oam_bug/rom_singles/2-causes.gb", "tests/expected/blargg/oam_bug/2-causes.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/oam_bug/rom_singles/3-non_causes.gb", "tests/expected/blargg/oam_bug/3-non_causes.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/oam_bug/rom_singles/4-scanline_timing.gb", "tests/expected/blargg/oam_bug/4-scanline_timing.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/oam_bug/rom_singles/5-timing_bug.gb", "tests/expected/blargg/oam_bug/5-timing_bug.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/oam_bug/rom_singles/6-timing_no_bug.gb", "tests/expected/blargg/oam_bug/6-timing_no_bug.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/blargg/oam_bug/rom_singles/8-instr_effect.gb", "tests/expected/blargg/oam_bug/8-instr_effect.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m2_win_en_toggle.gb", "tests/expected/mealybug-tearoom-tests/ppu/m2_win_en_toggle.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_bgp_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_bgp_change.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_bgp_change_sprites.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_bgp_change_sprites.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_bg_en_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_bg_en_change.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_bg_map_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_bg_map_change.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_obj_en_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_obj_en_change.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_obj_en_change_variant.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_obj_en_change_variant.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_obj_size_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_obj_size_change.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_obj_size_change_scx.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_obj_size_change_scx.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_scx_low_3_bits.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_scx_low_3_bits.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_wx_4_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_wx_4_change.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_wx_4_change_sprites.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_wx_4_change_sprites.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_wx_5_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_wx_5_change.gb.screen", bootroms.dmgBootrom, Mode::DMG},
};

static auto &romFutures() {
    using SF = std::shared_future<bool>;
    static std::vector<SF> futures = [] {
        std::vector<SF> tmp;
        tmp.reserve(romTestcases.size());

        for (const auto &tc: romTestcases) {
            tmp.emplace_back(
                std::async(std::launch::async, [tc] {
                    return runRomTest(tc.rom, tc.expected, tc.bios, tc.mode);
                }).share()
            );
        }
        return tmp;
    }();
    return futures;
}

#define ROM_TEST(IDX, ROM_STR)                       \
TEST_CASE("rom: " ROM_STR) {                         \
auto& futures = romFutures();                        \
CHECK_MESSAGE(futures[IDX].get(), "failed: " ROM_STR);  \
}

ROM_TEST(0, "roms/blargg/halt_bug.gb")
ROM_TEST(1, "roms/blargg/instr_timing/instr_timing.gb")
ROM_TEST(2, "roms/blargg/interrupt_time/interrupt_time.gb")
ROM_TEST(3, "roms/blargg/cpu_instrs/individual/01-special.gb")
ROM_TEST(4, "roms/blargg/cpu_instrs/individual/02-interrupts.gb")
ROM_TEST(5, "roms/blargg/cpu_instrs/individual/03-op sp,hl.gb")
ROM_TEST(6, "roms/blargg/cpu_instrs/individual/04-op r,imm.gb")
ROM_TEST(7, "roms/blargg/cpu_instrs/individual/05-op rp.gb")
ROM_TEST(8, "roms/blargg/cpu_instrs/individual/06-ld r,r.gb")
ROM_TEST(9, "roms/blargg/cpu_instrs/individual/07-jr,jp,call,ret,rst.gb")
ROM_TEST(10, "roms/blargg/cpu_instrs/individual/08-misc instrs.gb")
ROM_TEST(11, "roms/blargg/cpu_instrs/individual/09-op r,r.gb")
ROM_TEST(12, "roms/blargg/cpu_instrs/individual/10-bit ops.gb")
ROM_TEST(13, "roms/blargg/cpu_instrs/individual/11-op a,(hl).gb")
ROM_TEST(14, "roms/blargg/mem_timing/individual/01-read_timing.gb")
ROM_TEST(15, "roms/blargg/mem_timing/individual/02-write_timing.gb")
ROM_TEST(16, "roms/blargg/mem_timing/individual/03-modify_timing.gb")
ROM_TEST(17, "roms/blargg/mem_timing-2/rom_singles/01-read_timing.gb")
ROM_TEST(18, "roms/blargg/mem_timing-2/rom_singles/02-write_timing.gb")
ROM_TEST(19, "roms/blargg/mem_timing-2/rom_singles/03-modify_timing.gb")
ROM_TEST(20, "roms/blargg/dmg_sound/rom_singles/01-registers.gb")
ROM_TEST(21, "roms/blargg/dmg_sound/rom_singles/02-len ctr.gb")
ROM_TEST(22, "roms/blargg/dmg_sound/rom_singles/03-trigger.gb")
ROM_TEST(23, "roms/blargg/dmg_sound/rom_singles/04-sweep.gb")
ROM_TEST(24, "roms/blargg/dmg_sound/rom_singles/05-sweep details.gb")
ROM_TEST(25, "roms/blargg/dmg_sound/rom_singles/06-overflow on trigger.gb")
ROM_TEST(26, "roms/blargg/dmg_sound/rom_singles/07-len sweep period sync.gb")
ROM_TEST(27, "roms/blargg/dmg_sound/rom_singles/08-len ctr during power.gb")
ROM_TEST(28, "roms/blargg/dmg_sound/rom_singles/09-wave read while on.gb")
ROM_TEST(29, "roms/blargg/dmg_sound/rom_singles/10-wave trigger while on.gb")
ROM_TEST(30, "roms/blargg/dmg_sound/rom_singles/11-regs after power.gb")
ROM_TEST(31, "roms/blargg/dmg_sound/rom_singles/12-wave write while on.gb")
ROM_TEST(32, "roms/blargg/cgb_sound/rom_singles/01-registers.gb")
ROM_TEST(33, "roms/blargg/cgb_sound/rom_singles/02-len ctr.gb")
ROM_TEST(34, "roms/blargg/cgb_sound/rom_singles/03-trigger.gb")
ROM_TEST(35, "roms/blargg/cgb_sound/rom_singles/04-sweep.gb")
ROM_TEST(36, "roms/blargg/cgb_sound/rom_singles/05-sweep details.gb")
ROM_TEST(37, "roms/blargg/cgb_sound/rom_singles/06-overflow on trigger.gb")
ROM_TEST(38, "roms/blargg/cgb_sound/rom_singles/07-len sweep period sync.gb")
ROM_TEST(39, "roms/blargg/cgb_sound/rom_singles/08-len ctr during power.gb")
ROM_TEST(40, "roms/blargg/cgb_sound/rom_singles/09-wave read while on.gb")
ROM_TEST(41, "roms/blargg/cgb_sound/rom_singles/10-wave trigger while on.gb")
ROM_TEST(42, "roms/blargg/cgb_sound/rom_singles/11-regs after power.gb")
ROM_TEST(43, "roms/blargg/cgb_sound/rom_singles/12-wave.gb")
ROM_TEST(44, "roms/blargg/oam_bug/rom_singles/1-lcd_sync.gb")
ROM_TEST(45, "roms/blargg/oam_bug/rom_singles/2-causes.gb")
ROM_TEST(46, "roms/blargg/oam_bug/rom_singles/3-non_causes.gb")
ROM_TEST(47, "roms/blargg/oam_bug/rom_singles/4-scanline_timing.gb")
ROM_TEST(48, "roms/blargg/oam_bug/rom_singles/5-timing_bug.gb")
ROM_TEST(49, "roms/blargg/oam_bug/rom_singles/6-timing_no_bug.gb")
ROM_TEST(50, "roms/blargg/oam_bug/rom_singles/8-instr_effect.gb")
ROM_TEST(51, "roms/mealybug-tearoom-tests/ppu/m2_win_en_toggle.gb")
ROM_TEST(52, "roms/mealybug-tearoom-tests/ppu/m3_bgp_change.gb")
ROM_TEST(53, "roms/mealybug-tearoom-tests/ppu/m3_bgp_change_sprites.gb")
ROM_TEST(54, "roms/mealybug-tearoom-tests/ppu/m3_lcdc_bg_en_change.gb")
ROM_TEST(55, "roms/mealybug-tearoom-tests/ppu/m3_lcdc_bg_map_change.gb")
ROM_TEST(56, "roms/mealybug-tearoom-tests/ppu/m3_lcdc_obj_en_change.gb")
ROM_TEST(57, "roms/mealybug-tearoom-tests/ppu/m3_lcdc_obj_en_change_variant.gb")
ROM_TEST(58, "roms/mealybug-tearoom-tests/ppu/m3_lcdc_obj_size_change.gb")
ROM_TEST(59, "roms/mealybug-tearoom-tests/ppu/m3_lcdc_obj_size_change_scx.gb")
ROM_TEST(60, "roms/mealybug-tearoom-tests/ppu/m3_scx_low_3_bits.gb")
ROM_TEST(61, "roms/mealybug-tearoom-tests/ppu/m3_wx_4_change.gb")
ROM_TEST(62, "roms/mealybug-tearoom-tests/ppu/m3_wx_4_change_sprites.gb")
ROM_TEST(63, "roms/mealybug-tearoom-tests/ppu/m3_wx_5_change.gb")


inline int ExecuteTestRoms(const int argc, char **argv) {
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

#endif //STARGBC_TESTROMS_H