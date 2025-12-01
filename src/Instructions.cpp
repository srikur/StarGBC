#include "Instructions.h"
#include <cstring>

void Instructions::HandleOAMCorruption(const CPU &cpu, const uint16_t location, const CorruptionType type) const {
    if (!cpu.IsDMG() || (location < 0xFE00 || location > 0xFEFF) || cpu.bus->gpu_->stat.mode != GPU::Mode::MODE_2) return;
    if (cpu.bus->gpu_->scanlineCounter >= 76) return;
    const int currentRowIndex = cpu.bus->gpu_->GetOAMScanRow();

    auto ReadWord = [&](const int index) -> uint16_t {
        return static_cast<uint16_t>(cpu.bus->gpu_->oam[index]) << 8 | cpu.bus->gpu_->oam[index + 1];
    };
    auto WriteWord = [&](const int index, const uint16_t value) {
        cpu.bus->gpu_->oam[index] = static_cast<uint8_t>(value >> 8);
        cpu.bus->gpu_->oam[index + 1] = static_cast<uint8_t>(value & 0xFF);
    };

    if (type == CorruptionType::ReadWrite) {
        if (currentRowIndex >= 4 && currentRowIndex < 19) {
            const int row_n_addr = currentRowIndex * 8;
            const int row_n_minus_1_addr = (currentRowIndex - 1) * 8;
            const int row_n_minus_2_addr = (currentRowIndex - 2) * 8;

            const uint16_t a_rw = ReadWord(row_n_minus_2_addr);
            const uint16_t b_rw = ReadWord(row_n_minus_1_addr);
            const uint16_t c_rw = ReadWord(row_n_addr);
            const uint16_t d_rw = ReadWord(row_n_minus_1_addr + 4);

            const uint16_t corrupted_b = (b_rw & (a_rw | c_rw | d_rw)) | (a_rw & c_rw & d_rw);
            WriteWord(row_n_minus_1_addr, corrupted_b);

            uint8_t temp_row[8];
            std::memcpy(temp_row, &cpu.bus->gpu_->oam[row_n_minus_1_addr], 8);
            std::memcpy(&cpu.bus->gpu_->oam[row_n_addr], temp_row, 8);
            std::memcpy(&cpu.bus->gpu_->oam[row_n_minus_2_addr], temp_row, 8);
        }

        if (currentRowIndex > 0) {
            const int currentRowAddr = currentRowIndex * 8;
            const int prevRowAddr = (currentRowIndex - 1) * 8;

            const uint16_t a_read = ReadWord(currentRowAddr);
            const uint16_t b_read = ReadWord(prevRowAddr);
            const uint16_t c_read = ReadWord(prevRowAddr + 4);

            const uint16_t corruptedWord = b_read | (a_read & c_read);
            WriteWord(currentRowAddr, corruptedWord);

            std::memcpy(&cpu.bus->gpu_->oam[currentRowAddr + 2], &cpu.bus->gpu_->oam[prevRowAddr + 2], 6);
        }
    } else {
        if (currentRowIndex == 0) return;

        const int currentRowAddr = currentRowIndex * 8;
        const int prevRowAddr = (currentRowIndex - 1) * 8;

        const uint16_t a = ReadWord(currentRowAddr);
        const uint16_t b = ReadWord(prevRowAddr);
        const uint16_t c = ReadWord(prevRowAddr + 4);

        const uint16_t corruptedWord = (type == CorruptionType::Write)
                                           ? ((a ^ c) & (b ^ c)) ^ c
                                           : (b | (a & c));
        WriteWord(currentRowAddr, corruptedWord);
        std::memcpy(&cpu.bus->gpu_->oam[currentRowAddr + 2], &cpu.bus->gpu_->oam[prevRowAddr + 2], 6);
    }
}

template<Register source>
bool Instructions::RLC(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = cpu.regs.get()->*sourceReg;
    const uint8_t old = value & 0x80 ? 1 : 0;
    cpu.regs->SetCarry(old != 0);
    const uint8_t newValue = (value << 1) | old;
    cpu.regs->SetZero(newValue == 0);
    cpu.regs.get()->*sourceReg = newValue;
    cpu.regs->SetSubtract(false);
    cpu.regs->SetHalf(false);
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::RLCAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t old = byte & 0x80 ? 1 : 0;
        cpu.regs->SetCarry(old != 0);
        byte = byte << 1 | old;
        cpu.bus->WriteByte(cpu.regs->GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.regs->SetZero(byte == 0);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetHalf(false);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::DAA(CPU &cpu) const {
    uint8_t adjust = 0;
    bool carry = cpu.regs->FlagCarry();

    if (!cpu.regs->FlagSubtract()) {
        if (cpu.regs->FlagHalf() || (cpu.regs->a & 0x0F) > 0x09)
            adjust |= 0x06;

        if (cpu.regs->FlagCarry() || cpu.regs->a > 0x99) {
            adjust |= 0x60;
            carry = true;
        }

        cpu.regs->a += adjust;
    } else {
        if (cpu.regs->FlagHalf()) { adjust |= 0x06; }
        if (cpu.regs->FlagCarry()) { adjust |= 0x60; }
        cpu.regs->a -= adjust;
    }

    cpu.regs->SetCarry(carry);
    cpu.regs->SetZero(cpu.regs->a == 0);
    cpu.regs->SetHalf(false);
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::RETI(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = cpu.bus->ReadByte(cpu.sp);
        cpu.sp += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(cpu.bus->ReadByte(cpu.sp)) << 8;
        cpu.sp += 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.pc = word;
        cpu.bus->interruptMasterEnable = true;
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::DI(CPU &cpu) const {
    cpu.bus->interruptDelay = false;
    cpu.bus->interruptMasterEnable = false;
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::EI(CPU &cpu) const {
    if (!cpu.bus->interruptMasterEnable) {
        cpu.icount = 0;
        cpu.bus->interruptDelay = true;
        cpu.bus->interruptMasterEnable = true;
    }
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::HALT(CPU &cpu) const {
    if (const bool bug = (cpu.bus->interruptEnable & cpu.bus->interruptFlag & 0x1F) != 0; !cpu.bus->interruptMasterEnable && bug) {
        cpu.haltBug = true;
        cpu.halted = false;
    } else {
        cpu.haltBug = false;
        cpu.halted = true;
    }
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

template<RSTTarget target>
bool Instructions::RST(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        cpu.sp -= 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.bus->WriteByte(cpu.sp, (cpu.pc & 0xFF00) >> 8);
        cpu.sp -= 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.bus->WriteByte(cpu.sp, cpu.pc & 0xFF);
        constexpr auto location = GetRSTAddress<target>();
        if constexpr (target == RSTTarget::H38) {
            std::fprintf(stderr, "RST 38\n");
        }
        cpu.pc = location;
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::CALLUnconditional(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = cpu.bus->ReadByte(cpu.pc);
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(cpu.bus->ReadByte(cpu.pc)) << 8;
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.sp -= 1;
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.bus->WriteByte(cpu.sp, (cpu.pc & 0xFF00) >> 8);
        cpu.sp -= 1;
        return false;
    }
    if (cpu.mCycleCounter == 6) {
        cpu.bus->WriteByte(cpu.sp, cpu.pc & 0xFF);
        cpu.pc = word;
        return false;
    }
    if (cpu.mCycleCounter == 7) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<JumpTest test>
bool Instructions::CALL(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = cpu.bus->ReadByte(cpu.pc);
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        if constexpr (test == JumpTest::NotZero) jumpCondition = !cpu.regs->FlagZero();
        else if constexpr (test == JumpTest::Zero) jumpCondition = cpu.regs->FlagZero();
        else if constexpr (test == JumpTest::Carry) jumpCondition = cpu.regs->FlagCarry();
        else if constexpr (test == JumpTest::NotCarry) jumpCondition = !cpu.regs->FlagCarry();
        word |= static_cast<uint16_t>(cpu.bus->ReadByte(cpu.pc)) << 8;
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        if (jumpCondition) {
            cpu.sp -= 1;
            return false;
        }

        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.bus->WriteByte(cpu.sp, (cpu.pc & 0xFF00) >> 8);
        cpu.sp -= 1;
        return false;
    }
    if (cpu.mCycleCounter == 6) {
        cpu.bus->WriteByte(cpu.sp, cpu.pc & 0xFF);
        cpu.pc = word;
        return false;
    }
    if (cpu.mCycleCounter == 7) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::RLCA(CPU &cpu) const {
    const uint8_t old = (cpu.regs->a & 0x80) != 0 ? 1 : 0;
    cpu.regs->SetCarry(old != 0);
    cpu.regs->a = cpu.regs->a << 1 | old;
    cpu.regs->SetZero(false);
    cpu.regs->SetHalf(false);
    cpu.regs->SetSubtract(false);
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::RLA(CPU &cpu) const {
    const bool flag_c = (cpu.regs->a & 0x80) >> 7 == 0x01;
    const uint8_t r = (cpu.regs->a << 1) + static_cast<uint8_t>(cpu.regs->FlagCarry());
    cpu.regs->SetCarry(flag_c);
    cpu.regs->SetZero(false);
    cpu.regs->SetHalf(false);
    cpu.regs->SetSubtract(false);
    cpu.regs->a = r;
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

template<Register source>
bool Instructions::RL(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = cpu.regs.get()->*sourceReg;
    const bool flag_c = (value & 0x80) >> 7 == 0x01;
    const uint8_t newValue = value << 1 | static_cast<uint8_t>(cpu.regs->FlagCarry());
    cpu.regs->SetCarry(flag_c);
    cpu.regs->SetZero(newValue == 0);
    cpu.regs->SetHalf(false);
    cpu.regs->SetSubtract(false);
    cpu.regs.get()->*sourceReg = newValue;
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::RLAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t oldCarry = cpu.regs->FlagCarry() ? 1 : 0;
        cpu.regs->SetCarry((byte & 0x80) != 0);
        byte = (byte << 1) | oldCarry;
        cpu.bus->WriteByte(cpu.regs->GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.regs->SetZero(byte == 0);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetHalf(false);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::CCF(CPU &cpu) const {
    cpu.regs->SetSubtract(false);
    cpu.regs->SetCarry(!cpu.regs->FlagCarry());
    cpu.regs->SetHalf(false);
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::CPL(CPU &cpu) const {
    cpu.regs->SetHalf(true);
    cpu.regs->SetSubtract(true);
    cpu.regs->a = ~cpu.regs->a;
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::SCF(CPU &cpu) const {
    cpu.regs->SetSubtract(false);
    cpu.regs->SetHalf(false);
    cpu.regs->SetCarry(true);
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::RRCA(CPU &cpu) const {
    cpu.regs->SetCarry((cpu.regs->a & 0x01) != 0);
    cpu.regs->a = cpu.regs->a >> 1 | (cpu.regs->a & 0x01) << 7;
    cpu.regs->SetZero(false);
    cpu.regs->SetSubtract(false);
    cpu.regs->SetHalf(false);
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

template<Register source>
bool Instructions::RRC(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = cpu.regs.get()->*sourceReg;
    const bool carry = (value & 0x01) == 0x01;
    cpu.regs->SetCarry(carry);
    const uint8_t newValue = cpu.regs->FlagCarry() ? 0x80 | value >> 1 : value >> 1;
    cpu.regs->SetZero(newValue == 0);
    cpu.regs.get()->*sourceReg = newValue;
    cpu.regs->SetSubtract(false);
    cpu.regs->SetHalf(false);
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::RRCAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs->SetCarry(byte & 0x01);
        byte = cpu.regs->FlagCarry() ? 0x80 | byte >> 1 : byte >> 1;
        cpu.bus->WriteByte(cpu.regs->GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.regs->SetZero(byte == 0);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetHalf(false);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::RRAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word = (byte & 0x01) == 0x01; // hack for storing carry
        byte = cpu.regs->FlagCarry() ? 0x80 | (byte >> 1) : byte >> 1;
        cpu.bus->WriteByte(cpu.regs->GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.regs->SetCarry(word);
        cpu.regs->SetZero(byte == 0);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetHalf(false);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::RR(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = cpu.regs.get()->*sourceReg;
    const bool carry = (value & 0x01) == 0x01;
    uint8_t newValue = cpu.regs->FlagCarry() ? 0x80 | (value >> 1) : value >> 1;
    cpu.regs.get()->*sourceReg = newValue;
    cpu.regs->SetCarry(carry);
    cpu.regs->SetZero(newValue == 0);
    cpu.regs->SetSubtract(false);
    cpu.regs->SetHalf(false);
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::RRA(CPU &cpu) const {
    const bool carry = (cpu.regs->a & 0x01) == 0x01;
    const uint8_t newValue = cpu.regs->FlagCarry() ? 0x80 | cpu.regs->a >> 1 : cpu.regs->a >> 1;
    cpu.regs->SetZero(false);
    cpu.regs->a = newValue;
    cpu.regs->SetCarry(carry);
    cpu.regs->SetSubtract(false);
    cpu.regs->SetHalf(false);
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::RETUnconditional(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = cpu.bus->ReadByte(cpu.sp);
        cpu.sp += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(cpu.bus->ReadByte(cpu.sp)) << 8;
        cpu.sp += 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.pc = word;
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<JumpTest test>
bool Instructions::RETConditional(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        if constexpr (test == JumpTest::NotZero) jumpCondition = !cpu.regs->FlagZero();
        else if constexpr (test == JumpTest::Zero) jumpCondition = cpu.regs->FlagZero();
        else if constexpr (test == JumpTest::Carry) jumpCondition = cpu.regs->FlagCarry();
        else if constexpr (test == JumpTest::NotCarry) jumpCondition = !cpu.regs->FlagCarry();
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        if (jumpCondition) {
            word = cpu.bus->ReadByte(cpu.sp);
            cpu.sp += 1;
            return false;
        }

        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    if (cpu.mCycleCounter == 4) {
        word |= static_cast<uint16_t>(cpu.bus->ReadByte(cpu.sp)) << 8;
        cpu.sp += 1;
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.pc = word;
        return false;
    }
    if (cpu.mCycleCounter == 6) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::JRUnconditional(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        signedByte = std::bit_cast<int8_t>(cpu.bus->ReadByte(cpu.pc));
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint16_t next = cpu.pc;
        if (signedByte >= 0) {
            cpu.pc = next + static_cast<uint16_t>(signedByte);
        } else {
            cpu.pc = next - static_cast<uint16_t>(abs(signedByte));
        }
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<JumpTest test>
bool Instructions::JR(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        signedByte = std::bit_cast<int8_t>(cpu.bus->ReadByte(cpu.pc));
        cpu.pc += 1;
        if constexpr (test == JumpTest::NotZero) jumpCondition = !cpu.regs->FlagZero();
        else if constexpr (test == JumpTest::Zero) jumpCondition = cpu.regs->FlagZero();
        else if constexpr (test == JumpTest::Carry) jumpCondition = cpu.regs->FlagCarry();
        else if constexpr (test == JumpTest::NotCarry) jumpCondition = !cpu.regs->FlagCarry();
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint16_t next = cpu.pc;
        if (jumpCondition) {
            if (signedByte >= 0) {
                cpu.pc = next + static_cast<uint16_t>(signedByte);
            } else {
                cpu.pc = next - static_cast<uint16_t>(abs(signedByte));
            }
            return false;
        }
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::JPUnconditional(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = cpu.bus->ReadByte(cpu.pc);
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(cpu.bus->ReadByte(cpu.pc)) << 8;
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.pc = word;
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<JumpTest test>
bool Instructions::JP(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = cpu.bus->ReadByte(cpu.pc);
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(cpu.bus->ReadByte(cpu.pc)) << 8;
        cpu.pc += 1;
        if constexpr (test == JumpTest::NotZero) jumpCondition = !cpu.regs->FlagZero();
        else if constexpr (test == JumpTest::Zero) jumpCondition = cpu.regs->FlagZero();
        else if constexpr (test == JumpTest::Carry) jumpCondition = cpu.regs->FlagCarry();
        else if constexpr (test == JumpTest::NotCarry) jumpCondition = !cpu.regs->FlagCarry();
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        if (jumpCondition) {
            cpu.pc = word;
            return false;
        }
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::JPHL(CPU &cpu) const {
    cpu.pc = cpu.regs->GetHL();
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::NOP(CPU &cpu) const {
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::PREFIX(CPU &cpu) const {
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    cpu.currentInstruction = 0xCB00 | cpu.nextInstruction;
    cpu.prefixed = true;
    return true;
}

bool Instructions::STOP(CPU &cpu) const {
    const uint8_t key1 = cpu.bus->ReadByte(0xFF4D);
    const bool speedSwitchRequested = cpu.bus->prepareSpeedShift && (key1 & 0x01);
    cpu.bus->WriteByte(0xFF04, 0x00);

    if (speedSwitchRequested) {
        cpu.bus->ChangeSpeed();
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }

    cpu.stopped = true;
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

template<Register source>
bool Instructions::DECRegister(CPU &cpu) const {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t sourceValue = cpu.regs.get()->*sourceReg;
    cpu.regs->SetHalf((sourceValue & 0xF) == 0x00);
    const uint8_t newValue = sourceValue - 1;
    cpu.regs->SetZero(newValue == 0);
    cpu.regs->SetSubtract(true);
    cpu.regs.get()->*sourceReg = newValue;
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::DECIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs->SetHalf((byte & 0xF) == 0x00);
        const uint8_t newValue = byte - 1;
        cpu.regs->SetZero(newValue == 0);
        cpu.regs->SetSubtract(true);
        cpu.bus->WriteByte(cpu.regs->GetHL(), newValue);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Arithmetic16Target target>
bool Instructions::DEC16(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        if constexpr (target == Arithmetic16Target::BC) {
            word = cpu.regs->GetBC();
            HandleOAMCorruption(cpu, word, CorruptionType::Write);
            cpu.regs->SetBC(word - 1);
        } else if constexpr (target == Arithmetic16Target::DE) {
            word = cpu.regs->GetDE();
            HandleOAMCorruption(cpu, word, CorruptionType::Write);
            cpu.regs->SetDE(word - 1);
        } else if constexpr (target == Arithmetic16Target::HL) {
            word = cpu.regs->GetHL();
            cpu.regs->SetHL(word - 1);
            HandleOAMCorruption(cpu, word, CorruptionType::Write);
        } else if constexpr (target == Arithmetic16Target::SP) {
            HandleOAMCorruption(cpu, cpu.sp, CorruptionType::Write);
            cpu.sp -= 1;
        }
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::INCRegister(CPU &cpu) const {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t sourceValue = cpu.regs.get()->*sourceReg;
    cpu.regs->SetHalf((sourceValue & 0xF) == 0xF);
    const uint8_t newValue = sourceValue + 1;
    cpu.regs->SetZero(newValue == 0);
    cpu.regs->SetSubtract(false);
    cpu.regs.get()->*sourceReg = newValue;
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::INCIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs->SetHalf((byte & 0xF) == 0xF);
        const uint8_t newValue = byte + 1;
        cpu.regs->SetZero(newValue == 0);
        cpu.regs->SetSubtract(false);
        cpu.bus->WriteByte(cpu.regs->GetHL(), newValue);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Arithmetic16Target target>
bool Instructions::INC16(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        if constexpr (target == Arithmetic16Target::BC) {
            word = cpu.regs->GetBC();
            HandleOAMCorruption(cpu, word, CorruptionType::Write);
            cpu.regs->SetBC(word + 1);
        } else if constexpr (target == Arithmetic16Target::DE) {
            word = cpu.regs->GetDE();
            HandleOAMCorruption(cpu, word, CorruptionType::Write);
            cpu.regs->SetDE(word + 1);
        } else if constexpr (target == Arithmetic16Target::HL) {
            word = cpu.regs->GetHL();
            HandleOAMCorruption(cpu, word, CorruptionType::Write);
            cpu.regs->SetHL(word + 1);
        } else if constexpr (target == Arithmetic16Target::SP) {
            HandleOAMCorruption(cpu, cpu.sp, CorruptionType::Write);
            cpu.sp += 1;
        }
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

// 0xE0
bool Instructions::LoadFromAccumulatorDirectA(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = static_cast<uint16_t>(cpu.bus->ReadByte(cpu.pc));
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.bus->WriteByte(static_cast<uint16_t>(0xFF00) | word, cpu.regs->a);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

// 0xE2
bool Instructions::LoadFromAccumulatorIndirectC(CPU &cpu) const {
    if (cpu.mCycleCounter == 2) {
        const uint16_t c = 0xFF00 | static_cast<uint16_t>(cpu.regs->c);
        cpu.bus->WriteByte(c, cpu.regs->a);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

// 0xF0
bool Instructions::LoadAccumulatorA(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word = 0xFF00 | static_cast<uint16_t>(byte);
        byte = cpu.bus->ReadByte(word);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        cpu.regs->a = byte;
        return true;
    }
    return false;
}

// 0xF2
bool Instructions::LoadAccumulatorIndirectC(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(0xFF00 | static_cast<uint16_t>(cpu.regs->c));
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs->a = byte;
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

/* M2 -- one cycle */
template<Register target, Register source>
bool Instructions::LDRegister(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    constexpr auto targetReg = GetRegisterPtr<target>();
    const uint8_t sourceValue = cpu.regs.get()->*sourceReg;
    cpu.regs.get()->*targetReg = sourceValue;
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

template<Register source>
bool Instructions::LDRegisterImmediate(CPU &cpu) {
    constexpr auto targetReg = GetRegisterPtr<source>();
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs.get()->*targetReg = byte;
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::LDRegisterIndirect(CPU &cpu) {
    constexpr auto targetReg = GetRegisterPtr<source>();
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs.get()->*targetReg = byte;
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::LDAddrRegister(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    if (cpu.mCycleCounter == 2) {
        const uint8_t sourceValue = cpu.regs.get()->*sourceReg;
        cpu.bus->WriteByte(cpu.regs->GetHL(), sourceValue);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDAddrImmediate(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.bus->WriteByte(cpu.regs->GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDAccumulatorBC(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetBC());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs->a = byte;
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDAccumulatorDE(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetDE());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs->a = byte;
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDFromAccBC(CPU &cpu) const {
    if (cpu.mCycleCounter == 2) {
        cpu.bus->WriteByte(cpu.regs->GetBC(), cpu.regs->a);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDFromAccDE(CPU &cpu) const {
    if (cpu.mCycleCounter == 2) {
        cpu.bus->WriteByte(cpu.regs->GetDE(), cpu.regs->a);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDAccumulatorDirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = cpu.bus->ReadByte(cpu.pc);
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(cpu.bus->ReadByte(cpu.pc)) << 8;
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.regs->a = cpu.bus->ReadByte(word);
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDFromAccumulatorDirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = cpu.bus->ReadByte(cpu.pc);
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(cpu.bus->ReadByte(cpu.pc)) << 8;
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.bus->WriteByte(word, cpu.regs->a);
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc);
        cpu.pc += 1;
        return true;
    }
    return false;
}

bool Instructions::LDAccumulatorIndirectDec(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        HandleOAMCorruption(cpu, cpu.regs->GetHL(), CorruptionType::ReadWrite);
        cpu.regs->SetHL(cpu.regs->GetHL() - 1);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs->a = byte;
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDFromAccumulatorIndirectDec(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = cpu.regs->GetHL();
        HandleOAMCorruption(cpu, word, CorruptionType::Write);
        cpu.bus->WriteByte(word, cpu.regs->a);
        HandleOAMCorruption(cpu, word, CorruptionType::Write);
        cpu.regs->SetHL(word - 1);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDAccumulatorIndirectInc(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        HandleOAMCorruption(cpu, cpu.regs->GetHL(), CorruptionType::ReadWrite);
        cpu.regs->SetHL(cpu.regs->GetHL() + 1);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs->a = byte;
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDFromAccumulatorIndirectInc(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = cpu.regs->GetHL();
        HandleOAMCorruption(cpu, word, CorruptionType::Write);
        cpu.bus->WriteByte(word, cpu.regs->a);
        HandleOAMCorruption(cpu, word, CorruptionType::Write);
        cpu.regs->SetHL(word + 1);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LD16FromStack(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = cpu.bus->ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(cpu.bus->ReadByte(cpu.pc++)) << 8;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.bus->WriteByte(word, cpu.sp & 0xFF);
        word += 1;
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.bus->WriteByte(word, cpu.sp >> 8);
        return false;
    }
    if (cpu.mCycleCounter == 6) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<LoadWordTarget target>
bool Instructions::LD16Register(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = cpu.bus->ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(cpu.bus->ReadByte(cpu.pc++)) << 8;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        if constexpr (target == LoadWordTarget::HL) cpu.regs->SetHL(word);
        else if constexpr (target == LoadWordTarget::SP) cpu.sp = word;
        else if constexpr (target == LoadWordTarget::BC) cpu.regs->SetBC(word);
        else if constexpr (target == LoadWordTarget::DE) cpu.regs->SetDE(word);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LD16Stack(CPU &cpu) const {
    if (cpu.mCycleCounter == 2) {
        cpu.sp = cpu.regs->GetHL();
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LD16StackAdjusted(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(
            cpu.bus->ReadByte(cpu.pc++))));
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs->SetCarry((cpu.sp & 0xFF) + (word & 0xFF) > 0xFF);
        cpu.regs->SetHalf((cpu.sp & 0xF) + (word & 0xF) > 0xF);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetZero(false);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.regs->SetHL(cpu.sp + word);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<StackTarget target>
bool Instructions::PUSH(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        HandleOAMCorruption(cpu, cpu.sp, CorruptionType::Write);
        cpu.sp -= 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        HandleOAMCorruption(cpu, cpu.sp, CorruptionType::Write);
        if constexpr (target == StackTarget::BC) word = cpu.regs->GetBC();
        if constexpr (target == StackTarget::DE) word = cpu.regs->GetDE();
        if constexpr (target == StackTarget::HL) word = cpu.regs->GetHL();
        if constexpr (target == StackTarget::AF) word = cpu.regs->GetAF();
        cpu.bus->WriteByte(cpu.sp, (word & 0xFF00) >> 8);
        cpu.sp -= 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        HandleOAMCorruption(cpu, cpu.sp, CorruptionType::Write);
        cpu.bus->WriteByte(cpu.sp, word & 0xFF);
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<StackTarget target>
bool Instructions::POP(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        HandleOAMCorruption(cpu, cpu.sp, CorruptionType::ReadWrite);
        word = cpu.bus->ReadByte(cpu.sp);
        cpu.sp += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        HandleOAMCorruption(cpu, cpu.sp, CorruptionType::Read);
        word |= static_cast<uint16_t>(cpu.bus->ReadByte(cpu.sp)) << 8;
        cpu.sp += 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        if constexpr (target == StackTarget::BC) cpu.regs->SetBC(word);
        if constexpr (target == StackTarget::DE) cpu.regs->SetDE(word);
        if constexpr (target == StackTarget::HL) cpu.regs->SetHL(word);
        if constexpr (target == StackTarget::AF) cpu.regs->SetAF(word & 0xFFF0);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::CPRegister(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = cpu.regs.get()->*sourceReg;
    const uint8_t new_value = cpu.regs->a - value;
    cpu.regs->SetCarry(cpu.regs->a < value);
    cpu.regs->SetHalf((cpu.regs->a & 0xF) < (value & 0xF));
    cpu.regs->SetSubtract(true);
    cpu.regs->SetZero(new_value == 0x0);
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::CPIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t new_value = cpu.regs->a - byte;
        cpu.regs->SetCarry(cpu.regs->a < byte);
        cpu.regs->SetHalf((cpu.regs->a & 0xF) < (byte & 0xF));
        cpu.regs->SetSubtract(true);
        cpu.regs->SetZero(new_value == 0x0);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::CPImmediate(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t new_value = cpu.regs->a - byte;
        cpu.regs->SetCarry(cpu.regs->a < byte);
        cpu.regs->SetHalf((cpu.regs->a & 0xF) < (byte & 0xF));
        cpu.regs->SetSubtract(true);
        cpu.regs->SetZero(new_value == 0x0);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return true;
}

template<Register source>
bool Instructions::ORRegister(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = cpu.regs.get()->*sourceReg;
    cpu.regs->a |= value;
    cpu.regs->SetZero(cpu.regs->a == 0);
    cpu.regs->SetSubtract(false);
    cpu.regs->SetHalf(false);
    cpu.regs->SetCarry(false);
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::ORIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs->a |= byte;
        cpu.regs->SetZero(cpu.regs->a == 0);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetHalf(false);
        cpu.regs->SetCarry(false);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::ORImmediate(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs->a |= byte;
        cpu.regs->SetZero(cpu.regs->a == 0);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetHalf(false);
        cpu.regs->SetCarry(false);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::XORRegister(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = cpu.regs.get()->*sourceReg;
    cpu.regs->a ^= value;
    cpu.regs->SetZero(cpu.regs->a == 0);
    cpu.regs->SetSubtract(false);
    cpu.regs->SetHalf(false);
    cpu.regs->SetCarry(false);
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::XORIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs->a ^= byte;
        cpu.regs->SetZero(cpu.regs->a == 0);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetHalf(false);
        cpu.regs->SetCarry(false);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::XORImmediate(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs->a ^= byte;
        cpu.regs->SetZero(cpu.regs->a == 0);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetHalf(false);
        cpu.regs->SetCarry(false);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::AND(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = cpu.regs.get()->*sourceReg;
    cpu.regs->a &= value;
    cpu.regs->SetZero(cpu.regs->a == 0);
    cpu.regs->SetSubtract(false);
    cpu.regs->SetHalf(true);
    cpu.regs->SetCarry(false);
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::ANDImmediate(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs->a &= byte;
        cpu.regs->SetZero(cpu.regs->a == 0);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetHalf(true);
        cpu.regs->SetCarry(false);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::ANDIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs->a &= byte;
        cpu.regs->SetZero(cpu.regs->a == 0);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetHalf(true);
        cpu.regs->SetCarry(false);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::SUB(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = cpu.regs.get()->*sourceReg;
    const uint8_t new_value = cpu.regs->a - value;
    cpu.regs->SetCarry(cpu.regs->a < value);
    cpu.regs->SetHalf((cpu.regs->a & 0xF) < (value & 0xF));
    cpu.regs->SetSubtract(true);
    cpu.regs->SetZero(new_value == 0x0);
    cpu.regs->a = new_value;
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::SUBImmediate(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t new_value = cpu.regs->a - byte;
        cpu.regs->SetCarry(cpu.regs->a < byte);
        cpu.regs->SetHalf((cpu.regs->a & 0xF) < (byte & 0xF));
        cpu.regs->SetSubtract(true);
        cpu.regs->SetZero(new_value == 0x0);
        cpu.regs->a = new_value;
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::SUBIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t new_value = cpu.regs->a - byte;
        cpu.regs->SetCarry(cpu.regs->a < byte);
        cpu.regs->SetHalf((cpu.regs->a & 0xF) < (byte & 0xF));
        cpu.regs->SetSubtract(true);
        cpu.regs->SetZero(new_value == 0x0);
        cpu.regs->a = new_value;
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::SLA(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = cpu.regs.get()->*sourceReg;
    cpu.regs->SetCarry(((value & 0x80) >> 7) == 0x01);
    const uint8_t newValue = value << 1;
    cpu.regs.get()->*sourceReg = newValue;
    cpu.regs->SetZero(newValue == 0);
    cpu.regs->SetSubtract(false);
    cpu.regs->SetHalf(false);
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::SLAAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs->SetCarry((byte & 0x80) != 0);
        byte <<= 1;
        cpu.bus->WriteByte(cpu.regs->GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.regs->SetZero(byte == 0);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetHalf(false);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::SRA(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = cpu.regs.get()->*sourceReg;
    const uint8_t newValue = (value >> 1) | (value & 0x80);
    cpu.regs.get()->*sourceReg = newValue;
    cpu.regs->SetCarry((value & 0x01) != 0);
    cpu.regs->SetZero(newValue == 0);
    cpu.regs->SetSubtract(false);
    cpu.regs->SetHalf(false);
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::SRAAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs->SetCarry((byte & 0x01) != 0);
        byte = (byte >> 1) | (byte & 0x80);
        cpu.bus->WriteByte(cpu.regs->GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.regs->SetZero(byte == 0);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetHalf(false);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::SWAP(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    uint8_t value = cpu.regs.get()->*sourceReg;
    value = (value >> 4) | (value << 4);
    cpu.regs.get()->*sourceReg = value;
    cpu.regs->SetZero(value == 0);
    cpu.regs->SetCarry(false);
    cpu.regs->SetSubtract(false);
    cpu.regs->SetHalf(false);
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::SWAPAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        byte = (byte >> 4) | (byte << 4);
        cpu.bus->WriteByte(cpu.regs->GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.regs->SetZero(byte == 0);
        cpu.regs->SetCarry(false);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetHalf(false);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::SRL(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    uint8_t value = cpu.regs.get()->*sourceReg;
    cpu.regs->SetCarry((value & 0x01) != 0);
    value = value >> 1;
    cpu.regs.get()->*sourceReg = value;
    cpu.regs->SetZero(value == 0);
    cpu.regs->SetSubtract(false);
    cpu.regs->SetHalf(false);
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::SRLAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        cpu.regs->SetCarry((byte & 0x01) != 0);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        byte = byte >> 1;
        cpu.bus->WriteByte(cpu.regs->GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.regs->SetZero(byte == 0);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetHalf(false);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

/* M3 -- prefixed, so only one */
template<Register source, int bit>
bool Instructions::BIT(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t sourceValue = cpu.regs.get()->*sourceReg;
    cpu.regs->SetZero((sourceValue & (1 << bit)) == 0);
    cpu.regs->SetSubtract(false);
    cpu.regs->SetHalf(true);
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

/* M4 -- prefixed, M3 fetches byte */
template<int bit>
bool Instructions::BITAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.regs->SetZero((byte & (1 << bit)) == 0);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetHalf(true);
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source, int bit>
bool Instructions::RES(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    uint8_t sourceValue = cpu.regs.get()->*sourceReg;
    sourceValue &= ~(1 << bit);
    cpu.regs.get()->*sourceReg = sourceValue;
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

template<int bit>
bool Instructions::RESAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        byte &= ~(1 << bit);
        cpu.bus->WriteByte(cpu.regs->GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source, int bit>
bool Instructions::SET(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    uint8_t sourceValue = cpu.regs.get()->*sourceReg;
    sourceValue |= 1 << bit;
    cpu.regs.get()->*sourceReg = sourceValue;
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

template<int bit>
bool Instructions::SETAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        byte |= 1 << bit;
        cpu.bus->WriteByte(cpu.regs->GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::SBCRegister(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = cpu.regs.get()->*sourceReg;
    const uint8_t flag_carry = cpu.regs->FlagCarry() ? 1 : 0;
    const uint8_t r = cpu.regs->a - value - flag_carry;
    cpu.regs->SetCarry(cpu.regs->a < value + static_cast<uint16_t>(flag_carry));
    cpu.regs->SetHalf((cpu.regs->a & 0xF) < (value & 0xF) + flag_carry);
    cpu.regs->SetSubtract(true);
    cpu.regs->SetZero(r == 0x0);
    cpu.regs->a = r;
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::SBCIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t flag_carry = cpu.regs->FlagCarry() ? 1 : 0;
        const uint8_t r = cpu.regs->a - byte - flag_carry;
        cpu.regs->SetCarry(cpu.regs->a < byte + static_cast<uint16_t>(flag_carry));
        cpu.regs->SetHalf((cpu.regs->a & 0xF) < (byte & 0xF) + flag_carry);
        cpu.regs->SetSubtract(true);
        cpu.regs->SetZero(r == 0x0);
        cpu.regs->a = r;
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::SBCImmediate(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t flag_carry = cpu.regs->FlagCarry() ? 1 : 0;
        const uint8_t r = cpu.regs->a - byte - flag_carry;
        cpu.regs->SetCarry(cpu.regs->a < byte + static_cast<uint16_t>(flag_carry));
        cpu.regs->SetHalf((cpu.regs->a & 0xF) < (byte & 0xF) + flag_carry);
        cpu.regs->SetSubtract(true);
        cpu.regs->SetZero(r == 0x0);
        cpu.regs->a = r;
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::ADCRegister(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = cpu.regs.get()->*sourceReg;
    const uint8_t flag_carry = cpu.regs->FlagCarry() ? 1 : 0;
    const uint8_t r = cpu.regs->a + value + flag_carry;
    cpu.regs->SetCarry(static_cast<uint16_t>(cpu.regs->a) + value + static_cast<uint16_t>(flag_carry) > 0xFF);
    cpu.regs->SetHalf(((cpu.regs->a & 0xF) + (value & 0xF) + (flag_carry & 0xF)) > 0xF);
    cpu.regs->SetSubtract(false);
    cpu.regs->SetZero(r == 0x0);
    cpu.regs->a = r;
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::ADCIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t flag_carry = cpu.regs->FlagCarry() ? 1 : 0;
        const uint8_t r = cpu.regs->a + byte + flag_carry;
        cpu.regs->SetCarry(static_cast<uint16_t>(cpu.regs->a) + byte + static_cast<uint16_t>(flag_carry) > 0xFF);
        cpu.regs->SetHalf(((cpu.regs->a & 0xF) + (byte & 0xF) + (flag_carry & 0xF)) > 0xF);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetZero(r == 0x0);
        cpu.regs->a = r;
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::ADCImmediate(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t flag_carry = cpu.regs->FlagCarry() ? 1 : 0;
        const uint8_t r = cpu.regs->a + byte + flag_carry;
        cpu.regs->SetCarry(static_cast<uint16_t>(cpu.regs->a) + byte + static_cast<uint16_t>(flag_carry) > 0xFF);
        cpu.regs->SetHalf(((cpu.regs->a & 0xF) + (byte & 0xF) + (flag_carry & 0xF)) > 0xF);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetZero(r == 0x0);
        cpu.regs->a = r;
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Arithmetic16Target target>
bool Instructions::ADD16(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        if constexpr (target == Arithmetic16Target::BC) word = cpu.regs->GetBC();
        if constexpr (target == Arithmetic16Target::DE) word = cpu.regs->GetDE();
        if constexpr (target == Arithmetic16Target::HL) word = cpu.regs->GetHL();
        if constexpr (target == Arithmetic16Target::SP) word = cpu.sp;
        return false;
    }

    if (cpu.mCycleCounter == 3) {
        const uint16_t reg = cpu.regs->GetHL();
        const uint16_t sum = reg + word;

        cpu.regs->SetCarry(reg > 0xFFFF - word);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetHalf((reg & 0x07FF) + (word & 0x07FF) > 0x07FF);
        cpu.regs->SetHL(sum);

        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::ADDRegister(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = cpu.regs.get()->*sourceReg;
    const uint8_t a = cpu.regs->a;
    const uint8_t new_value = a + value;
    cpu.regs->SetCarry(static_cast<uint16_t>(a) + static_cast<uint16_t>(value) > 0xFF);
    cpu.regs->SetZero(new_value == 0);
    cpu.regs->SetSubtract(false);
    cpu.regs->SetHalf((cpu.regs->a & 0xF) + (value & 0xF) > 0xF);
    cpu.regs->a = new_value;
    cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
    return true;
}

bool Instructions::ADDIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.regs->GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t a = cpu.regs->a;
        const uint8_t new_value = a + byte;
        cpu.regs->SetCarry(static_cast<uint16_t>(a) + static_cast<uint16_t>(byte) > 0xFF);
        cpu.regs->SetZero(new_value == 0);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetHalf((cpu.regs->a & 0xF) + (byte & 0xF) > 0xF);
        cpu.regs->a = new_value;
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::ADDImmediate(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = cpu.bus->ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t a = cpu.regs->a;
        const uint8_t new_value = a + byte;
        cpu.regs->SetCarry(static_cast<uint16_t>(a) + static_cast<uint16_t>(byte) > 0xFF);
        cpu.regs->SetZero(new_value == 0);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetHalf((cpu.regs->a & 0xF) + (byte & 0xF) > 0xF);
        cpu.regs->a = new_value;
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

// Not exactly M-cycle accurate (GBCTR page 80)
bool Instructions::ADDSigned(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(
            cpu.bus->ReadByte(cpu.pc++))));
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word2 = cpu.sp;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.regs->SetCarry(((word2 & 0xFF) + (word & 0xFF)) > 0xFF);
        cpu.regs->SetHalf(((word2 & 0xF) + (word & 0xF)) > 0xF);
        cpu.regs->SetSubtract(false);
        cpu.regs->SetZero(false);
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.sp = word2 + word;
        cpu.nextInstruction = cpu.bus->ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

// template function definitions to solve linker errors
template bool Instructions::RST<RSTTarget::H00>(CPU &cpu);

template bool Instructions::RST<RSTTarget::H08>(CPU &cpu);

template bool Instructions::RST<RSTTarget::H10>(CPU &cpu);

template bool Instructions::RST<RSTTarget::H18>(CPU &cpu);

template bool Instructions::RST<RSTTarget::H20>(CPU &cpu);

template bool Instructions::RST<RSTTarget::H28>(CPU &cpu);

template bool Instructions::RST<RSTTarget::H30>(CPU &cpu);

template bool Instructions::RST<RSTTarget::H38>(CPU &cpu);

template bool Instructions::CALL<JumpTest::NotZero>(CPU &cpu);

template bool Instructions::CALL<JumpTest::Zero>(CPU &cpu);

template bool Instructions::CALL<JumpTest::NotCarry>(CPU &cpu);

template bool Instructions::CALL<JumpTest::Carry>(CPU &cpu);

template bool Instructions::RETConditional<JumpTest::NotZero>(CPU &cpu);

template bool Instructions::RETConditional<JumpTest::Zero>(CPU &cpu);

template bool Instructions::RETConditional<JumpTest::NotCarry>(CPU &cpu);

template bool Instructions::RETConditional<JumpTest::Carry>(CPU &cpu);

template bool Instructions::JR<JumpTest::NotZero>(CPU &cpu);

template bool Instructions::JR<JumpTest::Zero>(CPU &cpu);

template bool Instructions::JR<JumpTest::NotCarry>(CPU &cpu);

template bool Instructions::JR<JumpTest::Carry>(CPU &cpu);

template bool Instructions::JP<JumpTest::NotZero>(CPU &cpu);

template bool Instructions::JP<JumpTest::Zero>(CPU &cpu);

template bool Instructions::JP<JumpTest::NotCarry>(CPU &cpu);

template bool Instructions::JP<JumpTest::Carry>(CPU &cpu);

template bool Instructions::DECRegister<Register::A>(CPU &cpu) const;

template bool Instructions::DECRegister<Register::B>(CPU &cpu) const;

template bool Instructions::DECRegister<Register::C>(CPU &cpu) const;

template bool Instructions::DECRegister<Register::D>(CPU &cpu) const;

template bool Instructions::DECRegister<Register::E>(CPU &cpu) const;

template bool Instructions::DECRegister<Register::H>(CPU &cpu) const;

template bool Instructions::DECRegister<Register::L>(CPU &cpu) const;

template bool Instructions::DEC16<Arithmetic16Target::BC>(CPU &cpu);

template bool Instructions::DEC16<Arithmetic16Target::DE>(CPU &cpu);

template bool Instructions::DEC16<Arithmetic16Target::HL>(CPU &cpu);

template bool Instructions::DEC16<Arithmetic16Target::SP>(CPU &cpu);

template bool Instructions::INCRegister<Register::A>(CPU &cpu) const;

template bool Instructions::INCRegister<Register::B>(CPU &cpu) const;

template bool Instructions::INCRegister<Register::C>(CPU &cpu) const;

template bool Instructions::INCRegister<Register::D>(CPU &cpu) const;

template bool Instructions::INCRegister<Register::E>(CPU &cpu) const;

template bool Instructions::INCRegister<Register::H>(CPU &cpu) const;

template bool Instructions::INCRegister<Register::L>(CPU &cpu) const;

template bool Instructions::INC16<Arithmetic16Target::BC>(CPU &cpu);

template bool Instructions::INC16<Arithmetic16Target::DE>(CPU &cpu);

template bool Instructions::INC16<Arithmetic16Target::HL>(CPU &cpu);

template bool Instructions::INC16<Arithmetic16Target::SP>(CPU &cpu);

template bool Instructions::LDRegister<Register::A, Register::A>(CPU &cpu);

template bool Instructions::LDRegister<Register::A, Register::B>(CPU &cpu);

template bool Instructions::LDRegister<Register::A, Register::C>(CPU &cpu);

template bool Instructions::LDRegister<Register::A, Register::D>(CPU &cpu);

template bool Instructions::LDRegister<Register::A, Register::E>(CPU &cpu);

template bool Instructions::LDRegister<Register::A, Register::H>(CPU &cpu);

template bool Instructions::LDRegister<Register::A, Register::L>(CPU &cpu);

template bool Instructions::LDRegister<Register::B, Register::A>(CPU &cpu);

template bool Instructions::LDRegister<Register::B, Register::B>(CPU &cpu);

template bool Instructions::LDRegister<Register::B, Register::C>(CPU &cpu);

template bool Instructions::LDRegister<Register::B, Register::D>(CPU &cpu);

template bool Instructions::LDRegister<Register::B, Register::E>(CPU &cpu);

template bool Instructions::LDRegister<Register::B, Register::H>(CPU &cpu);

template bool Instructions::LDRegister<Register::B, Register::L>(CPU &cpu);

template bool Instructions::LDRegister<Register::C, Register::A>(CPU &cpu);

template bool Instructions::LDRegister<Register::C, Register::B>(CPU &cpu);

template bool Instructions::LDRegister<Register::C, Register::C>(CPU &cpu);

template bool Instructions::LDRegister<Register::C, Register::D>(CPU &cpu);

template bool Instructions::LDRegister<Register::C, Register::E>(CPU &cpu);

template bool Instructions::LDRegister<Register::C, Register::H>(CPU &cpu);

template bool Instructions::LDRegister<Register::C, Register::L>(CPU &cpu);

template bool Instructions::LDRegister<Register::D, Register::A>(CPU &cpu);

template bool Instructions::LDRegister<Register::D, Register::B>(CPU &cpu);

template bool Instructions::LDRegister<Register::D, Register::C>(CPU &cpu);

template bool Instructions::LDRegister<Register::D, Register::D>(CPU &cpu);

template bool Instructions::LDRegister<Register::D, Register::E>(CPU &cpu);

template bool Instructions::LDRegister<Register::D, Register::H>(CPU &cpu);

template bool Instructions::LDRegister<Register::D, Register::L>(CPU &cpu);

template bool Instructions::LDRegister<Register::E, Register::A>(CPU &cpu);

template bool Instructions::LDRegister<Register::E, Register::B>(CPU &cpu);

template bool Instructions::LDRegister<Register::E, Register::C>(CPU &cpu);

template bool Instructions::LDRegister<Register::E, Register::D>(CPU &cpu);

template bool Instructions::LDRegister<Register::E, Register::E>(CPU &cpu);

template bool Instructions::LDRegister<Register::E, Register::H>(CPU &cpu);

template bool Instructions::LDRegister<Register::E, Register::L>(CPU &cpu);

template bool Instructions::LDRegister<Register::H, Register::A>(CPU &cpu);

template bool Instructions::LDRegister<Register::H, Register::B>(CPU &cpu);

template bool Instructions::LDRegister<Register::H, Register::C>(CPU &cpu);

template bool Instructions::LDRegister<Register::H, Register::D>(CPU &cpu);

template bool Instructions::LDRegister<Register::H, Register::E>(CPU &cpu);

template bool Instructions::LDRegister<Register::H, Register::H>(CPU &cpu);

template bool Instructions::LDRegister<Register::H, Register::L>(CPU &cpu);

template bool Instructions::LDRegister<Register::L, Register::A>(CPU &cpu);

template bool Instructions::LDRegister<Register::L, Register::B>(CPU &cpu);

template bool Instructions::LDRegister<Register::L, Register::C>(CPU &cpu);

template bool Instructions::LDRegister<Register::L, Register::D>(CPU &cpu);

template bool Instructions::LDRegister<Register::L, Register::E>(CPU &cpu);

template bool Instructions::LDRegister<Register::L, Register::H>(CPU &cpu);

template bool Instructions::LDRegister<Register::L, Register::L>(CPU &cpu);

template bool Instructions::LDRegisterImmediate<Register::A>(CPU &cpu);

template bool Instructions::LDRegisterImmediate<Register::B>(CPU &cpu);

template bool Instructions::LDRegisterImmediate<Register::C>(CPU &cpu);

template bool Instructions::LDRegisterImmediate<Register::D>(CPU &cpu);

template bool Instructions::LDRegisterImmediate<Register::E>(CPU &cpu);

template bool Instructions::LDRegisterImmediate<Register::H>(CPU &cpu);

template bool Instructions::LDRegisterImmediate<Register::L>(CPU &cpu);

template bool Instructions::LDRegisterIndirect<Register::A>(CPU &cpu);

template bool Instructions::LDRegisterIndirect<Register::B>(CPU &cpu);

template bool Instructions::LDRegisterIndirect<Register::C>(CPU &cpu);

template bool Instructions::LDRegisterIndirect<Register::D>(CPU &cpu);

template bool Instructions::LDRegisterIndirect<Register::E>(CPU &cpu);

template bool Instructions::LDRegisterIndirect<Register::H>(CPU &cpu);

template bool Instructions::LDRegisterIndirect<Register::L>(CPU &cpu);

template bool Instructions::LDAddrRegister<Register::A>(CPU &cpu);

template bool Instructions::LDAddrRegister<Register::B>(CPU &cpu);

template bool Instructions::LDAddrRegister<Register::C>(CPU &cpu);

template bool Instructions::LDAddrRegister<Register::D>(CPU &cpu);

template bool Instructions::LDAddrRegister<Register::E>(CPU &cpu);

template bool Instructions::LDAddrRegister<Register::H>(CPU &cpu);

template bool Instructions::LDAddrRegister<Register::L>(CPU &cpu);

template bool Instructions::LD16Register<LoadWordTarget::BC>(CPU &cpu);

template bool Instructions::LD16Register<LoadWordTarget::DE>(CPU &cpu);

template bool Instructions::LD16Register<LoadWordTarget::HL>(CPU &cpu);

template bool Instructions::LD16Register<LoadWordTarget::SP>(CPU &cpu);

template bool Instructions::PUSH<StackTarget::BC>(CPU &cpu);

template bool Instructions::PUSH<StackTarget::AF>(CPU &cpu);

template bool Instructions::PUSH<StackTarget::DE>(CPU &cpu);

template bool Instructions::PUSH<StackTarget::HL>(CPU &cpu);

template bool Instructions::POP<StackTarget::BC>(CPU &cpu);

template bool Instructions::POP<StackTarget::AF>(CPU &cpu);

template bool Instructions::POP<StackTarget::DE>(CPU &cpu);

template bool Instructions::POP<StackTarget::HL>(CPU &cpu);

template bool Instructions::CPRegister<Register::A>(CPU &cpu);

template bool Instructions::CPRegister<Register::B>(CPU &cpu);

template bool Instructions::CPRegister<Register::C>(CPU &cpu);

template bool Instructions::CPRegister<Register::D>(CPU &cpu);

template bool Instructions::CPRegister<Register::E>(CPU &cpu);

template bool Instructions::CPRegister<Register::H>(CPU &cpu);

template bool Instructions::CPRegister<Register::L>(CPU &cpu);

template bool Instructions::ORRegister<Register::A>(CPU &cpu);

template bool Instructions::ORRegister<Register::B>(CPU &cpu);

template bool Instructions::ORRegister<Register::C>(CPU &cpu);

template bool Instructions::ORRegister<Register::D>(CPU &cpu);

template bool Instructions::ORRegister<Register::E>(CPU &cpu);

template bool Instructions::ORRegister<Register::H>(CPU &cpu);

template bool Instructions::ORRegister<Register::L>(CPU &cpu);

template bool Instructions::XORRegister<Register::A>(CPU &cpu);

template bool Instructions::XORRegister<Register::B>(CPU &cpu);

template bool Instructions::XORRegister<Register::C>(CPU &cpu);

template bool Instructions::XORRegister<Register::D>(CPU &cpu);

template bool Instructions::XORRegister<Register::E>(CPU &cpu);

template bool Instructions::XORRegister<Register::H>(CPU &cpu);

template bool Instructions::XORRegister<Register::L>(CPU &cpu);

template bool Instructions::AND<Register::A>(CPU &cpu);

template bool Instructions::AND<Register::B>(CPU &cpu);

template bool Instructions::AND<Register::C>(CPU &cpu);

template bool Instructions::AND<Register::D>(CPU &cpu);

template bool Instructions::AND<Register::E>(CPU &cpu);

template bool Instructions::AND<Register::H>(CPU &cpu);

template bool Instructions::AND<Register::L>(CPU &cpu);

template bool Instructions::SUB<Register::A>(CPU &cpu);

template bool Instructions::SUB<Register::B>(CPU &cpu);

template bool Instructions::SUB<Register::C>(CPU &cpu);

template bool Instructions::SUB<Register::D>(CPU &cpu);

template bool Instructions::SUB<Register::E>(CPU &cpu);

template bool Instructions::SUB<Register::H>(CPU &cpu);

template bool Instructions::SUB<Register::L>(CPU &cpu);

template bool Instructions::RRC<Register::A>(CPU &cpu);

template bool Instructions::RRC<Register::B>(CPU &cpu);

template bool Instructions::RRC<Register::C>(CPU &cpu);

template bool Instructions::RRC<Register::D>(CPU &cpu);

template bool Instructions::RRC<Register::E>(CPU &cpu);

template bool Instructions::RRC<Register::H>(CPU &cpu);

template bool Instructions::RRC<Register::L>(CPU &cpu);

template bool Instructions::RLC<Register::A>(CPU &cpu);

template bool Instructions::RLC<Register::B>(CPU &cpu);

template bool Instructions::RLC<Register::C>(CPU &cpu);

template bool Instructions::RLC<Register::D>(CPU &cpu);

template bool Instructions::RLC<Register::E>(CPU &cpu);

template bool Instructions::RLC<Register::H>(CPU &cpu);

template bool Instructions::RLC<Register::L>(CPU &cpu);

template bool Instructions::RR<Register::A>(CPU &cpu);

template bool Instructions::RR<Register::B>(CPU &cpu);

template bool Instructions::RR<Register::C>(CPU &cpu);

template bool Instructions::RR<Register::D>(CPU &cpu);

template bool Instructions::RR<Register::E>(CPU &cpu);

template bool Instructions::RR<Register::H>(CPU &cpu);

template bool Instructions::RR<Register::L>(CPU &cpu);

template bool Instructions::RL<Register::A>(CPU &cpu);

template bool Instructions::RL<Register::B>(CPU &cpu);

template bool Instructions::RL<Register::C>(CPU &cpu);

template bool Instructions::RL<Register::D>(CPU &cpu);

template bool Instructions::RL<Register::E>(CPU &cpu);

template bool Instructions::RL<Register::H>(CPU &cpu);

template bool Instructions::RL<Register::L>(CPU &cpu);

template bool Instructions::SLA<Register::A>(CPU &cpu);

template bool Instructions::SLA<Register::B>(CPU &cpu);

template bool Instructions::SLA<Register::C>(CPU &cpu);

template bool Instructions::SLA<Register::D>(CPU &cpu);

template bool Instructions::SLA<Register::E>(CPU &cpu);

template bool Instructions::SLA<Register::H>(CPU &cpu);

template bool Instructions::SLA<Register::L>(CPU &cpu);

template bool Instructions::SRA<Register::A>(CPU &cpu);

template bool Instructions::SRA<Register::B>(CPU &cpu);

template bool Instructions::SRA<Register::C>(CPU &cpu);

template bool Instructions::SRA<Register::D>(CPU &cpu);

template bool Instructions::SRA<Register::E>(CPU &cpu);

template bool Instructions::SRA<Register::H>(CPU &cpu);

template bool Instructions::SRA<Register::L>(CPU &cpu);

template bool Instructions::SWAP<Register::A>(CPU &cpu);

template bool Instructions::SWAP<Register::B>(CPU &cpu);

template bool Instructions::SWAP<Register::C>(CPU &cpu);

template bool Instructions::SWAP<Register::D>(CPU &cpu);

template bool Instructions::SWAP<Register::E>(CPU &cpu);

template bool Instructions::SWAP<Register::H>(CPU &cpu);

template bool Instructions::SWAP<Register::L>(CPU &cpu);

template bool Instructions::SRL<Register::A>(CPU &cpu);

template bool Instructions::SRL<Register::B>(CPU &cpu);

template bool Instructions::SRL<Register::C>(CPU &cpu);

template bool Instructions::SRL<Register::D>(CPU &cpu);

template bool Instructions::SRL<Register::E>(CPU &cpu);

template bool Instructions::SRL<Register::H>(CPU &cpu);

template bool Instructions::SRL<Register::L>(CPU &cpu);

template bool Instructions::BIT<Register::A, 0>(CPU &cpu);

template bool Instructions::BIT<Register::B, 0>(CPU &cpu);

template bool Instructions::BIT<Register::C, 0>(CPU &cpu);

template bool Instructions::BIT<Register::D, 0>(CPU &cpu);

template bool Instructions::BIT<Register::E, 0>(CPU &cpu);

template bool Instructions::BIT<Register::H, 0>(CPU &cpu);

template bool Instructions::BIT<Register::L, 0>(CPU &cpu);

template bool Instructions::BIT<Register::A, 1>(CPU &cpu);

template bool Instructions::BIT<Register::B, 1>(CPU &cpu);

template bool Instructions::BIT<Register::C, 1>(CPU &cpu);

template bool Instructions::BIT<Register::D, 1>(CPU &cpu);

template bool Instructions::BIT<Register::E, 1>(CPU &cpu);

template bool Instructions::BIT<Register::H, 1>(CPU &cpu);

template bool Instructions::BIT<Register::L, 1>(CPU &cpu);

template bool Instructions::BIT<Register::A, 2>(CPU &cpu);

template bool Instructions::BIT<Register::B, 2>(CPU &cpu);

template bool Instructions::BIT<Register::C, 2>(CPU &cpu);

template bool Instructions::BIT<Register::D, 2>(CPU &cpu);

template bool Instructions::BIT<Register::E, 2>(CPU &cpu);

template bool Instructions::BIT<Register::H, 2>(CPU &cpu);

template bool Instructions::BIT<Register::L, 2>(CPU &cpu);

template bool Instructions::BIT<Register::A, 3>(CPU &cpu);

template bool Instructions::BIT<Register::B, 3>(CPU &cpu);

template bool Instructions::BIT<Register::C, 3>(CPU &cpu);

template bool Instructions::BIT<Register::D, 3>(CPU &cpu);

template bool Instructions::BIT<Register::E, 3>(CPU &cpu);

template bool Instructions::BIT<Register::H, 3>(CPU &cpu);

template bool Instructions::BIT<Register::L, 3>(CPU &cpu);

template bool Instructions::BIT<Register::A, 4>(CPU &cpu);

template bool Instructions::BIT<Register::B, 4>(CPU &cpu);

template bool Instructions::BIT<Register::C, 4>(CPU &cpu);

template bool Instructions::BIT<Register::D, 4>(CPU &cpu);

template bool Instructions::BIT<Register::E, 4>(CPU &cpu);

template bool Instructions::BIT<Register::H, 4>(CPU &cpu);

template bool Instructions::BIT<Register::L, 4>(CPU &cpu);

template bool Instructions::BIT<Register::A, 5>(CPU &cpu);

template bool Instructions::BIT<Register::B, 5>(CPU &cpu);

template bool Instructions::BIT<Register::C, 5>(CPU &cpu);

template bool Instructions::BIT<Register::D, 5>(CPU &cpu);

template bool Instructions::BIT<Register::E, 5>(CPU &cpu);

template bool Instructions::BIT<Register::H, 5>(CPU &cpu);

template bool Instructions::BIT<Register::L, 5>(CPU &cpu);

template bool Instructions::BIT<Register::A, 6>(CPU &cpu);

template bool Instructions::BIT<Register::B, 6>(CPU &cpu);

template bool Instructions::BIT<Register::C, 6>(CPU &cpu);

template bool Instructions::BIT<Register::D, 6>(CPU &cpu);

template bool Instructions::BIT<Register::E, 6>(CPU &cpu);

template bool Instructions::BIT<Register::H, 6>(CPU &cpu);

template bool Instructions::BIT<Register::L, 6>(CPU &cpu);

template bool Instructions::BIT<Register::A, 7>(CPU &cpu);

template bool Instructions::BIT<Register::B, 7>(CPU &cpu);

template bool Instructions::BIT<Register::C, 7>(CPU &cpu);

template bool Instructions::BIT<Register::D, 7>(CPU &cpu);

template bool Instructions::BIT<Register::E, 7>(CPU &cpu);

template bool Instructions::BIT<Register::H, 7>(CPU &cpu);

template bool Instructions::BIT<Register::L, 7>(CPU &cpu);

template bool Instructions::RES<Register::A, 0>(CPU &cpu);

template bool Instructions::RES<Register::B, 0>(CPU &cpu);

template bool Instructions::RES<Register::C, 0>(CPU &cpu);

template bool Instructions::RES<Register::D, 0>(CPU &cpu);

template bool Instructions::RES<Register::E, 0>(CPU &cpu);

template bool Instructions::RES<Register::H, 0>(CPU &cpu);

template bool Instructions::RES<Register::L, 0>(CPU &cpu);

template bool Instructions::RES<Register::A, 1>(CPU &cpu);

template bool Instructions::RES<Register::B, 1>(CPU &cpu);

template bool Instructions::RES<Register::C, 1>(CPU &cpu);

template bool Instructions::RES<Register::D, 1>(CPU &cpu);

template bool Instructions::RES<Register::E, 1>(CPU &cpu);

template bool Instructions::RES<Register::H, 1>(CPU &cpu);

template bool Instructions::RES<Register::L, 1>(CPU &cpu);

template bool Instructions::RES<Register::A, 2>(CPU &cpu);

template bool Instructions::RES<Register::B, 2>(CPU &cpu);

template bool Instructions::RES<Register::C, 2>(CPU &cpu);

template bool Instructions::RES<Register::D, 2>(CPU &cpu);

template bool Instructions::RES<Register::E, 2>(CPU &cpu);

template bool Instructions::RES<Register::H, 2>(CPU &cpu);

template bool Instructions::RES<Register::L, 2>(CPU &cpu);

template bool Instructions::RES<Register::A, 3>(CPU &cpu);

template bool Instructions::RES<Register::B, 3>(CPU &cpu);

template bool Instructions::RES<Register::C, 3>(CPU &cpu);

template bool Instructions::RES<Register::D, 3>(CPU &cpu);

template bool Instructions::RES<Register::E, 3>(CPU &cpu);

template bool Instructions::RES<Register::H, 3>(CPU &cpu);

template bool Instructions::RES<Register::L, 3>(CPU &cpu);

template bool Instructions::RES<Register::A, 4>(CPU &cpu);

template bool Instructions::RES<Register::B, 4>(CPU &cpu);

template bool Instructions::RES<Register::C, 4>(CPU &cpu);

template bool Instructions::RES<Register::D, 4>(CPU &cpu);

template bool Instructions::RES<Register::E, 4>(CPU &cpu);

template bool Instructions::RES<Register::H, 4>(CPU &cpu);

template bool Instructions::RES<Register::L, 4>(CPU &cpu);

template bool Instructions::RES<Register::A, 5>(CPU &cpu);

template bool Instructions::RES<Register::B, 5>(CPU &cpu);

template bool Instructions::RES<Register::C, 5>(CPU &cpu);

template bool Instructions::RES<Register::D, 5>(CPU &cpu);

template bool Instructions::RES<Register::E, 5>(CPU &cpu);

template bool Instructions::RES<Register::H, 5>(CPU &cpu);

template bool Instructions::RES<Register::L, 5>(CPU &cpu);

template bool Instructions::RES<Register::A, 6>(CPU &cpu);

template bool Instructions::RES<Register::B, 6>(CPU &cpu);

template bool Instructions::RES<Register::C, 6>(CPU &cpu);

template bool Instructions::RES<Register::D, 6>(CPU &cpu);

template bool Instructions::RES<Register::E, 6>(CPU &cpu);

template bool Instructions::RES<Register::H, 6>(CPU &cpu);

template bool Instructions::RES<Register::L, 6>(CPU &cpu);

template bool Instructions::RES<Register::A, 7>(CPU &cpu);

template bool Instructions::RES<Register::B, 7>(CPU &cpu);

template bool Instructions::RES<Register::C, 7>(CPU &cpu);

template bool Instructions::RES<Register::D, 7>(CPU &cpu);

template bool Instructions::RES<Register::E, 7>(CPU &cpu);

template bool Instructions::RES<Register::H, 7>(CPU &cpu);

template bool Instructions::RES<Register::L, 7>(CPU &cpu);

template bool Instructions::SET<Register::A, 0>(CPU &cpu);

template bool Instructions::SET<Register::B, 0>(CPU &cpu);

template bool Instructions::SET<Register::C, 0>(CPU &cpu);

template bool Instructions::SET<Register::D, 0>(CPU &cpu);

template bool Instructions::SET<Register::E, 0>(CPU &cpu);

template bool Instructions::SET<Register::H, 0>(CPU &cpu);

template bool Instructions::SET<Register::L, 0>(CPU &cpu);

template bool Instructions::SET<Register::A, 1>(CPU &cpu);

template bool Instructions::SET<Register::B, 1>(CPU &cpu);

template bool Instructions::SET<Register::C, 1>(CPU &cpu);

template bool Instructions::SET<Register::D, 1>(CPU &cpu);

template bool Instructions::SET<Register::E, 1>(CPU &cpu);

template bool Instructions::SET<Register::H, 1>(CPU &cpu);

template bool Instructions::SET<Register::L, 1>(CPU &cpu);

template bool Instructions::SET<Register::A, 2>(CPU &cpu);

template bool Instructions::SET<Register::B, 2>(CPU &cpu);

template bool Instructions::SET<Register::C, 2>(CPU &cpu);

template bool Instructions::SET<Register::D, 2>(CPU &cpu);

template bool Instructions::SET<Register::E, 2>(CPU &cpu);

template bool Instructions::SET<Register::H, 2>(CPU &cpu);

template bool Instructions::SET<Register::L, 2>(CPU &cpu);

template bool Instructions::SET<Register::A, 3>(CPU &cpu);

template bool Instructions::SET<Register::B, 3>(CPU &cpu);

template bool Instructions::SET<Register::C, 3>(CPU &cpu);

template bool Instructions::SET<Register::D, 3>(CPU &cpu);

template bool Instructions::SET<Register::E, 3>(CPU &cpu);

template bool Instructions::SET<Register::H, 3>(CPU &cpu);

template bool Instructions::SET<Register::L, 3>(CPU &cpu);

template bool Instructions::SET<Register::A, 4>(CPU &cpu);

template bool Instructions::SET<Register::B, 4>(CPU &cpu);

template bool Instructions::SET<Register::C, 4>(CPU &cpu);

template bool Instructions::SET<Register::D, 4>(CPU &cpu);

template bool Instructions::SET<Register::E, 4>(CPU &cpu);

template bool Instructions::SET<Register::H, 4>(CPU &cpu);

template bool Instructions::SET<Register::L, 4>(CPU &cpu);

template bool Instructions::SET<Register::A, 5>(CPU &cpu);

template bool Instructions::SET<Register::B, 5>(CPU &cpu);

template bool Instructions::SET<Register::C, 5>(CPU &cpu);

template bool Instructions::SET<Register::D, 5>(CPU &cpu);

template bool Instructions::SET<Register::E, 5>(CPU &cpu);

template bool Instructions::SET<Register::H, 5>(CPU &cpu);

template bool Instructions::SET<Register::L, 5>(CPU &cpu);

template bool Instructions::SET<Register::A, 6>(CPU &cpu);

template bool Instructions::SET<Register::B, 6>(CPU &cpu);

template bool Instructions::SET<Register::C, 6>(CPU &cpu);

template bool Instructions::SET<Register::D, 6>(CPU &cpu);

template bool Instructions::SET<Register::E, 6>(CPU &cpu);

template bool Instructions::SET<Register::H, 6>(CPU &cpu);

template bool Instructions::SET<Register::L, 6>(CPU &cpu);

template bool Instructions::SET<Register::A, 7>(CPU &cpu);

template bool Instructions::SET<Register::B, 7>(CPU &cpu);

template bool Instructions::SET<Register::C, 7>(CPU &cpu);

template bool Instructions::SET<Register::D, 7>(CPU &cpu);

template bool Instructions::SET<Register::E, 7>(CPU &cpu);

template bool Instructions::SET<Register::H, 7>(CPU &cpu);

template bool Instructions::SET<Register::L, 7>(CPU &cpu);

template bool Instructions::BITAddr<0>(CPU &cpu);

template bool Instructions::BITAddr<1>(CPU &cpu);

template bool Instructions::BITAddr<2>(CPU &cpu);

template bool Instructions::BITAddr<3>(CPU &cpu);

template bool Instructions::BITAddr<4>(CPU &cpu);

template bool Instructions::BITAddr<5>(CPU &cpu);

template bool Instructions::BITAddr<6>(CPU &cpu);

template bool Instructions::BITAddr<7>(CPU &cpu);

template bool Instructions::RESAddr<0>(CPU &cpu);

template bool Instructions::RESAddr<1>(CPU &cpu);

template bool Instructions::RESAddr<2>(CPU &cpu);

template bool Instructions::RESAddr<3>(CPU &cpu);

template bool Instructions::RESAddr<4>(CPU &cpu);

template bool Instructions::RESAddr<5>(CPU &cpu);

template bool Instructions::RESAddr<6>(CPU &cpu);

template bool Instructions::RESAddr<7>(CPU &cpu);

template bool Instructions::SETAddr<0>(CPU &cpu);

template bool Instructions::SETAddr<1>(CPU &cpu);

template bool Instructions::SETAddr<2>(CPU &cpu);

template bool Instructions::SETAddr<3>(CPU &cpu);

template bool Instructions::SETAddr<4>(CPU &cpu);

template bool Instructions::SETAddr<5>(CPU &cpu);

template bool Instructions::SETAddr<6>(CPU &cpu);

template bool Instructions::SETAddr<7>(CPU &cpu);

template bool Instructions::SBCRegister<Register::A>(CPU &cpu);

template bool Instructions::SBCRegister<Register::B>(CPU &cpu);

template bool Instructions::SBCRegister<Register::C>(CPU &cpu);

template bool Instructions::SBCRegister<Register::D>(CPU &cpu);

template bool Instructions::SBCRegister<Register::E>(CPU &cpu);

template bool Instructions::SBCRegister<Register::H>(CPU &cpu);

template bool Instructions::SBCRegister<Register::L>(CPU &cpu);

template bool Instructions::ADCRegister<Register::A>(CPU &cpu);

template bool Instructions::ADCRegister<Register::B>(CPU &cpu);

template bool Instructions::ADCRegister<Register::C>(CPU &cpu);

template bool Instructions::ADCRegister<Register::D>(CPU &cpu);

template bool Instructions::ADCRegister<Register::E>(CPU &cpu);

template bool Instructions::ADCRegister<Register::H>(CPU &cpu);

template bool Instructions::ADCRegister<Register::L>(CPU &cpu);

template bool Instructions::ADD16<Arithmetic16Target::BC>(CPU &cpu);

template bool Instructions::ADD16<Arithmetic16Target::DE>(CPU &cpu);

template bool Instructions::ADD16<Arithmetic16Target::HL>(CPU &cpu);

template bool Instructions::ADD16<Arithmetic16Target::SP>(CPU &cpu);

template bool Instructions::ADDRegister<Register::A>(CPU &cpu);

template bool Instructions::ADDRegister<Register::B>(CPU &cpu);

template bool Instructions::ADDRegister<Register::C>(CPU &cpu);

template bool Instructions::ADDRegister<Register::D>(CPU &cpu);

template bool Instructions::ADDRegister<Register::E>(CPU &cpu);

template bool Instructions::ADDRegister<Register::H>(CPU &cpu);

template bool Instructions::ADDRegister<Register::L>(CPU &cpu);
