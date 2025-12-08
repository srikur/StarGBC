#include "Instructions.h"
#include <cstring>

void Instructions::HandleOAMCorruption(const CPU &cpu, const uint16_t location, const CorruptionType type) const {
    if (!cpu.IsDMG() || (location < 0xFE00 || location > 0xFEFF) || bus_.gpu_.stat.mode != GPUMode::MODE_2) return;
    if (bus_.gpu_.scanlineCounter >= 76) return;
    const int currentRowIndex = bus_.gpu_.GetOAMScanRow();

    auto ReadWord = [&](const int index) -> uint16_t {
        return static_cast<uint16_t>(bus_.gpu_.oam[index]) << 8 | bus_.gpu_.oam[index + 1];
    };
    auto WriteWord = [&](const int index, const uint16_t value) {
        bus_.gpu_.oam[index] = static_cast<uint8_t>(value >> 8);
        bus_.gpu_.oam[index + 1] = static_cast<uint8_t>(value & 0xFF);
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
            std::memcpy(temp_row, &bus_.gpu_.oam[row_n_minus_1_addr], 8);
            std::memcpy(&bus_.gpu_.oam[row_n_addr], temp_row, 8);
            std::memcpy(&bus_.gpu_.oam[row_n_minus_2_addr], temp_row, 8);
        }

        if (currentRowIndex > 0) {
            const int currentRowAddr = currentRowIndex * 8;
            const int prevRowAddr = (currentRowIndex - 1) * 8;

            const uint16_t a_read = ReadWord(currentRowAddr);
            const uint16_t b_read = ReadWord(prevRowAddr);
            const uint16_t c_read = ReadWord(prevRowAddr + 4);

            const uint16_t corruptedWord = b_read | (a_read & c_read);
            WriteWord(currentRowAddr, corruptedWord);

            std::memcpy(&bus_.gpu_.oam[currentRowAddr + 2], &bus_.gpu_.oam[prevRowAddr + 2], 6);
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
        std::memcpy(&bus_.gpu_.oam[currentRowAddr + 2], &bus_.gpu_.oam[prevRowAddr + 2], 6);
    }
}

template<Register source>
bool Instructions::RLC(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = regs_.*sourceReg;
    const uint8_t old = value & 0x80 ? 1 : 0;
    regs_.SetCarry(old != 0);
    const uint8_t newValue = (value << 1) | old;
    regs_.SetZero(newValue == 0);
    regs_.*sourceReg = newValue;
    regs_.SetSubtract(false);
    regs_.SetHalf(false);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::RLCAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t old = byte & 0x80 ? 1 : 0;
        regs_.SetCarry(old != 0);
        byte = byte << 1 | old;
        bus_.WriteByte(regs_.GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        regs_.SetZero(byte == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::DAA(CPU &cpu) const {
    uint8_t adjust = 0;
    bool carry = regs_.FlagCarry();

    if (!regs_.FlagSubtract()) {
        if (regs_.FlagHalf() || (regs_.a & 0x0F) > 0x09)
            adjust |= 0x06;

        if (regs_.FlagCarry() || regs_.a > 0x99) {
            adjust |= 0x60;
            carry = true;
        }

        regs_.a += adjust;
    } else {
        if (regs_.FlagHalf()) { adjust |= 0x06; }
        if (regs_.FlagCarry()) { adjust |= 0x60; }
        regs_.a -= adjust;
    }

    regs_.SetCarry(carry);
    regs_.SetZero(regs_.a == 0);
    regs_.SetHalf(false);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::RETI(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = bus_.ReadByte(cpu.sp);
        cpu.sp += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(bus_.ReadByte(cpu.sp)) << 8;
        cpu.sp += 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.pc = word;
        bus_.interruptMasterEnable = true;
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::DI(CPU &cpu) const {
    bus_.interruptDelay = false;
    bus_.interruptMasterEnable = false;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::EI(CPU &cpu) const {
    if (!bus_.interruptMasterEnable) {
        cpu.icount = 0;
        bus_.interruptDelay = true;
        bus_.interruptMasterEnable = true;
    }
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::HALT(CPU &cpu) const {
    if (const bool bug = (bus_.interruptEnable & bus_.interruptFlag & 0x1F) != 0; !bus_.interruptMasterEnable && bug) {
        cpu.haltBug = true;
        cpu.halted = false;
    } else {
        cpu.haltBug = false;
        cpu.halted = true;
    }
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<RSTTarget target>
bool Instructions::RST(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        cpu.sp -= 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        bus_.WriteByte(cpu.sp, (cpu.pc & 0xFF00) >> 8);
        cpu.sp -= 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        bus_.WriteByte(cpu.sp, cpu.pc & 0xFF);
        constexpr auto location = GetRSTAddress<target>();
        if constexpr (target == RSTTarget::H38) {
            std::fprintf(stderr, "RST 38\n");
        }
        cpu.pc = location;
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::CALLUnconditional(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = bus_.ReadByte(cpu.pc);
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(bus_.ReadByte(cpu.pc)) << 8;
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.sp -= 1;
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        bus_.WriteByte(cpu.sp, (cpu.pc & 0xFF00) >> 8);
        cpu.sp -= 1;
        return false;
    }
    if (cpu.mCycleCounter == 6) {
        bus_.WriteByte(cpu.sp, cpu.pc & 0xFF);
        cpu.pc = word;
        return false;
    }
    if (cpu.mCycleCounter == 7) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<JumpTest test>
bool Instructions::CALL(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = bus_.ReadByte(cpu.pc);
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        if constexpr (test == JumpTest::NotZero) jumpCondition = !regs_.FlagZero();
        else if constexpr (test == JumpTest::Zero) jumpCondition = regs_.FlagZero();
        else if constexpr (test == JumpTest::Carry) jumpCondition = regs_.FlagCarry();
        else if constexpr (test == JumpTest::NotCarry) jumpCondition = !regs_.FlagCarry();
        word |= static_cast<uint16_t>(bus_.ReadByte(cpu.pc)) << 8;
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        if (jumpCondition) {
            cpu.sp -= 1;
            return false;
        }

        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    if (cpu.mCycleCounter == 5) {
        bus_.WriteByte(cpu.sp, (cpu.pc & 0xFF00) >> 8);
        cpu.sp -= 1;
        return false;
    }
    if (cpu.mCycleCounter == 6) {
        bus_.WriteByte(cpu.sp, cpu.pc & 0xFF);
        cpu.pc = word;
        return false;
    }
    if (cpu.mCycleCounter == 7) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::RLCA(CPU &cpu) const {
    const uint8_t old = (regs_.a & 0x80) != 0 ? 1 : 0;
    regs_.SetCarry(old != 0);
    regs_.a = regs_.a << 1 | old;
    regs_.SetZero(false);
    regs_.SetHalf(false);
    regs_.SetSubtract(false);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::RLA(CPU &cpu) const {
    const bool flag_c = (regs_.a & 0x80) >> 7 == 0x01;
    const uint8_t r = (regs_.a << 1) + static_cast<uint8_t>(regs_.FlagCarry());
    regs_.SetCarry(flag_c);
    regs_.SetZero(false);
    regs_.SetHalf(false);
    regs_.SetSubtract(false);
    regs_.a = r;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<Register source>
bool Instructions::RL(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = regs_.*sourceReg;
    const bool flag_c = (value & 0x80) >> 7 == 0x01;
    const uint8_t newValue = value << 1 | static_cast<uint8_t>(regs_.FlagCarry());
    regs_.SetCarry(flag_c);
    regs_.SetZero(newValue == 0);
    regs_.SetHalf(false);
    regs_.SetSubtract(false);
    regs_.*sourceReg = newValue;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::RLAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t oldCarry = regs_.FlagCarry() ? 1 : 0;
        regs_.SetCarry((byte & 0x80) != 0);
        byte = (byte << 1) | oldCarry;
        bus_.WriteByte(regs_.GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        regs_.SetZero(byte == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::CCF(CPU &cpu) const {
    regs_.SetSubtract(false);
    regs_.SetCarry(!regs_.FlagCarry());
    regs_.SetHalf(false);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::CPL(CPU &cpu) const {
    regs_.SetHalf(true);
    regs_.SetSubtract(true);
    regs_.a = ~regs_.a;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::SCF(CPU &cpu) const {
    regs_.SetSubtract(false);
    regs_.SetHalf(false);
    regs_.SetCarry(true);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::RRCA(CPU &cpu) const {
    regs_.SetCarry((regs_.a & 0x01) != 0);
    regs_.a = regs_.a >> 1 | (regs_.a & 0x01) << 7;
    regs_.SetZero(false);
    regs_.SetSubtract(false);
    regs_.SetHalf(false);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<Register source>
bool Instructions::RRC(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = regs_.*sourceReg;
    const bool carry = (value & 0x01) == 0x01;
    regs_.SetCarry(carry);
    const uint8_t newValue = regs_.FlagCarry() ? 0x80 | value >> 1 : value >> 1;
    regs_.SetZero(newValue == 0);
    regs_.*sourceReg = newValue;
    regs_.SetSubtract(false);
    regs_.SetHalf(false);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::RRCAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.SetCarry(byte & 0x01);
        byte = regs_.FlagCarry() ? 0x80 | byte >> 1 : byte >> 1;
        bus_.WriteByte(regs_.GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        regs_.SetZero(byte == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::RRAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word = (byte & 0x01) == 0x01; // hack for storing carry
        byte = regs_.FlagCarry() ? 0x80 | (byte >> 1) : byte >> 1;
        bus_.WriteByte(regs_.GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        regs_.SetCarry(word);
        regs_.SetZero(byte == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::RR(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = regs_.*sourceReg;
    const bool carry = (value & 0x01) == 0x01;
    uint8_t newValue = regs_.FlagCarry() ? 0x80 | (value >> 1) : value >> 1;
    regs_.*sourceReg = newValue;
    regs_.SetCarry(carry);
    regs_.SetZero(newValue == 0);
    regs_.SetSubtract(false);
    regs_.SetHalf(false);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::RRA(CPU &cpu) const {
    const bool carry = (regs_.a & 0x01) == 0x01;
    const uint8_t newValue = regs_.FlagCarry() ? 0x80 | regs_.a >> 1 : regs_.a >> 1;
    regs_.SetZero(false);
    regs_.a = newValue;
    regs_.SetCarry(carry);
    regs_.SetSubtract(false);
    regs_.SetHalf(false);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::RETUnconditional(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = bus_.ReadByte(cpu.sp);
        cpu.sp += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(bus_.ReadByte(cpu.sp)) << 8;
        cpu.sp += 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.pc = word;
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<JumpTest test>
bool Instructions::RETConditional(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        if constexpr (test == JumpTest::NotZero) jumpCondition = !regs_.FlagZero();
        else if constexpr (test == JumpTest::Zero) jumpCondition = regs_.FlagZero();
        else if constexpr (test == JumpTest::Carry) jumpCondition = regs_.FlagCarry();
        else if constexpr (test == JumpTest::NotCarry) jumpCondition = !regs_.FlagCarry();
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        if (jumpCondition) {
            word = bus_.ReadByte(cpu.sp);
            cpu.sp += 1;
            return false;
        }

        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    if (cpu.mCycleCounter == 4) {
        word |= static_cast<uint16_t>(bus_.ReadByte(cpu.sp)) << 8;
        cpu.sp += 1;
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.pc = word;
        return false;
    }
    if (cpu.mCycleCounter == 6) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::JRUnconditional(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        signedByte = std::bit_cast<int8_t>(bus_.ReadByte(cpu.pc));
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
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<JumpTest test>
bool Instructions::JR(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        signedByte = std::bit_cast<int8_t>(bus_.ReadByte(cpu.pc));
        cpu.pc += 1;
        if constexpr (test == JumpTest::NotZero) jumpCondition = !regs_.FlagZero();
        else if constexpr (test == JumpTest::Zero) jumpCondition = regs_.FlagZero();
        else if constexpr (test == JumpTest::Carry) jumpCondition = regs_.FlagCarry();
        else if constexpr (test == JumpTest::NotCarry) jumpCondition = !regs_.FlagCarry();
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
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::JPUnconditional(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = bus_.ReadByte(cpu.pc);
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(bus_.ReadByte(cpu.pc)) << 8;
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.pc = word;
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<JumpTest test>
bool Instructions::JP(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = bus_.ReadByte(cpu.pc);
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(bus_.ReadByte(cpu.pc)) << 8;
        cpu.pc += 1;
        if constexpr (test == JumpTest::NotZero) jumpCondition = !regs_.FlagZero();
        else if constexpr (test == JumpTest::Zero) jumpCondition = regs_.FlagZero();
        else if constexpr (test == JumpTest::Carry) jumpCondition = regs_.FlagCarry();
        else if constexpr (test == JumpTest::NotCarry) jumpCondition = !regs_.FlagCarry();
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        if (jumpCondition) {
            cpu.pc = word;
            return false;
        }
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::JPHL(CPU &cpu) const {
    cpu.pc = regs_.GetHL();
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::NOP(CPU &cpu) const {
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::PREFIX(CPU &cpu) const {
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    cpu.currentInstruction = 0xCB00 | cpu.nextInstruction;
    cpu.prefixed = true;
    return true;
}

bool Instructions::STOP(CPU &cpu) const {
    const uint8_t key1 = bus_.ReadByte(0xFF4D);
    const bool speedSwitchRequested = bus_.prepareSpeedShift && (key1 & 0x01);
    bus_.WriteByte(0xFF04, 0x00);

    if (speedSwitchRequested) {
        bus_.ChangeSpeed();
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }

    cpu.stopped = true;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<Register source>
bool Instructions::DECRegister(CPU &cpu) const {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t sourceValue = regs_.*sourceReg;
    regs_.SetHalf((sourceValue & 0xF) == 0x00);
    const uint8_t newValue = sourceValue - 1;
    regs_.SetZero(newValue == 0);
    regs_.SetSubtract(true);
    regs_.*sourceReg = newValue;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::DECIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.SetHalf((byte & 0xF) == 0x00);
        const uint8_t newValue = byte - 1;
        regs_.SetZero(newValue == 0);
        regs_.SetSubtract(true);
        bus_.WriteByte(regs_.GetHL(), newValue);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Arithmetic16Target target>
bool Instructions::DEC16(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        if constexpr (target == Arithmetic16Target::BC) {
            word = regs_.GetBC();
            HandleOAMCorruption(cpu, word, CorruptionType::Write);
            regs_.SetBC(word - 1);
        } else if constexpr (target == Arithmetic16Target::DE) {
            word = regs_.GetDE();
            HandleOAMCorruption(cpu, word, CorruptionType::Write);
            regs_.SetDE(word - 1);
        } else if constexpr (target == Arithmetic16Target::HL) {
            word = regs_.GetHL();
            regs_.SetHL(word - 1);
            HandleOAMCorruption(cpu, word, CorruptionType::Write);
        } else if constexpr (target == Arithmetic16Target::SP) {
            HandleOAMCorruption(cpu, cpu.sp, CorruptionType::Write);
            cpu.sp -= 1;
        }
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::INCRegister(CPU &cpu) const {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t sourceValue = regs_.*sourceReg;
    regs_.SetHalf((sourceValue & 0xF) == 0xF);
    const uint8_t newValue = sourceValue + 1;
    regs_.SetZero(newValue == 0);
    regs_.SetSubtract(false);
    regs_.*sourceReg = newValue;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::INCIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.SetHalf((byte & 0xF) == 0xF);
        const uint8_t newValue = byte + 1;
        regs_.SetZero(newValue == 0);
        regs_.SetSubtract(false);
        bus_.WriteByte(regs_.GetHL(), newValue);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Arithmetic16Target target>
bool Instructions::INC16(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        if constexpr (target == Arithmetic16Target::BC) {
            word = regs_.GetBC();
            HandleOAMCorruption(cpu, word, CorruptionType::Write);
            regs_.SetBC(word + 1);
        } else if constexpr (target == Arithmetic16Target::DE) {
            word = regs_.GetDE();
            HandleOAMCorruption(cpu, word, CorruptionType::Write);
            regs_.SetDE(word + 1);
        } else if constexpr (target == Arithmetic16Target::HL) {
            word = regs_.GetHL();
            HandleOAMCorruption(cpu, word, CorruptionType::Write);
            regs_.SetHL(word + 1);
        } else if constexpr (target == Arithmetic16Target::SP) {
            HandleOAMCorruption(cpu, cpu.sp, CorruptionType::Write);
            cpu.sp += 1;
        }
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

// 0xE0
bool Instructions::LoadFromAccumulatorDirectA(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = static_cast<uint16_t>(bus_.ReadByte(cpu.pc));
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        bus_.WriteByte(static_cast<uint16_t>(0xFF00) | word, regs_.a);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

// 0xE2
bool Instructions::LoadFromAccumulatorIndirectC(CPU &cpu) const {
    if (cpu.mCycleCounter == 2) {
        const uint16_t c = 0xFF00 | static_cast<uint16_t>(regs_.c);
        bus_.WriteByte(c, regs_.a);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

// 0xF0
bool Instructions::LoadAccumulatorA(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word = 0xFF00 | static_cast<uint16_t>(byte);
        byte = bus_.ReadByte(word);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        regs_.a = byte;
        return true;
    }
    return false;
}

// 0xF2
bool Instructions::LoadAccumulatorIndirectC(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(0xFF00 | static_cast<uint16_t>(regs_.c));
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.a = byte;
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

/* M2 -- one cycle */
template<Register target, Register source>
bool Instructions::LDRegister(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    constexpr auto targetReg = GetRegisterPtr<target>();
    const uint8_t sourceValue = regs_.*sourceReg;
    regs_.*targetReg = sourceValue;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<Register source>
bool Instructions::LDRegisterImmediate(CPU &cpu) {
    constexpr auto targetReg = GetRegisterPtr<source>();
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.*targetReg = byte;
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::LDRegisterIndirect(CPU &cpu) {
    constexpr auto targetReg = GetRegisterPtr<source>();
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.*targetReg = byte;
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::LDAddrRegister(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    if (cpu.mCycleCounter == 2) {
        const uint8_t sourceValue = regs_.*sourceReg;
        bus_.WriteByte(regs_.GetHL(), sourceValue);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDAddrImmediate(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        bus_.WriteByte(regs_.GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDAccumulatorBC(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetBC());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.a = byte;
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDAccumulatorDE(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetDE());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.a = byte;
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDFromAccBC(CPU &cpu) const {
    if (cpu.mCycleCounter == 2) {
        bus_.WriteByte(regs_.GetBC(), regs_.a);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDFromAccDE(CPU &cpu) const {
    if (cpu.mCycleCounter == 2) {
        bus_.WriteByte(regs_.GetDE(), regs_.a);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDAccumulatorDirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = bus_.ReadByte(cpu.pc);
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(bus_.ReadByte(cpu.pc)) << 8;
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        regs_.a = bus_.ReadByte(word);
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDFromAccumulatorDirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = bus_.ReadByte(cpu.pc);
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(bus_.ReadByte(cpu.pc)) << 8;
        cpu.pc += 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        bus_.WriteByte(word, regs_.a);
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc);
        cpu.pc += 1;
        return true;
    }
    return false;
}

bool Instructions::LDAccumulatorIndirectDec(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        HandleOAMCorruption(cpu, regs_.GetHL(), CorruptionType::ReadWrite);
        regs_.SetHL(regs_.GetHL() - 1);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.a = byte;
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDFromAccumulatorIndirectDec(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = regs_.GetHL();
        HandleOAMCorruption(cpu, word, CorruptionType::Write);
        bus_.WriteByte(word, regs_.a);
        HandleOAMCorruption(cpu, word, CorruptionType::Write);
        regs_.SetHL(word - 1);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDAccumulatorIndirectInc(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        HandleOAMCorruption(cpu, regs_.GetHL(), CorruptionType::ReadWrite);
        regs_.SetHL(regs_.GetHL() + 1);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.a = byte;
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LDFromAccumulatorIndirectInc(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = regs_.GetHL();
        HandleOAMCorruption(cpu, word, CorruptionType::Write);
        bus_.WriteByte(word, regs_.a);
        HandleOAMCorruption(cpu, word, CorruptionType::Write);
        regs_.SetHL(word + 1);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LD16FromStack(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = bus_.ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(bus_.ReadByte(cpu.pc++)) << 8;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        bus_.WriteByte(word, cpu.sp & 0xFF);
        word += 1;
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        bus_.WriteByte(word, cpu.sp >> 8);
        return false;
    }
    if (cpu.mCycleCounter == 6) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<LoadWordTarget target>
bool Instructions::LD16Register(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = bus_.ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word |= static_cast<uint16_t>(bus_.ReadByte(cpu.pc++)) << 8;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        if constexpr (target == LoadWordTarget::HL) regs_.SetHL(word);
        else if constexpr (target == LoadWordTarget::SP) cpu.sp = word;
        else if constexpr (target == LoadWordTarget::BC) regs_.SetBC(word);
        else if constexpr (target == LoadWordTarget::DE) regs_.SetDE(word);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LD16Stack(CPU &cpu) const {
    if (cpu.mCycleCounter == 2) {
        cpu.sp = regs_.GetHL();
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::LD16StackAdjusted(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(
            bus_.ReadByte(cpu.pc++))));
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.SetCarry((cpu.sp & 0xFF) + (word & 0xFF) > 0xFF);
        regs_.SetHalf((cpu.sp & 0xF) + (word & 0xF) > 0xF);
        regs_.SetSubtract(false);
        regs_.SetZero(false);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        regs_.SetHL(cpu.sp + word);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
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
        if constexpr (target == StackTarget::BC) word = regs_.GetBC();
        if constexpr (target == StackTarget::DE) word = regs_.GetDE();
        if constexpr (target == StackTarget::HL) word = regs_.GetHL();
        if constexpr (target == StackTarget::AF) word = regs_.GetAF();
        bus_.WriteByte(cpu.sp, (word & 0xFF00) >> 8);
        cpu.sp -= 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        HandleOAMCorruption(cpu, cpu.sp, CorruptionType::Write);
        bus_.WriteByte(cpu.sp, word & 0xFF);
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<StackTarget target>
bool Instructions::POP(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        HandleOAMCorruption(cpu, cpu.sp, CorruptionType::ReadWrite);
        word = bus_.ReadByte(cpu.sp);
        cpu.sp += 1;
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        HandleOAMCorruption(cpu, cpu.sp, CorruptionType::Read);
        word |= static_cast<uint16_t>(bus_.ReadByte(cpu.sp)) << 8;
        cpu.sp += 1;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        if constexpr (target == StackTarget::BC) regs_.SetBC(word);
        if constexpr (target == StackTarget::DE) regs_.SetDE(word);
        if constexpr (target == StackTarget::HL) regs_.SetHL(word);
        if constexpr (target == StackTarget::AF) regs_.SetAF(word & 0xFFF0);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::CPRegister(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = regs_.*sourceReg;
    const uint8_t new_value = regs_.a - value;
    regs_.SetCarry(regs_.a < value);
    regs_.SetHalf((regs_.a & 0xF) < (value & 0xF));
    regs_.SetSubtract(true);
    regs_.SetZero(new_value == 0x0);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::CPIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t new_value = regs_.a - byte;
        regs_.SetCarry(regs_.a < byte);
        regs_.SetHalf((regs_.a & 0xF) < (byte & 0xF));
        regs_.SetSubtract(true);
        regs_.SetZero(new_value == 0x0);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::CPImmediate(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t new_value = regs_.a - byte;
        regs_.SetCarry(regs_.a < byte);
        regs_.SetHalf((regs_.a & 0xF) < (byte & 0xF));
        regs_.SetSubtract(true);
        regs_.SetZero(new_value == 0x0);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return true;
}

template<Register source>
bool Instructions::ORRegister(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = regs_.*sourceReg;
    regs_.a |= value;
    regs_.SetZero(regs_.a == 0);
    regs_.SetSubtract(false);
    regs_.SetHalf(false);
    regs_.SetCarry(false);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::ORIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.a |= byte;
        regs_.SetZero(regs_.a == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        regs_.SetCarry(false);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::ORImmediate(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.a |= byte;
        regs_.SetZero(regs_.a == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        regs_.SetCarry(false);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::XORRegister(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = regs_.*sourceReg;
    regs_.a ^= value;
    regs_.SetZero(regs_.a == 0);
    regs_.SetSubtract(false);
    regs_.SetHalf(false);
    regs_.SetCarry(false);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::XORIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.a ^= byte;
        regs_.SetZero(regs_.a == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        regs_.SetCarry(false);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::XORImmediate(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.a ^= byte;
        regs_.SetZero(regs_.a == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        regs_.SetCarry(false);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::AND(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = regs_.*sourceReg;
    regs_.a &= value;
    regs_.SetZero(regs_.a == 0);
    regs_.SetSubtract(false);
    regs_.SetHalf(true);
    regs_.SetCarry(false);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::ANDImmediate(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.a &= byte;
        regs_.SetZero(regs_.a == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(true);
        regs_.SetCarry(false);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::ANDIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.a &= byte;
        regs_.SetZero(regs_.a == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(true);
        regs_.SetCarry(false);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::SUB(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = regs_.*sourceReg;
    const uint8_t new_value = regs_.a - value;
    regs_.SetCarry(regs_.a < value);
    regs_.SetHalf((regs_.a & 0xF) < (value & 0xF));
    regs_.SetSubtract(true);
    regs_.SetZero(new_value == 0x0);
    regs_.a = new_value;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::SUBImmediate(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t new_value = regs_.a - byte;
        regs_.SetCarry(regs_.a < byte);
        regs_.SetHalf((regs_.a & 0xF) < (byte & 0xF));
        regs_.SetSubtract(true);
        regs_.SetZero(new_value == 0x0);
        regs_.a = new_value;
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::SUBIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t new_value = regs_.a - byte;
        regs_.SetCarry(regs_.a < byte);
        regs_.SetHalf((regs_.a & 0xF) < (byte & 0xF));
        regs_.SetSubtract(true);
        regs_.SetZero(new_value == 0x0);
        regs_.a = new_value;
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::SLA(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = regs_.*sourceReg;
    regs_.SetCarry(((value & 0x80) >> 7) == 0x01);
    const uint8_t newValue = value << 1;
    regs_.*sourceReg = newValue;
    regs_.SetZero(newValue == 0);
    regs_.SetSubtract(false);
    regs_.SetHalf(false);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::SLAAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.SetCarry((byte & 0x80) != 0);
        byte <<= 1;
        bus_.WriteByte(regs_.GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        regs_.SetZero(byte == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::SRA(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = regs_.*sourceReg;
    const uint8_t newValue = (value >> 1) | (value & 0x80);
    regs_.*sourceReg = newValue;
    regs_.SetCarry((value & 0x01) != 0);
    regs_.SetZero(newValue == 0);
    regs_.SetSubtract(false);
    regs_.SetHalf(false);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::SRAAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.SetCarry((byte & 0x01) != 0);
        byte = (byte >> 1) | (byte & 0x80);
        bus_.WriteByte(regs_.GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        regs_.SetZero(byte == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::SWAP(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    uint8_t value = regs_.*sourceReg;
    value = (value >> 4) | (value << 4);
    regs_.*sourceReg = value;
    regs_.SetZero(value == 0);
    regs_.SetCarry(false);
    regs_.SetSubtract(false);
    regs_.SetHalf(false);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::SWAPAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        byte = (byte >> 4) | (byte << 4);
        bus_.WriteByte(regs_.GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        regs_.SetZero(byte == 0);
        regs_.SetCarry(false);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::SRL(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    uint8_t value = regs_.*sourceReg;
    regs_.SetCarry((value & 0x01) != 0);
    value = value >> 1;
    regs_.*sourceReg = value;
    regs_.SetZero(value == 0);
    regs_.SetSubtract(false);
    regs_.SetHalf(false);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::SRLAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        regs_.SetCarry((byte & 0x01) != 0);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        byte = byte >> 1;
        bus_.WriteByte(regs_.GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        regs_.SetZero(byte == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

/* M3 -- prefixed, so only one */
template<Register source, int bit>
bool Instructions::BIT(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t sourceValue = regs_.*sourceReg;
    regs_.SetZero((sourceValue & (1 << bit)) == 0);
    regs_.SetSubtract(false);
    regs_.SetHalf(true);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

/* M4 -- prefixed, M3 fetches byte */
template<int bit>
bool Instructions::BITAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        regs_.SetZero((byte & (1 << bit)) == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(true);
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source, int bit>
bool Instructions::RES(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    uint8_t sourceValue = regs_.*sourceReg;
    sourceValue &= ~(1 << bit);
    regs_.*sourceReg = sourceValue;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<int bit>
bool Instructions::RESAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        byte &= ~(1 << bit);
        bus_.WriteByte(regs_.GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source, int bit>
bool Instructions::SET(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    uint8_t sourceValue = regs_.*sourceReg;
    sourceValue |= 1 << bit;
    regs_.*sourceReg = sourceValue;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<int bit>
bool Instructions::SETAddr(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        byte |= 1 << bit;
        bus_.WriteByte(regs_.GetHL(), byte);
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::SBCRegister(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = regs_.*sourceReg;
    const uint8_t flag_carry = regs_.FlagCarry() ? 1 : 0;
    const uint8_t r = regs_.a - value - flag_carry;
    regs_.SetCarry(regs_.a < value + static_cast<uint16_t>(flag_carry));
    regs_.SetHalf((regs_.a & 0xF) < (value & 0xF) + flag_carry);
    regs_.SetSubtract(true);
    regs_.SetZero(r == 0x0);
    regs_.a = r;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::SBCIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t flag_carry = regs_.FlagCarry() ? 1 : 0;
        const uint8_t r = regs_.a - byte - flag_carry;
        regs_.SetCarry(regs_.a < byte + static_cast<uint16_t>(flag_carry));
        regs_.SetHalf((regs_.a & 0xF) < (byte & 0xF) + flag_carry);
        regs_.SetSubtract(true);
        regs_.SetZero(r == 0x0);
        regs_.a = r;
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::SBCImmediate(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t flag_carry = regs_.FlagCarry() ? 1 : 0;
        const uint8_t r = regs_.a - byte - flag_carry;
        regs_.SetCarry(regs_.a < byte + static_cast<uint16_t>(flag_carry));
        regs_.SetHalf((regs_.a & 0xF) < (byte & 0xF) + flag_carry);
        regs_.SetSubtract(true);
        regs_.SetZero(r == 0x0);
        regs_.a = r;
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::ADCRegister(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = regs_.*sourceReg;
    const uint8_t flag_carry = regs_.FlagCarry() ? 1 : 0;
    const uint8_t r = regs_.a + value + flag_carry;
    regs_.SetCarry(static_cast<uint16_t>(regs_.a) + value + static_cast<uint16_t>(flag_carry) > 0xFF);
    regs_.SetHalf(((regs_.a & 0xF) + (value & 0xF) + (flag_carry & 0xF)) > 0xF);
    regs_.SetSubtract(false);
    regs_.SetZero(r == 0x0);
    regs_.a = r;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::ADCIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t flag_carry = regs_.FlagCarry() ? 1 : 0;
        const uint8_t r = regs_.a + byte + flag_carry;
        regs_.SetCarry(static_cast<uint16_t>(regs_.a) + byte + static_cast<uint16_t>(flag_carry) > 0xFF);
        regs_.SetHalf(((regs_.a & 0xF) + (byte & 0xF) + (flag_carry & 0xF)) > 0xF);
        regs_.SetSubtract(false);
        regs_.SetZero(r == 0x0);
        regs_.a = r;
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::ADCImmediate(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t flag_carry = regs_.FlagCarry() ? 1 : 0;
        const uint8_t r = regs_.a + byte + flag_carry;
        regs_.SetCarry(static_cast<uint16_t>(regs_.a) + byte + static_cast<uint16_t>(flag_carry) > 0xFF);
        regs_.SetHalf(((regs_.a & 0xF) + (byte & 0xF) + (flag_carry & 0xF)) > 0xF);
        regs_.SetSubtract(false);
        regs_.SetZero(r == 0x0);
        regs_.a = r;
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Arithmetic16Target target>
bool Instructions::ADD16(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        if constexpr (target == Arithmetic16Target::BC) word = regs_.GetBC();
        if constexpr (target == Arithmetic16Target::DE) word = regs_.GetDE();
        if constexpr (target == Arithmetic16Target::HL) word = regs_.GetHL();
        if constexpr (target == Arithmetic16Target::SP) word = cpu.sp;
        return false;
    }

    if (cpu.mCycleCounter == 3) {
        const uint16_t reg = regs_.GetHL();
        const uint16_t sum = reg + word;

        regs_.SetCarry(reg > 0xFFFF - word);
        regs_.SetSubtract(false);
        regs_.SetHalf((reg & 0x07FF) + (word & 0x07FF) > 0x07FF);
        regs_.SetHL(sum);

        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<Register source>
bool Instructions::ADDRegister(CPU &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t value = regs_.*sourceReg;
    const uint8_t a = regs_.a;
    const uint8_t new_value = a + value;
    regs_.SetCarry(static_cast<uint16_t>(a) + static_cast<uint16_t>(value) > 0xFF);
    regs_.SetZero(new_value == 0);
    regs_.SetSubtract(false);
    regs_.SetHalf((regs_.a & 0xF) + (value & 0xF) > 0xF);
    regs_.a = new_value;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

bool Instructions::ADDIndirect(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(regs_.GetHL());
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t a = regs_.a;
        const uint8_t new_value = a + byte;
        regs_.SetCarry(static_cast<uint16_t>(a) + static_cast<uint16_t>(byte) > 0xFF);
        regs_.SetZero(new_value == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf((regs_.a & 0xF) + (byte & 0xF) > 0xF);
        regs_.a = new_value;
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

bool Instructions::ADDImmediate(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        byte = bus_.ReadByte(cpu.pc++);
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        const uint8_t a = regs_.a;
        const uint8_t new_value = a + byte;
        regs_.SetCarry(static_cast<uint16_t>(a) + static_cast<uint16_t>(byte) > 0xFF);
        regs_.SetZero(new_value == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf((regs_.a & 0xF) + (byte & 0xF) > 0xF);
        regs_.a = new_value;
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

// Not exactly M-cycle accurate (GBCTR page 80)
bool Instructions::ADDSigned(CPU &cpu) {
    if (cpu.mCycleCounter == 2) {
        word = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(
            bus_.ReadByte(cpu.pc++))));
        return false;
    }
    if (cpu.mCycleCounter == 3) {
        word2 = cpu.sp;
        return false;
    }
    if (cpu.mCycleCounter == 4) {
        regs_.SetCarry(((word2 & 0xFF) + (word & 0xFF)) > 0xFF);
        regs_.SetHalf(((word2 & 0xF) + (word & 0xF)) > 0xF);
        regs_.SetSubtract(false);
        regs_.SetZero(false);
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.sp = word2 + word;
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
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
