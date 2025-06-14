# StarGBC

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

### Blargg's tests

| Test                                  | StarGBC            |
|---------------------------------------|--------------------|
| cpu_instrs/01-special.gb              | :white_check_mark: |
| cpu_instrs/02-interrupts.gb           | :white_check_mark: |
| cpu_instrs/03-op sp,hl.gb             | :white_check_mark: |
| cpu_instrs/04-op r,imm.gb             | :white_check_mark: |
| cpu_instrs/05-op rp.gb                | :white_check_mark: |
| cpu_instrs/06-ld r,r.gb               | :white_check_mark: |
| cpu_instrs/07-jr,jp,call,ret,rst.gb   | :white_check_mark: |
| cpu_instrs/08-misc instrs.gb          | :white_check_mark: |
| cpu_instrs/09-op r,r.gb               | :white_check_mark: |
| cpu_instrs/10-bit ops.gb              | :white_check_mark: |
| cpu_instrs/11-op a,(hl).gb            | :white_check_mark: |
| instr_timing.gb                       | :white_check_mark: |
| mem_timing/01-read_timing.gb          | :white_check_mark: |
| mem_timing/02-write_timing.gb         | :white_check_mark: |
| mem_timing/03-modify_timing.gb        | :white_check_mark: |
| mem_timing-2/01-read_timing.gb        | :white_check_mark: |
| mem_timing-2/02-write_timing.gb       | :white_check_mark: |
| mem_timing-2/03-modify_timing.gb      | :white_check_mark: |
| interrupt_time                        | :x:                |
| halt_bug                              | :white_check_mark: |
| dmg_sound/01-registers.gb             | :x:                |
| dmg_sound/02-len ctr.gb               | :x:                |
| dmg_sound/03-trigger.gb               | :x:                |
| dmg_sound/04-sweep.gb                 | :x:                |
| dmg_sound/05-sweep details.gb         | :x:                |
| dmg_sound/06-overflow on trigger.gb   | :x:                |
| dmg_sound/07-len sweep period sync.gb | :x:                |
| dmg_sound/08-len ctr during power.gb  | :x:                |
| dmg_sound/09-wave read while on.gb    | :x:                |
| dmg_sound/10-wave trigger while on.gb | :x:                |
| dmg_sound/11-regs after power.gb      | :x:                |
| dmg_sound/12-wave write while on.gb   | :x:                |
| cgb_sound/01-registers.gb             | :x:                |
| cgb_sound/02-len ctr.gb               | :x:                |
| cgb_sound/03-trigger.gb               | :x:                |
| cgb_sound/04-sweep.gb                 | :x:                |
| cgb_sound/05-sweep details.gb         | :x:                |
| cgb_sound/06-overflow on trigger.gb   | :x:                |
| cgb_sound/07-len sweep period sync.gb | :x:                |
| cgb_sound/08-len ctr during power.gb  | :x:                |
| cgb_sound/09-wave read while on.gb    | :x:                |
| cgb_sound/10-wave trigger while on.gb | :x:                |
| cgb_sound/11-regs after power.gb      | :x:                |
| cgb_sound/12-wave.gb                  | :x:                |
| oam_bug/1-lcd_sync                    | :x:                |
| oam_bug/2-causes                      | :x:                |
| oam_bug/3-non_causes                  | :white_check_mark: |
| oam_bug/4-scanline_timing             | :x:                |
| oam_bug/5-timing_bug                  | :x:                |
| oam_bug/6-timing_no_bug               | :white_check_mark: |
| oam_bug/7-timing_effect               | :x:                |
| oam_bug/8-instr_effect                | :x:                |