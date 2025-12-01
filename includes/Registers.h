#pragma once

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

    enum Model : std::size_t { DMG = 0x01, MGB, SGB, SGB2, CGB_DMG, AGB_DMG, AGS_DMG, CGB_GBC, AGB_GBC, AGS_GBC };

    void SetStartupValues(Model model);

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

static constexpr std::array<Registers, 10> DefaultValues = {
    {
        /* DMG in DMG mode */ {
            .a = 0x01, .f = 0xB0, .b = 0x00, .c = 0x13,
            .d = 0x00, .e = 0xD8, .h = 0x01, .l = 0x4D
        },
        /* MGB in DMG mode */
        {
            .a = 0xFF, .f = 0xB0, .b = 0x00, .c = 0x13,
            .d = 0x00, .e = 0xD8, .h = 0x01, .l = 0x4D
        },
        /* SGB in DMG mode */
        {
            .a = 0x01, .f = 0x00, .b = 0x00, .c = 0x14,
            .d = 0x00, .e = 0x00, .h = 0xC0, .l = 0x60
        },
        /* SGB2 in DMG mode -- reusing SGB values since unknown */
        {
            .a = 0x01, .f = 0x00, .b = 0x00, .c = 0x14,
            .d = 0x00, .e = 0x00, .h = 0xC0, .l = 0x60
        },
        /* CGB in DMG mode */ {
            .a = 0x11, .f = 0x80, .b = 0x00, .c = 0x00,
            .d = 0x00, .e = 0x08, .h = 0x00, .l = 0x7C
        },
        /* AGB in DMG mode */ {
            .a = 0x11, .f = 0x00, .b = 0x01, .c = 0x00,
            .d = 0x00, .e = 0x08, .h = 0x00, .l = 0x7C
        },
        /* AGS in DMG mode */ {
            .a = 0x11, .f = 0x00, .b = 0x01, .c = 0x00,
            .d = 0x00, .e = 0x08, .h = 0x00, .l = 0x7C
        },
        /* CGB in GBC mode */ {
            .a = 0x11, .f = 0x80, .b = 0x00, .c = 0x00,
            .d = 0xFF, .e = 0x56, .h = 0x00, .l = 0x0D
        },
        /* AGB in GBC mode */ {
            .a = 0x11, .f = 0x00, .b = 0x01, .c = 0x00,
            .d = 0xFF, .e = 0x56, .h = 0x00, .l = 0x0D
        },
        /* AGS in GBC mode */ {
            .a = 0x11, .f = 0x00, .b = 0x01, .c = 0x00,
            .d = 0xFF, .e = 0x56, .h = 0x00, .l = 0x0D
        },
    }
};

inline void Registers::SetStartupValues(const Model model) {
    SetAF(DefaultValues[model].GetAF());
    SetBC(DefaultValues[model].GetBC());
    SetDE(DefaultValues[model].GetDE());
    SetHL(DefaultValues[model].GetHL());
}
