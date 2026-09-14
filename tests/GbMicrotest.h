#ifndef STARGBC_GBMICROTEST_H
#define STARGBC_GBMICROTEST_H

#include "TestRoms.h"
#include "GbMicrotestDiagnostics.h"
#include <array>
#include <sstream>

// https://github.com/aappleby/GBMicrotest
//   0xFF80 = actual value, 0xFF81 = expected value,
//   0xFF82 = 0x01 on pass, 0xFF on fail

struct GbMicrotestResult {
    bool passed;
    std::string details;
};

static GbMicrotestResult runGbMicrotest(const std::string &rom) {
    ThreadPermit _permit;
    try {
        const auto gameboy = Gameboy::init({
            .romName = rom,
            .biosPath = bootroms.dmgBootrom,
            .model = Model::DMGB,
        });

        const auto &hram = gameboy->[:testGameboyMember("memory_"):].hram_;
        const auto &bus = gameboy->[:testGameboyMember("bus_"):];
        std::optional<MicrotestDiagnosticCheck> diagnostic;
        for (const auto &test : microtestDiagnostics) {
            if (rom == "roms/gbmicrotest/" + std::string(test.name) + ".gb") {
                diagnostic.emplace(test);
                break;
            }
        }
        const auto result = [&](const unsigned frames) -> GbMicrotestResult {
            if (diagnostic) {
                std::ostringstream details;
                details << rom << " [dmgb] after " << frames << " frames";
                const auto passed = diagnostic->passed(*gameboy, details);
                return {passed, details.str()};
            }
            const bool completed = !bus.bootromRunning && (hram[2] == 0x01 || hram[2] == 0xFF);
            std::ostringstream details;
            details << rom << " [dmgb] "
                    << (completed ? (hram[2] == 0x01 ? "passed" : "failed") : "no result")
                    << " after " << frames << " frames"
                    << std::hex << " (FF80=" << +hram[0]
                    << " FF81=" << +hram[1] << " FF82=" << +hram[2] << ")";
            return {completed && hram[2] == 0x01, details.str()};
        };

        const unsigned frameLimit = romFrames ? romFrames : 600;
        std::array<uint8_t, 3> previous{};
        unsigned matchingFrames = 0;
        for (unsigned frames = 1; frames <= frameLimit; ++frames) {
            gameboy->RunFrame();
            if (diagnostic) {
                const auto current = result(frames);
                matchingFrames = current.passed ? matchingFrames + 1 : 0;
                if (!romFrames && matchingFrames >= 2) return current;
                continue;
            }
            const std::array<uint8_t, 3> current{hram[0], hram[1], hram[2]};
            if (!romFrames && !bus.bootromRunning && current == previous &&
                (current[2] == 0x01 || current[2] == 0xFF)) {
                return result(frames);
            }
            previous = current;
        }
        return result(frameLimit);
    } catch (const std::exception &e) {
        return {false, rom + " [dmgb]: " + e.what()};
    }
}

static const std::vector<std::string> gbMicrotestRoms = {
    "roms/gbmicrotest/000-oam_lock.gb",
    "roms/gbmicrotest/000-write_to_x8000.gb",
    "roms/gbmicrotest/001-vram_unlocked.gb",
    "roms/gbmicrotest/002-vram_locked.gb",
    "roms/gbmicrotest/004-tima_boot_phase.gb",
    "roms/gbmicrotest/004-tima_cycle_timer.gb",
    "roms/gbmicrotest/007-lcd_on_stat.gb",
    "roms/gbmicrotest/400-dma.gb",
    "roms/gbmicrotest/500-scx-timing.gb",
    "roms/gbmicrotest/800-ppu-latch-scx.gb",
    "roms/gbmicrotest/801-ppu-latch-scy.gb",
    "roms/gbmicrotest/802-ppu-latch-tileselect.gb",
    "roms/gbmicrotest/803-ppu-latch-bgdisplay.gb",
    "roms/gbmicrotest/audio_testbench.gb",
    "roms/gbmicrotest/cpu_bus_1.gb",
    "roms/gbmicrotest/div_inc_timing_a.gb",
    "roms/gbmicrotest/div_inc_timing_b.gb",
    "roms/gbmicrotest/dma_0x1000.gb",
    "roms/gbmicrotest/dma_0x9000.gb",
    "roms/gbmicrotest/dma_0xA000.gb",
    "roms/gbmicrotest/dma_0xC000.gb",
    "roms/gbmicrotest/dma_0xE000.gb",
    "roms/gbmicrotest/dma_basic.gb",
    "roms/gbmicrotest/dma_timing_a.gb",
    "roms/gbmicrotest/flood_vram.gb",
    "roms/gbmicrotest/halt_bug.gb",
    "roms/gbmicrotest/halt_op_dupe.gb",
    "roms/gbmicrotest/halt_op_dupe_delay.gb",
    "roms/gbmicrotest/hblank_int_di_timing_a.gb",
    "roms/gbmicrotest/hblank_int_di_timing_b.gb",
    "roms/gbmicrotest/hblank_int_if_a.gb",
    "roms/gbmicrotest/hblank_int_if_b.gb",
    "roms/gbmicrotest/hblank_int_l0.gb",
    "roms/gbmicrotest/hblank_int_l1.gb",
    "roms/gbmicrotest/hblank_int_l2.gb",
    "roms/gbmicrotest/hblank_int_scx0.gb",
    "roms/gbmicrotest/hblank_int_scx0_if_a.gb",
    "roms/gbmicrotest/hblank_int_scx0_if_b.gb",
    "roms/gbmicrotest/hblank_int_scx0_if_c.gb",
    "roms/gbmicrotest/hblank_int_scx0_if_d.gb",
    "roms/gbmicrotest/hblank_int_scx1.gb",
    "roms/gbmicrotest/hblank_int_scx1_if_a.gb",
    "roms/gbmicrotest/hblank_int_scx1_if_b.gb",
    "roms/gbmicrotest/hblank_int_scx1_if_c.gb",
    "roms/gbmicrotest/hblank_int_scx1_if_d.gb",
    "roms/gbmicrotest/hblank_int_scx1_nops_a.gb",
    "roms/gbmicrotest/hblank_int_scx1_nops_b.gb",
    "roms/gbmicrotest/hblank_int_scx2.gb",
    "roms/gbmicrotest/hblank_int_scx2_if_a.gb",
    "roms/gbmicrotest/hblank_int_scx2_if_b.gb",
    "roms/gbmicrotest/hblank_int_scx2_if_c.gb",
    "roms/gbmicrotest/hblank_int_scx2_if_d.gb",
    "roms/gbmicrotest/hblank_int_scx2_nops_a.gb",
    "roms/gbmicrotest/hblank_int_scx2_nops_b.gb",
    "roms/gbmicrotest/hblank_int_scx3.gb",
    "roms/gbmicrotest/hblank_int_scx3_if_a.gb",
    "roms/gbmicrotest/hblank_int_scx3_if_b.gb",
    "roms/gbmicrotest/hblank_int_scx3_if_c.gb",
    "roms/gbmicrotest/hblank_int_scx3_if_d.gb",
    "roms/gbmicrotest/hblank_int_scx3_nops_a.gb",
    "roms/gbmicrotest/hblank_int_scx3_nops_b.gb",
    "roms/gbmicrotest/hblank_int_scx4.gb",
    "roms/gbmicrotest/hblank_int_scx4_if_a.gb",
    "roms/gbmicrotest/hblank_int_scx4_if_b.gb",
    "roms/gbmicrotest/hblank_int_scx4_if_c.gb",
    "roms/gbmicrotest/hblank_int_scx4_if_d.gb",
    "roms/gbmicrotest/hblank_int_scx4_nops_a.gb",
    "roms/gbmicrotest/hblank_int_scx4_nops_b.gb",
    "roms/gbmicrotest/hblank_int_scx5.gb",
    "roms/gbmicrotest/hblank_int_scx5_if_a.gb",
    "roms/gbmicrotest/hblank_int_scx5_if_b.gb",
    "roms/gbmicrotest/hblank_int_scx5_if_c.gb",
    "roms/gbmicrotest/hblank_int_scx5_if_d.gb",
    "roms/gbmicrotest/hblank_int_scx5_nops_a.gb",
    "roms/gbmicrotest/hblank_int_scx5_nops_b.gb",
    "roms/gbmicrotest/hblank_int_scx6.gb",
    "roms/gbmicrotest/hblank_int_scx6_if_a.gb",
    "roms/gbmicrotest/hblank_int_scx6_if_b.gb",
    "roms/gbmicrotest/hblank_int_scx6_if_c.gb",
    "roms/gbmicrotest/hblank_int_scx6_if_d.gb",
    "roms/gbmicrotest/hblank_int_scx6_nops_a.gb",
    "roms/gbmicrotest/hblank_int_scx6_nops_b.gb",
    "roms/gbmicrotest/hblank_int_scx7.gb",
    "roms/gbmicrotest/hblank_int_scx7_if_a.gb",
    "roms/gbmicrotest/hblank_int_scx7_if_b.gb",
    "roms/gbmicrotest/hblank_int_scx7_if_c.gb",
    "roms/gbmicrotest/hblank_int_scx7_if_d.gb",
    "roms/gbmicrotest/hblank_int_scx7_nops_a.gb",
    "roms/gbmicrotest/hblank_int_scx7_nops_b.gb",
    "roms/gbmicrotest/hblank_scx2_if_a.gb",
    "roms/gbmicrotest/hblank_scx3_if_a.gb",
    "roms/gbmicrotest/hblank_scx3_if_b.gb",
    "roms/gbmicrotest/hblank_scx3_if_c.gb",
    "roms/gbmicrotest/hblank_scx3_if_d.gb",
    "roms/gbmicrotest/hblank_scx3_int_a.gb",
    "roms/gbmicrotest/hblank_scx3_int_b.gb",
    "roms/gbmicrotest/int_hblank_halt_bug_a.gb",
    "roms/gbmicrotest/int_hblank_halt_bug_b.gb",
    "roms/gbmicrotest/int_hblank_halt_scx0.gb",
    "roms/gbmicrotest/int_hblank_halt_scx1.gb",
    "roms/gbmicrotest/int_hblank_halt_scx2.gb",
    "roms/gbmicrotest/int_hblank_halt_scx3.gb",
    "roms/gbmicrotest/int_hblank_halt_scx4.gb",
    "roms/gbmicrotest/int_hblank_halt_scx5.gb",
    "roms/gbmicrotest/int_hblank_halt_scx6.gb",
    "roms/gbmicrotest/int_hblank_halt_scx7.gb",
    "roms/gbmicrotest/int_hblank_incs_scx0.gb",
    "roms/gbmicrotest/int_hblank_incs_scx1.gb",
    "roms/gbmicrotest/int_hblank_incs_scx2.gb",
    "roms/gbmicrotest/int_hblank_incs_scx3.gb",
    "roms/gbmicrotest/int_hblank_incs_scx4.gb",
    "roms/gbmicrotest/int_hblank_incs_scx5.gb",
    "roms/gbmicrotest/int_hblank_incs_scx6.gb",
    "roms/gbmicrotest/int_hblank_incs_scx7.gb",
    "roms/gbmicrotest/int_hblank_nops_scx0.gb",
    "roms/gbmicrotest/int_hblank_nops_scx1.gb",
    "roms/gbmicrotest/int_hblank_nops_scx2.gb",
    "roms/gbmicrotest/int_hblank_nops_scx3.gb",
    "roms/gbmicrotest/int_hblank_nops_scx4.gb",
    "roms/gbmicrotest/int_hblank_nops_scx5.gb",
    "roms/gbmicrotest/int_hblank_nops_scx6.gb",
    "roms/gbmicrotest/int_hblank_nops_scx7.gb",
    "roms/gbmicrotest/int_lyc_halt.gb",
    "roms/gbmicrotest/int_lyc_incs.gb",
    "roms/gbmicrotest/int_lyc_nops.gb",
    "roms/gbmicrotest/int_oam_halt.gb",
    "roms/gbmicrotest/int_oam_incs.gb",
    "roms/gbmicrotest/int_oam_nops.gb",
    "roms/gbmicrotest/int_timer_halt.gb",
    "roms/gbmicrotest/int_timer_halt_div_a.gb",
    "roms/gbmicrotest/int_timer_halt_div_b.gb",
    "roms/gbmicrotest/int_timer_incs.gb",
    "roms/gbmicrotest/int_timer_nops.gb",
    "roms/gbmicrotest/int_timer_nops_div_a.gb",
    "roms/gbmicrotest/int_timer_nops_div_b.gb",
    "roms/gbmicrotest/int_vblank1_halt.gb",
    "roms/gbmicrotest/int_vblank1_incs.gb",
    "roms/gbmicrotest/int_vblank1_nops.gb",
    "roms/gbmicrotest/int_vblank2_halt.gb",
    "roms/gbmicrotest/int_vblank2_incs.gb",
    "roms/gbmicrotest/int_vblank2_nops.gb",
    "roms/gbmicrotest/is_if_set_during_ime0.gb",
    "roms/gbmicrotest/lcdon_halt_to_vblank_int_a.gb",
    "roms/gbmicrotest/lcdon_halt_to_vblank_int_b.gb",
    "roms/gbmicrotest/lcdon_nops_to_vblank_int_a.gb",
    "roms/gbmicrotest/lcdon_nops_to_vblank_int_b.gb",
    "roms/gbmicrotest/lcdon_to_if_oam_a.gb",
    "roms/gbmicrotest/lcdon_to_if_oam_b.gb",
    "roms/gbmicrotest/lcdon_to_ly1_a.gb",
    "roms/gbmicrotest/lcdon_to_ly1_b.gb",
    "roms/gbmicrotest/lcdon_to_ly2_a.gb",
    "roms/gbmicrotest/lcdon_to_ly2_b.gb",
    "roms/gbmicrotest/lcdon_to_ly3_a.gb",
    "roms/gbmicrotest/lcdon_to_ly3_b.gb",
    "roms/gbmicrotest/lcdon_to_lyc1_int.gb",
    "roms/gbmicrotest/lcdon_to_lyc2_int.gb",
    "roms/gbmicrotest/lcdon_to_lyc3_int.gb",
    "roms/gbmicrotest/lcdon_to_oam_int_l0.gb",
    "roms/gbmicrotest/lcdon_to_oam_int_l1.gb",
    "roms/gbmicrotest/lcdon_to_oam_int_l2.gb",
    "roms/gbmicrotest/lcdon_to_oam_unlock_a.gb",
    "roms/gbmicrotest/lcdon_to_oam_unlock_b.gb",
    "roms/gbmicrotest/lcdon_to_oam_unlock_c.gb",
    "roms/gbmicrotest/lcdon_to_oam_unlock_d.gb",
    "roms/gbmicrotest/lcdon_to_stat0_a.gb",
    "roms/gbmicrotest/lcdon_to_stat0_b.gb",
    "roms/gbmicrotest/lcdon_to_stat0_c.gb",
    "roms/gbmicrotest/lcdon_to_stat0_d.gb",
    "roms/gbmicrotest/lcdon_to_stat1_a.gb",
    "roms/gbmicrotest/lcdon_to_stat1_b.gb",
    "roms/gbmicrotest/lcdon_to_stat1_c.gb",
    "roms/gbmicrotest/lcdon_to_stat1_d.gb",
    "roms/gbmicrotest/lcdon_to_stat1_e.gb",
    "roms/gbmicrotest/lcdon_to_stat2_a.gb",
    "roms/gbmicrotest/lcdon_to_stat2_b.gb",
    "roms/gbmicrotest/lcdon_to_stat2_c.gb",
    "roms/gbmicrotest/lcdon_to_stat2_d.gb",
    "roms/gbmicrotest/lcdon_to_stat3_a.gb",
    "roms/gbmicrotest/lcdon_to_stat3_b.gb",
    "roms/gbmicrotest/lcdon_to_stat3_c.gb",
    "roms/gbmicrotest/lcdon_to_stat3_d.gb",
    "roms/gbmicrotest/lcdon_write_timing.gb",
    "roms/gbmicrotest/line_144_oam_int_a.gb",
    "roms/gbmicrotest/line_144_oam_int_b.gb",
    "roms/gbmicrotest/line_144_oam_int_c.gb",
    "roms/gbmicrotest/line_144_oam_int_d.gb",
    "roms/gbmicrotest/line_153_ly_a.gb",
    "roms/gbmicrotest/line_153_ly_b.gb",
    "roms/gbmicrotest/line_153_ly_c.gb",
    "roms/gbmicrotest/line_153_ly_d.gb",
    "roms/gbmicrotest/line_153_ly_e.gb",
    "roms/gbmicrotest/line_153_ly_f.gb",
    "roms/gbmicrotest/line_153_lyc0_int_inc_sled.gb",
    "roms/gbmicrotest/line_153_lyc0_stat_timing_a.gb",
    "roms/gbmicrotest/line_153_lyc0_stat_timing_b.gb",
    "roms/gbmicrotest/line_153_lyc0_stat_timing_c.gb",
    "roms/gbmicrotest/line_153_lyc0_stat_timing_d.gb",
    "roms/gbmicrotest/line_153_lyc0_stat_timing_e.gb",
    "roms/gbmicrotest/line_153_lyc0_stat_timing_f.gb",
    "roms/gbmicrotest/line_153_lyc0_stat_timing_g.gb",
    "roms/gbmicrotest/line_153_lyc0_stat_timing_h.gb",
    "roms/gbmicrotest/line_153_lyc0_stat_timing_i.gb",
    "roms/gbmicrotest/line_153_lyc0_stat_timing_j.gb",
    "roms/gbmicrotest/line_153_lyc0_stat_timing_k.gb",
    "roms/gbmicrotest/line_153_lyc0_stat_timing_l.gb",
    "roms/gbmicrotest/line_153_lyc0_stat_timing_m.gb",
    "roms/gbmicrotest/line_153_lyc0_stat_timing_n.gb",
    "roms/gbmicrotest/line_153_lyc153_stat_timing_a.gb",
    "roms/gbmicrotest/line_153_lyc153_stat_timing_b.gb",
    "roms/gbmicrotest/line_153_lyc153_stat_timing_c.gb",
    "roms/gbmicrotest/line_153_lyc153_stat_timing_d.gb",
    "roms/gbmicrotest/line_153_lyc153_stat_timing_e.gb",
    "roms/gbmicrotest/line_153_lyc153_stat_timing_f.gb",
    "roms/gbmicrotest/line_153_lyc_a.gb",
    "roms/gbmicrotest/line_153_lyc_b.gb",
    "roms/gbmicrotest/line_153_lyc_c.gb",
    "roms/gbmicrotest/line_153_lyc_int_a.gb",
    "roms/gbmicrotest/line_153_lyc_int_b.gb",
    "roms/gbmicrotest/line_65_ly.gb",
    "roms/gbmicrotest/ly_while_lcd_off.gb",
    "roms/gbmicrotest/lyc1_int_halt_a.gb",
    "roms/gbmicrotest/lyc1_int_halt_b.gb",
    "roms/gbmicrotest/lyc1_int_if_edge_a.gb",
    "roms/gbmicrotest/lyc1_int_if_edge_b.gb",
    "roms/gbmicrotest/lyc1_int_if_edge_c.gb",
    "roms/gbmicrotest/lyc1_int_if_edge_d.gb",
    "roms/gbmicrotest/lyc1_int_nops_a.gb",
    "roms/gbmicrotest/lyc1_int_nops_b.gb",
    "roms/gbmicrotest/lyc1_write_timing_a.gb",
    "roms/gbmicrotest/lyc1_write_timing_b.gb",
    "roms/gbmicrotest/lyc1_write_timing_c.gb",
    "roms/gbmicrotest/lyc1_write_timing_d.gb",
    "roms/gbmicrotest/lyc2_int_halt_a.gb",
    "roms/gbmicrotest/lyc2_int_halt_b.gb",
    "roms/gbmicrotest/lyc_int_halt_a.gb",
    "roms/gbmicrotest/lyc_int_halt_b.gb",
    "roms/gbmicrotest/mbc1_ram_banks.gb",
    "roms/gbmicrotest/mbc1_rom_banks.gb",
    "roms/gbmicrotest/minimal.gb",
    "roms/gbmicrotest/mode2_stat_int_to_oam_unlock.gb",
    "roms/gbmicrotest/oam_int_halt_a.gb",
    "roms/gbmicrotest/oam_int_halt_b.gb",
    "roms/gbmicrotest/oam_int_if_edge_a.gb",
    "roms/gbmicrotest/oam_int_if_edge_b.gb",
    "roms/gbmicrotest/oam_int_if_edge_c.gb",
    "roms/gbmicrotest/oam_int_if_edge_d.gb",
    "roms/gbmicrotest/oam_int_if_level_c.gb",
    "roms/gbmicrotest/oam_int_if_level_d.gb",
    "roms/gbmicrotest/oam_int_inc_sled.gb",
    "roms/gbmicrotest/oam_int_nops_a.gb",
    "roms/gbmicrotest/oam_int_nops_b.gb",
    "roms/gbmicrotest/oam_read_l0_a.gb",
    "roms/gbmicrotest/oam_read_l0_b.gb",
    "roms/gbmicrotest/oam_read_l0_c.gb",
    "roms/gbmicrotest/oam_read_l0_d.gb",
    "roms/gbmicrotest/oam_read_l1_a.gb",
    "roms/gbmicrotest/oam_read_l1_b.gb",
    "roms/gbmicrotest/oam_read_l1_c.gb",
    "roms/gbmicrotest/oam_read_l1_d.gb",
    "roms/gbmicrotest/oam_read_l1_e.gb",
    "roms/gbmicrotest/oam_read_l1_f.gb",
    "roms/gbmicrotest/oam_sprite_trashing.gb",
    "roms/gbmicrotest/oam_write_l0_a.gb",
    "roms/gbmicrotest/oam_write_l0_b.gb",
    "roms/gbmicrotest/oam_write_l0_c.gb",
    "roms/gbmicrotest/oam_write_l0_d.gb",
    "roms/gbmicrotest/oam_write_l0_e.gb",
    "roms/gbmicrotest/oam_write_l1_a.gb",
    "roms/gbmicrotest/oam_write_l1_b.gb",
    "roms/gbmicrotest/oam_write_l1_c.gb",
    "roms/gbmicrotest/oam_write_l1_d.gb",
    "roms/gbmicrotest/oam_write_l1_e.gb",
    "roms/gbmicrotest/oam_write_l1_f.gb",
    "roms/gbmicrotest/poweron.gb",
    "roms/gbmicrotest/poweron_bgp_000.gb",
    "roms/gbmicrotest/poweron_div_000.gb",
    "roms/gbmicrotest/poweron_div_004.gb",
    "roms/gbmicrotest/poweron_div_005.gb",
    "roms/gbmicrotest/poweron_dma_000.gb",
    "roms/gbmicrotest/poweron_if_000.gb",
    "roms/gbmicrotest/poweron_joy_000.gb",
    "roms/gbmicrotest/poweron_lcdc_000.gb",
    "roms/gbmicrotest/poweron_ly_000.gb",
    "roms/gbmicrotest/poweron_ly_119.gb",
    "roms/gbmicrotest/poweron_ly_120.gb",
    "roms/gbmicrotest/poweron_ly_233.gb",
    "roms/gbmicrotest/poweron_ly_234.gb",
    "roms/gbmicrotest/poweron_lyc_000.gb",
    "roms/gbmicrotest/poweron_oam_000.gb",
    "roms/gbmicrotest/poweron_oam_005.gb",
    "roms/gbmicrotest/poweron_oam_006.gb",
    "roms/gbmicrotest/poweron_oam_069.gb",
    "roms/gbmicrotest/poweron_oam_070.gb",
    "roms/gbmicrotest/poweron_oam_119.gb",
    "roms/gbmicrotest/poweron_oam_120.gb",
    "roms/gbmicrotest/poweron_oam_121.gb",
    "roms/gbmicrotest/poweron_oam_183.gb",
    "roms/gbmicrotest/poweron_oam_184.gb",
    "roms/gbmicrotest/poweron_oam_233.gb",
    "roms/gbmicrotest/poweron_oam_234.gb",
    "roms/gbmicrotest/poweron_oam_235.gb",
    "roms/gbmicrotest/poweron_obp0_000.gb",
    "roms/gbmicrotest/poweron_obp1_000.gb",
    "roms/gbmicrotest/poweron_sb_000.gb",
    "roms/gbmicrotest/poweron_sc_000.gb",
    "roms/gbmicrotest/poweron_scx_000.gb",
    "roms/gbmicrotest/poweron_scy_000.gb",
    "roms/gbmicrotest/poweron_stat_000.gb",
    "roms/gbmicrotest/poweron_stat_005.gb",
    "roms/gbmicrotest/poweron_stat_006.gb",
    "roms/gbmicrotest/poweron_stat_007.gb",
    "roms/gbmicrotest/poweron_stat_026.gb",
    "roms/gbmicrotest/poweron_stat_027.gb",
    "roms/gbmicrotest/poweron_stat_069.gb",
    "roms/gbmicrotest/poweron_stat_070.gb",
    "roms/gbmicrotest/poweron_stat_119.gb",
    "roms/gbmicrotest/poweron_stat_120.gb",
    "roms/gbmicrotest/poweron_stat_121.gb",
    "roms/gbmicrotest/poweron_stat_140.gb",
    "roms/gbmicrotest/poweron_stat_141.gb",
    "roms/gbmicrotest/poweron_stat_183.gb",
    "roms/gbmicrotest/poweron_stat_184.gb",
    "roms/gbmicrotest/poweron_stat_234.gb",
    "roms/gbmicrotest/poweron_stat_235.gb",
    "roms/gbmicrotest/poweron_tac_000.gb",
    "roms/gbmicrotest/poweron_tima_000.gb",
    "roms/gbmicrotest/poweron_tma_000.gb",
    "roms/gbmicrotest/poweron_vram_000.gb",
    "roms/gbmicrotest/poweron_vram_025.gb",
    "roms/gbmicrotest/poweron_vram_026.gb",
    "roms/gbmicrotest/poweron_vram_069.gb",
    "roms/gbmicrotest/poweron_vram_070.gb",
    "roms/gbmicrotest/poweron_vram_139.gb",
    "roms/gbmicrotest/poweron_vram_140.gb",
    "roms/gbmicrotest/poweron_vram_183.gb",
    "roms/gbmicrotest/poweron_vram_184.gb",
    "roms/gbmicrotest/poweron_wx_000.gb",
    "roms/gbmicrotest/poweron_wy_000.gb",
    "roms/gbmicrotest/ppu_scx_vs_bgp.gb",
    "roms/gbmicrotest/ppu_sprite0_scx0_a.gb",
    "roms/gbmicrotest/ppu_sprite0_scx0_b.gb",
    "roms/gbmicrotest/ppu_sprite0_scx1_a.gb",
    "roms/gbmicrotest/ppu_sprite0_scx1_b.gb",
    "roms/gbmicrotest/ppu_sprite0_scx2_a.gb",
    "roms/gbmicrotest/ppu_sprite0_scx2_b.gb",
    "roms/gbmicrotest/ppu_sprite0_scx3_a.gb",
    "roms/gbmicrotest/ppu_sprite0_scx3_b.gb",
    "roms/gbmicrotest/ppu_sprite0_scx4_a.gb",
    "roms/gbmicrotest/ppu_sprite0_scx4_b.gb",
    "roms/gbmicrotest/ppu_sprite0_scx5_a.gb",
    "roms/gbmicrotest/ppu_sprite0_scx5_b.gb",
    "roms/gbmicrotest/ppu_sprite0_scx6_a.gb",
    "roms/gbmicrotest/ppu_sprite0_scx6_b.gb",
    "roms/gbmicrotest/ppu_sprite0_scx7_a.gb",
    "roms/gbmicrotest/ppu_sprite0_scx7_b.gb",
    "roms/gbmicrotest/ppu_sprite_testbench.gb",
    "roms/gbmicrotest/ppu_spritex_vs_scx.gb",
    "roms/gbmicrotest/ppu_win_vs_wx.gb",
    "roms/gbmicrotest/ppu_wx_early.gb",
    "roms/gbmicrotest/sprite4_0_a.gb",
    "roms/gbmicrotest/sprite4_0_b.gb",
    "roms/gbmicrotest/sprite4_1_a.gb",
    "roms/gbmicrotest/sprite4_1_b.gb",
    "roms/gbmicrotest/sprite4_2_a.gb",
    "roms/gbmicrotest/sprite4_2_b.gb",
    "roms/gbmicrotest/sprite4_3_a.gb",
    "roms/gbmicrotest/sprite4_3_b.gb",
    "roms/gbmicrotest/sprite4_4_a.gb",
    "roms/gbmicrotest/sprite4_4_b.gb",
    "roms/gbmicrotest/sprite4_5_a.gb",
    "roms/gbmicrotest/sprite4_5_b.gb",
    "roms/gbmicrotest/sprite4_6_a.gb",
    "roms/gbmicrotest/sprite4_6_b.gb",
    "roms/gbmicrotest/sprite4_7_a.gb",
    "roms/gbmicrotest/sprite4_7_b.gb",
    "roms/gbmicrotest/sprite_0_a.gb",
    "roms/gbmicrotest/sprite_0_b.gb",
    "roms/gbmicrotest/sprite_1_a.gb",
    "roms/gbmicrotest/sprite_1_b.gb",
    "roms/gbmicrotest/stat_write_glitch_l0_a.gb",
    "roms/gbmicrotest/stat_write_glitch_l0_b.gb",
    "roms/gbmicrotest/stat_write_glitch_l0_c.gb",
    "roms/gbmicrotest/stat_write_glitch_l143_a.gb",
    "roms/gbmicrotest/stat_write_glitch_l143_b.gb",
    "roms/gbmicrotest/stat_write_glitch_l143_c.gb",
    "roms/gbmicrotest/stat_write_glitch_l143_d.gb",
    "roms/gbmicrotest/stat_write_glitch_l154_a.gb",
    "roms/gbmicrotest/stat_write_glitch_l154_b.gb",
    "roms/gbmicrotest/stat_write_glitch_l154_c.gb",
    "roms/gbmicrotest/stat_write_glitch_l154_d.gb",
    "roms/gbmicrotest/stat_write_glitch_l1_a.gb",
    "roms/gbmicrotest/stat_write_glitch_l1_b.gb",
    "roms/gbmicrotest/stat_write_glitch_l1_c.gb",
    "roms/gbmicrotest/stat_write_glitch_l1_d.gb",
    "roms/gbmicrotest/temp.gb",
    "roms/gbmicrotest/timer_div_phase_c.gb",
    "roms/gbmicrotest/timer_div_phase_d.gb",
    "roms/gbmicrotest/timer_tima_inc_256k_a.gb",
    "roms/gbmicrotest/timer_tima_inc_256k_b.gb",
    "roms/gbmicrotest/timer_tima_inc_256k_c.gb",
    "roms/gbmicrotest/timer_tima_inc_256k_d.gb",
    "roms/gbmicrotest/timer_tima_inc_256k_e.gb",
    "roms/gbmicrotest/timer_tima_inc_256k_f.gb",
    "roms/gbmicrotest/timer_tima_inc_256k_g.gb",
    "roms/gbmicrotest/timer_tima_inc_256k_h.gb",
    "roms/gbmicrotest/timer_tima_inc_256k_i.gb",
    "roms/gbmicrotest/timer_tima_inc_256k_j.gb",
    "roms/gbmicrotest/timer_tima_inc_256k_k.gb",
    "roms/gbmicrotest/timer_tima_inc_64k_a.gb",
    "roms/gbmicrotest/timer_tima_inc_64k_b.gb",
    "roms/gbmicrotest/timer_tima_inc_64k_c.gb",
    "roms/gbmicrotest/timer_tima_inc_64k_d.gb",
    "roms/gbmicrotest/timer_tima_phase_a.gb",
    "roms/gbmicrotest/timer_tima_phase_b.gb",
    "roms/gbmicrotest/timer_tima_phase_c.gb",
    "roms/gbmicrotest/timer_tima_phase_d.gb",
    "roms/gbmicrotest/timer_tima_phase_e.gb",
    "roms/gbmicrotest/timer_tima_phase_f.gb",
    "roms/gbmicrotest/timer_tima_phase_g.gb",
    "roms/gbmicrotest/timer_tima_phase_h.gb",
    "roms/gbmicrotest/timer_tima_phase_i.gb",
    "roms/gbmicrotest/timer_tima_phase_j.gb",
    "roms/gbmicrotest/timer_tima_reload_256k_a.gb",
    "roms/gbmicrotest/timer_tima_reload_256k_b.gb",
    "roms/gbmicrotest/timer_tima_reload_256k_c.gb",
    "roms/gbmicrotest/timer_tima_reload_256k_d.gb",
    "roms/gbmicrotest/timer_tima_reload_256k_e.gb",
    "roms/gbmicrotest/timer_tima_reload_256k_f.gb",
    "roms/gbmicrotest/timer_tima_reload_256k_g.gb",
    "roms/gbmicrotest/timer_tima_reload_256k_h.gb",
    "roms/gbmicrotest/timer_tima_reload_256k_i.gb",
    "roms/gbmicrotest/timer_tima_reload_256k_j.gb",
    "roms/gbmicrotest/timer_tima_reload_256k_k.gb",
    "roms/gbmicrotest/timer_tima_write_a.gb",
    "roms/gbmicrotest/timer_tima_write_b.gb",
    "roms/gbmicrotest/timer_tima_write_c.gb",
    "roms/gbmicrotest/timer_tima_write_d.gb",
    "roms/gbmicrotest/timer_tima_write_e.gb",
    "roms/gbmicrotest/timer_tima_write_f.gb",
    "roms/gbmicrotest/timer_tma_write_a.gb",
    "roms/gbmicrotest/timer_tma_write_b.gb",
    "roms/gbmicrotest/toggle_lcdc.gb",
    "roms/gbmicrotest/vblank2_int_halt_a.gb",
    "roms/gbmicrotest/vblank2_int_halt_b.gb",
    "roms/gbmicrotest/vblank2_int_if_a.gb",
    "roms/gbmicrotest/vblank2_int_if_b.gb",
    "roms/gbmicrotest/vblank2_int_if_c.gb",
    "roms/gbmicrotest/vblank2_int_if_d.gb",
    "roms/gbmicrotest/vblank2_int_inc_sled.gb",
    "roms/gbmicrotest/vblank2_int_nops_a.gb",
    "roms/gbmicrotest/vblank2_int_nops_b.gb",
    "roms/gbmicrotest/vblank_int_halt_a.gb",
    "roms/gbmicrotest/vblank_int_halt_b.gb",
    "roms/gbmicrotest/vblank_int_if_a.gb",
    "roms/gbmicrotest/vblank_int_if_b.gb",
    "roms/gbmicrotest/vblank_int_if_c.gb",
    "roms/gbmicrotest/vblank_int_if_d.gb",
    "roms/gbmicrotest/vblank_int_inc_sled.gb",
    "roms/gbmicrotest/vblank_int_nops_a.gb",
    "roms/gbmicrotest/vblank_int_nops_b.gb",
    "roms/gbmicrotest/vram_read_l0_a.gb",
    "roms/gbmicrotest/vram_read_l0_b.gb",
    "roms/gbmicrotest/vram_read_l0_c.gb",
    "roms/gbmicrotest/vram_read_l0_d.gb",
    "roms/gbmicrotest/vram_read_l1_a.gb",
    "roms/gbmicrotest/vram_read_l1_b.gb",
    "roms/gbmicrotest/vram_read_l1_c.gb",
    "roms/gbmicrotest/vram_read_l1_d.gb",
    "roms/gbmicrotest/vram_write_l0_a.gb",
    "roms/gbmicrotest/vram_write_l0_b.gb",
    "roms/gbmicrotest/vram_write_l0_c.gb",
    "roms/gbmicrotest/vram_write_l0_d.gb",
    "roms/gbmicrotest/vram_write_l1_a.gb",
    "roms/gbmicrotest/vram_write_l1_b.gb",
    "roms/gbmicrotest/vram_write_l1_c.gb",
    "roms/gbmicrotest/vram_write_l1_d.gb",
    "roms/gbmicrotest/wave_write_to_0xC003.gb",
    "roms/gbmicrotest/win0_a.gb",
    "roms/gbmicrotest/win0_b.gb",
    "roms/gbmicrotest/win0_scx3_a.gb",
    "roms/gbmicrotest/win0_scx3_b.gb",
    "roms/gbmicrotest/win10_a.gb",
    "roms/gbmicrotest/win10_b.gb",
    "roms/gbmicrotest/win10_scx3_a.gb",
    "roms/gbmicrotest/win10_scx3_b.gb",
    "roms/gbmicrotest/win11_a.gb",
    "roms/gbmicrotest/win11_b.gb",
    "roms/gbmicrotest/win12_a.gb",
    "roms/gbmicrotest/win12_b.gb",
    "roms/gbmicrotest/win13_a.gb",
    "roms/gbmicrotest/win13_b.gb",
    "roms/gbmicrotest/win14_a.gb",
    "roms/gbmicrotest/win14_b.gb",
    "roms/gbmicrotest/win15_a.gb",
    "roms/gbmicrotest/win15_b.gb",
    "roms/gbmicrotest/win1_a.gb",
    "roms/gbmicrotest/win1_b.gb",
    "roms/gbmicrotest/win2_a.gb",
    "roms/gbmicrotest/win2_b.gb",
    "roms/gbmicrotest/win3_a.gb",
    "roms/gbmicrotest/win3_b.gb",
    "roms/gbmicrotest/win4_a.gb",
    "roms/gbmicrotest/win4_b.gb",
    "roms/gbmicrotest/win5_a.gb",
    "roms/gbmicrotest/win5_b.gb",
    "roms/gbmicrotest/win6_a.gb",
    "roms/gbmicrotest/win6_b.gb",
    "roms/gbmicrotest/win7_a.gb",
    "roms/gbmicrotest/win7_b.gb",
    "roms/gbmicrotest/win8_a.gb",
    "roms/gbmicrotest/win8_b.gb",
    "roms/gbmicrotest/win9_a.gb",
    "roms/gbmicrotest/win9_b.gb",
};

static auto &gbMicrotestFutures() {
    using SF = std::shared_future<GbMicrotestResult>;
    static std::vector<SF> futures = [] {
        std::vector<SF> tmp;
        tmp.reserve(gbMicrotestRoms.size());

        for (const auto &rom : gbMicrotestRoms) {
            tmp.emplace_back(
                std::async(parallelRomTests ? std::launch::async : std::launch::deferred, [rom] {
                    return runGbMicrotest(rom);
                }).share()
            );
        }
        return tmp;
    }();
    return futures;
}

#define GBMICRO_TEST(IDX, ROM_STR)                        \
TEST_CASE("gbmicrotest: " ROM_STR) {                      \
    const auto &result = gbMicrotestFutures()[IDX].get(); \
    CHECK_MESSAGE(result.passed, result.details);        \
}

GBMICRO_TEST(0, "000-oam_lock.gb")
GBMICRO_TEST(1, "000-write_to_x8000.gb")
GBMICRO_TEST(2, "001-vram_unlocked.gb")
GBMICRO_TEST(3, "002-vram_locked.gb")
GBMICRO_TEST(4, "004-tima_boot_phase.gb")
GBMICRO_TEST(5, "004-tima_cycle_timer.gb")
GBMICRO_TEST(6, "007-lcd_on_stat.gb")
GBMICRO_TEST(7, "400-dma.gb")
GBMICRO_TEST(8, "500-scx-timing.gb")
GBMICRO_TEST(9, "800-ppu-latch-scx.gb")
GBMICRO_TEST(10, "801-ppu-latch-scy.gb")
GBMICRO_TEST(11, "802-ppu-latch-tileselect.gb")
GBMICRO_TEST(12, "803-ppu-latch-bgdisplay.gb")
GBMICRO_TEST(13, "audio_testbench.gb")
GBMICRO_TEST(14, "cpu_bus_1.gb")
GBMICRO_TEST(15, "div_inc_timing_a.gb")
GBMICRO_TEST(16, "div_inc_timing_b.gb")
GBMICRO_TEST(17, "dma_0x1000.gb")
GBMICRO_TEST(18, "dma_0x9000.gb")
GBMICRO_TEST(19, "dma_0xA000.gb")
GBMICRO_TEST(20, "dma_0xC000.gb")
GBMICRO_TEST(21, "dma_0xE000.gb")
GBMICRO_TEST(22, "dma_basic.gb")
GBMICRO_TEST(23, "dma_timing_a.gb")
GBMICRO_TEST(24, "flood_vram.gb")
GBMICRO_TEST(25, "halt_bug.gb")
GBMICRO_TEST(26, "halt_op_dupe.gb")
GBMICRO_TEST(27, "halt_op_dupe_delay.gb")
GBMICRO_TEST(28, "hblank_int_di_timing_a.gb")
GBMICRO_TEST(29, "hblank_int_di_timing_b.gb")
GBMICRO_TEST(30, "hblank_int_if_a.gb")
GBMICRO_TEST(31, "hblank_int_if_b.gb")
GBMICRO_TEST(32, "hblank_int_l0.gb")
GBMICRO_TEST(33, "hblank_int_l1.gb")
GBMICRO_TEST(34, "hblank_int_l2.gb")
GBMICRO_TEST(35, "hblank_int_scx0.gb")
GBMICRO_TEST(36, "hblank_int_scx0_if_a.gb")
GBMICRO_TEST(37, "hblank_int_scx0_if_b.gb")
GBMICRO_TEST(38, "hblank_int_scx0_if_c.gb")
GBMICRO_TEST(39, "hblank_int_scx0_if_d.gb")
GBMICRO_TEST(40, "hblank_int_scx1.gb")
GBMICRO_TEST(41, "hblank_int_scx1_if_a.gb")
GBMICRO_TEST(42, "hblank_int_scx1_if_b.gb")
GBMICRO_TEST(43, "hblank_int_scx1_if_c.gb")
GBMICRO_TEST(44, "hblank_int_scx1_if_d.gb")
GBMICRO_TEST(45, "hblank_int_scx1_nops_a.gb")
GBMICRO_TEST(46, "hblank_int_scx1_nops_b.gb")
GBMICRO_TEST(47, "hblank_int_scx2.gb")
GBMICRO_TEST(48, "hblank_int_scx2_if_a.gb")
GBMICRO_TEST(49, "hblank_int_scx2_if_b.gb")
GBMICRO_TEST(50, "hblank_int_scx2_if_c.gb")
GBMICRO_TEST(51, "hblank_int_scx2_if_d.gb")
GBMICRO_TEST(52, "hblank_int_scx2_nops_a.gb")
GBMICRO_TEST(53, "hblank_int_scx2_nops_b.gb")
GBMICRO_TEST(54, "hblank_int_scx3.gb")
GBMICRO_TEST(55, "hblank_int_scx3_if_a.gb")
GBMICRO_TEST(56, "hblank_int_scx3_if_b.gb")
GBMICRO_TEST(57, "hblank_int_scx3_if_c.gb")
GBMICRO_TEST(58, "hblank_int_scx3_if_d.gb")
GBMICRO_TEST(59, "hblank_int_scx3_nops_a.gb")
GBMICRO_TEST(60, "hblank_int_scx3_nops_b.gb")
GBMICRO_TEST(61, "hblank_int_scx4.gb")
GBMICRO_TEST(62, "hblank_int_scx4_if_a.gb")
GBMICRO_TEST(63, "hblank_int_scx4_if_b.gb")
GBMICRO_TEST(64, "hblank_int_scx4_if_c.gb")
GBMICRO_TEST(65, "hblank_int_scx4_if_d.gb")
GBMICRO_TEST(66, "hblank_int_scx4_nops_a.gb")
GBMICRO_TEST(67, "hblank_int_scx4_nops_b.gb")
GBMICRO_TEST(68, "hblank_int_scx5.gb")
GBMICRO_TEST(69, "hblank_int_scx5_if_a.gb")
GBMICRO_TEST(70, "hblank_int_scx5_if_b.gb")
GBMICRO_TEST(71, "hblank_int_scx5_if_c.gb")
GBMICRO_TEST(72, "hblank_int_scx5_if_d.gb")
GBMICRO_TEST(73, "hblank_int_scx5_nops_a.gb")
GBMICRO_TEST(74, "hblank_int_scx5_nops_b.gb")
GBMICRO_TEST(75, "hblank_int_scx6.gb")
GBMICRO_TEST(76, "hblank_int_scx6_if_a.gb")
GBMICRO_TEST(77, "hblank_int_scx6_if_b.gb")
GBMICRO_TEST(78, "hblank_int_scx6_if_c.gb")
GBMICRO_TEST(79, "hblank_int_scx6_if_d.gb")
GBMICRO_TEST(80, "hblank_int_scx6_nops_a.gb")
GBMICRO_TEST(81, "hblank_int_scx6_nops_b.gb")
GBMICRO_TEST(82, "hblank_int_scx7.gb")
GBMICRO_TEST(83, "hblank_int_scx7_if_a.gb")
GBMICRO_TEST(84, "hblank_int_scx7_if_b.gb")
GBMICRO_TEST(85, "hblank_int_scx7_if_c.gb")
GBMICRO_TEST(86, "hblank_int_scx7_if_d.gb")
GBMICRO_TEST(87, "hblank_int_scx7_nops_a.gb")
GBMICRO_TEST(88, "hblank_int_scx7_nops_b.gb")
GBMICRO_TEST(89, "hblank_scx2_if_a.gb")
GBMICRO_TEST(90, "hblank_scx3_if_a.gb")
GBMICRO_TEST(91, "hblank_scx3_if_b.gb")
GBMICRO_TEST(92, "hblank_scx3_if_c.gb")
GBMICRO_TEST(93, "hblank_scx3_if_d.gb")
GBMICRO_TEST(94, "hblank_scx3_int_a.gb")
GBMICRO_TEST(95, "hblank_scx3_int_b.gb")
GBMICRO_TEST(96, "int_hblank_halt_bug_a.gb")
GBMICRO_TEST(97, "int_hblank_halt_bug_b.gb")
GBMICRO_TEST(98, "int_hblank_halt_scx0.gb")
GBMICRO_TEST(99, "int_hblank_halt_scx1.gb")
GBMICRO_TEST(100, "int_hblank_halt_scx2.gb")
GBMICRO_TEST(101, "int_hblank_halt_scx3.gb")
GBMICRO_TEST(102, "int_hblank_halt_scx4.gb")
GBMICRO_TEST(103, "int_hblank_halt_scx5.gb")
GBMICRO_TEST(104, "int_hblank_halt_scx6.gb")
GBMICRO_TEST(105, "int_hblank_halt_scx7.gb")
GBMICRO_TEST(106, "int_hblank_incs_scx0.gb")
GBMICRO_TEST(107, "int_hblank_incs_scx1.gb")
GBMICRO_TEST(108, "int_hblank_incs_scx2.gb")
GBMICRO_TEST(109, "int_hblank_incs_scx3.gb")
GBMICRO_TEST(110, "int_hblank_incs_scx4.gb")
GBMICRO_TEST(111, "int_hblank_incs_scx5.gb")
GBMICRO_TEST(112, "int_hblank_incs_scx6.gb")
GBMICRO_TEST(113, "int_hblank_incs_scx7.gb")
GBMICRO_TEST(114, "int_hblank_nops_scx0.gb")
GBMICRO_TEST(115, "int_hblank_nops_scx1.gb")
GBMICRO_TEST(116, "int_hblank_nops_scx2.gb")
GBMICRO_TEST(117, "int_hblank_nops_scx3.gb")
GBMICRO_TEST(118, "int_hblank_nops_scx4.gb")
GBMICRO_TEST(119, "int_hblank_nops_scx5.gb")
GBMICRO_TEST(120, "int_hblank_nops_scx6.gb")
GBMICRO_TEST(121, "int_hblank_nops_scx7.gb")
GBMICRO_TEST(122, "int_lyc_halt.gb")
GBMICRO_TEST(123, "int_lyc_incs.gb")
GBMICRO_TEST(124, "int_lyc_nops.gb")
GBMICRO_TEST(125, "int_oam_halt.gb")
GBMICRO_TEST(126, "int_oam_incs.gb")
GBMICRO_TEST(127, "int_oam_nops.gb")
GBMICRO_TEST(128, "int_timer_halt.gb")
GBMICRO_TEST(129, "int_timer_halt_div_a.gb")
GBMICRO_TEST(130, "int_timer_halt_div_b.gb")
GBMICRO_TEST(131, "int_timer_incs.gb")
GBMICRO_TEST(132, "int_timer_nops.gb")
GBMICRO_TEST(133, "int_timer_nops_div_a.gb")
GBMICRO_TEST(134, "int_timer_nops_div_b.gb")
GBMICRO_TEST(135, "int_vblank1_halt.gb")
GBMICRO_TEST(136, "int_vblank1_incs.gb")
GBMICRO_TEST(137, "int_vblank1_nops.gb")
GBMICRO_TEST(138, "int_vblank2_halt.gb")
GBMICRO_TEST(139, "int_vblank2_incs.gb")
GBMICRO_TEST(140, "int_vblank2_nops.gb")
GBMICRO_TEST(141, "is_if_set_during_ime0.gb")
GBMICRO_TEST(142, "lcdon_halt_to_vblank_int_a.gb")
GBMICRO_TEST(143, "lcdon_halt_to_vblank_int_b.gb")
GBMICRO_TEST(144, "lcdon_nops_to_vblank_int_a.gb")
GBMICRO_TEST(145, "lcdon_nops_to_vblank_int_b.gb")
GBMICRO_TEST(146, "lcdon_to_if_oam_a.gb")
GBMICRO_TEST(147, "lcdon_to_if_oam_b.gb")
GBMICRO_TEST(148, "lcdon_to_ly1_a.gb")
GBMICRO_TEST(149, "lcdon_to_ly1_b.gb")
GBMICRO_TEST(150, "lcdon_to_ly2_a.gb")
GBMICRO_TEST(151, "lcdon_to_ly2_b.gb")
GBMICRO_TEST(152, "lcdon_to_ly3_a.gb")
GBMICRO_TEST(153, "lcdon_to_ly3_b.gb")
GBMICRO_TEST(154, "lcdon_to_lyc1_int.gb")
GBMICRO_TEST(155, "lcdon_to_lyc2_int.gb")
GBMICRO_TEST(156, "lcdon_to_lyc3_int.gb")
GBMICRO_TEST(157, "lcdon_to_oam_int_l0.gb")
GBMICRO_TEST(158, "lcdon_to_oam_int_l1.gb")
GBMICRO_TEST(159, "lcdon_to_oam_int_l2.gb")
GBMICRO_TEST(160, "lcdon_to_oam_unlock_a.gb")
GBMICRO_TEST(161, "lcdon_to_oam_unlock_b.gb")
GBMICRO_TEST(162, "lcdon_to_oam_unlock_c.gb")
GBMICRO_TEST(163, "lcdon_to_oam_unlock_d.gb")
GBMICRO_TEST(164, "lcdon_to_stat0_a.gb")
GBMICRO_TEST(165, "lcdon_to_stat0_b.gb")
GBMICRO_TEST(166, "lcdon_to_stat0_c.gb")
GBMICRO_TEST(167, "lcdon_to_stat0_d.gb")
GBMICRO_TEST(168, "lcdon_to_stat1_a.gb")
GBMICRO_TEST(169, "lcdon_to_stat1_b.gb")
GBMICRO_TEST(170, "lcdon_to_stat1_c.gb")
GBMICRO_TEST(171, "lcdon_to_stat1_d.gb")
GBMICRO_TEST(172, "lcdon_to_stat1_e.gb")
GBMICRO_TEST(173, "lcdon_to_stat2_a.gb")
GBMICRO_TEST(174, "lcdon_to_stat2_b.gb")
GBMICRO_TEST(175, "lcdon_to_stat2_c.gb")
GBMICRO_TEST(176, "lcdon_to_stat2_d.gb")
GBMICRO_TEST(177, "lcdon_to_stat3_a.gb")
GBMICRO_TEST(178, "lcdon_to_stat3_b.gb")
GBMICRO_TEST(179, "lcdon_to_stat3_c.gb")
GBMICRO_TEST(180, "lcdon_to_stat3_d.gb")
GBMICRO_TEST(181, "lcdon_write_timing.gb")
GBMICRO_TEST(182, "line_144_oam_int_a.gb")
GBMICRO_TEST(183, "line_144_oam_int_b.gb")
GBMICRO_TEST(184, "line_144_oam_int_c.gb")
GBMICRO_TEST(185, "line_144_oam_int_d.gb")
GBMICRO_TEST(186, "line_153_ly_a.gb")
GBMICRO_TEST(187, "line_153_ly_b.gb")
GBMICRO_TEST(188, "line_153_ly_c.gb")
GBMICRO_TEST(189, "line_153_ly_d.gb")
GBMICRO_TEST(190, "line_153_ly_e.gb")
GBMICRO_TEST(191, "line_153_ly_f.gb")
GBMICRO_TEST(192, "line_153_lyc0_int_inc_sled.gb")
GBMICRO_TEST(193, "line_153_lyc0_stat_timing_a.gb")
GBMICRO_TEST(194, "line_153_lyc0_stat_timing_b.gb")
GBMICRO_TEST(195, "line_153_lyc0_stat_timing_c.gb")
GBMICRO_TEST(196, "line_153_lyc0_stat_timing_d.gb")
GBMICRO_TEST(197, "line_153_lyc0_stat_timing_e.gb")
GBMICRO_TEST(198, "line_153_lyc0_stat_timing_f.gb")
GBMICRO_TEST(199, "line_153_lyc0_stat_timing_g.gb")
GBMICRO_TEST(200, "line_153_lyc0_stat_timing_h.gb")
GBMICRO_TEST(201, "line_153_lyc0_stat_timing_i.gb")
GBMICRO_TEST(202, "line_153_lyc0_stat_timing_j.gb")
GBMICRO_TEST(203, "line_153_lyc0_stat_timing_k.gb")
GBMICRO_TEST(204, "line_153_lyc0_stat_timing_l.gb")
GBMICRO_TEST(205, "line_153_lyc0_stat_timing_m.gb")
GBMICRO_TEST(206, "line_153_lyc0_stat_timing_n.gb")
GBMICRO_TEST(207, "line_153_lyc153_stat_timing_a.gb")
GBMICRO_TEST(208, "line_153_lyc153_stat_timing_b.gb")
GBMICRO_TEST(209, "line_153_lyc153_stat_timing_c.gb")
GBMICRO_TEST(210, "line_153_lyc153_stat_timing_d.gb")
GBMICRO_TEST(211, "line_153_lyc153_stat_timing_e.gb")
GBMICRO_TEST(212, "line_153_lyc153_stat_timing_f.gb")
GBMICRO_TEST(213, "line_153_lyc_a.gb")
GBMICRO_TEST(214, "line_153_lyc_b.gb")
GBMICRO_TEST(215, "line_153_lyc_c.gb")
GBMICRO_TEST(216, "line_153_lyc_int_a.gb")
GBMICRO_TEST(217, "line_153_lyc_int_b.gb")
GBMICRO_TEST(218, "line_65_ly.gb")
GBMICRO_TEST(219, "ly_while_lcd_off.gb")
GBMICRO_TEST(220, "lyc1_int_halt_a.gb")
GBMICRO_TEST(221, "lyc1_int_halt_b.gb")
GBMICRO_TEST(222, "lyc1_int_if_edge_a.gb")
GBMICRO_TEST(223, "lyc1_int_if_edge_b.gb")
GBMICRO_TEST(224, "lyc1_int_if_edge_c.gb")
GBMICRO_TEST(225, "lyc1_int_if_edge_d.gb")
GBMICRO_TEST(226, "lyc1_int_nops_a.gb")
GBMICRO_TEST(227, "lyc1_int_nops_b.gb")
GBMICRO_TEST(228, "lyc1_write_timing_a.gb")
GBMICRO_TEST(229, "lyc1_write_timing_b.gb")
GBMICRO_TEST(230, "lyc1_write_timing_c.gb")
GBMICRO_TEST(231, "lyc1_write_timing_d.gb")
GBMICRO_TEST(232, "lyc2_int_halt_a.gb")
GBMICRO_TEST(233, "lyc2_int_halt_b.gb")
GBMICRO_TEST(234, "lyc_int_halt_a.gb")
GBMICRO_TEST(235, "lyc_int_halt_b.gb")
GBMICRO_TEST(236, "mbc1_ram_banks.gb")
GBMICRO_TEST(237, "mbc1_rom_banks.gb")
GBMICRO_TEST(238, "minimal.gb")
GBMICRO_TEST(239, "mode2_stat_int_to_oam_unlock.gb")
GBMICRO_TEST(240, "oam_int_halt_a.gb")
GBMICRO_TEST(241, "oam_int_halt_b.gb")
GBMICRO_TEST(242, "oam_int_if_edge_a.gb")
GBMICRO_TEST(243, "oam_int_if_edge_b.gb")
GBMICRO_TEST(244, "oam_int_if_edge_c.gb")
GBMICRO_TEST(245, "oam_int_if_edge_d.gb")
GBMICRO_TEST(246, "oam_int_if_level_c.gb")
GBMICRO_TEST(247, "oam_int_if_level_d.gb")
GBMICRO_TEST(248, "oam_int_inc_sled.gb")
GBMICRO_TEST(249, "oam_int_nops_a.gb")
GBMICRO_TEST(250, "oam_int_nops_b.gb")
GBMICRO_TEST(251, "oam_read_l0_a.gb")
GBMICRO_TEST(252, "oam_read_l0_b.gb")
GBMICRO_TEST(253, "oam_read_l0_c.gb")
GBMICRO_TEST(254, "oam_read_l0_d.gb")
GBMICRO_TEST(255, "oam_read_l1_a.gb")
GBMICRO_TEST(256, "oam_read_l1_b.gb")
GBMICRO_TEST(257, "oam_read_l1_c.gb")
GBMICRO_TEST(258, "oam_read_l1_d.gb")
GBMICRO_TEST(259, "oam_read_l1_e.gb")
GBMICRO_TEST(260, "oam_read_l1_f.gb")
GBMICRO_TEST(261, "oam_sprite_trashing.gb")
GBMICRO_TEST(262, "oam_write_l0_a.gb")
GBMICRO_TEST(263, "oam_write_l0_b.gb")
GBMICRO_TEST(264, "oam_write_l0_c.gb")
GBMICRO_TEST(265, "oam_write_l0_d.gb")
GBMICRO_TEST(266, "oam_write_l0_e.gb")
GBMICRO_TEST(267, "oam_write_l1_a.gb")
GBMICRO_TEST(268, "oam_write_l1_b.gb")
GBMICRO_TEST(269, "oam_write_l1_c.gb")
GBMICRO_TEST(270, "oam_write_l1_d.gb")
GBMICRO_TEST(271, "oam_write_l1_e.gb")
GBMICRO_TEST(272, "oam_write_l1_f.gb")
GBMICRO_TEST(273, "poweron.gb")
GBMICRO_TEST(274, "poweron_bgp_000.gb")
GBMICRO_TEST(275, "poweron_div_000.gb")
GBMICRO_TEST(276, "poweron_div_004.gb")
GBMICRO_TEST(277, "poweron_div_005.gb")
GBMICRO_TEST(278, "poweron_dma_000.gb")
GBMICRO_TEST(279, "poweron_if_000.gb")
GBMICRO_TEST(280, "poweron_joy_000.gb")
GBMICRO_TEST(281, "poweron_lcdc_000.gb")
GBMICRO_TEST(282, "poweron_ly_000.gb")
GBMICRO_TEST(283, "poweron_ly_119.gb")
GBMICRO_TEST(284, "poweron_ly_120.gb")
GBMICRO_TEST(285, "poweron_ly_233.gb")
GBMICRO_TEST(286, "poweron_ly_234.gb")
GBMICRO_TEST(287, "poweron_lyc_000.gb")
GBMICRO_TEST(288, "poweron_oam_000.gb")
GBMICRO_TEST(289, "poweron_oam_005.gb")
GBMICRO_TEST(290, "poweron_oam_006.gb")
GBMICRO_TEST(291, "poweron_oam_069.gb")
GBMICRO_TEST(292, "poweron_oam_070.gb")
GBMICRO_TEST(293, "poweron_oam_119.gb")
GBMICRO_TEST(294, "poweron_oam_120.gb")
GBMICRO_TEST(295, "poweron_oam_121.gb")
GBMICRO_TEST(296, "poweron_oam_183.gb")
GBMICRO_TEST(297, "poweron_oam_184.gb")
GBMICRO_TEST(298, "poweron_oam_233.gb")
GBMICRO_TEST(299, "poweron_oam_234.gb")
GBMICRO_TEST(300, "poweron_oam_235.gb")
GBMICRO_TEST(301, "poweron_obp0_000.gb")
GBMICRO_TEST(302, "poweron_obp1_000.gb")
GBMICRO_TEST(303, "poweron_sb_000.gb")
GBMICRO_TEST(304, "poweron_sc_000.gb")
GBMICRO_TEST(305, "poweron_scx_000.gb")
GBMICRO_TEST(306, "poweron_scy_000.gb")
GBMICRO_TEST(307, "poweron_stat_000.gb")
GBMICRO_TEST(308, "poweron_stat_005.gb")
GBMICRO_TEST(309, "poweron_stat_006.gb")
GBMICRO_TEST(310, "poweron_stat_007.gb")
GBMICRO_TEST(311, "poweron_stat_026.gb")
GBMICRO_TEST(312, "poweron_stat_027.gb")
GBMICRO_TEST(313, "poweron_stat_069.gb")
GBMICRO_TEST(314, "poweron_stat_070.gb")
GBMICRO_TEST(315, "poweron_stat_119.gb")
GBMICRO_TEST(316, "poweron_stat_120.gb")
GBMICRO_TEST(317, "poweron_stat_121.gb")
GBMICRO_TEST(318, "poweron_stat_140.gb")
GBMICRO_TEST(319, "poweron_stat_141.gb")
GBMICRO_TEST(320, "poweron_stat_183.gb")
GBMICRO_TEST(321, "poweron_stat_184.gb")
GBMICRO_TEST(322, "poweron_stat_234.gb")
GBMICRO_TEST(323, "poweron_stat_235.gb")
GBMICRO_TEST(324, "poweron_tac_000.gb")
GBMICRO_TEST(325, "poweron_tima_000.gb")
GBMICRO_TEST(326, "poweron_tma_000.gb")
GBMICRO_TEST(327, "poweron_vram_000.gb")
GBMICRO_TEST(328, "poweron_vram_025.gb")
GBMICRO_TEST(329, "poweron_vram_026.gb")
GBMICRO_TEST(330, "poweron_vram_069.gb")
GBMICRO_TEST(331, "poweron_vram_070.gb")
GBMICRO_TEST(332, "poweron_vram_139.gb")
GBMICRO_TEST(333, "poweron_vram_140.gb")
GBMICRO_TEST(334, "poweron_vram_183.gb")
GBMICRO_TEST(335, "poweron_vram_184.gb")
GBMICRO_TEST(336, "poweron_wx_000.gb")
GBMICRO_TEST(337, "poweron_wy_000.gb")
GBMICRO_TEST(338, "ppu_scx_vs_bgp.gb")
GBMICRO_TEST(339, "ppu_sprite0_scx0_a.gb")
GBMICRO_TEST(340, "ppu_sprite0_scx0_b.gb")
GBMICRO_TEST(341, "ppu_sprite0_scx1_a.gb")
GBMICRO_TEST(342, "ppu_sprite0_scx1_b.gb")
GBMICRO_TEST(343, "ppu_sprite0_scx2_a.gb")
GBMICRO_TEST(344, "ppu_sprite0_scx2_b.gb")
GBMICRO_TEST(345, "ppu_sprite0_scx3_a.gb")
GBMICRO_TEST(346, "ppu_sprite0_scx3_b.gb")
GBMICRO_TEST(347, "ppu_sprite0_scx4_a.gb")
GBMICRO_TEST(348, "ppu_sprite0_scx4_b.gb")
GBMICRO_TEST(349, "ppu_sprite0_scx5_a.gb")
GBMICRO_TEST(350, "ppu_sprite0_scx5_b.gb")
GBMICRO_TEST(351, "ppu_sprite0_scx6_a.gb")
GBMICRO_TEST(352, "ppu_sprite0_scx6_b.gb")
GBMICRO_TEST(353, "ppu_sprite0_scx7_a.gb")
GBMICRO_TEST(354, "ppu_sprite0_scx7_b.gb")
GBMICRO_TEST(355, "ppu_sprite_testbench.gb")
GBMICRO_TEST(356, "ppu_spritex_vs_scx.gb")
GBMICRO_TEST(357, "ppu_win_vs_wx.gb")
GBMICRO_TEST(358, "ppu_wx_early.gb")
GBMICRO_TEST(359, "sprite4_0_a.gb")
GBMICRO_TEST(360, "sprite4_0_b.gb")
GBMICRO_TEST(361, "sprite4_1_a.gb")
GBMICRO_TEST(362, "sprite4_1_b.gb")
GBMICRO_TEST(363, "sprite4_2_a.gb")
GBMICRO_TEST(364, "sprite4_2_b.gb")
GBMICRO_TEST(365, "sprite4_3_a.gb")
GBMICRO_TEST(366, "sprite4_3_b.gb")
GBMICRO_TEST(367, "sprite4_4_a.gb")
GBMICRO_TEST(368, "sprite4_4_b.gb")
GBMICRO_TEST(369, "sprite4_5_a.gb")
GBMICRO_TEST(370, "sprite4_5_b.gb")
GBMICRO_TEST(371, "sprite4_6_a.gb")
GBMICRO_TEST(372, "sprite4_6_b.gb")
GBMICRO_TEST(373, "sprite4_7_a.gb")
GBMICRO_TEST(374, "sprite4_7_b.gb")
GBMICRO_TEST(375, "sprite_0_a.gb")
GBMICRO_TEST(376, "sprite_0_b.gb")
GBMICRO_TEST(377, "sprite_1_a.gb")
GBMICRO_TEST(378, "sprite_1_b.gb")
GBMICRO_TEST(379, "stat_write_glitch_l0_a.gb")
GBMICRO_TEST(380, "stat_write_glitch_l0_b.gb")
GBMICRO_TEST(381, "stat_write_glitch_l0_c.gb")
GBMICRO_TEST(382, "stat_write_glitch_l143_a.gb")
GBMICRO_TEST(383, "stat_write_glitch_l143_b.gb")
GBMICRO_TEST(384, "stat_write_glitch_l143_c.gb")
GBMICRO_TEST(385, "stat_write_glitch_l143_d.gb")
GBMICRO_TEST(386, "stat_write_glitch_l154_a.gb")
GBMICRO_TEST(387, "stat_write_glitch_l154_b.gb")
GBMICRO_TEST(388, "stat_write_glitch_l154_c.gb")
GBMICRO_TEST(389, "stat_write_glitch_l154_d.gb")
GBMICRO_TEST(390, "stat_write_glitch_l1_a.gb")
GBMICRO_TEST(391, "stat_write_glitch_l1_b.gb")
GBMICRO_TEST(392, "stat_write_glitch_l1_c.gb")
GBMICRO_TEST(393, "stat_write_glitch_l1_d.gb")
GBMICRO_TEST(394, "temp.gb")
GBMICRO_TEST(395, "timer_div_phase_c.gb")
GBMICRO_TEST(396, "timer_div_phase_d.gb")
GBMICRO_TEST(397, "timer_tima_inc_256k_a.gb")
GBMICRO_TEST(398, "timer_tima_inc_256k_b.gb")
GBMICRO_TEST(399, "timer_tima_inc_256k_c.gb")
GBMICRO_TEST(400, "timer_tima_inc_256k_d.gb")
GBMICRO_TEST(401, "timer_tima_inc_256k_e.gb")
GBMICRO_TEST(402, "timer_tima_inc_256k_f.gb")
GBMICRO_TEST(403, "timer_tima_inc_256k_g.gb")
GBMICRO_TEST(404, "timer_tima_inc_256k_h.gb")
GBMICRO_TEST(405, "timer_tima_inc_256k_i.gb")
GBMICRO_TEST(406, "timer_tima_inc_256k_j.gb")
GBMICRO_TEST(407, "timer_tima_inc_256k_k.gb")
GBMICRO_TEST(408, "timer_tima_inc_64k_a.gb")
GBMICRO_TEST(409, "timer_tima_inc_64k_b.gb")
GBMICRO_TEST(410, "timer_tima_inc_64k_c.gb")
GBMICRO_TEST(411, "timer_tima_inc_64k_d.gb")
GBMICRO_TEST(412, "timer_tima_phase_a.gb")
GBMICRO_TEST(413, "timer_tima_phase_b.gb")
GBMICRO_TEST(414, "timer_tima_phase_c.gb")
GBMICRO_TEST(415, "timer_tima_phase_d.gb")
GBMICRO_TEST(416, "timer_tima_phase_e.gb")
GBMICRO_TEST(417, "timer_tima_phase_f.gb")
GBMICRO_TEST(418, "timer_tima_phase_g.gb")
GBMICRO_TEST(419, "timer_tima_phase_h.gb")
GBMICRO_TEST(420, "timer_tima_phase_i.gb")
GBMICRO_TEST(421, "timer_tima_phase_j.gb")
GBMICRO_TEST(422, "timer_tima_reload_256k_a.gb")
GBMICRO_TEST(423, "timer_tima_reload_256k_b.gb")
GBMICRO_TEST(424, "timer_tima_reload_256k_c.gb")
GBMICRO_TEST(425, "timer_tima_reload_256k_d.gb")
GBMICRO_TEST(426, "timer_tima_reload_256k_e.gb")
GBMICRO_TEST(427, "timer_tima_reload_256k_f.gb")
GBMICRO_TEST(428, "timer_tima_reload_256k_g.gb")
GBMICRO_TEST(429, "timer_tima_reload_256k_h.gb")
GBMICRO_TEST(430, "timer_tima_reload_256k_i.gb")
GBMICRO_TEST(431, "timer_tima_reload_256k_j.gb")
GBMICRO_TEST(432, "timer_tima_reload_256k_k.gb")
GBMICRO_TEST(433, "timer_tima_write_a.gb")
GBMICRO_TEST(434, "timer_tima_write_b.gb")
GBMICRO_TEST(435, "timer_tima_write_c.gb")
GBMICRO_TEST(436, "timer_tima_write_d.gb")
GBMICRO_TEST(437, "timer_tima_write_e.gb")
GBMICRO_TEST(438, "timer_tima_write_f.gb")
GBMICRO_TEST(439, "timer_tma_write_a.gb")
GBMICRO_TEST(440, "timer_tma_write_b.gb")
GBMICRO_TEST(441, "toggle_lcdc.gb")
GBMICRO_TEST(442, "vblank2_int_halt_a.gb")
GBMICRO_TEST(443, "vblank2_int_halt_b.gb")
GBMICRO_TEST(444, "vblank2_int_if_a.gb")
GBMICRO_TEST(445, "vblank2_int_if_b.gb")
GBMICRO_TEST(446, "vblank2_int_if_c.gb")
GBMICRO_TEST(447, "vblank2_int_if_d.gb")
GBMICRO_TEST(448, "vblank2_int_inc_sled.gb")
GBMICRO_TEST(449, "vblank2_int_nops_a.gb")
GBMICRO_TEST(450, "vblank2_int_nops_b.gb")
GBMICRO_TEST(451, "vblank_int_halt_a.gb")
GBMICRO_TEST(452, "vblank_int_halt_b.gb")
GBMICRO_TEST(453, "vblank_int_if_a.gb")
GBMICRO_TEST(454, "vblank_int_if_b.gb")
GBMICRO_TEST(455, "vblank_int_if_c.gb")
GBMICRO_TEST(456, "vblank_int_if_d.gb")
GBMICRO_TEST(457, "vblank_int_inc_sled.gb")
GBMICRO_TEST(458, "vblank_int_nops_a.gb")
GBMICRO_TEST(459, "vblank_int_nops_b.gb")
GBMICRO_TEST(460, "vram_read_l0_a.gb")
GBMICRO_TEST(461, "vram_read_l0_b.gb")
GBMICRO_TEST(462, "vram_read_l0_c.gb")
GBMICRO_TEST(463, "vram_read_l0_d.gb")
GBMICRO_TEST(464, "vram_read_l1_a.gb")
GBMICRO_TEST(465, "vram_read_l1_b.gb")
GBMICRO_TEST(466, "vram_read_l1_c.gb")
GBMICRO_TEST(467, "vram_read_l1_d.gb")
GBMICRO_TEST(468, "vram_write_l0_a.gb")
GBMICRO_TEST(469, "vram_write_l0_b.gb")
GBMICRO_TEST(470, "vram_write_l0_c.gb")
GBMICRO_TEST(471, "vram_write_l0_d.gb")
GBMICRO_TEST(472, "vram_write_l1_a.gb")
GBMICRO_TEST(473, "vram_write_l1_b.gb")
GBMICRO_TEST(474, "vram_write_l1_c.gb")
GBMICRO_TEST(475, "vram_write_l1_d.gb")
GBMICRO_TEST(476, "wave_write_to_0xC003.gb")
GBMICRO_TEST(477, "win0_a.gb")
GBMICRO_TEST(478, "win0_b.gb")
GBMICRO_TEST(479, "win0_scx3_a.gb")
GBMICRO_TEST(480, "win0_scx3_b.gb")
GBMICRO_TEST(481, "win10_a.gb")
GBMICRO_TEST(482, "win10_b.gb")
GBMICRO_TEST(483, "win10_scx3_a.gb")
GBMICRO_TEST(484, "win10_scx3_b.gb")
GBMICRO_TEST(485, "win11_a.gb")
GBMICRO_TEST(486, "win11_b.gb")
GBMICRO_TEST(487, "win12_a.gb")
GBMICRO_TEST(488, "win12_b.gb")
GBMICRO_TEST(489, "win13_a.gb")
GBMICRO_TEST(490, "win13_b.gb")
GBMICRO_TEST(491, "win14_a.gb")
GBMICRO_TEST(492, "win14_b.gb")
GBMICRO_TEST(493, "win15_a.gb")
GBMICRO_TEST(494, "win15_b.gb")
GBMICRO_TEST(495, "win1_a.gb")
GBMICRO_TEST(496, "win1_b.gb")
GBMICRO_TEST(497, "win2_a.gb")
GBMICRO_TEST(498, "win2_b.gb")
GBMICRO_TEST(499, "win3_a.gb")
GBMICRO_TEST(500, "win3_b.gb")
GBMICRO_TEST(501, "win4_a.gb")
GBMICRO_TEST(502, "win4_b.gb")
GBMICRO_TEST(503, "win5_a.gb")
GBMICRO_TEST(504, "win5_b.gb")
GBMICRO_TEST(505, "win6_a.gb")
GBMICRO_TEST(506, "win6_b.gb")
GBMICRO_TEST(507, "win7_a.gb")
GBMICRO_TEST(508, "win7_b.gb")
GBMICRO_TEST(509, "win8_a.gb")
GBMICRO_TEST(510, "win8_b.gb")
GBMICRO_TEST(511, "win9_a.gb")
GBMICRO_TEST(512, "win9_b.gb")

#undef GBMICRO_TEST

#endif //STARGBC_GBMICROTEST_H
