#pragma once

#include <filesystem>
#include "Gameboy.h"

namespace host {
    struct LaunchSettings {
        std::string rom_path;
        std::string bios_path;
        GameboySettings emulation;
    };

    [[nodiscard]] std::vector<u8> read_file(const std::filesystem::path &path);

    [[nodiscard]] std::filesystem::path battery_path(const std::filesystem::path &rom);

    [[nodiscard]] GameboyMedia load_media(const LaunchSettings &settings);

    [[nodiscard]] u64 wall_clock_seconds();

    [[nodiscard]] Gameboy load_gameboy(const LaunchSettings &settings);

    [[nodiscard]] std::unique_ptr<Gameboy> init_gameboy(const LaunchSettings &settings);

    void save_battery(Gameboy &machine, const std::filesystem::path &rom);
} // namespace host
