#include "CPU.h"
#include "EmbeddedBootroms.h"

#include <bit>
#include <iterator>
#include <map>

// The register file a model hands off with depends on whether the boot dropped to DMG-compat mode
static constexpr Registers::Model StartupModelFor(const Mode mode, const bool cgbMode) {
    switch (mode) {
        case Mode::DMG0: return Registers::DMG0;
        case Mode::MBG: return Registers::MGB;
        case Mode::SGB: return Registers::SGB;
        case Mode::SGB2: return Registers::SGB2;
        case Mode::CGB_DMG:
        case Mode::CGB_GBC: return cgbMode ? Registers::CGB_GBC : Registers::CGB_DMG;
        case Mode::CGB0: return cgbMode ? Registers::CGB0 : Registers::CGB_DMG;
        case Mode::AGB_DMG:
        case Mode::AGB_GBC: return cgbMode ? Registers::AGB_GBC : Registers::AGB_DMG;
        case Mode::AGS_DMG:
        case Mode::AGS_GBC: return cgbMode ? Registers::AGS_GBC : Registers::AGS_DMG;
        default: return Registers::DMG;
    }
}

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
void CPU<BusT>::InitializeEmbeddedBootrom(const bool cgb) const {
    using stargbc::bootroms::kCgbBootrom;
    using stargbc::bootroms::kDmgBootrom;
    if (cgb) {
        bus_.bootrom.assign(kCgbBootrom.begin(), kCgbBootrom.end());
    } else {
        bus_.bootrom.assign(kDmgBootrom.begin(), kDmgBootrom.end());
    }
}

template<BusLike BusT>
void CPU<BusT>::InitializeSystem(const Mode mode) {
    regs_.SetStartupValues(StartupModelFor(mode, bus_.cgbMode));
    sp_ = 0xFFFE;
    const Hardware hw = bus_.gpu_.hardware;
    const bool sgbFamily = hw == Hardware::SGB || hw == Hardware::SGB2;

    static const std::map<uint16_t, uint8_t> initialData = {
        {0xFF00, 0xCF}, {0xFF02, 0x7C}, {0xFF03, 0xFF}, {0xFF04, 0x1E}, {0xFF07, 0xF8}, {0xFF08, 0xFF}, {0xFF09, 0xFF},
        {0xFF0A, 0xFF}, {0xFF0B, 0xFF}, {0xFF0C, 0xFF}, {0xFF0D, 0xFF}, {0xFF0E, 0xFF}, {0xFF0F, 0xE1}, {0xFF10, 0x80},
        {0xFF11, 0xBF}, {0xFF12, 0xF3}, {0xFF13, 0xFF}, {0xFF14, 0xBF}, {0xFF15, 0xFF}, {0xFF16, 0x3F}, {0xFF18, 0xFF},
        // NRx4 trigger bits stay clear for ch2/ch3/ch4: the bootroms never
        // trigger them, and the trigger bit is unreadable (only ch1 plays the
        // DMG boot beep, via FF14 below)
        {0xFF19, 0x3F}, {0xFF1A, 0x7F}, {0xFF1B, 0xFF}, {0xFF1C, 0x9F}, {0xFF1D, 0xFF}, {0xFF1E, 0x3F}, {0xFF1F, 0xFF},
        {0xFF20, 0xFF}, {0xFF23, 0x3F}, {0xFF24, 0x77}, {0xFF25, 0xF3}, {0xFF26, 0xF1}, {0xFF27, 0xFF}, {0xFF28, 0xFF},
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

    // Power the APU on before the register table below: the map iterates in
    // address order, so NR10-NR51 come before NR52 and would be dropped while
    // the APU is still off
    bus_.WriteByte(0xFF26, 0xF1, ComponentSource::CPU);

    for (const auto &[address, value]: initialData) {
        uint8_t effective = value;
        if (sgbFamily) {
            // The SGB bootrom leaves both joypad select lines deselected
            // (P1 reads $FF), and never triggers the boot chime on the GB side
            // (the SNES plays it), so NR52 reads $F0 with no channel active
            if (address == 0xFF00) effective = 0xFF;
            if (address == 0xFF14) effective = 0x3F;
        }
        bus_.WriteByte(address, effective, ComponentSource::CPU);
    }

    if (sgbFamily) {
        // The SGB bootrom's duration depends on the cartridge header it sends
        // to the SNES: each set bit in $0104-$014F shortens the transfer by
        // one M-cycle (mooneye boot_div-S vs boot_div2-S differ only in the
        // global checksum and need exactly popcount-delta more NOPs). The base
        // is calibrated against mooneye boot_div-S.
        uint32_t setBits = 0;
        for (uint16_t address = 0x0104; address <= 0x014F; ++address) {
            setBits += std::popcount(bus_.cartridge_.ReadByte(address));
        }
        bus_.timer_.divCounter = static_cast<uint16_t>(0xDC88 - 4 * setBits);
    }
}

template<BusLike BusT>
void CPU<BusT>::ExecuteMicroOp(Instructions<Self> &instructions, const bool hdmaActive) {
    if (hdmaActive) return;
    if (!instrRunning) {
        if (ProcessInterrupts()) return;
        if (halted_) return;
    }
    BeginMCycle();
    if (RunInstructionCycle(instructions, currentInstruction, prefixed)) {
        RunPostCompletion(instructions);
    }
}

template<BusLike BusT>
void CPU<BusT>::BeginMCycle() {
    ++mCycleCounter_;
    if (bus_.bootromRunning && pc_ == 0x100) {
        bus_.bootromRunning = false;
        // A bootrom that never wrote KEY0 (e.g. the embedded one) hands off
        // with the mode implied by the cart header
        if (IsCgb(bus_.gpu_.hardware) && !bus_.key0Written) {
            bus_.cgbMode = (bus_.cartridge_.ReadByte(0x143) & 0x80) == 0x80;
            bus_.gpu_.dmgCompat = !bus_.cgbMode;
        }
    }
    instrRunning = true;
}

template<BusLike BusT>
void CPU<BusT>::RunPostCompletion(Instructions<Self> &instructions) {
    prefixed = currentInstruction >> 8 == 0xCB;
    currentInstruction = nextInstruction_;
    mCycleCounter_ = 1;
    if (haltBug_) {
        haltBug_ = false;
        pc_ -= 1;
    }
    instructions.ResetState();
    instrRunning = false;
}

template<BusLike BusT>
uint8_t CPU<BusT>::RunInstructionCycle(Instructions<Self> &instructions, const uint8_t opcode, const bool isPrefixed) {
    return isPrefixed
               ? instructions.prefixedInstr(opcode, *this)
               : instructions.nonPrefixedInstr(opcode, *this);
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
    if (prefixed) return false;
    using enum InterruptState;
    switch (interruptState) {
        case M1: {
            if (interrupts_.interruptDelay && ++icount_ == 2) {
                interrupts_.interruptDelay = false;
                interrupts_.interruptMasterEnable = true;
                icount_ = 0;
            }
            const uint8_t pending = interrupts_.interruptEnable & interrupts_.interruptFlag & 0x1F;
            if (pending == 0) {
                return false;
            }
            if (halted_ && !interrupts_.interruptMasterEnable) {
                halted_ = false;
                return false;
            }

            if (interrupts_.interruptDelay || !interrupts_.interruptMasterEnable) {
                return false;
            }

            // Waking from HALT into a dispatch costs one extra M-cycle before
            // the normal 5-cycle interrupt sequence begins
            if (halted_) {
                halted_ = false;
                return true;
            }

            interruptState = M2;
            interrupts_.interruptMasterEnable = false;

            interruptBit = static_cast<uint8_t>(std::countr_zero(pending));
            interruptMask = static_cast<uint8_t>(1u << interruptBit);

            pc_ -= 1;
            return true;
        }
        case M2: {
            sp_ -= 1;
            bus_.WriteByte(sp_, static_cast<uint8_t>(pc_ >> 8), ComponentSource::CPU);
            interruptState = M3;
            return true;
        }
        case M3: {
            if (const uint8_t newPending = interrupts_.interruptEnable & interrupts_.interruptFlag & 0x1F; !(newPending & interruptMask)) {
                if (!newPending) {
                    sp_--;
                    bus_.WriteByte(sp_, pc_ & 0xFF, ComponentSource::CPU);
                    pc_ = 0x0000;
                    interruptState = M4;
                    return true;
                } else {
                    interruptBit = std::countr_zero(newPending);
                    interruptMask = 1u << interruptBit;
                }
            }

            sp_ -= 1;
            bus_.WriteByte(sp_, static_cast<uint8_t>(pc_ & 0xFF), ComponentSource::CPU);
            interrupts_.interruptFlag &= ~interruptMask;
            pc_ = InterruptAddress(interruptBit);

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
            currentInstruction = bus_.ReadByte(pc_++, ComponentSource::CPU);
            interruptState = M1;
            mCycleCounter_ = 1;
            return false;
        }
    }
    return true;
}

template class CPU<Bus>;
