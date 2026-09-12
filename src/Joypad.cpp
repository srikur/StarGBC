#include "Joypad.h"

#include <cstring>

uint8_t Joypad::GetJoypadState() const {
    // Unselected key lines float high; selecting both rows ANDs them
    uint8_t result = (select_ & 0x30) | 0x0F;
    if (sgb_ && (select_ & 0x30) == 0x30) {
        // With both lines deselected the SGB drives the current joypad ID ($FF down to $FC)
        return (result & 0xF0) | (0x0F - sgbCurrentPlayer_);
    }
    if (sgb_ && sgbCurrentPlayer_ != 0) {
        // Only controller 1 is physically attached; other players read as all-released
        return result;
    }
    if ((select_ & 0x10) == 0x00)
        result &= 0xF0 | (matrix_ & 0x0F);
    if ((select_ & 0x20) == 0x00)
        result &= 0xF0 | (matrix_ >> 4);
    return result;
}

void Joypad::SetJoypadState(const uint8_t value) {
    if (sgb_) SgbWrite(value);
    select_ = value;
    UpdateKeyFlag();
}

void Joypad::ConfigureSgb(const bool enabled) {
    sgb_ = enabled;
}

void Joypad::SgbWrite(const uint8_t value) {
    // Byte counts, not bits: header byte 0's low 3 bits give the packet count for this command
    uint16_t commandSize = (sgbCommand_[0] & 7 ? sgbCommand_[0] & 7 : 1) * kSgbPacketSize * 8;
    if ((sgbCommand_[0] & 0xF1) == 0xF1) {
        // Boot-time header transfer commands are always a single packet
        commandSize = kSgbPacketSize * 8;
    }

    // P15 rising edge advances the joypad ID whenever a multiplayer mode is active
    if ((value & 0x20) != 0 && (select_ & 0x20) == 0 && (sgbPlayerCount_ & 1) == 0) {
        sgbCurrentPlayer_ = (sgbCurrentPlayer_ + 1) & (sgbPlayerCount_ - 1);
    }

    switch (value >> 4 & 3) {
        case 3: // Idle
            sgbReadyForPulse_ = true;
            break;
        case 2: // "0" bit / stop bit
            if (!sgbReadyForPulse_ || !sgbReadyForWrite_) return;
            if (sgbReadyForStop_) {
                if (sgbCommandIndex_ == commandSize) {
                    SgbCommandReady();
                    sgbCommandIndex_ = 0;
                    std::memset(sgbCommand_, 0, sizeof(sgbCommand_));
                }
                sgbReadyForPulse_ = false;
                sgbReadyForWrite_ = false;
                sgbReadyForStop_ = false;
            } else if (sgbCommandIndex_ < sizeof(sgbCommand_) * 8) {
                sgbCommandIndex_++;
                sgbReadyForPulse_ = false;
                if ((sgbCommandIndex_ & (kSgbPacketSize * 8 - 1)) == 0) {
                    sgbReadyForStop_ = true;
                }
            }
            break;
        case 1: // "1" bit
            if (!sgbReadyForPulse_ || !sgbReadyForWrite_) return;
            if (sgbReadyForStop_) {
                // A "1" where the stop bit belongs corrupts the whole command
                sgbReadyForPulse_ = false;
                sgbReadyForWrite_ = false;
                sgbCommandIndex_ = 0;
                std::memset(sgbCommand_, 0, sizeof(sgbCommand_));
            } else if (sgbCommandIndex_ < sizeof(sgbCommand_) * 8) {
                sgbCommand_[sgbCommandIndex_ / 8] |= 1 << (sgbCommandIndex_ & 7);
                sgbCommandIndex_++;
                sgbReadyForPulse_ = false;
                if ((sgbCommandIndex_ & (kSgbPacketSize * 8 - 1)) == 0) {
                    sgbReadyForStop_ = true;
                }
            }
            break;
        case 0: // Reset pulse
            if (!sgbReadyForPulse_) return;
            sgbReadyForWrite_ = true;
            sgbReadyForPulse_ = false;
            if ((sgbCommandIndex_ & (kSgbPacketSize * 8 - 1)) != 0 || sgbCommandIndex_ == 0 ||
                sgbReadyForStop_) {
                sgbCommandIndex_ = 0;
                std::memset(sgbCommand_, 0, sizeof(sgbCommand_));
                sgbReadyForStop_ = false;
            }
            break;
        default: break;
    }
}

void Joypad::SgbCommandReady() {
    // Only MLT_REQ is emulated; everything else the SNES side would handle is ignored
    if (sgbCommand_[0] >> 3 == 0x11) {
        sgbPlayerCount_ = (sgbCommand_[1] & 3) + 1;
        if (sgbPlayerCount_ == 3) {
            // Unsupported mode 2: the ID counter still bumps once, then sticks (count stays odd)
            sgbCurrentPlayer_++;
        }
        sgbCurrentPlayer_ &= sgbPlayerCount_ - 1;
    }
}

void Joypad::SetMatrix(const uint8_t value) {
    matrix_ = value;
    UpdateKeyFlag();
}

uint8_t Joypad::GetMatrix() const { return matrix_; }

uint8_t Joypad::GetSelect() const { return select_; }

void Joypad::KeyDown(Keys key) {
    SetMatrix(GetMatrix() & ~static_cast<uint8_t>(key));
    interrupts_.Set(InterruptType::Joypad, false);
}

void Joypad::KeyUp(Keys key) {
    SetMatrix(GetMatrix() | static_cast<uint8_t>(key));
}

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
