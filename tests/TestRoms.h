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

#include <doctest/doctest.h>
#include "ThreadContext.h"

struct Bootroms {
    std::string dmg0Bootrom = "roms/dmg0_boot.bin";
    std::string dmgBootrom = "roms/dmg_boot.bin";
    std::string mgbBootrom = "roms/mgb_boot.bin";
    std::string cgb0Bootrom = "roms/cgb0_boot.bin";
    std::string cgbEBootrom = "roms/cgbE_boot.bin";
    std::string cgbBootrom = "roms/cgb_boot.bin";
    std::string cgbAGBBootrom = "roms/cgb_agb_boot.bin";
    std::string cgbAGB0Bootrom = "roms/cgb_agb0_boot.bin";
    std::string sgbBootrom = "roms/sgb_boot.bin";
    std::string sgb2Bootrom = "roms/sgb2_boot.bin";
};

const Bootroms bootroms{};
static unsigned romFrames = 0;
static bool parallelRomTests = true;

static consteval std::meta::info testGameboyMember(const std::string_view name) {
    for (const auto m : std::meta::nonstatic_data_members_of(^^Gameboy, std::meta::access_context::unchecked())) {
        if (std::meta::identifier_of(m) == name) return m;
    }
    throw "Gameboy member not found";
}

static bool mooneyePassed(const Gameboy &gameboy) {
    const auto &regs = gameboy.[:testGameboyMember("registers_"):];
    return regs.GetBC() == 0x0305 && regs.GetDE() == 0x080D && regs.GetHL() == 0x1522;
}

static std::vector<uint32_t> readBinaryFile(const std::string &path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) {
        throw std::runtime_error("Could not open file " + path);
    }
    constexpr int SCREEN_SIZE = 160 * 144;
    std::vector<uint32_t> result(SCREEN_SIZE);
    ifs.read(reinterpret_cast<char *>(result.data()), SCREEN_SIZE * sizeof(uint32_t));
    if (!ifs) throw std::runtime_error("Incomplete screen file " + path);
    return result;
}

static bool runRomTest(const std::string &rom,
                       const std::string &expected_screen,
                       const std::string &bios,
                       const Model model,
                       const bool requiresStop) {
    ThreadPermit _permit;
    try {
        const std::vector<uint32_t> expectedResult = expected_screen.empty()
            ? std::vector<uint32_t>{} : readBinaryFile(expected_screen);

        const auto gameboy = Gameboy::init({
            .romName = rom,
            .biosPath = bios,
            .model = model,
        });

        const auto passed = [&] {
            // STOP references can be completely blank. Require the CPU to have
            // stopped so an unrendered startup buffer cannot count as success.
            if (requiresStop) {
                return gameboy->[:testGameboyMember("cpu_"):].stopped() &&
                    std::ranges::equal(std::span(gameboy->GetScreenData(), expectedResult.size()), expectedResult,
                        [](uint32_t a, uint32_t b) { return (a & 0xFFFFFF) == (b & 0xFFFFFF); });
            }
            return expectedResult.empty() ? mooneyePassed(*gameboy)
                : std::ranges::equal(std::span(gameboy->GetScreenData(), expectedResult.size()), expectedResult);
        };
        // An emulated-time budget is independent of host load. Most ROMs finish
        // much earlier; require two matching frames before accepting their screen.
        const unsigned frameLimit = romFrames ? romFrames : 6000;
        unsigned matchingFrames = 0;
        for (unsigned frames = 0; frames < frameLimit; ++frames) {
            gameboy->RunFrame();
            matchingFrames = passed() ? matchingFrames + 1 : 0;
            if (!romFrames && matchingFrames >= 2) return true;
        }

        if (!passed()) {
            std::cerr << "Failed " << rom << " [" << ModelName(model) << "] after " << frameLimit << " frames" << std::endl;
            return false;
        }
        return true;
    } catch (const std::exception &e) {
        std::cerr << "Failed " << rom << " [" << ModelName(model) << "]: " << e.what() << std::endl;
        return false;
    }
}

struct Case {
    std::string rom;
    std::string expected;
    std::string bios;
    Model model;
    bool requiresStop{false};
};

static const std::vector<Case> romTestcases = {
    {"roms/blargg/halt_bug.gb", "tests/expected/blargg/halt_bug.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/instr_timing/instr_timing.gb", "tests/expected/blargg/instr_timing.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/interrupt_time/interrupt_time.gb", "tests/expected/blargg/interrupt_time.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/blargg/cpu_instrs/individual/01-special.gb", "tests/expected/blargg/cpu_instrs/01-special.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/cpu_instrs/individual/02-interrupts.gb", "tests/expected/blargg/cpu_instrs/02-interrupts.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/cpu_instrs/individual/03-op sp,hl.gb", "tests/expected/blargg/cpu_instrs/03-op sp,hl.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/cpu_instrs/individual/04-op r,imm.gb", "tests/expected/blargg/cpu_instrs/04-op r,imm.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/cpu_instrs/individual/05-op rp.gb", "tests/expected/blargg/cpu_instrs/05-op rp.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/cpu_instrs/individual/06-ld r,r.gb", "tests/expected/blargg/cpu_instrs/06-ld r,r.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/cpu_instrs/individual/07-jr,jp,call,ret,rst.gb", "tests/expected/blargg/cpu_instrs/07-jr,jp,call,ret,rst.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/cpu_instrs/individual/08-misc instrs.gb", "tests/expected/blargg/cpu_instrs/08-misc instrs.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/cpu_instrs/individual/09-op r,r.gb", "tests/expected/blargg/cpu_instrs/09-op r,r.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/cpu_instrs/individual/10-bit ops.gb", "tests/expected/blargg/cpu_instrs/10-bit ops.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/cpu_instrs/individual/11-op a,(hl).gb", "tests/expected/blargg/cpu_instrs/11-op a,(hl).gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/mem_timing/individual/01-read_timing.gb", "tests/expected/blargg/mem_timing/01-read_timing.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/mem_timing/individual/02-write_timing.gb", "tests/expected/blargg/mem_timing/02-write_timing.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/mem_timing/individual/03-modify_timing.gb", "tests/expected/blargg/mem_timing/03-modify_timing.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/mem_timing-2/rom_singles/01-read_timing.gb", "tests/expected/blargg/mem_timing/01-read_timing.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/mem_timing-2/rom_singles/02-write_timing.gb", "tests/expected/blargg/mem_timing/02-write_timing.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/mem_timing-2/rom_singles/03-modify_timing.gb", "tests/expected/blargg/mem_timing/03-modify_timing.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/dmg_sound/rom_singles/01-registers.gb", "tests/expected/blargg/dmg_sound/01-registers.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/dmg_sound/rom_singles/02-len ctr.gb", "tests/expected/blargg/dmg_sound/02-len ctr.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/dmg_sound/rom_singles/03-trigger.gb", "tests/expected/blargg/dmg_sound/03-trigger.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/dmg_sound/rom_singles/04-sweep.gb", "tests/expected/blargg/dmg_sound/04-sweep.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/dmg_sound/rom_singles/05-sweep details.gb", "tests/expected/blargg/dmg_sound/05-sweep details.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/dmg_sound/rom_singles/06-overflow on trigger.gb", "tests/expected/blargg/dmg_sound/06-overflow on trigger.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/dmg_sound/rom_singles/07-len sweep period sync.gb", "tests/expected/blargg/dmg_sound/07-len sweep period sync.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/dmg_sound/rom_singles/08-len ctr during power.gb", "tests/expected/blargg/dmg_sound/08-len ctr during power.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/dmg_sound/rom_singles/09-wave read while on.gb", "tests/expected/blargg/dmg_sound/09-wave read while on.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/dmg_sound/rom_singles/10-wave trigger while on.gb", "tests/expected/blargg/dmg_sound/10-wave trigger while on.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/dmg_sound/rom_singles/11-regs after power.gb", "tests/expected/blargg/dmg_sound/11-regs after power.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/dmg_sound/rom_singles/12-wave write while on.gb", "tests/expected/blargg/dmg_sound/12-wave write while on.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/cgb_sound/rom_singles/01-registers.gb", "tests/expected/blargg/cgb_sound/01-registers.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/blargg/cgb_sound/rom_singles/02-len ctr.gb", "tests/expected/blargg/cgb_sound/02-len ctr.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/blargg/cgb_sound/rom_singles/03-trigger.gb", "tests/expected/blargg/cgb_sound/03-trigger.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/blargg/cgb_sound/rom_singles/04-sweep.gb", "tests/expected/blargg/cgb_sound/04-sweep.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/blargg/cgb_sound/rom_singles/05-sweep details.gb", "tests/expected/blargg/cgb_sound/05-sweep details.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/blargg/cgb_sound/rom_singles/06-overflow on trigger.gb", "tests/expected/blargg/cgb_sound/06-overflow on trigger.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/blargg/cgb_sound/rom_singles/07-len sweep period sync.gb", "tests/expected/blargg/cgb_sound/07-len sweep period sync.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/blargg/cgb_sound/rom_singles/08-len ctr during power.gb", "tests/expected/blargg/cgb_sound/08-len ctr during power.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/blargg/cgb_sound/rom_singles/09-wave read while on.gb", "tests/expected/blargg/cgb_sound/09-wave read while on.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/blargg/cgb_sound/rom_singles/10-wave trigger while on.gb", "tests/expected/blargg/cgb_sound/10-wave trigger while on.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/blargg/cgb_sound/rom_singles/11-regs after power.gb", "tests/expected/blargg/cgb_sound/11-regs after power.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/blargg/cgb_sound/rom_singles/12-wave.gb", "tests/expected/blargg/cgb_sound/12-wave.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/blargg/oam_bug/rom_singles/1-lcd_sync.gb", "tests/expected/blargg/oam_bug/1-lcd_sync.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/oam_bug/rom_singles/2-causes.gb", "tests/expected/blargg/oam_bug/2-causes.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/oam_bug/rom_singles/3-non_causes.gb", "tests/expected/blargg/oam_bug/3-non_causes.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/oam_bug/rom_singles/4-scanline_timing.gb", "tests/expected/blargg/oam_bug/4-scanline_timing.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/oam_bug/rom_singles/5-timing_bug.gb", "tests/expected/blargg/oam_bug/5-timing_bug.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/oam_bug/rom_singles/6-timing_no_bug.gb", "tests/expected/blargg/oam_bug/6-timing_no_bug.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/blargg/oam_bug/rom_singles/8-instr_effect.gb", "tests/expected/blargg/oam_bug/8-instr_effect.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m2_win_en_toggle.gb", "tests/expected/mealybug-tearoom-tests/ppu/m2_win_en_toggle.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_bgp_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_bgp_change.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_bgp_change_sprites.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_bgp_change_sprites.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_bg_en_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_bg_en_change.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_bg_map_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_bg_map_change.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_obj_en_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_obj_en_change.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_obj_en_change_variant.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_obj_en_change_variant.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_obj_size_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_obj_size_change.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_obj_size_change_scx.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_obj_size_change_scx.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_scx_low_3_bits.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_scx_low_3_bits.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_wx_4_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_wx_4_change.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_wx_4_change_sprites.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_wx_4_change_sprites.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_wx_5_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_wx_5_change.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_window_timing.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_window_timing.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_window_timing_wx_0.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_window_timing_wx_0.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_scx_high_5_bits.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_scx_high_5_bits.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_obp0_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_obp0_change.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_win_map_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_win_map_change.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_tile_sel_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_tile_sel_change.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_tile_sel_win_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_tile_sel_win_change.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_scy_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_scy_change.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_win_en_change_multiple.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_win_en_change_multiple.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_win_en_change_multiple_wx.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_win_en_change_multiple_wx.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mealybug-tearoom-tests/ppu/m3_wx_6_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_wx_6_change.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/ppu/hblank_ly_scx_timing-GS.gb", "tests/expected/mooneye/ppu/hblank_ly_scx_timing-GS.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/ppu/intr_1_2_timing-GS.gb", "tests/expected/mooneye/ppu/intr_1_2_timing-GS.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/ppu/intr_2_mode0_timing.gb", "tests/expected/mooneye/ppu/intr_2_mode0_timing.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/ppu/intr_2_mode3_timing.gb", "tests/expected/mooneye/ppu/intr_2_mode3_timing.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/ppu/intr_2_oam_ok_timing.gb", "tests/expected/mooneye/ppu/intr_2_oam_ok_timing.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/ppu/vblank_stat_intr-GS.gb", "tests/expected/mooneye/ppu/vblank_stat_intr-GS.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/ppu/intr_2_0_timing.gb", "tests/expected/mooneye/ppu/intr_2_0_timing.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/ppu/stat_lyc_onoff.gb", "tests/expected/mooneye/ppu/stat_lyc_onoff.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/ppu/stat_irq_blocking.gb", "tests/expected/mooneye/ppu/stat_irq_blocking.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/misc/ppu/vblank_stat_intr-C.gb", "tests/expected/mooneye/ppu/vblank_stat_intr-C.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_1/channel_1_align.gb", "tests/expected/samesuite/apu/channel_1/channel_1_align.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_1/channel_1_align_cpu.gb", "tests/expected/samesuite/apu/channel_1/channel_1_align_cpu.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_1/channel_1_delay.gb", "tests/expected/samesuite/apu/channel_1/channel_1_delay.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_1/channel_1_duty.gb", "tests/expected/samesuite/apu/channel_1/channel_1_duty.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_1/channel_1_duty_delay.gb", "tests/expected/samesuite/apu/channel_1/channel_1_duty_delay.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_1/channel_1_freq_change.gb", "tests/expected/samesuite/apu/channel_1/channel_1_freq_change.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_1/channel_1_freq_change_timing-cgbDE.gb", "tests/expected/samesuite/apu/channel_1/channel_1_freq_change_timing-cgbDE.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_1/channel_1_restart.gb", "tests/expected/samesuite/apu/channel_1/channel_1_restart.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_1/channel_1_restart_nrx2_glitch.gb", "tests/expected/samesuite/apu/channel_1/channel_1_restart_nrx2_glitch.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_1/channel_1_stop_restart.gb", "tests/expected/samesuite/apu/channel_1/channel_1_stop_restart.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_2/channel_2_align.gb", "tests/expected/samesuite/apu/channel_2/channel_2_align.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_2/channel_2_align_cpu.gb", "tests/expected/samesuite/apu/channel_2/channel_2_align_cpu.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_2/channel_2_delay.gb", "tests/expected/samesuite/apu/channel_2/channel_2_delay.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_2/channel_2_duty.gb", "tests/expected/samesuite/apu/channel_2/channel_2_duty.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_2/channel_2_duty_delay.gb", "tests/expected/samesuite/apu/channel_2/channel_2_duty_delay.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_2/channel_2_freq_change.gb", "tests/expected/samesuite/apu/channel_2/channel_2_freq_change.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_2/channel_2_restart.gb", "tests/expected/samesuite/apu/channel_2/channel_2_restart.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_2/channel_2_restart_nrx2_glitch.gb", "tests/expected/samesuite/apu/channel_2/channel_2_restart_nrx2_glitch.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_2/channel_2_stop_restart.gb", "tests/expected/samesuite/apu/channel_2/channel_2_stop_restart.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_2/channel_2_volume.gb", "tests/expected/samesuite/apu/channel_2/channel_2_volume.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_3/channel_3_and_glitch.gb", "tests/expected/samesuite/apu/channel_3/channel_3_and_glitch.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_3/channel_3_delay.gb", "tests/expected/samesuite/apu/channel_3/channel_3_delay.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_3/channel_3_first_sample.gb", "tests/expected/samesuite/apu/channel_3/channel_3_first_sample.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_3/channel_3_freq_change_delay.gb", "tests/expected/samesuite/apu/channel_3/channel_3_freq_change_delay.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_3/channel_3_restart_delay.gb", "tests/expected/samesuite/apu/channel_3/channel_3_restart_delay.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_3/channel_3_restart_during_delay.gb", "tests/expected/samesuite/apu/channel_3/channel_3_restart_during_delay.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_3/channel_3_restart_stop_delay.gb", "tests/expected/samesuite/apu/channel_3/channel_3_restart_stop_delay.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_3/channel_3_shift_delay.gb", "tests/expected/samesuite/apu/channel_3/channel_3_shift_delay.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_3/channel_3_shift_skip_delay.gb", "tests/expected/samesuite/apu/channel_3/channel_3_shift_skip_delay.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_3/channel_3_stop_delay.gb", "tests/expected/samesuite/apu/channel_3/channel_3_stop_delay.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_3/channel_3_stop_div.gb", "tests/expected/samesuite/apu/channel_3/channel_3_stop_div.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_3/channel_3_wave_ram_sync.gb", "tests/expected/samesuite/apu/channel_3/channel_3_wave_ram_sync.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_4/channel_4_lfsr.gb", "tests/expected/samesuite/apu/channel_4/channel_4_lfsr.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_1/channel_1_nrx2_glitch.gb", "tests/expected/samesuite/apu/channel_1/channel_1_nrx2_glitch.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_1/channel_1_volume.gb", "tests/expected/samesuite/apu/channel_1/channel_1_volume.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_2/channel_2_nrx2_glitch.gb", "tests/expected/samesuite/apu/channel_2/channel_2_nrx2_glitch.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_1/channel_1_nrx2_speed_change.gb", "tests/expected/samesuite/apu/channel_1/channel_1_nrx2_speed_change.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_2/channel_2_nrx2_speed_change.gb", "tests/expected/samesuite/apu/channel_2/channel_2_nrx2_speed_change.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/div_write_trigger.gb", "tests/expected/samesuite/apu/div_write_trigger.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_1/channel_1_stop_div.gb", "tests/expected/samesuite/apu/channel_1/channel_1_stop_div.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_1/channel_1_volume_div.gb", "tests/expected/samesuite/apu/channel_1/channel_1_volume_div.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_2/channel_2_stop_div.gb", "tests/expected/samesuite/apu/channel_2/channel_2_stop_div.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_2/channel_2_volume_div.gb", "tests/expected/samesuite/apu/channel_2/channel_2_volume_div.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_4/channel_4_volume_div.gb", "tests/expected/samesuite/apu/channel_4/channel_4_volume_div.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/div_trigger_volume_10.gb", "tests/expected/samesuite/apu/div_trigger_volume_10.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/div_write_trigger_volume.gb", "tests/expected/samesuite/apu/div_write_trigger_volume.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/div_write_trigger_volume_10.gb", "tests/expected/samesuite/apu/div_write_trigger_volume_10.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/div_write_trigger_10.gb", "tests/expected/samesuite/apu/div_write_trigger_10.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_1/channel_1_sweep.gb", "tests/expected/samesuite/apu/channel_1/channel_1_sweep.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_1/channel_1_sweep_restart.gb", "tests/expected/samesuite/apu/channel_1/channel_1_sweep_restart.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_1/channel_1_sweep_restart_2.gb", "tests/expected/samesuite/apu/channel_1/channel_1_sweep_restart_2.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_3/channel_3_wave_ram_locked_write.gb", "tests/expected/samesuite/apu/channel_3/channel_3_wave_ram_locked_write.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_4/channel_4_align.gb", "tests/expected/samesuite/apu/channel_4/channel_4_align.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_4/channel_4_delay.gb", "tests/expected/samesuite/apu/channel_4/channel_4_delay.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_4/channel_4_equivalent_frequencies.gb", "tests/expected/samesuite/apu/channel_4/channel_4_equivalent_frequencies.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_4/channel_4_freq_change.gb", "tests/expected/samesuite/apu/channel_4/channel_4_freq_change.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_4/channel_4_frequency_alignment.gb", "tests/expected/samesuite/apu/channel_4/channel_4_frequency_alignment.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_4/channel_4_lfsr15.gb", "tests/expected/samesuite/apu/channel_4/channel_4_lfsr15.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_4/channel_4_lfsr_15_7.gb", "tests/expected/samesuite/apu/channel_4/channel_4_lfsr_15_7.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_4/channel_4_lfsr_7_15.gb", "tests/expected/samesuite/apu/channel_4/channel_4_lfsr_7_15.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_4/channel_4_lfsr_restart.gb", "tests/expected/samesuite/apu/channel_4/channel_4_lfsr_restart.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/apu/channel_4/channel_4_lfsr_restart_fast.gb", "tests/expected/samesuite/apu/channel_4/channel_4_lfsr_restart_fast.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/mooneye/acceptance/bits/unused_hwio-GS.gb", "tests/expected/mooneye/bits/unused_hwio-GS.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/misc/bits/unused_hwio-C.gb", "tests/expected/mooneye/bits/unused_hwio-C.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/mooneye/acceptance/boot_div-dmgABCmgb.gb", "tests/expected/mooneye/boot_div-dmgABCmgb.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/misc/boot_div-cgbABCDE.gb", "tests/expected/mooneye/boot_div-cgbABCDE.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/mooneye/acceptance/boot_hwio-dmgABCmgb.gb", "tests/expected/mooneye/boot_hwio-dmgABCmgb.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/misc/boot_hwio-C.gb", "tests/expected/mooneye/boot_hwio-C.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/ppu/blocking_bgpi_increase.gb", "tests/expected/samesuite/ppu/blocking_bgpi_increase.gb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/daid/speed_switch_timing_div.gbc", "tests/expected/daid/speed_switch_timing_div.gbc.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/daid/speed_switch_timing_ly.gbc", "tests/expected/daid/speed_switch_timing_ly.gbc.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/daid/speed_switch_timing_stat.gbc", "tests/expected/daid/speed_switch_timing_stat.gbc.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/daid/ppu_scanline_bgp.gb", "tests/expected/daid/ppu_scanline_bgp.gb.dmg.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/daid/ppu_scanline_bgp.gb", "tests/expected/daid/ppu_scanline_bgp.gb.cgb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/hacktix/strikethrough.gb", "tests/expected/hacktix/strikethrough.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/hacktix/bully.gb", "tests/expected/hacktix/bully.gb.dmg.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/hacktix/bully.gb", "tests/expected/hacktix/bully.gb.cgb.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/mooneye/acceptance/boot_div-dmg0.gb", "tests/expected/mooneye/boot_div-dmg0.gb.screen", bootroms.dmg0Bootrom, Model::DMG0},
    {"roms/mooneye/acceptance/boot_hwio-dmg0.gb", "tests/expected/mooneye/boot_hwio-dmg0.gb.screen", bootroms.dmg0Bootrom, Model::DMG0},
    {"roms/mooneye/acceptance/boot_div-S.gb", "tests/expected/mooneye/boot_div-S.gb.screen", bootroms.sgbBootrom, Model::SGB},
    {"roms/mooneye/acceptance/boot_div2-S.gb", "tests/expected/mooneye/boot_div2-S.gb.screen", bootroms.sgbBootrom, Model::SGB},
    {"roms/mooneye/acceptance/boot_hwio-S.gb", "tests/expected/mooneye/boot_hwio-S.gb.screen", bootroms.sgbBootrom, Model::SGB},
    {"roms/mooneye/misc/boot_div-A.gb", "tests/expected/mooneye/boot_div-A.gb.screen", bootroms.cgbAGBBootrom, Model::AGBA},
    {"roms/mooneye/misc/boot_regs-A.gb", "tests/expected/mooneye/boot_regs-A.gb.screen", bootroms.cgbAGBBootrom, Model::AGBA},
    {"roms/mooneye/misc/boot_div-cgb0.gb", "tests/expected/mooneye/boot_div-cgb0.gb.screen", bootroms.cgb0Bootrom, Model::CGB0},
    {"roms/mooneye/acceptance/boot_regs-sgb.gb", "tests/expected/mooneye/boot_regs-sgb.gb.screen", bootroms.sgbBootrom, Model::SGB},
    {"roms/mooneye/acceptance/boot_regs-sgb2.gb", "tests/expected/mooneye/boot_regs-sgb2.gb.screen", bootroms.sgb2Bootrom, Model::SGB2},
    {"roms/samesuite/sgb/command_mlt_req.gb", "tests/expected/samesuite/sgb/command_mlt_req.gb.screen", bootroms.sgbBootrom, Model::SGB},
    {"roms/samesuite/sgb/command_mlt_req_1_incrementing.gb", "tests/expected/samesuite/sgb/command_mlt_req_1_incrementing.gb.screen", bootroms.sgbBootrom, Model::SGB},
    {"roms/mooneye/acceptance/ppu/lcdon_timing-GS.gb", "tests/expected/mooneye/lcdon_timing-GS.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/ppu/lcdon_write_timing-GS.gb", "tests/expected/mooneye/lcdon_write_timing-GS.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/acid/cgb-acid-hell.gbc", "tests/expected/acid/cgb-acid-hell.gbc.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/mooneye/acceptance/ppu/intr_2_mode0_timing_sprites.gb", "tests/expected/mooneye/ppu/intr_2_mode0_timing_sprites.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/add_sp_e_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/bits/mem_oam.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/bits/reg_f.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/boot_regs-dmgABC.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/boot_regs-dmg0.gb", "", bootroms.dmg0Bootrom, Model::DMG0},
    {"roms/mooneye/acceptance/boot_regs-mgb.gb", "", bootroms.mgbBootrom, Model::MGB},
    {"roms/mooneye/acceptance/call_cc_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/call_cc_timing2.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/call_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/call_timing2.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/div_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/di_timing-GS.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/ei_sequence.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/ei_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/halt_ime0_ei.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/halt_ime0_nointr_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/halt_ime1_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/halt_ime1_timing2-GS.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/if_ie_registers.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/instr/daa.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/interrupts/ie_push.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/intr_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/jp_cc_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/jp_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/ld_hl_sp_e_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/oam_dma/basic.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/oam_dma/reg_read.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/oam_dma/sources-GS.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/oam_dma_restart.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/oam_dma_start.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/oam_dma_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/pop_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/push_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/rapid_di_ei.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/reti_intr_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/reti_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/ret_cc_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/ret_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/rst_timing.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/serial/boot_sclk_align-dmgABCmgb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/timer/div_write.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/timer/rapid_toggle.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/timer/tim00.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/timer/tim00_div_trigger.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/timer/tim01.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/timer/tim01_div_trigger.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/timer/tim10.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/timer/tim10_div_trigger.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/timer/tim11.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/timer/tim11_div_trigger.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/timer/tima_reload.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/timer/tima_write_reloading.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/acceptance/timer/tma_write_reloading.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc1/bits_bank1.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc1/bits_bank2.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc1/bits_mode.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc1/bits_ramg.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc1/multicart_rom_8Mb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc1/ram_256kb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc1/ram_64kb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc1/rom_16Mb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc1/rom_1Mb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc1/rom_2Mb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc1/rom_4Mb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc1/rom_512kb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc1/rom_8Mb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc2/bits_ramg.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc2/bits_romb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc2/bits_unused.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc2/ram.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc2/rom_1Mb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc2/rom_2Mb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc2/rom_512kb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc5/rom_16Mb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc5/rom_1Mb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc5/rom_2Mb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc5/rom_32Mb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc5/rom_4Mb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc5/rom_512kb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc5/rom_64Mb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/emulator-only/mbc5/rom_8Mb.gb", "", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/misc/boot_regs-cgb.gb", "", bootroms.cgbBootrom, Model::CGBE},
    {"roms/mooneye/acceptance/boot_div-S.gb", "", bootroms.sgb2Bootrom, Model::SGB2},
    {"roms/mooneye/acceptance/boot_div2-S.gb", "", bootroms.sgb2Bootrom, Model::SGB2},
    {"roms/acid/dmg-acid2.gb", "tests/expected/acid/dmg-acid2.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/acid/cgb-acid2.gbc", "tests/expected/acid/cgb-acid2.gbc.screen", bootroms.cgbBootrom, Model::CGBE},
    {"roms/daid/stop_instr.gb", "tests/expected/daid/stop_instr.gb.dmg.screen", bootroms.dmgBootrom, Model::DMGB, true},
    {"roms/daid/stop_instr.gb", "tests/expected/daid/stop_instr.gb.cgb.screen", bootroms.cgbBootrom, Model::CGBE, true},
    {"roms/daid/stop_instr_gbc_mode3.gb", "tests/expected/daid/stop_instr_gbc_mode3.gb.screen", bootroms.cgbBootrom, Model::CGBE, true},
    {"roms/ax6/rtc3test-1.gb", "tests/expected/ax6/rtc3test-1.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/ax6/rtc3test-2.gb", "tests/expected/ax6/rtc3test-2.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/ax6/rtc3test-3.gb", "tests/expected/ax6/rtc3test-3.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/samesuite/dma/gbc_dma_cont.gb", "", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/dma/gdma_addr_mask.gb", "", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/dma/hdma_lcd_off.gb", "", bootroms.cgbBootrom, Model::CGBE},
    {"roms/samesuite/dma/hdma_mode0.gb", "", bootroms.cgbBootrom, Model::CGBE},
    {"roms/cpp/rtc-invalid-banks-test.gb", "tests/expected/cpp/rtc-invalid-banks-test.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/cpp/latch-rtc-test.gb", "tests/expected/cpp/latch-rtc-test.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/cpp/ramg-mbc3-test.gb", "tests/expected/cpp/ramg-mbc3-test.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mbc3-tester/mbc3-tester.gb", "tests/expected/mbc3-tester/mbc3-tester.gb.screen", bootroms.dmgBootrom, Model::DMGB},
    {"roms/mooneye/manual-only/sprite_priority.gb", "tests/expected/mooneye/manual-only/sprite_priority.gb.screen", bootroms.dmgBootrom, Model::DMGB},
};

// Model-specific ROMs are exercised on each documented silicon revision.
// A ROM's suffix describes its expected targets, not the mode to force on it.
static std::vector<Case> modelRomCases() {
    std::vector<Case> cases;
    const auto add = [&](Model model, const std::string &bios, const std::string &rom) {
        cases.push_back({"roms/" + rom, "", bios, model});
    };
    for (unsigned i = 1; i < ModelNames.size(); ++i) {
        const auto model = static_cast<Model>(i);
        std::string bios;
        if (model == Model::DMG0) bios = bootroms.dmg0Bootrom;
        else if (model == Model::MGB) bios = bootroms.mgbBootrom;
        else if (model == Model::SGB) bios = bootroms.sgbBootrom;
        else if (model == Model::SGB2) bios = bootroms.sgb2Bootrom;
        else if (IsDmg(model)) bios = bootroms.dmgBootrom;
        else if (model == Model::CGB0) bios = bootroms.cgb0Bootrom;
        else if (model == Model::CGBE) bios = bootroms.cgbEBootrom;
        else if (model == Model::AGB0) bios = bootroms.cgbAGB0Bootrom;
        else if (IsAgb(model)) bios = bootroms.cgbAGBBootrom;
        else bios = bootroms.cgbBootrom;

        if (IsDmg(model)) {
            const std::string registers = model == Model::DMG0 ? "dmg0" : model == Model::MGB ? "mgb" :
                model == Model::SGB ? "sgb" : model == Model::SGB2 ? "sgb2" : "dmgABC";
            const std::string io = model == Model::DMG0 ? "dmg0" : IsSgb(model) ? "S" : "dmgABCmgb";
            add(model, bios, "mooneye/acceptance/boot_regs-" + registers + ".gb");
            add(model, bios, "mooneye/acceptance/boot_hwio-" + io + ".gb");
            add(model, bios, "mooneye/acceptance/boot_div-" + io + ".gb");
            if (IsSgb(model)) add(model, bios, "mooneye/acceptance/boot_div2-S.gb");
            for (const auto rom : {"di_timing-GS", "halt_ime0_nointr_timing", "halt_ime1_timing2-GS"}) {
                add(model, bios, std::string("mooneye/acceptance/") + rom + ".gb");
            }
        } else {
            add(model, bios, IsAgb(model) ? "mooneye/misc/boot_regs-A.gb" : "mooneye/misc/boot_regs-cgb.gb");
            add(model, bios, "mooneye/misc/boot_hwio-C.gb");
            add(model, bios, IsAgb(model) ? "mooneye/misc/boot_div-A.gb" : model == Model::CGB0 ?
                "mooneye/misc/boot_div-cgb0.gb" : "mooneye/misc/boot_div-cgbABCDE.gb");
        }
        if (model == Model::CGB0 || model == Model::CGBB || model == Model::CGBC) {
            add(model, bios, "samesuite/apu/channel_1/channel_1_freq_change_timing-cgb0BC.gb");
        } else if (model == Model::CGBD || model == Model::CGBE) {
            add(model, bios, "samesuite/apu/channel_1/channel_1_freq_change_timing-cgbDE.gb");
        } else if (IsAgb(model)) {
            add(model, bios, "samesuite/apu/channel_1/channel_1_freq_change_timing-A.gb");
        }
        // SameSuite has measured expectations for CGB-0 and CGB-B. CGB-A
        // shares the early circuit, but is covered by component tests below.
        if (model == Model::CGB0 || model == Model::CGBB) {
            for (const auto channel : {"1", "2", "4"}) {
                const std::string base = std::string("samesuite/apu/channel_") + channel + "/channel_" + channel;
                add(model, bios, base + "_extra_length_clocking-cgb0B.gb");
            }
            add(model, bios, model == Model::CGB0 ?
                "samesuite/apu/channel_3/channel_3_extra_length_clocking-cgb0.gb" :
                "samesuite/apu/channel_3/channel_3_extra_length_clocking-cgbB.gb");
        }
    }
    const auto externalBootCases = cases;
    for (const auto &tc : externalBootCases) {
        if (tc.rom.find("/boot_regs-") == std::string::npos) continue;
        auto embedded = tc;
        embedded.bios.clear();
        cases.push_back(std::move(embedded));
    }
    return cases;
}

TEST_CASE("model ROMs: Mooneye startup and HALT, SameSuite early CGB audio") {
    const auto cases = modelRomCases();
    std::vector<std::future<bool>> results;
    for (const auto &tc : cases) {
        results.push_back(std::async(std::launch::async, [tc] {
            return runRomTest(tc.rom, tc.expected, tc.bios, tc.model, false);
        }));
    }
    for (unsigned i = 0; i < cases.size(); ++i) {
        CHECK_MESSAGE(results[i].get(), cases[i].rom, " [", ModelName(cases[i].model), ", ", cases[i].bios.empty() ? "built-in startup" : cases[i].bios, "]");
    }
}

static auto &romFutures() {
    using SF = std::shared_future<bool>;
    static std::vector<SF> futures = [] {
        std::vector<SF> tmp;
        tmp.reserve(romTestcases.size());

        for (const auto &tc: romTestcases) {
            tmp.emplace_back(
                std::async(parallelRomTests ? std::launch::async : std::launch::deferred, [tc] {
                    return runRomTest(tc.rom, tc.expected, tc.bios, tc.model, tc.requiresStop);
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
ROM_TEST(64, "roms/mealybug-tearoom-tests/ppu/m3_window_timing.gb")
ROM_TEST(65, "roms/mealybug-tearoom-tests/ppu/m3_window_timing_wx_0.gb")
ROM_TEST(66, "roms/mealybug-tearoom-tests/ppu/m3_scx_high_5_bits.gb")
ROM_TEST(67, "roms/mealybug-tearoom-tests/ppu/m3_obp0_change.gb")
ROM_TEST(68, "roms/mealybug-tearoom-tests/ppu/m3_lcdc_win_map_change.gb")
ROM_TEST(69, "roms/mealybug-tearoom-tests/ppu/m3_lcdc_tile_sel_change.gb")
ROM_TEST(70, "roms/mealybug-tearoom-tests/ppu/m3_lcdc_tile_sel_win_change.gb")
ROM_TEST(71, "roms/mealybug-tearoom-tests/ppu/m3_scy_change.gb")
ROM_TEST(72, "roms/mealybug-tearoom-tests/ppu/m3_lcdc_win_en_change_multiple.gb")
ROM_TEST(73, "roms/mealybug-tearoom-tests/ppu/m3_lcdc_win_en_change_multiple_wx.gb")
ROM_TEST(74, "roms/mealybug-tearoom-tests/ppu/m3_wx_6_change.gb")
ROM_TEST(75, "roms/mooneye/acceptance/ppu/hblank_ly_scx_timing-GS.gb")
ROM_TEST(76, "roms/mooneye/acceptance/ppu/intr_1_2_timing-GS.gb")
ROM_TEST(77, "roms/mooneye/acceptance/ppu/intr_2_mode0_timing.gb")
ROM_TEST(78, "roms/mooneye/acceptance/ppu/intr_2_mode3_timing.gb")
ROM_TEST(79, "roms/mooneye/acceptance/ppu/intr_2_oam_ok_timing.gb")
ROM_TEST(80, "roms/mooneye/acceptance/ppu/vblank_stat_intr-GS.gb")
ROM_TEST(81, "roms/mooneye/acceptance/ppu/intr_2_0_timing.gb")
ROM_TEST(82, "roms/mooneye/acceptance/ppu/stat_lyc_onoff.gb")
ROM_TEST(83, "roms/mooneye/acceptance/ppu/stat_irq_blocking.gb")
ROM_TEST(84, "roms/mooneye/misc/ppu/vblank_stat_intr-C.gb")
ROM_TEST(85, "roms/samesuite/apu/channel_1/channel_1_align.gb")
ROM_TEST(86, "roms/samesuite/apu/channel_1/channel_1_align_cpu.gb")
ROM_TEST(87, "roms/samesuite/apu/channel_1/channel_1_delay.gb")
ROM_TEST(88, "roms/samesuite/apu/channel_1/channel_1_duty.gb")
ROM_TEST(89, "roms/samesuite/apu/channel_1/channel_1_duty_delay.gb")
ROM_TEST(90, "roms/samesuite/apu/channel_1/channel_1_freq_change.gb")
ROM_TEST(91, "roms/samesuite/apu/channel_1/channel_1_freq_change_timing-cgbDE.gb")
ROM_TEST(92, "roms/samesuite/apu/channel_1/channel_1_restart.gb")
ROM_TEST(93, "roms/samesuite/apu/channel_1/channel_1_restart_nrx2_glitch.gb")
ROM_TEST(94, "roms/samesuite/apu/channel_1/channel_1_stop_restart.gb")
ROM_TEST(95, "roms/samesuite/apu/channel_2/channel_2_align.gb")
ROM_TEST(96, "roms/samesuite/apu/channel_2/channel_2_align_cpu.gb")
ROM_TEST(97, "roms/samesuite/apu/channel_2/channel_2_delay.gb")
ROM_TEST(98, "roms/samesuite/apu/channel_2/channel_2_duty.gb")
ROM_TEST(99, "roms/samesuite/apu/channel_2/channel_2_duty_delay.gb")
ROM_TEST(100, "roms/samesuite/apu/channel_2/channel_2_freq_change.gb")
ROM_TEST(101, "roms/samesuite/apu/channel_2/channel_2_restart.gb")
ROM_TEST(102, "roms/samesuite/apu/channel_2/channel_2_restart_nrx2_glitch.gb")
ROM_TEST(103, "roms/samesuite/apu/channel_2/channel_2_stop_restart.gb")
ROM_TEST(104, "roms/samesuite/apu/channel_2/channel_2_volume.gb")
ROM_TEST(105, "roms/samesuite/apu/channel_3/channel_3_and_glitch.gb")
ROM_TEST(106, "roms/samesuite/apu/channel_3/channel_3_delay.gb")
ROM_TEST(107, "roms/samesuite/apu/channel_3/channel_3_first_sample.gb")
ROM_TEST(108, "roms/samesuite/apu/channel_3/channel_3_freq_change_delay.gb")
ROM_TEST(109, "roms/samesuite/apu/channel_3/channel_3_restart_delay.gb")
ROM_TEST(110, "roms/samesuite/apu/channel_3/channel_3_restart_during_delay.gb")
ROM_TEST(111, "roms/samesuite/apu/channel_3/channel_3_restart_stop_delay.gb")
ROM_TEST(112, "roms/samesuite/apu/channel_3/channel_3_shift_delay.gb")
ROM_TEST(113, "roms/samesuite/apu/channel_3/channel_3_shift_skip_delay.gb")
ROM_TEST(114, "roms/samesuite/apu/channel_3/channel_3_stop_delay.gb")
ROM_TEST(115, "roms/samesuite/apu/channel_3/channel_3_stop_div.gb")
ROM_TEST(116, "roms/samesuite/apu/channel_3/channel_3_wave_ram_sync.gb")
ROM_TEST(117, "roms/samesuite/apu/channel_4/channel_4_lfsr.gb")
ROM_TEST(118, "roms/samesuite/apu/channel_1/channel_1_nrx2_glitch.gb")
ROM_TEST(119, "roms/samesuite/apu/channel_1/channel_1_volume.gb")
ROM_TEST(120, "roms/samesuite/apu/channel_2/channel_2_nrx2_glitch.gb")
ROM_TEST(121, "roms/samesuite/apu/channel_1/channel_1_nrx2_speed_change.gb")
ROM_TEST(122, "roms/samesuite/apu/channel_2/channel_2_nrx2_speed_change.gb")
ROM_TEST(123, "roms/samesuite/apu/div_write_trigger.gb")
ROM_TEST(124, "roms/samesuite/apu/channel_1/channel_1_stop_div.gb")
ROM_TEST(125, "roms/samesuite/apu/channel_1/channel_1_volume_div.gb")
ROM_TEST(126, "roms/samesuite/apu/channel_2/channel_2_stop_div.gb")
ROM_TEST(127, "roms/samesuite/apu/channel_2/channel_2_volume_div.gb")
ROM_TEST(128, "roms/samesuite/apu/channel_4/channel_4_volume_div.gb")
ROM_TEST(129, "roms/samesuite/apu/div_trigger_volume_10.gb")
ROM_TEST(130, "roms/samesuite/apu/div_write_trigger_volume.gb")
ROM_TEST(131, "roms/samesuite/apu/div_write_trigger_volume_10.gb")
ROM_TEST(132, "roms/samesuite/apu/div_write_trigger_10.gb")
ROM_TEST(133, "roms/samesuite/apu/channel_1/channel_1_sweep.gb")
ROM_TEST(134, "roms/samesuite/apu/channel_1/channel_1_sweep_restart.gb")
ROM_TEST(135, "roms/samesuite/apu/channel_1/channel_1_sweep_restart_2.gb")
ROM_TEST(136, "roms/samesuite/apu/channel_3/channel_3_wave_ram_locked_write.gb")
ROM_TEST(137, "roms/samesuite/apu/channel_4/channel_4_align.gb")
ROM_TEST(138, "roms/samesuite/apu/channel_4/channel_4_delay.gb")
ROM_TEST(139, "roms/samesuite/apu/channel_4/channel_4_equivalent_frequencies.gb")
ROM_TEST(140, "roms/samesuite/apu/channel_4/channel_4_freq_change.gb")
ROM_TEST(141, "roms/samesuite/apu/channel_4/channel_4_frequency_alignment.gb")
ROM_TEST(142, "roms/samesuite/apu/channel_4/channel_4_lfsr15.gb")
ROM_TEST(143, "roms/samesuite/apu/channel_4/channel_4_lfsr_15_7.gb")
ROM_TEST(144, "roms/samesuite/apu/channel_4/channel_4_lfsr_7_15.gb")
ROM_TEST(145, "roms/samesuite/apu/channel_4/channel_4_lfsr_restart.gb")
ROM_TEST(146, "roms/samesuite/apu/channel_4/channel_4_lfsr_restart_fast.gb")
ROM_TEST(147, "roms/mooneye/acceptance/bits/unused_hwio-GS.gb")
ROM_TEST(148, "roms/mooneye/misc/bits/unused_hwio-C.gb")
ROM_TEST(149, "roms/mooneye/acceptance/boot_div-dmgABCmgb.gb")
ROM_TEST(150, "roms/mooneye/misc/boot_div-cgbABCDE.gb")
ROM_TEST(151, "roms/mooneye/acceptance/boot_hwio-dmgABCmgb.gb")
ROM_TEST(152, "roms/mooneye/misc/boot_hwio-C.gb")
ROM_TEST(153, "roms/samesuite/ppu/blocking_bgpi_increase.gb")
ROM_TEST(154, "roms/daid/speed_switch_timing_div.gbc")
ROM_TEST(155, "roms/daid/speed_switch_timing_ly.gbc")
ROM_TEST(156, "roms/daid/speed_switch_timing_stat.gbc")
ROM_TEST(157, "roms/daid/ppu_scanline_bgp.gb (DMG)")
ROM_TEST(158, "roms/daid/ppu_scanline_bgp.gb (GBC)")
ROM_TEST(159, "roms/hacktix/strikethrough.gb")
ROM_TEST(160, "roms/hacktix/bully.gb (DMG)")
ROM_TEST(161, "roms/hacktix/bully.gb (GBC)")
ROM_TEST(162, "roms/mooneye/acceptance/boot_div-dmg0.gb")
ROM_TEST(163, "roms/mooneye/acceptance/boot_hwio-dmg0.gb")
ROM_TEST(164, "roms/mooneye/acceptance/boot_div-S.gb")
ROM_TEST(165, "roms/mooneye/acceptance/boot_div2-S.gb")
ROM_TEST(166, "roms/mooneye/acceptance/boot_hwio-S.gb")
ROM_TEST(167, "roms/mooneye/misc/boot_div-A.gb")
ROM_TEST(168, "roms/mooneye/misc/boot_regs-A.gb")
ROM_TEST(169, "roms/mooneye/misc/boot_div-cgb0.gb")
ROM_TEST(170, "roms/mooneye/acceptance/boot_regs-sgb.gb")
ROM_TEST(171, "roms/mooneye/acceptance/boot_regs-sgb2.gb")
ROM_TEST(172, "roms/samesuite/sgb/command_mlt_req.gb")
ROM_TEST(173, "roms/samesuite/sgb/command_mlt_req_1_incrementing.gb")
ROM_TEST(174, "roms/mooneye/acceptance/ppu/lcdon_timing-GS.gb")
ROM_TEST(175, "roms/mooneye/acceptance/ppu/lcdon_write_timing-GS.gb")
ROM_TEST(176, "roms/acid/cgb-acid-hell.gbc")
ROM_TEST(177, "roms/mooneye/acceptance/ppu/intr_2_mode0_timing_sprites.gb")


ROM_TEST(178, "roms/mooneye/acceptance/add_sp_e_timing.gb")
ROM_TEST(179, "roms/mooneye/acceptance/bits/mem_oam.gb")
ROM_TEST(180, "roms/mooneye/acceptance/bits/reg_f.gb")
ROM_TEST(181, "roms/mooneye/acceptance/boot_regs-dmgABC.gb")
ROM_TEST(182, "roms/mooneye/acceptance/boot_regs-dmg0.gb")
ROM_TEST(183, "roms/mooneye/acceptance/boot_regs-mgb.gb")
ROM_TEST(184, "roms/mooneye/acceptance/call_cc_timing.gb")
ROM_TEST(185, "roms/mooneye/acceptance/call_cc_timing2.gb")
ROM_TEST(186, "roms/mooneye/acceptance/call_timing.gb")
ROM_TEST(187, "roms/mooneye/acceptance/call_timing2.gb")
ROM_TEST(188, "roms/mooneye/acceptance/div_timing.gb")
ROM_TEST(189, "roms/mooneye/acceptance/di_timing-GS.gb")
ROM_TEST(190, "roms/mooneye/acceptance/ei_sequence.gb")
ROM_TEST(191, "roms/mooneye/acceptance/ei_timing.gb")
ROM_TEST(192, "roms/mooneye/acceptance/halt_ime0_ei.gb")
ROM_TEST(193, "roms/mooneye/acceptance/halt_ime0_nointr_timing.gb")
ROM_TEST(194, "roms/mooneye/acceptance/halt_ime1_timing.gb")
ROM_TEST(195, "roms/mooneye/acceptance/halt_ime1_timing2-GS.gb")
ROM_TEST(196, "roms/mooneye/acceptance/if_ie_registers.gb")
ROM_TEST(197, "roms/mooneye/acceptance/instr/daa.gb")
ROM_TEST(198, "roms/mooneye/acceptance/interrupts/ie_push.gb")
ROM_TEST(199, "roms/mooneye/acceptance/intr_timing.gb")
ROM_TEST(200, "roms/mooneye/acceptance/jp_cc_timing.gb")
ROM_TEST(201, "roms/mooneye/acceptance/jp_timing.gb")
ROM_TEST(202, "roms/mooneye/acceptance/ld_hl_sp_e_timing.gb")
ROM_TEST(203, "roms/mooneye/acceptance/oam_dma/basic.gb")
ROM_TEST(204, "roms/mooneye/acceptance/oam_dma/reg_read.gb")
ROM_TEST(205, "roms/mooneye/acceptance/oam_dma/sources-GS.gb")
ROM_TEST(206, "roms/mooneye/acceptance/oam_dma_restart.gb")
ROM_TEST(207, "roms/mooneye/acceptance/oam_dma_start.gb")
ROM_TEST(208, "roms/mooneye/acceptance/oam_dma_timing.gb")
ROM_TEST(209, "roms/mooneye/acceptance/pop_timing.gb")
ROM_TEST(210, "roms/mooneye/acceptance/push_timing.gb")
ROM_TEST(211, "roms/mooneye/acceptance/rapid_di_ei.gb")
ROM_TEST(212, "roms/mooneye/acceptance/reti_intr_timing.gb")
ROM_TEST(213, "roms/mooneye/acceptance/reti_timing.gb")
ROM_TEST(214, "roms/mooneye/acceptance/ret_cc_timing.gb")
ROM_TEST(215, "roms/mooneye/acceptance/ret_timing.gb")
ROM_TEST(216, "roms/mooneye/acceptance/rst_timing.gb")
ROM_TEST(217, "roms/mooneye/acceptance/serial/boot_sclk_align-dmgABCmgb.gb")
ROM_TEST(218, "roms/mooneye/acceptance/timer/div_write.gb")
ROM_TEST(219, "roms/mooneye/acceptance/timer/rapid_toggle.gb")
ROM_TEST(220, "roms/mooneye/acceptance/timer/tim00.gb")
ROM_TEST(221, "roms/mooneye/acceptance/timer/tim00_div_trigger.gb")
ROM_TEST(222, "roms/mooneye/acceptance/timer/tim01.gb")
ROM_TEST(223, "roms/mooneye/acceptance/timer/tim01_div_trigger.gb")
ROM_TEST(224, "roms/mooneye/acceptance/timer/tim10.gb")
ROM_TEST(225, "roms/mooneye/acceptance/timer/tim10_div_trigger.gb")
ROM_TEST(226, "roms/mooneye/acceptance/timer/tim11.gb")
ROM_TEST(227, "roms/mooneye/acceptance/timer/tim11_div_trigger.gb")
ROM_TEST(228, "roms/mooneye/acceptance/timer/tima_reload.gb")
ROM_TEST(229, "roms/mooneye/acceptance/timer/tima_write_reloading.gb")
ROM_TEST(230, "roms/mooneye/acceptance/timer/tma_write_reloading.gb")
ROM_TEST(231, "roms/mooneye/emulator-only/mbc1/bits_bank1.gb")
ROM_TEST(232, "roms/mooneye/emulator-only/mbc1/bits_bank2.gb")
ROM_TEST(233, "roms/mooneye/emulator-only/mbc1/bits_mode.gb")
ROM_TEST(234, "roms/mooneye/emulator-only/mbc1/bits_ramg.gb")
ROM_TEST(235, "roms/mooneye/emulator-only/mbc1/multicart_rom_8Mb.gb")
ROM_TEST(236, "roms/mooneye/emulator-only/mbc1/ram_256kb.gb")
ROM_TEST(237, "roms/mooneye/emulator-only/mbc1/ram_64kb.gb")
ROM_TEST(238, "roms/mooneye/emulator-only/mbc1/rom_16Mb.gb")
ROM_TEST(239, "roms/mooneye/emulator-only/mbc1/rom_1Mb.gb")
ROM_TEST(240, "roms/mooneye/emulator-only/mbc1/rom_2Mb.gb")
ROM_TEST(241, "roms/mooneye/emulator-only/mbc1/rom_4Mb.gb")
ROM_TEST(242, "roms/mooneye/emulator-only/mbc1/rom_512kb.gb")
ROM_TEST(243, "roms/mooneye/emulator-only/mbc1/rom_8Mb.gb")
ROM_TEST(244, "roms/mooneye/emulator-only/mbc2/bits_ramg.gb")
ROM_TEST(245, "roms/mooneye/emulator-only/mbc2/bits_romb.gb")
ROM_TEST(246, "roms/mooneye/emulator-only/mbc2/bits_unused.gb")
ROM_TEST(247, "roms/mooneye/emulator-only/mbc2/ram.gb")
ROM_TEST(248, "roms/mooneye/emulator-only/mbc2/rom_1Mb.gb")
ROM_TEST(249, "roms/mooneye/emulator-only/mbc2/rom_2Mb.gb")
ROM_TEST(250, "roms/mooneye/emulator-only/mbc2/rom_512kb.gb")
ROM_TEST(251, "roms/mooneye/emulator-only/mbc5/rom_16Mb.gb")
ROM_TEST(252, "roms/mooneye/emulator-only/mbc5/rom_1Mb.gb")
ROM_TEST(253, "roms/mooneye/emulator-only/mbc5/rom_2Mb.gb")
ROM_TEST(254, "roms/mooneye/emulator-only/mbc5/rom_32Mb.gb")
ROM_TEST(255, "roms/mooneye/emulator-only/mbc5/rom_4Mb.gb")
ROM_TEST(256, "roms/mooneye/emulator-only/mbc5/rom_512kb.gb")
ROM_TEST(257, "roms/mooneye/emulator-only/mbc5/rom_64Mb.gb")
ROM_TEST(258, "roms/mooneye/emulator-only/mbc5/rom_8Mb.gb")
ROM_TEST(259, "roms/mooneye/misc/boot_regs-cgb.gb")
ROM_TEST(260, "roms/mooneye/acceptance/boot_div-S.gb (SGB2)")
ROM_TEST(261, "roms/mooneye/acceptance/boot_div2-S.gb (SGB2)")

ROM_TEST(262, "roms/acid/dmg-acid2.gb")
ROM_TEST(263, "roms/acid/cgb-acid2.gbc")
ROM_TEST(264, "roms/daid/stop_instr.gb (DMG)")
ROM_TEST(265, "roms/daid/stop_instr.gb (CGB)")
ROM_TEST(266, "roms/daid/stop_instr_gbc_mode3.gb")
ROM_TEST(267, "roms/ax6/rtc3test-1.gb")
ROM_TEST(268, "roms/ax6/rtc3test-2.gb")
ROM_TEST(269, "roms/ax6/rtc3test-3.gb")
ROM_TEST(270, "roms/samesuite/dma/gbc_dma_cont.gb")
ROM_TEST(271, "roms/samesuite/dma/gdma_addr_mask.gb")
ROM_TEST(272, "roms/samesuite/dma/hdma_lcd_off.gb")
ROM_TEST(273, "roms/samesuite/dma/hdma_mode0.gb")
ROM_TEST(274, "roms/cpp/rtc-invalid-banks-test.gb")
ROM_TEST(275, "roms/cpp/latch-rtc-test.gb")
ROM_TEST(276, "roms/cpp/ramg-mbc3-test.gb")
ROM_TEST(277, "roms/mbc3-tester/mbc3-tester.gb")
ROM_TEST(278, "roms/mooneye/manual-only/sprite_priority.gb")

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
        } else if (constexpr std::string_view kFrames = "--frames="; arg.rfind(kFrames, 0) == 0) {
            try {
                size_t consumed = 0;
                const auto value = std::stoul(std::string(arg.substr(kFrames.size())), &consumed);
                if (consumed != arg.size() - kFrames.size() || value == 0 || value > 1'000'000) {
                    throw std::invalid_argument("frames out of range");
                }
                romFrames = static_cast<unsigned>(value);
            } catch (...) {
                std::cerr << "Invalid value for --frames (expected 1..1000000)" << std::endl;
                return EXIT_FAILURE;
            }
        } else {
            doctest_args.push_back(argv[i]);
        }
    }

    if (maxThreads == 0) { maxThreads = 1; }
    threadSemaphore = std::make_shared<std::counting_semaphore<> >(maxThreads);
    parallelRomTests = doctest_args.size() == 1;

    doctest::Context ctx;
    ctx.applyCommandLine(static_cast<int>(doctest_args.size()), doctest_args.data());
    return ctx.run();
}

#endif //STARGBC_TESTROMS_H
