#ifndef STARGBC_JOYPAD_H
#define STARGBC_JOYPAD_H

#include <fstream>

#include "Common.h"
#include "Interrupts.h"

struct Joypad {
    explicit Joypad(Interrupts &interrupts) : interrupts_(interrupts) {
    }

    [[nodiscard]] uint8_t GetJoypadState() const;

    void SetJoypadState(uint8_t);

    void SetMatrix(uint8_t);

    [[nodiscard]] uint8_t GetMatrix() const;

    [[nodiscard]] uint8_t GetSelect() const;

    void KeyDown(Keys key);

    void KeyUp(Keys key);

    void SetSelect(uint8_t);

    [[nodiscard]] bool KeyPressed() const;

    void ClearKeyPressed();

    // Enables the SGB ICD2 command interface (SGB/SGB2 hardware with an SGB-flagged cart)
    void ConfigureSgb(bool enabled);

    bool SaveState(std::ofstream &f) const;

    bool LoadState(std::ifstream &f);

private:
    void UpdateKeyFlag();

    void SgbWrite(uint8_t value);

    void SgbCommandReady();

    static constexpr std::size_t kSgbPacketSize = 16;

    uint8_t matrix_{0xFF};
    uint8_t select_{0x00};
    Interrupts &interrupts_;
    bool keyPressed_{false};

    // SGB packet receive state machine + MLT_REQ joypad-ID counter (ported from SameBoy's sgb.c,
    // validated against samesuite sgb/command_mlt_req*)
    bool sgb_{false};
    uint8_t sgbCommand_[kSgbPacketSize * 7]{};
    uint16_t sgbCommandIndex_{0};
    bool sgbReadyForPulse_{false};
    bool sgbReadyForWrite_{false};
    bool sgbReadyForStop_{false};
    uint8_t sgbPlayerCount_{1};
    uint8_t sgbCurrentPlayer_{0};
};

#endif //STARGBC_JOYPAD_H
