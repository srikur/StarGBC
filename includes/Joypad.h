#ifndef STARGBC_JOYPAD_H
#define STARGBC_JOYPAD_H

#include <fstream>

struct Joypad {
    [[nodiscard]] uint8_t GetJoypadState() const;

    void SetJoypadState(uint8_t);

    void SetMatrix(uint8_t);

    [[nodiscard]] uint8_t GetMatrix() const;

    [[nodiscard]] uint8_t GetSelect() const;

    void SetSelect(uint8_t);

    [[nodiscard]] bool KeyPressed() const;

    void ClearKeyPressed();

    bool SaveState(std::ofstream &f) const;

    bool LoadState(std::ifstream &f);

private:
    void UpdateKeyFlag();

    uint8_t matrix_{0xFF};
    uint8_t select_{0x00};
    bool keyPressed_{false};
};

#endif //STARGBC_JOYPAD_H
