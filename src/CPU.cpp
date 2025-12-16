#include "CPU.h"

#include <map>

template<BusLike BusT>
void CPU<BusT>::InitializeBootrom(const std::string &bios_path) const {
    std::ifstream file(bios_path, std::ios::binary);
    file.unsetf(std::ios::skipws);

    file.seekg(0, std::ios::end);
    const std::streampos fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    bus_.bootrom.reserve(fileSize);
    bus_.bootrom.insert(bus_.bootrom.begin(),
                        std::istream_iterator<uint8_t>(file),
                        std::istream_iterator<uint8_t>());
    file.close();
}

template<BusLike BusT>
bool CPU<BusT>::IsDMG() const {
    return bus_.gpu_.hardware == Hardware::DMG;
}

template<BusLike BusT>
void CPU<BusT>::InitializeSystem(const Mode mode) {
    regs_.SetStartupValues(static_cast<Registers::Model>(mode));
    sp = 0xFFFE;

    static const std::map<uint16_t, uint8_t> initialData = {
        {0xFF00, 0xCF}, {0xFF02, 0x7C}, {0xFF03, 0xFF}, {0xFF04, 0x1E}, {0xFF07, 0xF8}, {0xFF08, 0xFF}, {0xFF09, 0xFF},
        {0xFF0A, 0xFF}, {0xFF0B, 0xFF}, {0xFF0C, 0xFF}, {0xFF0D, 0xFF}, {0xFF0E, 0xFF}, {0xFF0F, 0xE1}, {0xFF10, 0x80},
        {0xFF11, 0xBF}, {0xFF12, 0xF3}, {0xFF13, 0xFF}, {0xFF14, 0xBF}, {0xFF15, 0xFF}, {0xFF16, 0x3F}, {0xFF18, 0xFF},
        {0xFF19, 0xBF}, {0xFF1A, 0x7F}, {0xFF1B, 0xFF}, {0xFF1C, 0x9F}, {0xFF1D, 0xFF}, {0xFF1E, 0xBF}, {0xFF1F, 0xFF},
        {0xFF20, 0xFF}, {0xFF23, 0xBF}, {0xFF24, 0x77}, {0xFF25, 0xF3}, {0xFF26, 0xF1}, {0xFF27, 0xFF}, {0xFF28, 0xFF},
        {0xFF29, 0xFF}, {0xFF2A, 0xFF}, {0xFF2B, 0xFF}, {0xFF2C, 0xFF}, {0xFF2D, 0xFF}, {0xFF2E, 0xFF}, {0xFF2F, 0xFF},
        {0xFF31, 0xFF}, {0xFF33, 0xFF}, {0xFF35, 0xFF}, {0xFF37, 0xFF}, {0xFF39, 0xFF}, {0xFF3B, 0xFF}, {0xFF3D, 0xFF},
        {0xFF3F, 0xFF}, {0xFF40, 0x91}, {0xFF41, 0x81}, {0xFF47, 0xFC}, {0xFF4C, 0xFF}, {0xFF4D, 0x7E}, {0xFF4E, 0xFF},
        {0xFF4F, 0xFE}, {0xFF50, 0xFF}, {0xFF51, 0xFF}, {0xFF52, 0xFF}, {0xFF53, 0xFF}, {0xFF54, 0xFF}, {0xFF55, 0xFF},
        {0xFF56, 0xFF}, {0xFF57, 0xFF}, {0xFF58, 0xFF}, {0xFF59, 0xFF}, {0xFF5A, 0xFF}, {0xFF5B, 0xFF}, {0xFF5C, 0xFF},
        {0xFF5D, 0xFF}, {0xFF5E, 0xFF}, {0xFF5F, 0xFF}, {0xFF60, 0xFF}, {0xFF61, 0xFF}, {0xFF62, 0xFF}, {0xFF63, 0xFF},
        {0xFF64, 0xFF}, {0xFF65, 0xFF}, {0xFF66, 0xFF}, {0xFF67, 0xFF}, {0xFF68, 0xC0}, {0xFF69, 0xFF}, {0xFF6A, 0xC1},
        {0xFF6B, 0x90}, {0xFF6C, 0xFE}, {0xFF6D, 0xFF}, {0xFF6E, 0xFF}, {0xFF6F, 0xFF}, {0xFF70, 0xF8}, {0xFF71, 0xFF},
        {0xFF75, 0x8F}, {0xFF78, 0xFF}, {0xFF79, 0xFF}, {0xFF7A, 0xFF}, {0xFF7B, 0xFF}, {0xFF7C, 0xFF}, {0xFF7D, 0xFF},
        {0xFF7E, 0xFF}, {0xFF7F, 0xFF},
    };

    for (const auto &[address, value]: initialData) {
        bus_.WriteByte(address, value);
    }
}

template<BusLike BusT>
void CPU<BusT>::ExecuteMicroOp() {
    if (!AdvanceTCycle()) return;
    if (!instrRunning && ProcessInterrupts()) return;
    if (halted) return;
    BeginMCycle();
    if (RunInstructionCycle(currentInstruction, prefixed)) {
        RunPostCompletion();
    }
}

template<BusLike BusT>
void CPU<BusT>::BeginMCycle() {
    ++mCycleCounter;
    if (bus_.bootromRunning && pc == 0x100) {
        bus_.bootromRunning = false;
    }
    instrRunning = true;
}

template<BusLike BusT>
bool CPU<BusT>::AdvanceTCycle() {
    ++tCycleCounter;
    if (tCycleCounter % 4 != 0) {
        return false;
    }
    tCycleCounter = 0;
    return true;
}

template<BusLike BusT>
void CPU<BusT>::RunPostCompletion() {
    prefixed = currentInstruction >> 8 == 0xCB;
    currentInstruction = nextInstruction;
    mCycleCounter = 1;
    if (haltBug) {
        haltBug = false;
        pc -= 1;
    }
    instructions_->ResetState();
    instrRunning = false;
}

template<BusLike BusT>
uint8_t CPU<BusT>::RunInstructionCycle(const uint8_t opcode, const bool isPrefixed) {
    return isPrefixed
               ? instructions_->prefixedInstr(opcode, *this)
               : instructions_->nonPrefixedInstr(opcode, *this);
}

template<BusLike BusT>
uint8_t CPU<BusT>::InterruptAddress(const uint8_t bit) const {
    switch (bit) {
        case 0: return 0x40; // VBlank
        case 1: return 0x48; // LCD STAT
        case 2: return 0x50; // Timer
        case 3: return 0x58; // Serial
        case 4: return 0x60; // Joypad
        default: return 0;
    }
}

template<BusLike BusT>
bool CPU<BusT>::ProcessInterrupts() {
    using enum InterruptState;
    switch (interruptState) {
        case M1: {
            if (interrupts_.interruptDelay && ++icount == 2) {
                interrupts_.interruptDelay = false;
                interrupts_.interruptMasterEnable = true;
                icount = 0;
            }
            const uint8_t pending = interrupts_.interruptEnable & interrupts_.interruptFlag & 0x1F;
            if (pending == 0) {
                return false;
            }
            if (halted && !interrupts_.interruptMasterEnable) {
                halted = false;
                return false;
            }

            if (interrupts_.interruptDelay || !interrupts_.interruptMasterEnable) {
                return false;
            }

            interruptState = M2;
            halted = false;
            interrupts_.interruptMasterEnable = false;

            interruptBit = static_cast<uint8_t>(std::countr_zero(pending));
            interruptMask = static_cast<uint8_t>(1u << interruptBit);

            pc -= 1;
            return true;
        }
        case M2: {
            sp -= 1;
            bus_.WriteByte(sp, static_cast<uint8_t>(pc >> 8));
            interruptState = M3;
            return true;
        }
        case M3: {
            if (const uint8_t newPending = interrupts_.interruptEnable & interrupts_.interruptFlag & 0x1F; !(newPending & interruptMask)) {
                if (!newPending) {
                    sp--;
                    bus_.WriteByte(sp, pc & 0xFF);
                    pc = 0x0000;
                    interruptState = M4;
                    return true;
                } else {
                    interruptBit = std::countr_zero(newPending);
                    interruptMask = 1u << interruptBit;
                }
            }

            sp -= 1;
            bus_.WriteByte(sp, static_cast<uint8_t>(pc & 0xFF));
            interrupts_.interruptFlag &= ~interruptMask;
            pc = InterruptAddress(interruptBit);

            interruptState = M4;
            return true;
        }
        case M4: {
            interruptState = M5;
            return true;
        }
        case M5: {
            interruptState = M6;
            return true;
        }
        case M6: {
            prefixed = false;
            currentInstruction = bus_.ReadByte(pc++);
            interruptState = M1;
            mCycleCounter = 1;
            return false;
        }
    }
    return true;
}

template class CPU<Bus>;
