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
3. [Ninja](https://github.com/ninja-build/ninja)
4. [spdlog](https://github.com/gabime/spdlog)

## Building

1. Clone the repository:
   ```bash
   git clone https://github.com/srikur/StarGBC.git
   ```
2. Configure and build (submodules are initialized automatically during configure):
   ```bash
   cmake --workflow --preset release
   ```
   If you prefer the two-step form (e.g. to rebuild without reconfiguring):
   ```bash
   cmake --preset release        # configure: fetches submodules, creates build/release/CMakeCache.txt
   cmake --build --preset release # build
   ```
   To opt out of the automatic submodule update, pass
   `-DSTARGBC_UPDATE_SUBMODULES=OFF` to the configure step.
