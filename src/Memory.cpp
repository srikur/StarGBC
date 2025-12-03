#include "Memory.h"

bool Memory::SaveState(std::ofstream &stateFile) const {
    try {
        if (!stateFile.is_open()) return false;
        stateFile.write(reinterpret_cast<const char *>(wram_.data()), 0x8000);
        stateFile.write(reinterpret_cast<const char *>(hram_.data()), 0x80);
        stateFile.write(reinterpret_cast<const char *>(&wramBank_), sizeof(wramBank_));
        return true;
    } catch ([[maybe_unused]] const std::exception &e) {
        return false;
    }
}

bool Memory::LoadState(std::ifstream &stateFile) {
    try {
        if (!stateFile.is_open()) return false;
        stateFile.read(reinterpret_cast<char *>(wram_.data()), 0x8000);
        stateFile.read(reinterpret_cast<char *>(hram_.data()), 0x80);
        stateFile.read(reinterpret_cast<char *>(&wramBank_), sizeof(wramBank_));
        return true;
    } catch ([[maybe_unused]] const std::exception &e) {
        return false;
    }
}
