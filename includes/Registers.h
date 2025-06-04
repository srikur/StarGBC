#pragma once

struct Registers {
    uint8_t a{}, f{}, b{}, c{}, d{}, e{}, h{}, l{};

    [[nodiscard]] uint16_t GetAF() const { return static_cast<uint16_t>(a) << 8 | f; }

    [[nodiscard]] uint16_t GetBC() const { return static_cast<uint16_t>(b) << 8 | c; }

    [[nodiscard]] uint16_t GetDE() const { return static_cast<uint16_t>(d) << 8 | e; }

    [[nodiscard]] uint16_t GetHL() const { return static_cast<uint16_t>(h) << 8 | l; }

    [[nodiscard]] bool FlagZero() const { return !!(f & 0x80); }

    [[nodiscard]] bool FlagSubtract() const { return !!(f & 0x40); }

    [[nodiscard]] bool FlagHalf() const { return !!(f & 0x20); }

    [[nodiscard]] bool FlagCarry() const { return !!(f & 0x10); }

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

    enum Model : std::size_t { DMG, CGB };

    void SetStartupValues(Model model);

    bool SaveState(std::ofstream &stateFile) const {
        try {
            if (!stateFile.is_open()) return false;
            stateFile.write(reinterpret_cast<const char *>(this), sizeof(Registers));
            return true;
        } catch (const std::exception &e) {
            std::cerr << "Error saving Registers state: " << e.what() << std::endl;
            return false;
        }
    }

    bool LoadState(std::ifstream &stateFile) {
        try {
            if (!stateFile.is_open()) return false;
            stateFile.read(reinterpret_cast<char *>(this), sizeof(Registers));
            return true;
        } catch (const std::exception &e) {
            std::cerr << "Error loading Registers state: " << e.what() << std::endl;
            return false;
        }
    }
};

static constexpr std::array<Registers, 2> DefaultValues = {
    {
        /* DMG */ {
            .a = 0x11, .f = 0x00, .b = 0x01, .c = 0x00,
            .d = 0x00, .e = 0x08, .h = 0x00, .l = 0x7C
        },
        /* CGB */ {
            .a = 0x11, .f = 0x80, .b = 0x00, .c = 0x00,
            .d = 0x00, .e = 0x08, .h = 0x00, .l = 0x7C
        }
    }
};

inline void Registers::SetStartupValues(const Model model) {
    SetAF(DefaultValues[model].GetAF());
    SetBC(DefaultValues[model].GetBC());
    SetDE(DefaultValues[model].GetDE());
    SetHL(DefaultValues[model].GetHL());
}
