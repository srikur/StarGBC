#include "Instructions.h"
#include <cstring>

template<BusLike BusT>
void Instructions<BusT>::HandleOAMCorruption(const CPU<BusT> &cpu, const uint16_t location, const CorruptionType type) const {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::RLC(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::RLCAddr(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::DAA(CPU<BusT> &cpu) const {
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

template<BusLike BusT>
bool Instructions<BusT>::RETI(CPU<BusT> &cpu) {
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
        interrupts_.interruptMasterEnable = true;
        return false;
    }
    if (cpu.mCycleCounter == 5) {
        cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
        return true;
    }
    return false;
}

template<BusLike BusT>
bool Instructions<BusT>::DI(CPU<BusT> &cpu) const {
    interrupts_.interruptDelay = false;
    interrupts_.interruptMasterEnable = false;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<BusLike BusT>
bool Instructions<BusT>::EI(CPU<BusT> &cpu) const {
    if (!interrupts_.interruptMasterEnable) {
        cpu.icount = 0;
        interrupts_.interruptDelay = true;
        interrupts_.interruptMasterEnable = true;
    }
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<BusLike BusT>
bool Instructions<BusT>::HALT(CPU<BusT> &cpu) const {
    if (const bool bug = (interrupts_.interruptEnable & interrupts_.interruptFlag & 0x1F) != 0; !interrupts_.interruptMasterEnable && bug) {
        cpu.haltBug = true;
        cpu.halted = false;
    } else {
        cpu.haltBug = false;
        cpu.halted = true;
    }
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<BusLike BusT>
template<RSTTarget target>
bool Instructions<BusT>::RST(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::CALLUnconditional(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<JumpTest test>
bool Instructions<BusT>::CALL(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::RLCA(CPU<BusT> &cpu) const {
    const uint8_t old = (regs_.a & 0x80) != 0 ? 1 : 0;
    regs_.SetCarry(old != 0);
    regs_.a = regs_.a << 1 | old;
    regs_.SetZero(false);
    regs_.SetHalf(false);
    regs_.SetSubtract(false);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<BusLike BusT>
bool Instructions<BusT>::RLA(CPU<BusT> &cpu) const {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::RL(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::RLAddr(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::CCF(CPU<BusT> &cpu) const {
    regs_.SetSubtract(false);
    regs_.SetCarry(!regs_.FlagCarry());
    regs_.SetHalf(false);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<BusLike BusT>
bool Instructions<BusT>::CPL(CPU<BusT> &cpu) const {
    regs_.SetHalf(true);
    regs_.SetSubtract(true);
    regs_.a = ~regs_.a;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<BusLike BusT>
bool Instructions<BusT>::SCF(CPU<BusT> &cpu) const {
    regs_.SetSubtract(false);
    regs_.SetHalf(false);
    regs_.SetCarry(true);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<BusLike BusT>
bool Instructions<BusT>::RRCA(CPU<BusT> &cpu) const {
    regs_.SetCarry((regs_.a & 0x01) != 0);
    regs_.a = regs_.a >> 1 | (regs_.a & 0x01) << 7;
    regs_.SetZero(false);
    regs_.SetSubtract(false);
    regs_.SetHalf(false);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::RRC(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::RRCAddr(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::RRAddr(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::RR(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::RRA(CPU<BusT> &cpu) const {
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

template<BusLike BusT>
bool Instructions<BusT>::RETUnconditional(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<JumpTest test>
bool Instructions<BusT>::RETConditional(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::JRUnconditional(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<JumpTest test>
bool Instructions<BusT>::JR(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::JPUnconditional(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<JumpTest test>
bool Instructions<BusT>::JP(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::JPHL(CPU<BusT> &cpu) const {
    cpu.pc = regs_.GetHL();
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<BusLike BusT>
bool Instructions<BusT>::NOP(CPU<BusT> &cpu) const {
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<BusLike BusT>
bool Instructions<BusT>::PREFIX(CPU<BusT> &cpu) const {
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    cpu.currentInstruction = 0xCB00 | cpu.nextInstruction;
    cpu.prefixed = true;
    return true;
}

template<BusLike BusT>
bool Instructions<BusT>::STOP(CPU<BusT> &cpu) const {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::DECRegister(CPU<BusT> &cpu) const {
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

template<BusLike BusT>
bool Instructions<BusT>::DECIndirect(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Arithmetic16Target target>
bool Instructions<BusT>::DEC16(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::INCRegister(CPU<BusT> &cpu) const {
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

template<BusLike BusT>
bool Instructions<BusT>::INCIndirect(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Arithmetic16Target target>
bool Instructions<BusT>::INC16(CPU<BusT> &cpu) {
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
template<BusLike BusT>
bool Instructions<BusT>::LoadFromAccumulatorDirectA(CPU<BusT> &cpu) {
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
template<BusLike BusT>
bool Instructions<BusT>::LoadFromAccumulatorIndirectC(CPU<BusT> &cpu) const {
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
template<BusLike BusT>
bool Instructions<BusT>::LoadAccumulatorA(CPU<BusT> &cpu) {
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
template<BusLike BusT>
bool Instructions<BusT>::LoadAccumulatorIndirectC(CPU<BusT> &cpu) {
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
template<BusLike BusT>
template<Register target, Register source>
bool Instructions<BusT>::LDRegister(CPU<BusT> &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    constexpr auto targetReg = GetRegisterPtr<target>();
    const uint8_t sourceValue = regs_.*sourceReg;
    regs_.*targetReg = sourceValue;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::LDRegisterImmediate(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::LDRegisterIndirect(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::LDAddrRegister(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::LDAddrImmediate(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::LDAccumulatorBC(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::LDAccumulatorDE(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::LDFromAccBC(CPU<BusT> &cpu) const {
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

template<BusLike BusT>
bool Instructions<BusT>::LDFromAccDE(CPU<BusT> &cpu) const {
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

template<BusLike BusT>
bool Instructions<BusT>::LDAccumulatorDirect(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::LDFromAccumulatorDirect(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::LDAccumulatorIndirectDec(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::LDFromAccumulatorIndirectDec(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::LDAccumulatorIndirectInc(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::LDFromAccumulatorIndirectInc(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::LD16FromStack(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<LoadWordTarget target>
bool Instructions<BusT>::LD16Register(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::LD16Stack(CPU<BusT> &cpu) const {
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

template<BusLike BusT>
bool Instructions<BusT>::LD16StackAdjusted(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<StackTarget target>
bool Instructions<BusT>::PUSH(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<StackTarget target>
bool Instructions<BusT>::POP(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::CPRegister(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::CPIndirect(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::CPImmediate(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::ORRegister(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::ORIndirect(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::ORImmediate(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::XORRegister(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::XORIndirect(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::XORImmediate(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::AND(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::ANDImmediate(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::ANDIndirect(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::SUB(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::SUBImmediate(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::SUBIndirect(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::SLA(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::SLAAddr(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::SRA(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::SRAAddr(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::SWAP(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::SWAPAddr(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::SRL(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::SRLAddr(CPU<BusT> &cpu) {
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
template<BusLike BusT>
template<Register source, int bit>
bool Instructions<BusT>::BIT(CPU<BusT> &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    const uint8_t sourceValue = regs_.*sourceReg;
    regs_.SetZero((sourceValue & (1 << bit)) == 0);
    regs_.SetSubtract(false);
    regs_.SetHalf(true);
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

/* M4 -- prefixed, M3 fetches byte */
template<BusLike BusT>
template<int bit>
bool Instructions<BusT>::BITAddr(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Register source, int bit>
bool Instructions<BusT>::RES(CPU<BusT> &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    uint8_t sourceValue = regs_.*sourceReg;
    sourceValue &= ~(1 << bit);
    regs_.*sourceReg = sourceValue;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<BusLike BusT>
template<int bit>
bool Instructions<BusT>::RESAddr(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Register source, int bit>
bool Instructions<BusT>::SET(CPU<BusT> &cpu) {
    constexpr auto sourceReg = GetRegisterPtr<source>();
    uint8_t sourceValue = regs_.*sourceReg;
    sourceValue |= 1 << bit;
    regs_.*sourceReg = sourceValue;
    cpu.nextInstruction = bus_.ReadByte(cpu.pc++);
    return true;
}

template<BusLike BusT>
template<int bit>
bool Instructions<BusT>::SETAddr(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::SBCRegister(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::SBCIndirect(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::SBCImmediate(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::ADCRegister(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::ADCIndirect(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::ADCImmediate(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Arithmetic16Target target>
bool Instructions<BusT>::ADD16(CPU<BusT> &cpu) {
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

template<BusLike BusT>
template<Register source>
bool Instructions<BusT>::ADDRegister(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::ADDIndirect(CPU<BusT> &cpu) {
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

template<BusLike BusT>
bool Instructions<BusT>::ADDImmediate(CPU<BusT> &cpu) {
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
template<BusLike BusT>
bool Instructions<BusT>::ADDSigned(CPU<BusT> &cpu) {
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

template class Instructions<Bus>;

// template function definitions to solve linker errors
template bool Instructions<Bus>::RST<RSTTarget::H00>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RST<RSTTarget::H08>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RST<RSTTarget::H10>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RST<RSTTarget::H18>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RST<RSTTarget::H20>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RST<RSTTarget::H28>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RST<RSTTarget::H30>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RST<RSTTarget::H38>(CPU<Bus> &cpu);

template bool Instructions<Bus>::CALL<JumpTest::NotZero>(CPU<Bus> &cpu);

template bool Instructions<Bus>::CALL<JumpTest::Zero>(CPU<Bus> &cpu);

template bool Instructions<Bus>::CALL<JumpTest::NotCarry>(CPU<Bus> &cpu);

template bool Instructions<Bus>::CALL<JumpTest::Carry>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RETConditional<JumpTest::NotZero>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RETConditional<JumpTest::Zero>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RETConditional<JumpTest::NotCarry>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RETConditional<JumpTest::Carry>(CPU<Bus> &cpu);

template bool Instructions<Bus>::JR<JumpTest::NotZero>(CPU<Bus> &cpu);

template bool Instructions<Bus>::JR<JumpTest::Zero>(CPU<Bus> &cpu);

template bool Instructions<Bus>::JR<JumpTest::NotCarry>(CPU<Bus> &cpu);

template bool Instructions<Bus>::JR<JumpTest::Carry>(CPU<Bus> &cpu);

template bool Instructions<Bus>::JP<JumpTest::NotZero>(CPU<Bus> &cpu);

template bool Instructions<Bus>::JP<JumpTest::Zero>(CPU<Bus> &cpu);

template bool Instructions<Bus>::JP<JumpTest::NotCarry>(CPU<Bus> &cpu);

template bool Instructions<Bus>::JP<JumpTest::Carry>(CPU<Bus> &cpu);

template bool Instructions<Bus>::DECRegister<Register::A>(CPU<Bus> &cpu) const;

template bool Instructions<Bus>::DECRegister<Register::B>(CPU<Bus> &cpu) const;

template bool Instructions<Bus>::DECRegister<Register::C>(CPU<Bus> &cpu) const;

template bool Instructions<Bus>::DECRegister<Register::D>(CPU<Bus> &cpu) const;

template bool Instructions<Bus>::DECRegister<Register::E>(CPU<Bus> &cpu) const;

template bool Instructions<Bus>::DECRegister<Register::H>(CPU<Bus> &cpu) const;

template bool Instructions<Bus>::DECRegister<Register::L>(CPU<Bus> &cpu) const;

template bool Instructions<Bus>::DEC16<Arithmetic16Target::BC>(CPU<Bus> &cpu);

template bool Instructions<Bus>::DEC16<Arithmetic16Target::DE>(CPU<Bus> &cpu);

template bool Instructions<Bus>::DEC16<Arithmetic16Target::HL>(CPU<Bus> &cpu);

template bool Instructions<Bus>::DEC16<Arithmetic16Target::SP>(CPU<Bus> &cpu);

template bool Instructions<Bus>::INCRegister<Register::A>(CPU<Bus> &cpu) const;

template bool Instructions<Bus>::INCRegister<Register::B>(CPU<Bus> &cpu) const;

template bool Instructions<Bus>::INCRegister<Register::C>(CPU<Bus> &cpu) const;

template bool Instructions<Bus>::INCRegister<Register::D>(CPU<Bus> &cpu) const;

template bool Instructions<Bus>::INCRegister<Register::E>(CPU<Bus> &cpu) const;

template bool Instructions<Bus>::INCRegister<Register::H>(CPU<Bus> &cpu) const;

template bool Instructions<Bus>::INCRegister<Register::L>(CPU<Bus> &cpu) const;

template bool Instructions<Bus>::INC16<Arithmetic16Target::BC>(CPU<Bus> &cpu);

template bool Instructions<Bus>::INC16<Arithmetic16Target::DE>(CPU<Bus> &cpu);

template bool Instructions<Bus>::INC16<Arithmetic16Target::HL>(CPU<Bus> &cpu);

template bool Instructions<Bus>::INC16<Arithmetic16Target::SP>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::A, Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::A, Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::A, Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::A, Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::A, Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::A, Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::A, Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::B, Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::B, Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::B, Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::B, Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::B, Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::B, Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::B, Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::C, Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::C, Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::C, Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::C, Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::C, Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::C, Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::C, Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::D, Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::D, Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::D, Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::D, Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::D, Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::D, Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::D, Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::E, Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::E, Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::E, Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::E, Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::E, Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::E, Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::E, Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::H, Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::H, Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::H, Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::H, Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::H, Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::H, Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::H, Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::L, Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::L, Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::L, Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::L, Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::L, Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::L, Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegister<Register::L, Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegisterImmediate<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegisterImmediate<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegisterImmediate<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegisterImmediate<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegisterImmediate<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegisterImmediate<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegisterImmediate<Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegisterIndirect<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegisterIndirect<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegisterIndirect<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegisterIndirect<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegisterIndirect<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegisterIndirect<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDRegisterIndirect<Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDAddrRegister<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDAddrRegister<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDAddrRegister<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDAddrRegister<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDAddrRegister<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDAddrRegister<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LDAddrRegister<Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LD16Register<LoadWordTarget::BC>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LD16Register<LoadWordTarget::DE>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LD16Register<LoadWordTarget::HL>(CPU<Bus> &cpu);

template bool Instructions<Bus>::LD16Register<LoadWordTarget::SP>(CPU<Bus> &cpu);

template bool Instructions<Bus>::PUSH<StackTarget::BC>(CPU<Bus> &cpu);

template bool Instructions<Bus>::PUSH<StackTarget::AF>(CPU<Bus> &cpu);

template bool Instructions<Bus>::PUSH<StackTarget::DE>(CPU<Bus> &cpu);

template bool Instructions<Bus>::PUSH<StackTarget::HL>(CPU<Bus> &cpu);

template bool Instructions<Bus>::POP<StackTarget::BC>(CPU<Bus> &cpu);

template bool Instructions<Bus>::POP<StackTarget::AF>(CPU<Bus> &cpu);

template bool Instructions<Bus>::POP<StackTarget::DE>(CPU<Bus> &cpu);

template bool Instructions<Bus>::POP<StackTarget::HL>(CPU<Bus> &cpu);

template bool Instructions<Bus>::CPRegister<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::CPRegister<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::CPRegister<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::CPRegister<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::CPRegister<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::CPRegister<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::CPRegister<Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ORRegister<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ORRegister<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ORRegister<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ORRegister<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ORRegister<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ORRegister<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ORRegister<Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::XORRegister<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::XORRegister<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::XORRegister<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::XORRegister<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::XORRegister<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::XORRegister<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::XORRegister<Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::AND<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::AND<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::AND<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::AND<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::AND<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::AND<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::AND<Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SUB<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SUB<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SUB<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SUB<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SUB<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SUB<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SUB<Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RRC<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RRC<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RRC<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RRC<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RRC<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RRC<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RRC<Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RLC<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RLC<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RLC<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RLC<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RLC<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RLC<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RLC<Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RR<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RR<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RR<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RR<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RR<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RR<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RR<Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RL<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RL<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RL<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RL<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RL<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RL<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RL<Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SLA<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SLA<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SLA<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SLA<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SLA<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SLA<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SLA<Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SRA<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SRA<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SRA<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SRA<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SRA<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SRA<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SRA<Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SWAP<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SWAP<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SWAP<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SWAP<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SWAP<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SWAP<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SWAP<Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SRL<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SRL<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SRL<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SRL<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SRL<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SRL<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SRL<Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::A, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::B, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::C, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::D, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::E, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::H, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::L, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::A, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::B, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::C, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::D, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::E, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::H, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::L, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::A, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::B, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::C, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::D, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::E, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::H, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::L, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::A, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::B, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::C, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::D, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::E, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::H, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::L, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::A, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::B, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::C, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::D, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::E, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::H, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::L, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::A, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::B, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::C, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::D, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::E, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::H, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::L, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::A, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::B, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::C, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::D, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::E, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::H, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::L, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::A, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::B, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::C, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::D, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::E, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::H, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BIT<Register::L, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::A, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::B, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::C, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::D, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::E, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::H, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::L, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::A, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::B, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::C, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::D, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::E, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::H, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::L, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::A, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::B, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::C, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::D, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::E, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::H, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::L, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::A, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::B, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::C, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::D, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::E, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::H, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::L, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::A, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::B, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::C, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::D, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::E, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::H, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::L, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::A, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::B, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::C, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::D, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::E, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::H, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::L, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::A, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::B, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::C, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::D, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::E, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::H, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::L, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::A, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::B, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::C, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::D, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::E, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::H, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RES<Register::L, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::A, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::B, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::C, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::D, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::E, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::H, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::L, 0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::A, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::B, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::C, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::D, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::E, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::H, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::L, 1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::A, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::B, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::C, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::D, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::E, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::H, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::L, 2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::A, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::B, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::C, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::D, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::E, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::H, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::L, 3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::A, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::B, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::C, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::D, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::E, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::H, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::L, 4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::A, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::B, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::C, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::D, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::E, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::H, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::L, 5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::A, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::B, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::C, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::D, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::E, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::H, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::L, 6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::A, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::B, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::C, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::D, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::E, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::H, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SET<Register::L, 7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BITAddr<0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BITAddr<1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BITAddr<2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BITAddr<3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BITAddr<4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BITAddr<5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BITAddr<6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::BITAddr<7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RESAddr<0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RESAddr<1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RESAddr<2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RESAddr<3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RESAddr<4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RESAddr<5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RESAddr<6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::RESAddr<7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SETAddr<0>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SETAddr<1>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SETAddr<2>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SETAddr<3>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SETAddr<4>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SETAddr<5>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SETAddr<6>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SETAddr<7>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SBCRegister<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SBCRegister<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SBCRegister<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SBCRegister<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SBCRegister<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SBCRegister<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::SBCRegister<Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ADCRegister<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ADCRegister<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ADCRegister<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ADCRegister<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ADCRegister<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ADCRegister<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ADCRegister<Register::L>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ADD16<Arithmetic16Target::BC>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ADD16<Arithmetic16Target::DE>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ADD16<Arithmetic16Target::HL>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ADD16<Arithmetic16Target::SP>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ADDRegister<Register::A>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ADDRegister<Register::B>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ADDRegister<Register::C>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ADDRegister<Register::D>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ADDRegister<Register::E>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ADDRegister<Register::H>(CPU<Bus> &cpu);

template bool Instructions<Bus>::ADDRegister<Register::L>(CPU<Bus> &cpu);
