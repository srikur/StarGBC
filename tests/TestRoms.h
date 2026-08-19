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

const Bootroms bootroms{};

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

        const auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < 10s) {
            gameboy->RunFrame();
        }

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
    {"roms/mealybug-tearoom-tests/ppu/m3_window_timing.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_window_timing.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_window_timing_wx_0.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_window_timing_wx_0.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_scx_high_5_bits.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_scx_high_5_bits.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_obp0_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_obp0_change.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_win_map_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_win_map_change.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_tile_sel_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_tile_sel_change.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_tile_sel_win_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_tile_sel_win_change.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_scy_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_scy_change.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_win_en_change_multiple.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_win_en_change_multiple.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_lcdc_win_en_change_multiple_wx.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_lcdc_win_en_change_multiple_wx.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mealybug-tearoom-tests/ppu/m3_wx_6_change.gb", "tests/expected/mealybug-tearoom-tests/ppu/m3_wx_6_change.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mooneye/acceptance/ppu/hblank_ly_scx_timing-GS.gb", "tests/expected/mooneye/ppu/hblank_ly_scx_timing-GS.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mooneye/acceptance/ppu/intr_1_2_timing-GS.gb", "tests/expected/mooneye/ppu/intr_1_2_timing-GS.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mooneye/acceptance/ppu/intr_2_mode0_timing.gb", "tests/expected/mooneye/ppu/intr_2_mode0_timing.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mooneye/acceptance/ppu/intr_2_mode3_timing.gb", "tests/expected/mooneye/ppu/intr_2_mode3_timing.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mooneye/acceptance/ppu/intr_2_oam_ok_timing.gb", "tests/expected/mooneye/ppu/intr_2_oam_ok_timing.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mooneye/acceptance/ppu/vblank_stat_intr-GS.gb", "tests/expected/mooneye/ppu/vblank_stat_intr-GS.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mooneye/acceptance/ppu/intr_2_0_timing.gb", "tests/expected/mooneye/ppu/intr_2_0_timing.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mooneye/acceptance/ppu/stat_lyc_onoff.gb", "tests/expected/mooneye/ppu/stat_lyc_onoff.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mooneye/acceptance/ppu/stat_irq_blocking.gb", "tests/expected/mooneye/ppu/stat_irq_blocking.gb.screen", bootroms.dmgBootrom, Mode::DMG},
    {"roms/mooneye/misc/ppu/vblank_stat_intr-C.gb", "tests/expected/mooneye/ppu/vblank_stat_intr-C.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_1/channel_1_align.gb", "tests/expected/samesuite/apu/channel_1/channel_1_align.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_1/channel_1_align_cpu.gb", "tests/expected/samesuite/apu/channel_1/channel_1_align_cpu.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_1/channel_1_delay.gb", "tests/expected/samesuite/apu/channel_1/channel_1_delay.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_1/channel_1_duty.gb", "tests/expected/samesuite/apu/channel_1/channel_1_duty.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_1/channel_1_duty_delay.gb", "tests/expected/samesuite/apu/channel_1/channel_1_duty_delay.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_1/channel_1_freq_change.gb", "tests/expected/samesuite/apu/channel_1/channel_1_freq_change.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_1/channel_1_freq_change_timing-cgbDE.gb", "tests/expected/samesuite/apu/channel_1/channel_1_freq_change_timing-cgbDE.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_1/channel_1_restart.gb", "tests/expected/samesuite/apu/channel_1/channel_1_restart.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_1/channel_1_restart_nrx2_glitch.gb", "tests/expected/samesuite/apu/channel_1/channel_1_restart_nrx2_glitch.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_1/channel_1_stop_restart.gb", "tests/expected/samesuite/apu/channel_1/channel_1_stop_restart.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_2/channel_2_align.gb", "tests/expected/samesuite/apu/channel_2/channel_2_align.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_2/channel_2_align_cpu.gb", "tests/expected/samesuite/apu/channel_2/channel_2_align_cpu.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_2/channel_2_delay.gb", "tests/expected/samesuite/apu/channel_2/channel_2_delay.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_2/channel_2_duty.gb", "tests/expected/samesuite/apu/channel_2/channel_2_duty.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_2/channel_2_duty_delay.gb", "tests/expected/samesuite/apu/channel_2/channel_2_duty_delay.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_2/channel_2_freq_change.gb", "tests/expected/samesuite/apu/channel_2/channel_2_freq_change.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_2/channel_2_restart.gb", "tests/expected/samesuite/apu/channel_2/channel_2_restart.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_2/channel_2_restart_nrx2_glitch.gb", "tests/expected/samesuite/apu/channel_2/channel_2_restart_nrx2_glitch.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_2/channel_2_stop_restart.gb", "tests/expected/samesuite/apu/channel_2/channel_2_stop_restart.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_2/channel_2_volume.gb", "tests/expected/samesuite/apu/channel_2/channel_2_volume.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_3/channel_3_and_glitch.gb", "tests/expected/samesuite/apu/channel_3/channel_3_and_glitch.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_3/channel_3_delay.gb", "tests/expected/samesuite/apu/channel_3/channel_3_delay.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_3/channel_3_first_sample.gb", "tests/expected/samesuite/apu/channel_3/channel_3_first_sample.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_3/channel_3_freq_change_delay.gb", "tests/expected/samesuite/apu/channel_3/channel_3_freq_change_delay.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_3/channel_3_restart_delay.gb", "tests/expected/samesuite/apu/channel_3/channel_3_restart_delay.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_3/channel_3_restart_during_delay.gb", "tests/expected/samesuite/apu/channel_3/channel_3_restart_during_delay.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_3/channel_3_restart_stop_delay.gb", "tests/expected/samesuite/apu/channel_3/channel_3_restart_stop_delay.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_3/channel_3_shift_delay.gb", "tests/expected/samesuite/apu/channel_3/channel_3_shift_delay.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_3/channel_3_shift_skip_delay.gb", "tests/expected/samesuite/apu/channel_3/channel_3_shift_skip_delay.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_3/channel_3_stop_delay.gb", "tests/expected/samesuite/apu/channel_3/channel_3_stop_delay.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_3/channel_3_stop_div.gb", "tests/expected/samesuite/apu/channel_3/channel_3_stop_div.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_3/channel_3_wave_ram_sync.gb", "tests/expected/samesuite/apu/channel_3/channel_3_wave_ram_sync.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_4/channel_4_lfsr.gb", "tests/expected/samesuite/apu/channel_4/channel_4_lfsr.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_1/channel_1_nrx2_glitch.gb", "tests/expected/samesuite/apu/channel_1/channel_1_nrx2_glitch.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_1/channel_1_volume.gb", "tests/expected/samesuite/apu/channel_1/channel_1_volume.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_2/channel_2_nrx2_glitch.gb", "tests/expected/samesuite/apu/channel_2/channel_2_nrx2_glitch.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_1/channel_1_nrx2_speed_change.gb", "tests/expected/samesuite/apu/channel_1/channel_1_nrx2_speed_change.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_2/channel_2_nrx2_speed_change.gb", "tests/expected/samesuite/apu/channel_2/channel_2_nrx2_speed_change.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/div_write_trigger.gb", "tests/expected/samesuite/apu/div_write_trigger.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_1/channel_1_stop_div.gb", "tests/expected/samesuite/apu/channel_1/channel_1_stop_div.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_1/channel_1_volume_div.gb", "tests/expected/samesuite/apu/channel_1/channel_1_volume_div.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_2/channel_2_stop_div.gb", "tests/expected/samesuite/apu/channel_2/channel_2_stop_div.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_2/channel_2_volume_div.gb", "tests/expected/samesuite/apu/channel_2/channel_2_volume_div.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_4/channel_4_volume_div.gb", "tests/expected/samesuite/apu/channel_4/channel_4_volume_div.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/div_trigger_volume_10.gb", "tests/expected/samesuite/apu/div_trigger_volume_10.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/div_write_trigger_volume.gb", "tests/expected/samesuite/apu/div_write_trigger_volume.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/div_write_trigger_volume_10.gb", "tests/expected/samesuite/apu/div_write_trigger_volume_10.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/div_write_trigger_10.gb", "tests/expected/samesuite/apu/div_write_trigger_10.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_1/channel_1_sweep.gb", "tests/expected/samesuite/apu/channel_1/channel_1_sweep.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_1/channel_1_sweep_restart.gb", "tests/expected/samesuite/apu/channel_1/channel_1_sweep_restart.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_1/channel_1_sweep_restart_2.gb", "tests/expected/samesuite/apu/channel_1/channel_1_sweep_restart_2.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
    {"roms/samesuite/apu/channel_3/channel_3_wave_ram_locked_write.gb", "tests/expected/samesuite/apu/channel_3/channel_3_wave_ram_locked_write.gb.screen", bootroms.cgbBootrom, Mode::CGB_GBC},
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