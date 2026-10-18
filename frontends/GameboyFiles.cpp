#include "GameboyFiles.h"

#include <fstream>
#include <limits>

namespace host {
    std::vector<u8> read_file(const std::filesystem::path &path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            throw std::runtime_error("Could not open " + path.string());
        const auto size = file.tellg();
        if (size < 0 || static_cast<uintmax_t>(size) > static_cast<uintmax_t>(std::numeric_limits<std::streamsize>::max()))
            throw std::runtime_error("Could not determine file size: " + path.string());
        std::vector<u8> bytes(size);
        file.seekg(0);
        if (!file || (size && !file.read(reinterpret_cast<char *>(bytes.data()), size)))
            throw std::runtime_error("Could not read " + path.string());
        return bytes;
    }

    std::filesystem::path battery_path(const std::filesystem::path &rom) {
        auto path = rom;
        return path.replace_extension(".sav");
    }

    GameboyMedia load_media(const LaunchSettings &settings) {
        GameboyMedia media{.rom = read_file(settings.rom_path)};
        if (!settings.bios_path.empty()) {
            media.bootrom = read_file(settings.bios_path);
            if (media.bootrom.empty())
                throw std::invalid_argument("Boot ROM is empty: " + settings.bios_path);
        }
        const auto save = battery_path(settings.rom_path);
        if (std::filesystem::exists(save))
            media.battery = read_file(save);
        return media;
    }

    u64 wall_clock_seconds() { return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }

    Gameboy load_gameboy(const LaunchSettings &settings) { return Gameboy(settings.emulation, load_media(settings), wall_clock_seconds); }

    std::unique_ptr<Gameboy> init_gameboy(const LaunchSettings &settings) {
        return Gameboy::init(settings.emulation, load_media(settings), wall_clock_seconds);
    }

    void save_battery(Gameboy &machine, const std::filesystem::path &rom) {
        const auto bytes = machine.SaveBattery();
        if (!bytes.empty()) {
            const auto path = battery_path(rom);
            const auto temporary = std::filesystem::path(path.string() + ".tmp");
            std::ofstream file;
            file.exceptions(std::ios::badbit | std::ios::failbit);
            file.open(temporary, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            file.close();
            std::filesystem::rename(temporary, path);
        }
        machine.AcknowledgeBatterySave();
    }
} // namespace host
