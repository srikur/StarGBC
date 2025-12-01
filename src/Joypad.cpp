#include "Joypad.h"

uint8_t Joypad::GetJoypadState() const {
    if ((select_ & 0x10) == 0x00)
        return static_cast<uint8_t>(select_ | (matrix_ & 0x0F));

    if ((select_ & 0x20) == 0x00)
        return static_cast<uint8_t>(select_ | (matrix_ >> 4));

    return select_;
}

void Joypad::SetJoypadState(const uint8_t value) {
    select_ = value;
    UpdateKeyFlag();
}

void Joypad::SetMatrix(const uint8_t value) {
    matrix_ = value;
    UpdateKeyFlag();
}

uint8_t Joypad::GetMatrix() const { return matrix_; }

uint8_t Joypad::GetSelect() const { return select_; }

void Joypad::SetSelect(const uint8_t value) {
    select_ = value;
    UpdateKeyFlag();
}

bool Joypad::KeyPressed() const { return keyPressed_; }

void Joypad::ClearKeyPressed() { keyPressed_ = false; }

bool Joypad::SaveState(std::ofstream &f) const {
    if (!f.is_open()) return false;
    f.write(reinterpret_cast<const char *>(&matrix_), sizeof matrix_);
    f.write(reinterpret_cast<const char *>(&select_), sizeof select_);
    f.write(reinterpret_cast<const char *>(&keyPressed_), sizeof keyPressed_);
    return true;
}

bool Joypad::LoadState(std::ifstream &f) {
    if (!f.is_open()) return false;
    f.read(reinterpret_cast<char *>(&matrix_), sizeof matrix_);
    f.read(reinterpret_cast<char *>(&select_), sizeof select_);
    f.read(reinterpret_cast<char *>(&keyPressed_), sizeof keyPressed_);
    return true;
}

void Joypad::UpdateKeyFlag() {
    const bool dirRowSelected = (select_ & 0x10) == 0;
    const bool btnRowSelected = (select_ & 0x20) == 0;

    const bool dirKeyLow = (matrix_ & 0x0F) != 0x0F;
    const bool btnKeyLow = ((matrix_ >> 4) & 0x0F) != 0x0F;

    if ((dirRowSelected && dirKeyLow) || (btnRowSelected && btnKeyLow))
        keyPressed_ = true;
}
