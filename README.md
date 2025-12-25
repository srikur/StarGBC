# StarGBC

<p align="center">
  <img src="images/blue_title.png" width="30%" />
  <img src="images/blue_ingame.png" width="30%" />
  <img src="images/gold_title.png" width="30%" />
  <br/>
  <img src="images/gold_ingame.png" width="30%" />
  <img src="images/smb_title.png" width="30%" />
  <img src="images/crystal_title.png" width="30%" />
</p>

## Dependencies

1. [SDL3](https://github.com/libsdl-org/SDL)
2. [doctest](https://github.com/doctest/doctest)
3. [Ninja](https://github.com/ninja-build/ninja)

## Building

1. Clone the repository:
   ```bash
   git clone https://github.com/srikur/StarGBC.git
2. Update submodules:
   ```bash
   git submodule update --init --recursive
   ```
3. Configure CMake:
   ```bash
   cmake -DCMAKE_MAKE_PROGRAM=/path/to/ninja -G Ninja -DBUILD_SHARED_LIBS=false -S . -B build
   ```
4. Build the project:
   ```bash
   cmake --build build
   ```

## Test ROM Performance

Current Performance: (150/270)

| Test                                                                  | StarGBC            |
|-----------------------------------------------------------------------|--------------------|
| acid/dmg-acid2.gb                                                     | :white_check_mark: |
| acid/cgb-acid2.gbc                                                    | :white_check_mark: |
| acid/cgb-acid-hell.gbc                                                | :x:                |
| blargg/cpu_instrs/01-special.gb                                       | :white_check_mark: |
| blargg/cpu_instrs/02-interrupts.gb                                    | :white_check_mark: |
| blargg/cpu_instrs/03-op sp,hl.gb                                      | :white_check_mark: |
| blargg/cpu_instrs/04-op r,imm.gb                                      | :white_check_mark: |
| blargg/cpu_instrs/05-op rp.gb                                         | :white_check_mark: |
| blargg/cpu_instrs/06-ld r,r.gb                                        | :white_check_mark: |
| blargg/cpu_instrs/07-jr,jp,call,ret,rst.gb                            | :white_check_mark: |
| blargg/cpu_instrs/08-misc instrs.gb                                   | :white_check_mark: |
| blargg/cpu_instrs/09-op r,r.gb                                        | :white_check_mark: |
| blargg/cpu_instrs/10-bit ops.gb                                       | :white_check_mark: |
| blargg/cpu_instrs/11-op a,(hl).gb                                     | :white_check_mark: |
| blargg/halt_bug.gb                                                    | :white_check_mark: |
| blargg/instr_timing.gb                                                | :white_check_mark: |
| blargg/interrupt_time.gb                                              | :white_check_mark: |
| blargg/mem_timing/01-read_timing.gb                                   | :white_check_mark: |
| blargg/mem_timing/02-write_timing.gb                                  | :white_check_mark: |
| blargg/mem_timing/03-modify_timing.gb                                 | :white_check_mark: |
| blargg/mem_timing-2/01-read_timing.gb                                 | :white_check_mark: |
| blargg/mem_timing-2/02-write_timing.gb                                | :white_check_mark: |
| blargg/mem_timing-2/03-modify_timing.gb                               | :white_check_mark: |
| blargg/oam_bug/1-lcd_sync.gb                                          | :white_check_mark: |
| blargg/oam_bug/2-causes.gb                                            | :white_check_mark: |
| blargg/oam_bug/3-non_causes.gb                                        | :white_check_mark: |
| blargg/oam_bug/4-scanline_timing.gb                                   | :white_check_mark: |
| blargg/oam_bug/5-timing_bug.gb                                        | :white_check_mark: |
| blargg/oam_bug/6-timing_no_bug.gb                                     | :white_check_mark: |
| blargg/oam_bug/8-instr_effect.gb                                      | :white_check_mark: |
| blargg/dmg_sound/01-registers.gb                                      | :white_check_mark: |
| blargg/dmg_sound/02-len ctr.gb                                        | :white_check_mark: |
| blargg/dmg_sound/03-trigger.gb                                        | :white_check_mark: |
| blargg/dmg_sound/04-sweep.gb                                          | :white_check_mark: |
| blargg/dmg_sound/05-sweep details.gb                                  | :white_check_mark: |
| blargg/dmg_sound/06-overflow on trigger.gb                            | :white_check_mark: |
| blargg/dmg_sound/07-len sweep period sync.gb                          | :white_check_mark: |
| blargg/dmg_sound/08-len ctr during power.gb                           | :white_check_mark: |
| blargg/dmg_sound/09-wave read while on.gb                             | :white_check_mark: |
| blargg/dmg_sound/10-wave trigger while on.gb                          | :white_check_mark: |
| blargg/dmg_sound/11-regs after power.gb                               | :white_check_mark: |
| blargg/dmg_sound/12-wave write while on.gb                            | :white_check_mark: |
| blargg/cgb_sound/01-registers.gb                                      | :white_check_mark: |
| blargg/cgb_sound/02-len ctr.gb                                        | :white_check_mark: |
| blargg/cgb_sound/03-trigger.gb                                        | :white_check_mark: |
| blargg/cgb_sound/04-sweep.gb                                          | :white_check_mark: |
| blargg/cgb_sound/05-sweep details.gb                                  | :white_check_mark: |
| blargg/cgb_sound/06-overflow on trigger.gb                            | :white_check_mark: |
| blargg/cgb_sound/07-len sweep period sync.gb                          | :white_check_mark: |
| blargg/cgb_sound/08-len ctr during power.gb                           | :white_check_mark: |
| blargg/cgb_sound/09-wave read while on.gb                             | :white_check_mark: |
| blargg/cgb_sound/10-wave trigger while on.gb                          | :white_check_mark: |
| blargg/cgb_sound/11-regs after power.gb                               | :white_check_mark: |
| blargg/cgb_sound/12-wave.gb                                           | :white_check_mark: |
| daid/ppu_scanline_bgp.gb (DMG)                                        | :x:                |
| daid/ppu_scanline_bgp.gb (GBC)                                        | :x:                |
| daid/stop_instr.gb (DMG)                                              | :white_check_mark: |
| daid/stop_instr.gb (GBC)                                              | :white_check_mark: |
| daid/stop_instr_gbc_mode3.gb                                          | :x:                |
| daid/speed_switch_timing_ly.gbc                                       | :x:                |
| ax6/rtc3test-1.gb                                                     | :white_check_mark: |
| ax6/rtc3test-2.gb                                                     | :white_check_mark: |
| ax6/rtc3test-3.gb                                                     | :white_check_mark: |
| mooneye/acceptance/add_sp_e_timing.gb                                 | :white_check_mark: |
| mooneye/acceptance/bits/mem_oam.gb                                    | :white_check_mark: |
| mooneye/acceptance/bits/reg_f.gb                                      | :white_check_mark: |
| mooneye/acceptance/bits/unused_hwio-GS.gb                             | :x:                |
| mooneye/acceptance/boot_div-dmgABCmgb.gb                              | :x:                |
| mooneye/acceptance/boot_hwio-dmgABCmgb.gb                             | :x:                |
| mooneye/acceptance/boot_regs-dmgABC.gb                                | :white_check_mark: |
| mooneye/acceptance/boot_div-dmg0.gb                                   | :x:                |
| mooneye/acceptance/boot_div-S.gb                                      | :x:                |
| mooneye/acceptance/boot_div2-s.gb                                     | :x:                |
| mooneye/acceptance/boot_hwio-dmg0.gb                                  | :x:                |
| mooneye/acceptance/boot_hwio-S.gb                                     | :x:                |
| mooneye/acceptance/boot_regs-dmg0.gb                                  | :white_check_mark: |
| mooneye/acceptance/boot_regs-mgb.gb                                   | :white_check_mark: |
| mooneye/acceptance/boot_regs-sgb.gb                                   | :white_check_mark: |
| mooneye/acceptance/boot_regs-sgb2.gb                                  | :white_check_mark: |
| mooneye/acceptance/call_cc_timing.gb                                  | :white_check_mark: |
| mooneye/acceptance/call_cc_timing2.gb                                 | :white_check_mark: |
| mooneye/acceptance/call_timing.gb                                     | :white_check_mark: |
| mooneye/acceptance/call_timing2.gb                                    | :white_check_mark: |
| mooneye/acceptance/div_timing.gb                                      | :white_check_mark: |
| mooneye/acceptance/di_timing-GS.gb                                    | :white_check_mark: |
| mooneye/acceptance/ei_sequence.gb                                     | :white_check_mark: |
| mooneye/acceptance/ei_timing.gb                                       | :white_check_mark: |
| mooneye/acceptance/halt_ime0_ei.gb                                    | :white_check_mark: |
| mooneye/acceptance/halt_ime0_nointr_timing.gb                         | :white_check_mark: |
| mooneye/acceptance/halt_ime1_timing.gb                                | :white_check_mark: |
| mooneye/acceptance/halt_ime1_timing2-GS.gb                            | :white_check_mark: |
| mooneye/acceptance/if_ie_registers.gb                                 | :white_check_mark: |
| mooneye/acceptance/instr/daa.gb                                       | :white_check_mark: |
| mooneye/acceptance/interrupts/ie_push.gb                              | :white_check_mark: |
| mooneye/acceptance/intr_timing.gb                                     | :white_check_mark: |
| mooneye/acceptance/jp_cc_timing.gb                                    | :white_check_mark: |
| mooneye/acceptance/jp_timing.gb                                       | :white_check_mark: |
| mooneye/acceptance/ld_hl_sp_e_timing.gb                               | :white_check_mark: |
| mooneye/acceptance/oam_dma/basic.gb                                   | :white_check_mark: |
| mooneye/acceptance/oam_dma/reg_read.gb                                | :white_check_mark: |
| mooneye/acceptance/oam_dma/sources-GS.gb                              | :white_check_mark: |
| mooneye/acceptance/oam_dma_restart.gb                                 | :white_check_mark: |
| mooneye/acceptance/oma_dma_start.gb                                   | :white_check_mark: |
| mooneye/acceptance/oam_dma_timing.gb                                  | :white_check_mark: |
| mooneye/acceptance/pop_timing.gb                                      | :white_check_mark: |
| mooneye/acceptance/ppu/hblank_ly_scx_timing-GS.gb                     | :x:                |
| mooneye/acceptance/ppu/intr_1_2_timing-GS.gb                          | :white_check_mark: |
| mooneye/acceptance/ppu/intr_2_0_timing.gb                             | :white_check_mark: |
| mooneye/acceptance/ppu/intr_2_mode0_timing.gb                         | :white_check_mark: |
| mooneye/acceptance/ppu/intr_2_mode0_timing_sprites.gb                 | :x:                |
| mooneye/acceptance/ppu/intr_2_mode3_timing.gb                         | :white_check_mark: |
| mooneye/acceptance/ppu/intr_2_oam_ok_timing.gb                        | :x:                |
| mooneye/acceptance/ppu/lcdon_timing-GS.gb                             | :x:                |
| mooneye/acceptance/ppu/lcdon_write_timing-GS.gb                       | :x:                |
| mooneye/acceptance/ppu/stat_irq_blocking.gb                           | :x:                |
| mooneye/acceptance/ppu/stat_lyc_onoff.gb                              | :x:                |
| mooneye/acceptance/ppu/vblank_stat_intr-GS.gb                         | :x:                |
| mooneye/acceptance/push_timing.gb                                     | :white_check_mark: |
| mooneye/acceptance/rapid_di_ei.gb                                     | :white_check_mark: |
| mooneye/acceptance/reti_intr_timing.gb                                | :white_check_mark: |
| mooneye/acceptance/reti_timing.gb                                     | :white_check_mark: |
| mooneye/acceptance/ret_cc_timing.gb                                   | :white_check_mark: |
| mooneye/acceptance/ret_timing.gb                                      | :white_check_mark: |
| mooneye/acceptance/rst_timing.gb                                      | :white_check_mark: |
| mooneye/acceptance/serial/boot_sclk_align-dmgABCmgb.gb                | :white_check_mark: |
| mooneye/acceptance/timer/div_write.gb                                 | :white_check_mark: |
| mooneye/acceptance/timer/rapid_toggle.gb                              | :white_check_mark: |
| mooneye/acceptance/timer/tim00.gb                                     | :white_check_mark: |
| mooneye/acceptance/timer/tim00_div_trigger.gb                         | :white_check_mark: |
| mooneye/acceptance/timer/tim01.gb                                     | :white_check_mark: |
| mooneye/acceptance/timer/tim01_div_trigger.gb                         | :white_check_mark: |
| mooneye/acceptance/timer/tim10.gb                                     | :white_check_mark: |
| mooneye/acceptance/timer/tim10_div_trigger.gb                         | :white_check_mark: |
| mooneye/acceptance/timer/tim11.gb                                     | :white_check_mark: |
| mooneye/acceptance/timer/tim11_div_trigger.gb                         | :white_check_mark: |
| mooneye/acceptance/timer/tima_reload.gb                               | :white_check_mark: |
| mooneye/acceptance/timer/tima_write_reloading.gb                      | :white_check_mark: |
| mooneye/acceptance/timer/tma_write_reloading.gb                       | :white_check_mark: |
| mooneye/emulator-only/mbc1/bits_bank1.gb                              | :white_check_mark: |
| mooneye/emulator-only/mbc1/bits_bank2.gb                              | :white_check_mark: |
| mooneye/emulator-only/mbc1/bits_mode.gb                               | :white_check_mark: |
| mooneye/emulator-only/mbc1/bits_ramg.gb                               | :white_check_mark: |
| mooneye/emulator-only/mbc1/multicart_rom_8Mb.gb                       | :white_check_mark: |
| mooneye/emulator-only/mbc1/ram_256kb.gb                               | :white_check_mark: |
| mooneye/emulator-only/mbc1/ram_64kb.gb                                | :white_check_mark: |
| mooneye/emulator-only/mbc1/rom_16Mb.gb                                | :white_check_mark: |
| mooneye/emulator-only/mbc1/rom_1Mb.gb                                 | :white_check_mark: |
| mooneye/emulator-only/mbc1/rom_2Mb.gb                                 | :white_check_mark: |
| mooneye/emulator-only/mbc1/rom_4Mb.gb                                 | :white_check_mark: |
| mooneye/emulator-only/mbc1/rom_512kb.gb                               | :white_check_mark: |
| mooneye/emulator-only/mbc1/rom_8Mb.gb                                 | :white_check_mark: |
| mooneye/emulator-only/mbc2/bits_ramg.gb                               | :white_check_mark: |
| mooneye/emulator-only/mbc2/bits_romb.gb                               | :white_check_mark: |
| mooneye/emulator-only/mbc2/bits_unused.gb                             | :white_check_mark: |
| mooneye/emulator-only/mbc2/ram.gb                                     | :white_check_mark: |
| mooneye/emulator-only/mbc2/rom_1Mb.gb                                 | :white_check_mark: |
| mooneye/emulator-only/mbc2/rom_2Mb.gb                                 | :white_check_mark: |
| mooneye/emulator-only/mbc2/rom_512kb.gb                               | :white_check_mark: |
| mooneye/emulator-only/mbc5/rom_16Mb.gb                                | :white_check_mark: |
| mooneye/emulator-only/mbc5/rom_1Mb.gb                                 | :white_check_mark: |
| mooneye/emulator-only/mbc5/rom_2Mb.gb                                 | :white_check_mark: |
| mooneye/emulator-only/mbc5/rom_32Mb.gb                                | :white_check_mark: |
| mooneye/emulator-only/mbc5/rom_4Mb.gb                                 | :white_check_mark: |
| mooneye/emulator-only/mbc5/rom_512kb.gb                               | :white_check_mark: |
| mooneye/emulator-only/mbc5/rom_64Mb.gb                                | :white_check_mark: |
| mooneye/emulator-only/mbc5/rom_8Mb.gb                                 | :white_check_mark: |
| mooneye/manual-only/sprite_priority.gb                                | :white_check_mark: |
| mooneye/misc/boot_div-cgbABCDE.gb                                     | :x:                |
| mooneye/misc/boot_regs-cgb.gb                                         | :white_check_mark: |
| mooneye/misc/boot_div-A.gb                                            | :x:                |
| mooneye/misc/boot_div-cgb0.gb                                         | :x:                |
| mooneye/misc/boot_hwio-C.gb                                           | :x:                |
| mooneye/misc/boot_regs-A.gb                                           | :x:                |
| mooneye/misc/bits/unused_hwio-C.gb                                    | :x:                |
| mooneye/misc/ppu/vblank_stat_intr-C.gb                                | :x:                |
| samesuite/apu/channel_1/channel_1_align.gb                            | :x:                |
| samesuite/apu/channel_1/channel_1_align-cpu.gb                        | :x:                |
| samesuite/apu/channel_1/channel_1_delay.gb                            | :x:                |
| samesuite/apu/channel_1/channel_1_duty.gb                             | :x:                |
| samesuite/apu/channel_1/channel_1_duty_delay.gb                       | :x:                |
| samesuite/apu/channel_1/channel_1_freq_change.gb                      | :x:                |
| samesuite/apu/channel_1/channel_1_nrx2_glitch.gb                      | :x:                |
| samesuite/apu/channel_1/channel_1_nrx2_speed_change.gb                | :x:                |
| samesuite/apu/channel_1/channel_1_restart.gb                          | :x:                |
| samesuite/apu/channel_1/channel_1_restart_nrx2_glitch.gb              | :x:                |
| samesuite/apu/channel_1/channel_1_stop_div.gb                         | :x:                |
| samesuite/apu/channel_1/channel_1_stop_restart.gb                     | :x:                |
| samesuite/apu/channel_1/channel_1_sweep.gb                            | :x:                |
| samesuite/apu/channel_1/channel_1_sweep_restart.gb                    | :x:                | 
| samesuite/apu/channel_1/channel_1_sweep_restart_2.gb                  | :x:                |
| samesuite/apu/channel_1/channel_1_volume.gb                           | :x:                |
| samesuite/apu/channel_1/channel_1_volume_div.gb                       | :x:                |
| samesuite/apu/channel_2/channel_2_align.gb                            | :x:                |
| samesuite/apu/channel_2/channel_2_align_cpu.gb                        | :x:                |
| samesuite/apu/channel_2/channel_2_delay.gb                            | :x:                |
| samesuite/apu/channel_2/channel_2_duty.gb                             | :x:                |
| samesuite/apu/channel_2/channel_2_duty_delay.gb                       | :x:                |
| samesuite/apu/channel_2/channel_2_freq_change.gb                      | :x:                |
| samesuite/apu/channel_2/channel_2_nrx2_glitch.gb                      | :x:                |
| samesuite/apu/channel_2/channel_2_nrx2_speed_change.gb                | :x:                |
| samesuite/apu/channel_2/channel_2_restart.gb                          | :x:                |
| samesuite/apu/channel_2/channel_2_restart_nrx2_glitch.gb              | :x:                |
| samesuite/apu/channel_2/channel_2_stop_div.gb                         | :x:                |
| samesuite/apu/channel_2/channel_2_stop_restart.gb                     | :x:                |
| samesuite/apu/channel_2/channel_2_volume_.gb                          | :x:                |
| samesuite/apu/channel_2/channel_2_volume_div.gb                       | :x:                |
| samesuite/apu/channel_3/channel_3_and_glitch.gb                       | :x:                |
| samesuite/apu/channel_3/channel_3_delay.gb                            | :x:                |
| samesuite/apu/channel_3/channel_3_first_sample.gb                     | :x:                |
| samesuite/apu/channel_3/channel_3_freq_change_delay.gb                | :x:                |
| samesuite/apu/channel_3/channel_3_restart_delay.gb                    | :x:                |
| samesuite/apu/channel_3/channel_3_restart_during_delay.gb             | :x:                |
| samesuite/apu/channel_3/channel_3_restart_stop_delay.gb               | :x:                |
| samesuite/apu/channel_3/channel_3_shift_delay.gb                      | :x:                |
| samesuite/apu/channel_3/channel_3_shift_skip_delay.gb                 | :x:                |
| samesuite/apu/channel_3/channel_3_stop_delay.gb                       | :x:                |
| samesuite/apu/channel_3/channel_3_stop_div.gb                         | :x:                |
| samesuite/apu/channel_3/channel_3_wave_ram_locked_write.gb            | :x:                |
| samesuite/apu/channel_3/channel_3_wave_ram_sync.gb                    | :x:                |
| samesuite/apu/channel_4/channel_4_align.gb                            | :x:                |
| samesuite/apu/channel_4/channel_4_delay.gb                            | :x:                |
| samesuite/apu/channel_4/channel_4_equivalent_frequencies.gb           | :x:                |
| samesuite/apu/channel_4/channel_4_frequency_alignment.gb              | :x:                |
| samesuite/apu/channel_4/channel_4_freq_change.gb                      | :x:                |
| samesuite/apu/channel_4/channel_4_lfsr.gb                             | :x:                |
| samesuite/apu/channel_4/channel_4_lfsr15.gb                           | :x:                |
| samesuite/apu/channel_4/channel_4_lfsr_15_7.gb                        | :x:                |
| samesuite/apu/channel_4/channel_4_lfsr_7_15.gb                        | :x:                |
| samesuite/apu/channel_4/channel_4_lfsr_restart.gb                     | :x:                |
| samesuite/apu/channel_4/channel_4_lfsr_restart_fast.gb                | :x:                |
| samesuite/apu/channel_4/channel_4_volume_div.gb                       | :x:                |
| samesuite/apu/div_write_trigger_10.gb                                 | :x:                |
| samesuite/apu/div_write_trigger_volume.gb                             | :x:                |
| samesuite/apu/div_write_trigger_volume_10.gb                          | :x:                |
| samesuite/dma/gbc_dma_cont.gb                                         | :white_check_mark: |
| samesuite/dma/gdma_addr_mask.gb                                       | :x:                |
| samesuite/dma/hdma_lcd_off.gb                                         | :x:                |
| samesuite/dma/hdma_mode0.gb                                           | :x:                |
| samesuite/ppu/blocking_bgpi_increase.gb                               | :x:                |
| samesuite/sgb/command_mit_req.gb                                      | :x:                |
| samesuite/sgb/command_mit_req_1_incrementing.gb                       | :x:                |
| hacktix/bully.gb (DMG)                                                | :x:                |
| hacktix/bully.gb (GBC)                                                | :x:                |
| hacktix/strikethrough.gb                                              | :x:                |
| cpp/rtc-invalid-banks-test.gb                                         | :white_check_mark: |
| cpp/latch-rtc-test.gb                                                 | :white_check_mark: |
| cpp/ramg-mbc3-test.gb                                                 | :white_check_mark: |
| mealybug-tearoom-tests/ppu/m2_win_en_toggle.gb (DMG)                  | :x:                |
| mealybug-tearoom-tests/ppu/m3_bgp_change.gb (DMG)                     | :x:                |
| mealybug-tearoom-tests/ppu/m3_bgp_change_sprites.gb (DMG)             | :x:                |
| mealybug-tearoom-tests/ppu/m3_lcdc_bg_en_change.gb (DMG)              | :x:                |
| mealybug-tearoom-tests/ppu/m3_lcdc_bg_map_change.gb (DMG)             | :x:                |
| mealybug-tearoom-tests/ppu/m3_lcdc_obj_en_change.gb (DMG)             | :x:                |
| mealybug-tearoom-tests/ppu/m3_lcdc_obj_en_change_variant.gb (DMG)     | :x:                |
| mealybug-tearoom-tests/ppu/m3_lcdc_obj_size_change.gb (DMG)           | :x:                |
| mealybug-tearoom-tests/ppu/m3_lcdc_obj_size_change_scx.gb (DMG)       | :x:                |
| mealybug-tearoom-tests/ppu/m3_lcdc_tile_sel_change.gb (DMG)           | :x:                |
| mealybug-tearoom-tests/ppu/m3_lcdc_tile_sel_win_change.gb (DMG)       | :x:                |
| mealybug-tearoom-tests/ppu/m3_lcdc_win_e_change_multiple.gb (DMG)     | :x:                |
| mealybug-tearoom-tests/ppu/m3_lcdc_win_en_change_multiple_wx.gb (DMG) | :x:                |
| mealybug-tearoom-tests/ppu/m3_lcdc_win_map_change.gb (DMG)            | :x:                |
| mealybug-tearoom-tests/ppu/m3_obp0_change_gb (DMG)                    | :x:                |
| mealybug-tearoom-tests/ppu/m3_scx_high_5_bits.gb (DMG)                | :x:                |
| mealybug-tearoom-tests/ppu/m3_scx_low_3_bits.gb (DMG)                 | :x:                |
| mealybug-tearoom-tests/ppu/m3_scy_change.gb (DMG)                     | :x:                |
| mealybug-tearoom-tests/ppu/m3_window_timing.gb (DMG)                  | :x:                |
| mealybug-tearoom-tests/ppu/m3_window_timing_wx_0.gb (DMG)             | :x:                |
| mealybug-tearoom-tests/ppu/m3_wx_4_change.gb (DMG)                    | :x:                |
| mealybug-tearoom-tests/ppu/m3_wx_4_change_sprites.gb (DMG)            | :x:                |
| mealybug-tearoom-tests/ppu/m3_wx_5_change.gb (DMG)                    | :x:                |
| mealybug-tearoom-tests/ppu/m3_x_6_change.gb (DMG)                     | :x:                |
| mbc3-tester/mbc3-tester.gb                                            | :white_check_mark: |