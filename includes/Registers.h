#pragma once

#include "Model.h"

#include <cstdint>
#include <fstream>

struct Registers {
    uint8_t a{}, f{}, b{}, c{}, d{}, e{}, h{}, l{};

    [[nodiscard]] uint16_t GetAF() const noexcept { return static_cast<uint16_t>(a) << 8 | f; }

    [[nodiscard]] uint16_t GetBC() const noexcept { return static_cast<uint16_t>(b) << 8 | c; }

    [[nodiscard]] uint16_t GetDE() const noexcept { return static_cast<uint16_t>(d) << 8 | e; }

    [[nodiscard]] uint16_t GetHL() const noexcept { return static_cast<uint16_t>(h) << 8 | l; }

    [[nodiscard]] bool FlagZero() const noexcept { return !!(f & 0x80); }

    [[nodiscard]] bool FlagSubtract() const noexcept { return !!(f & 0x40); }

    [[nodiscard]] bool FlagHalf() const noexcept { return !!(f & 0x20); }

    [[nodiscard]] bool FlagCarry() const noexcept { return !!(f & 0x10); }

    void SetZero(const bool x) { f = (f & 0x7F) | (x << 7); }

    void SetSubtract(const bool x) { f = (f & 0xBF) | (x << 6); }

    void SetHalf(const bool x) { f = (f & 0xDF) | (x << 5); }

    void SetCarry(const bool x) { f = (f & 0xEF) | (x << 4); }

    void SetAF(const uint16_t value) {
        a = value >> 8;
        f = value & 0xFF;
    }

    void SetBC(const uint16_t value) {
        b = value >> 8;
        c = value & 0xFF;
    }

    void SetDE(const uint16_t value) {
        d = value >> 8;
        e = value & 0xFF;
    }

    void SetHL(const uint16_t value) {
        h = value >> 8;
        l = value & 0xFF;
    }

    void SetStartupValues(Model model, bool cgbMode);

    bool SaveState(std::ofstream &stateFile) const {
        try {
            if (!stateFile.is_open()) return false;
            stateFile.write(reinterpret_cast<const char *>(this), sizeof(Registers));
            return true;
        } catch ([[maybe_unused]] const std::exception &e) {
            return false;
        }
    }

    bool LoadState(std::ifstream &stateFile) {
        try {
            if (!stateFile.is_open()) return false;
            stateFile.read(reinterpret_cast<char *>(this), sizeof(Registers));
            return true;
        } catch ([[maybe_unused]] const std::exception &e) {
            return false;
        }
    }
};

inline void Registers::SetStartupValues(const Model model, const bool cgbMode) {
    if (IsCgb(model)) {
        SetAF(IsAgb(model) ? 0x1100 : 0x1180);
        SetBC(IsAgb(model) ? 0x0100 : 0x0000);
        SetDE(cgbMode ? 0xFF56 : 0x0008);
        SetHL(cgbMode ? 0x000D : 0x007C);
    } else if (IsSgb(model)) {
        SetAF(model == Model::SGB2 ? 0xFF00 : 0x0100);
        SetBC(0x0014);
        SetDE(0x0000);
        SetHL(0xC060);
    } else if (model == Model::DMG0) {
        SetAF(0x0100);
        SetBC(0xFF13);
        SetDE(0x00C1);
        SetHL(0x8403);
    } else {
        SetAF(model == Model::MGB ? 0xFFB0 : 0x01B0);
        SetBC(0x0013);
        SetDE(0x00D8);
        SetHL(0x014D);
    }
}
