#ifndef STARGBC_CPU_H
#define STARGBC_CPU_H

#include <map>
#include <memory>

#include "Bus.h"
#include "Instructions.h"
#include "Registers.h"

class Instructions;

class CPU {
public:
    CPU() = delete;

    explicit CPU(const Mode mode, const std::string &biosPath, const bool useRealRTC) : mode_(mode) {
        if (mode_ != Mode::None) {
            bus->gpu_->hardware = mode == Mode::DMG ? GPU::Hardware::DMG : GPU::Hardware::CGB;
            bus->audio_->SetDMG(bus->gpu_->hardware == GPU::Hardware::DMG);
        } else if ((bus->cartridge_->ReadByte(0x143) & 0x80) == 0x80) {
            mode_ = Mode::CGB_GBC;
            bus->gpu_->hardware = GPU::Hardware::CGB;
            bus->audio_->SetDMG(false);
        }
        if (!biosPath.empty()) {
            bus->bootromRunning = true;
            InitializeBootrom(biosPath);
        } else {
            pc = 0x100;
            InitializeSystem(mode);
            currentInstruction = bus->ReadByte(pc++);
        }
        bus->cartridge_->rtc->realRTC = useRealRTC;
    }

    void Save() const;

    void KeyDown(Keys) const;

    void KeyUp(Keys) const;

    uint32_t *GetScreenData() const;

    void InitializeBootrom(const std::string &) const;

    bool IsDMG() const;

    uint32_t RunHDMA() const;

    void InitializeSystem(Mode);

    void AdvanceFrame();

    void ExecuteMicroOp();

private:
    friend class Instructions;

    uint8_t DecodeInstruction(uint8_t, bool);

    uint8_t InterruptAddress(uint8_t) const;

    bool ProcessInterrupts();

    std::unique_ptr<Bus> bus{nullptr};
    std::unique_ptr<Registers> regs{std::make_unique<Registers>()};
    std::unique_ptr<Instructions> instructions{std::make_unique<Instructions>()};

    Mode mode_{Mode::DMG};

    uint16_t pc{0x0000};
    uint16_t sp{0x0000};
    uint8_t icount{0};
    uint8_t mCycleCounter{0x01};
    uint16_t nextInstruction{0x0000};
    bool halted{false};
    bool haltBug{false};
    bool stopped{false};
    uint16_t currentInstruction{0x0000};
    bool prefixed{false};

    InterruptState interruptState{InterruptState::M1};
    uint16_t previousPC{0x00};
    uint8_t tCycleCounter{0x00};
    uint32_t masterCycles{0x00000000};
    uint8_t interruptBit{0x00};
    uint8_t interruptMask{0x00};
    bool instrComplete{true};
    uint16_t previousInstruction{0x0000};
    bool previousPrefixed{false};
    bool instrRunning{false};
};

#endif //STARGBC_CPU_H
