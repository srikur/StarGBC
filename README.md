# StarGBC -- C++20 Gameboy Color Emulator

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