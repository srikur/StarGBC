# StarGBC

<p align="center">
  <img src="images/blue_title.png" width="30%"  alt="Blue Title Screen"/>
  <img src="images/blue_ingame.png" width="30%"  alt="Blue In-Game"/>
  <img src="images/gold_title.png" width="30%"  alt="Gold Title Screen"/>
  <br/>
  <img src="images/gold_ingame.png" width="30%"  alt="Gold In-Game"/>
  <img src="images/smb_title.png" width="30%"  alt="Super Mario Bros Title Screen"/>
  <img src="images/crystal_title.png" width="30%"  alt="Crystal Title Screen"/>
</p>

## Dependencies

1. [SDL3](https://github.com/libsdl-org/SDL)
2. [doctest](https://github.com/doctest/doctest)
3. [starparse](https://github.com/srikur/starparse)
4. [Lua](https://www.lua.org/) 5.5.1 (optional, enabled by default)

All library dependencies are downloaded and built automatically at configure
time via CMake's FetchContent. Only CMake 4+ and C++26-compatible compiler are required. 
Ninja is optional but preferred (`-G Ninja` when building).

## Building

1. Clone the repository:
   ```bash
   git clone https://github.com/srikur/StarGBC.git
   ```
2. Configure and build:
   ```bash
   cmake --workflow --preset release
   ```

## Hardware models

Select a silicon revision with `--model`, for example:

```bash
build/release/Release/StarGBC --model cgbb --bios roms/cgb_boot.bin game.gbc
build/release/Release/StarGBC --model cgbe game.gb
```

The physical model stays fixed when a Color or Advance runs a monochrome
cartridge. `auto` selects DMG-B for monochrome cartridges and CGB-E for Color
cartridges. The aliases `dmg`, `cgb`, `agb`, and `ags` select DMG-B, CGB-E,
AGB-A, and AGB-B respectively.

## Lua scripting

Run gameplay automation and memory inspection scripts:

```bash
build/release/Release/StarGBC --script scripting/examples/autofire.lua game.gbc
```

Use Shift+F5 to start or stop the selected script; F5 reloads it while running.
