#pragma once

#include <stdexcept>

#include "Common.h"
#include "GPU.h"
#include "Interrupts.h"
#include "Registers.h"

template<BusLike BusT>
class CPU;

enum class JumpTest {
    NotZero, Zero, NotCarry, Carry, Always
};

enum class RSTTarget {
    H00, H10, H20, H30, H08, H18, H28, H38,
};

enum class Arithmetic16Target {
    HL, BC, DE, SP,
};

enum class LoadWordTarget {
    BC, DE, HL, SP
};

enum class StackTarget {
    BC, DE, HL, AF,
};

enum class Register {
    A, B, C, D, E, H, L,
};

template<typename CPUType>
class Instructions {
public:
    template<typename T = CPUType> requires requires(T cpu, uint16_t word, uint8_t byte, bool boolean)
    {
        { cpu.pc() } -> std::same_as<std::add_lvalue_reference_t<uint16_t> >;
        { cpu.sp() } -> std::same_as<std::add_lvalue_reference_t<uint16_t> >;
        { cpu.pc(word) } -> std::same_as<void>;
        { cpu.sp(word) } -> std::same_as<void>;
        { cpu.icount(word) } -> std::same_as<void>;
        { cpu.mCycleCounter() } -> std::same_as<std::add_lvalue_reference_t<uint8_t> >;
        { cpu.mCycleCounter(byte) } -> std::same_as<void>;
        { cpu.nextInstruction() } -> std::same_as<std::add_lvalue_reference_t<uint16_t> >;
        { cpu.nextInstruction(word) } -> std::same_as<void>;
        { cpu.halted(boolean) } -> std::same_as<void>;
        { cpu.haltBug(boolean) } -> std::same_as<void>;
        { cpu.stopped(boolean) } -> std::same_as<void>;
        { cpu.stopped() } -> std::same_as<std::add_lvalue_reference_t<bool> >;
    }

    explicit Instructions(Registers &regs, Interrupts &interrupts) : regs_(regs), interrupts_(interrupts) {
    }

    bool prefixedInstr(const uint8_t opcode, CPUType &cpu) {
        return prefixedTable[opcode](*this, cpu);
    }

    bool nonPrefixedInstr(const uint8_t opcode, CPUType &cpu) {
        return nonPrefixedTable[opcode](*this, cpu);
    }

    void ResetState() {
        word = word2 = byte = signedByte = 0;
        jumpCondition = false;
    }

    std::string GetMnemonic(uint16_t instruction) const {
        const bool prefixed = instruction >> 8 == 0xCB;
        instruction &= 0xFF;
        return prefixed ? prefixedInstructions[instruction] : nonPrefixedInstructions[instruction];
    }

private:
    Registers &regs_;
    Interrupts &interrupts_;
    int8_t signedByte{0};
    uint8_t byte{0};
    uint16_t word{0};
    uint16_t word2{0};
    bool jumpCondition{false};

    using WrappedFunction = bool (*)(Instructions &instr, CPUType &cpu);

    static bool IllegalOpcode(Instructions &, CPUType &) {
        throw std::runtime_error("Illegal opcode executed");
    }

    template<Register source>
    static constexpr auto GetRegisterPtr() {
        if constexpr (source == Register::A) return &Registers::a;
        if constexpr (source == Register::B) return &Registers::b;
        if constexpr (source == Register::C) return &Registers::c;
        if constexpr (source == Register::D) return &Registers::d;
        if constexpr (source == Register::E) return &Registers::e;
        if constexpr (source == Register::H) return &Registers::h;
        if constexpr (source == Register::L) return &Registers::l;
    }

    template<RSTTarget target>
    static constexpr auto GetRSTAddress() {
        if constexpr (target == RSTTarget::H00) return 0x00;
        if constexpr (target == RSTTarget::H08) return 0x08;
        if constexpr (target == RSTTarget::H10) return 0x10;
        if constexpr (target == RSTTarget::H18) return 0x18;
        if constexpr (target == RSTTarget::H20) return 0x20;
        if constexpr (target == RSTTarget::H28) return 0x28;
        if constexpr (target == RSTTarget::H30) return 0x30;
        if constexpr (target == RSTTarget::H38) return 0x38;
    }

    bool DAA(CPUType &cpu) const {
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
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool RETI(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            word = cpu.bus_.ReadByte(cpu.sp(), ComponentSource::CPU);
            cpu.sp() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            word |= static_cast<uint16_t>(cpu.bus_.ReadByte(cpu.sp(), ComponentSource::CPU)) << 8;
            cpu.sp() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            cpu.pc() = word;
            interrupts_.interruptMasterEnable = true;
            return false;
        }
        if (cpu.mCycleCounter() == 5) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool DI(CPUType &cpu) const {
        interrupts_.interruptDelay = false;
        interrupts_.interruptMasterEnable = false;
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool EI(CPUType &cpu) const {
        if (!interrupts_.interruptMasterEnable) {
            cpu.icount(0);
            interrupts_.interruptDelay = true;
            interrupts_.interruptMasterEnable = true;
        }
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool HALT(CPUType &cpu) const {
        if (const bool bug = (interrupts_.interruptEnable & interrupts_.interruptFlag & 0x1F) != 0; !interrupts_.interruptMasterEnable && bug) {
            cpu.haltBug(true);
            cpu.halted(false);
        } else {
            cpu.haltBug(false);
            cpu.halted(true);
        }
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    template<RSTTarget target>
    bool RST(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            cpu.sp() -= 1;
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            cpu.bus_.WriteByte(cpu.sp(), (cpu.pc() & 0xFF00) >> 8, ComponentSource::CPU);
            cpu.sp() -= 1;
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            cpu.bus_.WriteByte(cpu.sp(), cpu.pc() & 0xFF, ComponentSource::CPU);
            constexpr auto location = GetRSTAddress<target>();
            cpu.pc() = location;
            return false;
        }
        if (cpu.mCycleCounter() == 5) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool CALLUnconditional(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            word = cpu.bus_.ReadByte(cpu.pc(), ComponentSource::CPU);
            cpu.pc() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            word |= static_cast<uint16_t>(cpu.bus_.ReadByte(cpu.pc(), ComponentSource::CPU)) << 8;
            cpu.pc() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            cpu.sp() -= 1;
            return false;
        }
        if (cpu.mCycleCounter() == 5) {
            cpu.bus_.WriteByte(cpu.sp(), (cpu.pc() & 0xFF00) >> 8, ComponentSource::CPU);
            cpu.sp() -= 1;
            return false;
        }
        if (cpu.mCycleCounter() == 6) {
            cpu.bus_.WriteByte(cpu.sp(), cpu.pc() & 0xFF, ComponentSource::CPU);
            cpu.pc() = word;
            return false;
        }
        if (cpu.mCycleCounter() == 7) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<JumpTest test>
    bool CALL(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            word = cpu.bus_.ReadByte(cpu.pc(), ComponentSource::CPU);
            cpu.pc() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            if constexpr (test == JumpTest::NotZero) jumpCondition = !regs_.FlagZero();
            else if constexpr (test == JumpTest::Zero) jumpCondition = regs_.FlagZero();
            else if constexpr (test == JumpTest::Carry) jumpCondition = regs_.FlagCarry();
            else if constexpr (test == JumpTest::NotCarry) jumpCondition = !regs_.FlagCarry();
            word |= static_cast<uint16_t>(cpu.bus_.ReadByte(cpu.pc(), ComponentSource::CPU)) << 8;
            cpu.pc() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            if (jumpCondition) {
                cpu.sp() -= 1;
                return false;
            }

            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        if (cpu.mCycleCounter() == 5) {
            cpu.bus_.WriteByte(cpu.sp(), (cpu.pc() & 0xFF00) >> 8, ComponentSource::CPU);
            cpu.sp() -= 1;
            return false;
        }
        if (cpu.mCycleCounter() == 6) {
            cpu.bus_.WriteByte(cpu.sp(), cpu.pc() & 0xFF, ComponentSource::CPU);
            cpu.pc() = word;
            return false;
        }
        if (cpu.mCycleCounter() == 7) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool RLCA(CPUType &cpu) const {
        const uint8_t old = (regs_.a & 0x80) != 0 ? 1 : 0;
        regs_.SetCarry(old != 0);
        regs_.a = regs_.a << 1 | old;
        regs_.SetZero(false);
        regs_.SetHalf(false);
        regs_.SetSubtract(false);
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool RLA(CPUType &cpu) const {
        const bool flag_c = (regs_.a & 0x80) >> 7 == 0x01;
        const uint8_t r = (regs_.a << 1) + static_cast<uint8_t>(regs_.FlagCarry());
        regs_.SetCarry(flag_c);
        regs_.SetZero(false);
        regs_.SetHalf(false);
        regs_.SetSubtract(false);
        regs_.a = r;
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool CCF(CPUType &cpu) const {
        regs_.SetSubtract(false);
        regs_.SetCarry(!regs_.FlagCarry());
        regs_.SetHalf(false);
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool CPL(CPUType &cpu) const {
        regs_.SetHalf(true);
        regs_.SetSubtract(true);
        regs_.a = ~regs_.a;
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool SCF(CPUType &cpu) const {
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        regs_.SetCarry(true);
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool RRCA(CPUType &cpu) const {
        regs_.SetCarry((regs_.a & 0x01) != 0);
        regs_.a = regs_.a >> 1 | (regs_.a & 0x01) << 7;
        regs_.SetZero(false);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool RRA(CPUType &cpu) const {
        const bool carry = (regs_.a & 0x01) == 0x01;
        const uint8_t newValue = regs_.FlagCarry() ? 0x80 | regs_.a >> 1 : regs_.a >> 1;
        regs_.SetZero(false);
        regs_.a = newValue;
        regs_.SetCarry(carry);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool RETUnconditional(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            word = cpu.bus_.ReadByte(cpu.sp(), ComponentSource::CPU);
            cpu.sp() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            word |= static_cast<uint16_t>(cpu.bus_.ReadByte(cpu.sp(), ComponentSource::CPU)) << 8;
            cpu.sp() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            cpu.pc() = word;
            return false;
        }
        if (cpu.mCycleCounter() == 5) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<JumpTest test>
    bool RETConditional(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            if constexpr (test == JumpTest::NotZero) jumpCondition = !regs_.FlagZero();
            else if constexpr (test == JumpTest::Zero) jumpCondition = regs_.FlagZero();
            else if constexpr (test == JumpTest::Carry) jumpCondition = regs_.FlagCarry();
            else if constexpr (test == JumpTest::NotCarry) jumpCondition = !regs_.FlagCarry();
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            if (jumpCondition) {
                word = cpu.bus_.ReadByte(cpu.sp(), ComponentSource::CPU);
                cpu.sp() += 1;
                return false;
            }

            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        if (cpu.mCycleCounter() == 4) {
            word |= static_cast<uint16_t>(cpu.bus_.ReadByte(cpu.sp(), ComponentSource::CPU)) << 8;
            cpu.sp() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 5) {
            cpu.pc() = word;
            return false;
        }
        if (cpu.mCycleCounter() == 6) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool JRUnconditional(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            signedByte = std::bit_cast<int8_t>(cpu.bus_.ReadByte(cpu.pc(), ComponentSource::CPU));
            cpu.pc() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            const uint16_t next = cpu.pc();
            if (signedByte >= 0) {
                cpu.pc() = next + static_cast<uint16_t>(signedByte);
            } else {
                cpu.pc() = next - static_cast<uint16_t>(abs(signedByte));
            }
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<JumpTest test>
    bool JR(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            signedByte = std::bit_cast<int8_t>(cpu.bus_.ReadByte(cpu.pc(), ComponentSource::CPU));
            cpu.pc() += 1;
            if constexpr (test == JumpTest::NotZero) jumpCondition = !regs_.FlagZero();
            else if constexpr (test == JumpTest::Zero) jumpCondition = regs_.FlagZero();
            else if constexpr (test == JumpTest::Carry) jumpCondition = regs_.FlagCarry();
            else if constexpr (test == JumpTest::NotCarry) jumpCondition = !regs_.FlagCarry();
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            const uint16_t next = cpu.pc();
            if (jumpCondition) {
                if (signedByte >= 0) {
                    cpu.pc() = next + static_cast<uint16_t>(signedByte);
                } else {
                    cpu.pc() = next - static_cast<uint16_t>(abs(signedByte));
                }
                return false;
            }
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        if (cpu.mCycleCounter() == 4) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool JPUnconditional(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            word = cpu.bus_.ReadByte(cpu.pc(), ComponentSource::CPU);
            cpu.pc() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            word |= static_cast<uint16_t>(cpu.bus_.ReadByte(cpu.pc(), ComponentSource::CPU)) << 8;
            cpu.pc() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            cpu.pc() = word;
            return false;
        }
        if (cpu.mCycleCounter() == 5) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<JumpTest test>
    bool JP(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            word = cpu.bus_.ReadByte(cpu.pc(), ComponentSource::CPU);
            cpu.pc() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            word |= static_cast<uint16_t>(cpu.bus_.ReadByte(cpu.pc(), ComponentSource::CPU)) << 8;
            cpu.pc() += 1;
            if constexpr (test == JumpTest::NotZero) jumpCondition = !regs_.FlagZero();
            else if constexpr (test == JumpTest::Zero) jumpCondition = regs_.FlagZero();
            else if constexpr (test == JumpTest::Carry) jumpCondition = regs_.FlagCarry();
            else if constexpr (test == JumpTest::NotCarry) jumpCondition = !regs_.FlagCarry();
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            if (jumpCondition) {
                cpu.pc() = word;
                return false;
            }
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        if (cpu.mCycleCounter() == 5) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool JPHL(CPUType &cpu) const {
        cpu.pc() = regs_.GetHL();
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool NOP(CPUType &cpu) const {
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool PREFIX(CPUType &cpu) const {
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        cpu.currentInstruction = 0xCB00 | cpu.nextInstruction();
        cpu.prefixed = true;
        return true;
    }

    bool STOP(CPUType &cpu) const {
        const uint8_t key1 = cpu.bus_.ReadByte(0xFF4D, ComponentSource::CPU);
        const bool speedSwitchRequested = cpu.bus_.prepareSpeedShift && (key1 & 0x01);
        cpu.bus_.WriteByte(0xFF04, 0x00, ComponentSource::CPU);

        if (speedSwitchRequested) {
            cpu.bus_.ChangeSpeed();
            // The CPU stalls through the switch while the PPU and timers run.
            // SameBoy uses 0x20008 T-cycles; the +0x14 recenters the window for
            // this core's STOP micro-op timing (daid speed_switch_timing_ly/stat)
            cpu.bus_.speedSwitchHalt = 0x2001C;
            // The DIV reset takes effect 0x14 T-cycles into the switch, so the
            // counter starts slightly negative (daid speed_switch_timing_div)
            cpu.bus_.timer_.divCounter = static_cast<uint16_t>(-0x14);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        } else {
            cpu.stopped(true);
            // On DMG -- blank out the screen white, on CGB -- blank out the screen black, unless GPU is in Mode 3
            if (IsDmg(cpu.hardware())) {
                std::fill(cpu.bus_.gpu_.screenData.begin(), cpu.bus_.gpu_.screenData.end(), 0xFFFFFFFF);
            } else if (cpu.bus_.gpu_.stat.mode != GPUMode::MODE_3) {
                std::fill(cpu.bus_.gpu_.screenData.begin(), cpu.bus_.gpu_.screenData.end(), 0x00000000);
            }
        }
        return true;
    }

    template<Register source>
    bool DECRegister(CPUType &cpu) const {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        const uint8_t sourceValue = regs_.*sourceReg;
        regs_.SetHalf((sourceValue & 0xF) == 0x00);
        const uint8_t newValue = sourceValue - 1;
        regs_.SetZero(newValue == 0);
        regs_.SetSubtract(true);
        regs_.*sourceReg = newValue;
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool DECIndirect(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.SetHalf((byte & 0xF) == 0x00);
            const uint8_t newValue = byte - 1;
            regs_.SetZero(newValue == 0);
            regs_.SetSubtract(true);
            cpu.bus_.WriteByte(regs_.GetHL(), newValue, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Arithmetic16Target target>
    bool DEC16(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            if constexpr (target == Arithmetic16Target::BC) {
                word = regs_.GetBC();
                cpu.bus_.HandleOAMCorruption(word, CorruptionType::Write);
                regs_.SetBC(word - 1);
            } else if constexpr (target == Arithmetic16Target::DE) {
                word = regs_.GetDE();
                cpu.bus_.HandleOAMCorruption(word, CorruptionType::Write);
                regs_.SetDE(word - 1);
            } else if constexpr (target == Arithmetic16Target::HL) {
                word = regs_.GetHL();
                regs_.SetHL(word - 1);
                cpu.bus_.HandleOAMCorruption(word, CorruptionType::Write);
            } else if constexpr (target == Arithmetic16Target::SP) {
                cpu.bus_.HandleOAMCorruption(cpu.sp(), CorruptionType::Write);
                cpu.sp() -= 1;
            }
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool INCRegister(CPUType &cpu) const {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        const uint8_t sourceValue = regs_.*sourceReg;
        regs_.SetHalf((sourceValue & 0xF) == 0xF);
        const uint8_t newValue = sourceValue + 1;
        regs_.SetZero(newValue == 0);
        regs_.SetSubtract(false);
        regs_.*sourceReg = newValue;
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool INCIndirect(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.SetHalf((byte & 0xF) == 0xF);
            const uint8_t newValue = byte + 1;
            regs_.SetZero(newValue == 0);
            regs_.SetSubtract(false);
            cpu.bus_.WriteByte(regs_.GetHL(), newValue, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Arithmetic16Target target>
    bool INC16(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            if constexpr (target == Arithmetic16Target::BC) {
                word = regs_.GetBC();
                cpu.bus_.HandleOAMCorruption(word, CorruptionType::Write);
                regs_.SetBC(word + 1);
            } else if constexpr (target == Arithmetic16Target::DE) {
                word = regs_.GetDE();
                cpu.bus_.HandleOAMCorruption(word, CorruptionType::Write);
                regs_.SetDE(word + 1);
            } else if constexpr (target == Arithmetic16Target::HL) {
                word = regs_.GetHL();
                cpu.bus_.HandleOAMCorruption(word, CorruptionType::Write);
                regs_.SetHL(word + 1);
            } else if constexpr (target == Arithmetic16Target::SP) {
                cpu.bus_.HandleOAMCorruption(cpu.sp(), CorruptionType::Write);
                cpu.sp() += 1;
            }
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register target, Register source>
    bool LDRegister(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        constexpr auto targetReg = GetRegisterPtr<target>();
        const uint8_t sourceValue = regs_.*sourceReg;
        regs_.*targetReg = sourceValue;
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    template<Register source>
    bool LDRegisterImmediate(CPUType &cpu) {
        constexpr auto targetReg = GetRegisterPtr<source>();
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.*targetReg = byte;
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool LDRegisterIndirect(CPUType &cpu) {
        constexpr auto targetReg = GetRegisterPtr<source>();
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.*targetReg = byte;
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool LDAddrRegister(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        if (cpu.mCycleCounter() == 2) {
            const uint8_t sourceValue = regs_.*sourceReg;
            cpu.bus_.WriteByte(regs_.GetHL(), sourceValue, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool LDAddrImmediate(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            cpu.bus_.WriteByte(regs_.GetHL(), byte, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool LDAccumulatorBC(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetBC(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.a = byte;
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool LDAccumulatorDE(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetDE(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.a = byte;
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool LDFromAccBC(CPUType &cpu) const {
        if (cpu.mCycleCounter() == 2) {
            cpu.bus_.WriteByte(regs_.GetBC(), regs_.a, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool LDFromAccDE(CPUType &cpu) const {
        if (cpu.mCycleCounter() == 2) {
            cpu.bus_.WriteByte(regs_.GetDE(), regs_.a, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool LDAccumulatorDirect(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            word = cpu.bus_.ReadByte(cpu.pc(), ComponentSource::CPU);
            cpu.pc() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            word |= static_cast<uint16_t>(cpu.bus_.ReadByte(cpu.pc(), ComponentSource::CPU)) << 8;
            cpu.pc() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            regs_.a = cpu.bus_.ReadByte(word, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 5) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool LDFromAccumulatorDirect(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            word = cpu.bus_.ReadByte(cpu.pc(), ComponentSource::CPU);
            cpu.pc() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            word |= static_cast<uint16_t>(cpu.bus_.ReadByte(cpu.pc(), ComponentSource::CPU)) << 8;
            cpu.pc() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            cpu.bus_.WriteByte(word, regs_.a, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 5) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc(), ComponentSource::CPU);
            cpu.pc() += 1;
            return true;
        }
        return false;
    }

    bool LDAccumulatorIndirectDec(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            cpu.bus_.HandleOAMCorruption(regs_.GetHL(), CorruptionType::ReadWrite);
            regs_.SetHL(regs_.GetHL() - 1);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.a = byte;
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool LDFromAccumulatorIndirectDec(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            word = regs_.GetHL();
            cpu.bus_.HandleOAMCorruption(word, CorruptionType::Write);
            cpu.bus_.WriteByte(word, regs_.a, ComponentSource::CPU);
            cpu.bus_.HandleOAMCorruption(word, CorruptionType::Write);
            regs_.SetHL(word - 1);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool LDAccumulatorIndirectInc(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            cpu.bus_.HandleOAMCorruption(regs_.GetHL(), CorruptionType::ReadWrite);
            regs_.SetHL(regs_.GetHL() + 1);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.a = byte;
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool LDFromAccumulatorIndirectInc(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            word = regs_.GetHL();
            cpu.bus_.HandleOAMCorruption(word, CorruptionType::Write);
            cpu.bus_.WriteByte(word, regs_.a, ComponentSource::CPU);
            cpu.bus_.HandleOAMCorruption(word, CorruptionType::Write);
            regs_.SetHL(word + 1);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool LoadFromAccumulatorIndirectC(CPUType &cpu) const {
        if (cpu.mCycleCounter() == 2) {
            const uint16_t c = 0xFF00 | static_cast<uint16_t>(regs_.c);
            cpu.bus_.WriteByte(c, regs_.a, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool LoadFromAccumulatorDirectA(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            word = static_cast<uint16_t>(cpu.bus_.ReadByte(cpu.pc(), ComponentSource::CPU));
            cpu.pc() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            cpu.bus_.WriteByte(static_cast<uint16_t>(0xFF00) | word, regs_.a, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool LoadAccumulatorA(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            word = 0xFF00 | static_cast<uint16_t>(byte);
            byte = cpu.bus_.ReadByte(word, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            regs_.a = byte;
            return true;
        }
        return false;
    }

    bool LoadAccumulatorIndirectC(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(0xFF00 | static_cast<uint16_t>(regs_.c), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.a = byte;
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<LoadWordTarget target>
    bool LD16Register(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            word = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            word |= static_cast<uint16_t>(cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU)) << 8;
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            if constexpr (target == LoadWordTarget::HL) regs_.SetHL(word);
            else if constexpr (target == LoadWordTarget::SP) cpu.sp() = word;
            else if constexpr (target == LoadWordTarget::BC) regs_.SetBC(word);
            else if constexpr (target == LoadWordTarget::DE) regs_.SetDE(word);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool LD16FromStack(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            word = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            word |= static_cast<uint16_t>(cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU)) << 8;
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            cpu.bus_.WriteByte(word, cpu.sp() & 0xFF, ComponentSource::CPU);
            word += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 5) {
            cpu.bus_.WriteByte(word, cpu.sp() >> 8, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 6) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool LD16StackAdjusted(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            word = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(
                cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU))));
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.SetCarry((cpu.sp() & 0xFF) + (word & 0xFF) > 0xFF);
            regs_.SetHalf((cpu.sp() & 0xF) + (word & 0xF) > 0xF);
            regs_.SetSubtract(false);
            regs_.SetZero(false);
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            regs_.SetHL(cpu.sp() + word);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool LD16Stack(CPUType &cpu) const {
        if (cpu.mCycleCounter() == 2) {
            cpu.sp() = regs_.GetHL();
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<StackTarget target>
    bool PUSH(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            cpu.bus_.HandleOAMCorruption(cpu.sp(), CorruptionType::Write);
            cpu.sp() -= 1;
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            cpu.bus_.HandleOAMCorruption(cpu.sp(), CorruptionType::Write);
            if constexpr (target == StackTarget::BC) word = regs_.GetBC();
            if constexpr (target == StackTarget::DE) word = regs_.GetDE();
            if constexpr (target == StackTarget::HL) word = regs_.GetHL();
            if constexpr (target == StackTarget::AF) word = regs_.GetAF();
            cpu.bus_.WriteByte(cpu.sp(), (word & 0xFF00) >> 8, ComponentSource::CPU);
            cpu.sp() -= 1;
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            cpu.bus_.HandleOAMCorruption(cpu.sp(), CorruptionType::Write);
            cpu.bus_.WriteByte(cpu.sp(), word & 0xFF, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 5) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<StackTarget target>
    bool POP(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            cpu.bus_.HandleOAMCorruption(cpu.sp(), CorruptionType::ReadWrite);
            word = cpu.bus_.ReadByte(cpu.sp(), ComponentSource::CPU);
            cpu.sp() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            cpu.bus_.HandleOAMCorruption(cpu.sp(), CorruptionType::Read);
            word |= static_cast<uint16_t>(cpu.bus_.ReadByte(cpu.sp(), ComponentSource::CPU)) << 8;
            cpu.sp() += 1;
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            if constexpr (target == StackTarget::BC) regs_.SetBC(word);
            if constexpr (target == StackTarget::DE) regs_.SetDE(word);
            if constexpr (target == StackTarget::HL) regs_.SetHL(word);
            if constexpr (target == StackTarget::AF) regs_.SetAF(word & 0xFFF0);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool CPRegister(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        const uint8_t value = regs_.*sourceReg;
        const uint8_t new_value = regs_.a - value;
        regs_.SetCarry(regs_.a < value);
        regs_.SetHalf((regs_.a & 0xF) < (value & 0xF));
        regs_.SetSubtract(true);
        regs_.SetZero(new_value == 0x0);
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool CPIndirect(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            const uint8_t new_value = regs_.a - byte;
            regs_.SetCarry(regs_.a < byte);
            regs_.SetHalf((regs_.a & 0xF) < (byte & 0xF));
            regs_.SetSubtract(true);
            regs_.SetZero(new_value == 0x0);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool CPImmediate(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            const uint8_t new_value = regs_.a - byte;
            regs_.SetCarry(regs_.a < byte);
            regs_.SetHalf((regs_.a & 0xF) < (byte & 0xF));
            regs_.SetSubtract(true);
            regs_.SetZero(new_value == 0x0);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool ORRegister(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        const uint8_t value = regs_.*sourceReg;
        regs_.a |= value;
        regs_.SetZero(regs_.a == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        regs_.SetCarry(false);
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool ORIndirect(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.a |= byte;
            regs_.SetZero(regs_.a == 0);
            regs_.SetSubtract(false);
            regs_.SetHalf(false);
            regs_.SetCarry(false);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool ORImmediate(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.a |= byte;
            regs_.SetZero(regs_.a == 0);
            regs_.SetSubtract(false);
            regs_.SetHalf(false);
            regs_.SetCarry(false);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool XORRegister(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        const uint8_t value = regs_.*sourceReg;
        regs_.a ^= value;
        regs_.SetZero(regs_.a == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        regs_.SetCarry(false);
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool XORIndirect(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.a ^= byte;
            regs_.SetZero(regs_.a == 0);
            regs_.SetSubtract(false);
            regs_.SetHalf(false);
            regs_.SetCarry(false);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool XORImmediate(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.a ^= byte;
            regs_.SetZero(regs_.a == 0);
            regs_.SetSubtract(false);
            regs_.SetHalf(false);
            regs_.SetCarry(false);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool AND(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        const uint8_t value = regs_.*sourceReg;
        regs_.a &= value;
        regs_.SetZero(regs_.a == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(true);
        regs_.SetCarry(false);
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool ANDIndirect(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.a &= byte;
            regs_.SetZero(regs_.a == 0);
            regs_.SetSubtract(false);
            regs_.SetHalf(true);
            regs_.SetCarry(false);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool ANDImmediate(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.a &= byte;
            regs_.SetZero(regs_.a == 0);
            regs_.SetSubtract(false);
            regs_.SetHalf(true);
            regs_.SetCarry(false);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool SUB(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        const uint8_t value = regs_.*sourceReg;
        const uint8_t new_value = regs_.a - value;
        regs_.SetCarry(regs_.a < value);
        regs_.SetHalf((regs_.a & 0xF) < (value & 0xF));
        regs_.SetSubtract(true);
        regs_.SetZero(new_value == 0x0);
        regs_.a = new_value;
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool SUBIndirect(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            const uint8_t new_value = regs_.a - byte;
            regs_.SetCarry(regs_.a < byte);
            regs_.SetHalf((regs_.a & 0xF) < (byte & 0xF));
            regs_.SetSubtract(true);
            regs_.SetZero(new_value == 0x0);
            regs_.a = new_value;
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool SUBImmediate(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            const uint8_t new_value = regs_.a - byte;
            regs_.SetCarry(regs_.a < byte);
            regs_.SetHalf((regs_.a & 0xF) < (byte & 0xF));
            regs_.SetSubtract(true);
            regs_.SetZero(new_value == 0x0);
            regs_.a = new_value;
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool RRC(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        const uint8_t value = regs_.*sourceReg;
        const bool carry = (value & 0x01) == 0x01;
        regs_.SetCarry(carry);
        const uint8_t newValue = regs_.FlagCarry() ? 0x80 | value >> 1 : value >> 1;
        regs_.SetZero(newValue == 0);
        regs_.*sourceReg = newValue;
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool RRCAddr(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.SetCarry(byte & 0x01);
            byte = regs_.FlagCarry() ? 0x80 | byte >> 1 : byte >> 1;
            cpu.bus_.WriteByte(regs_.GetHL(), byte, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            regs_.SetZero(byte == 0);
            regs_.SetSubtract(false);
            regs_.SetHalf(false);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool RR(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        const uint8_t value = regs_.*sourceReg;
        const bool carry = (value & 0x01) == 0x01;
        uint8_t newValue = regs_.FlagCarry() ? 0x80 | (value >> 1) : value >> 1;
        regs_.*sourceReg = newValue;
        regs_.SetCarry(carry);
        regs_.SetZero(newValue == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool RRAddr(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            word = (byte & 0x01) == 0x01; // hack for storing carry
            byte = regs_.FlagCarry() ? 0x80 | (byte >> 1) : byte >> 1;
            cpu.bus_.WriteByte(regs_.GetHL(), byte, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            regs_.SetCarry(word);
            regs_.SetZero(byte == 0);
            regs_.SetSubtract(false);
            regs_.SetHalf(false);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool SLA(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        const uint8_t value = regs_.*sourceReg;
        regs_.SetCarry(((value & 0x80) >> 7) == 0x01);
        const uint8_t newValue = value << 1;
        regs_.*sourceReg = newValue;
        regs_.SetZero(newValue == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool SLAAddr(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.SetCarry((byte & 0x80) != 0);
            byte <<= 1;
            cpu.bus_.WriteByte(regs_.GetHL(), byte, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            regs_.SetZero(byte == 0);
            regs_.SetSubtract(false);
            regs_.SetHalf(false);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool RLC(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        const uint8_t value = regs_.*sourceReg;
        const uint8_t old = value & 0x80 ? 1 : 0;
        regs_.SetCarry(old != 0);
        const uint8_t newValue = (value << 1) | old;
        regs_.SetZero(newValue == 0);
        regs_.*sourceReg = newValue;
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool RLCAddr(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            const uint8_t old = byte & 0x80 ? 1 : 0;
            regs_.SetCarry(old != 0);
            byte = byte << 1 | old;
            cpu.bus_.WriteByte(regs_.GetHL(), byte, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            regs_.SetZero(byte == 0);
            regs_.SetSubtract(false);
            regs_.SetHalf(false);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool RL(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        const uint8_t value = regs_.*sourceReg;
        const bool flag_c = (value & 0x80) >> 7 == 0x01;
        const uint8_t newValue = value << 1 | static_cast<uint8_t>(regs_.FlagCarry());
        regs_.SetCarry(flag_c);
        regs_.SetZero(newValue == 0);
        regs_.SetHalf(false);
        regs_.SetSubtract(false);
        regs_.*sourceReg = newValue;
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool RLAddr(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            const uint8_t oldCarry = regs_.FlagCarry() ? 1 : 0;
            regs_.SetCarry((byte & 0x80) != 0);
            byte = (byte << 1) | oldCarry;
            cpu.bus_.WriteByte(regs_.GetHL(), byte, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            regs_.SetZero(byte == 0);
            regs_.SetSubtract(false);
            regs_.SetHalf(false);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool SRA(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        const uint8_t value = regs_.*sourceReg;
        const uint8_t newValue = (value >> 1) | (value & 0x80);
        regs_.*sourceReg = newValue;
        regs_.SetCarry((value & 0x01) != 0);
        regs_.SetZero(newValue == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool SRAAddr(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.SetCarry((byte & 0x01) != 0);
            byte = (byte >> 1) | (byte & 0x80);
            cpu.bus_.WriteByte(regs_.GetHL(), byte, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            regs_.SetZero(byte == 0);
            regs_.SetSubtract(false);
            regs_.SetHalf(false);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool SWAP(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        uint8_t value = regs_.*sourceReg;
        value = (value >> 4) | (value << 4);
        regs_.*sourceReg = value;
        regs_.SetZero(value == 0);
        regs_.SetCarry(false);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool SWAPAddr(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            byte = (byte >> 4) | (byte << 4);
            cpu.bus_.WriteByte(regs_.GetHL(), byte, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            regs_.SetZero(byte == 0);
            regs_.SetCarry(false);
            regs_.SetSubtract(false);
            regs_.SetHalf(false);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool SRL(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        uint8_t value = regs_.*sourceReg;
        regs_.SetCarry((value & 0x01) != 0);
        value = value >> 1;
        regs_.*sourceReg = value;
        regs_.SetZero(value == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(false);
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool SRLAddr(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            regs_.SetCarry((byte & 0x01) != 0);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            byte = byte >> 1;
            cpu.bus_.WriteByte(regs_.GetHL(), byte, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            regs_.SetZero(byte == 0);
            regs_.SetSubtract(false);
            regs_.SetHalf(false);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    /* M3 -- prefixed, so only one */
    template<Register source, int bit>
    bool BIT(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        const uint8_t sourceValue = regs_.*sourceReg;
        regs_.SetZero((sourceValue & (1 << bit)) == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf(true);
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    /* M4 -- prefixed, M3 fetches byte */
    template<int bit>
    bool BITAddr(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            regs_.SetZero((byte & (1 << bit)) == 0);
            regs_.SetSubtract(false);
            regs_.SetHalf(true);
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source, int bit>
    bool RES(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        uint8_t sourceValue = regs_.*sourceReg;
        sourceValue &= ~(1 << bit);
        regs_.*sourceReg = sourceValue;
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    template<int bit>
    bool RESAddr(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            byte &= ~(1 << bit);
            cpu.bus_.WriteByte(regs_.GetHL(), byte, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source, int bit>
    bool SET(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        uint8_t sourceValue = regs_.*sourceReg;
        sourceValue |= 1 << bit;
        regs_.*sourceReg = sourceValue;
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    template<int bit>
    bool SETAddr(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            byte |= 1 << bit;
            cpu.bus_.WriteByte(regs_.GetHL(), byte, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool SBCRegister(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        const uint8_t value = regs_.*sourceReg;
        const uint8_t flag_carry = regs_.FlagCarry() ? 1 : 0;
        const uint8_t r = regs_.a - value - flag_carry;
        regs_.SetCarry(regs_.a < value + static_cast<uint16_t>(flag_carry));
        regs_.SetHalf((regs_.a & 0xF) < (value & 0xF) + flag_carry);
        regs_.SetSubtract(true);
        regs_.SetZero(r == 0x0);
        regs_.a = r;
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool SBCIndirect(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            const uint8_t flag_carry = regs_.FlagCarry() ? 1 : 0;
            const uint8_t r = regs_.a - byte - flag_carry;
            regs_.SetCarry(regs_.a < byte + static_cast<uint16_t>(flag_carry));
            regs_.SetHalf((regs_.a & 0xF) < (byte & 0xF) + flag_carry);
            regs_.SetSubtract(true);
            regs_.SetZero(r == 0x0);
            regs_.a = r;
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool SBCImmediate(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            const uint8_t flag_carry = regs_.FlagCarry() ? 1 : 0;
            const uint8_t r = regs_.a - byte - flag_carry;
            regs_.SetCarry(regs_.a < byte + static_cast<uint16_t>(flag_carry));
            regs_.SetHalf((regs_.a & 0xF) < (byte & 0xF) + flag_carry);
            regs_.SetSubtract(true);
            regs_.SetZero(r == 0x0);
            regs_.a = r;
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool ADCRegister(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        const uint8_t value = regs_.*sourceReg;
        const uint8_t flag_carry = regs_.FlagCarry() ? 1 : 0;
        const uint8_t r = regs_.a + value + flag_carry;
        regs_.SetCarry(static_cast<uint16_t>(regs_.a) + value + static_cast<uint16_t>(flag_carry) > 0xFF);
        regs_.SetHalf(((regs_.a & 0xF) + (value & 0xF) + (flag_carry & 0xF)) > 0xF);
        regs_.SetSubtract(false);
        regs_.SetZero(r == 0x0);
        regs_.a = r;
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool ADCIndirect(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            const uint8_t flag_carry = regs_.FlagCarry() ? 1 : 0;
            const uint8_t r = regs_.a + byte + flag_carry;
            regs_.SetCarry(static_cast<uint16_t>(regs_.a) + byte + static_cast<uint16_t>(flag_carry) > 0xFF);
            regs_.SetHalf(((regs_.a & 0xF) + (byte & 0xF) + (flag_carry & 0xF)) > 0xF);
            regs_.SetSubtract(false);
            regs_.SetZero(r == 0x0);
            regs_.a = r;
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool ADCImmediate(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            const uint8_t flag_carry = regs_.FlagCarry() ? 1 : 0;
            const uint8_t r = regs_.a + byte + flag_carry;
            regs_.SetCarry(static_cast<uint16_t>(regs_.a) + byte + static_cast<uint16_t>(flag_carry) > 0xFF);
            regs_.SetHalf(((regs_.a & 0xF) + (byte & 0xF) + (flag_carry & 0xF)) > 0xF);
            regs_.SetSubtract(false);
            regs_.SetZero(r == 0x0);
            regs_.a = r;
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Arithmetic16Target target>
    bool ADD16(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            if constexpr (target == Arithmetic16Target::BC) word = regs_.GetBC();
            if constexpr (target == Arithmetic16Target::DE) word = regs_.GetDE();
            if constexpr (target == Arithmetic16Target::HL) word = regs_.GetHL();
            if constexpr (target == Arithmetic16Target::SP) word = cpu.sp();
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            const uint16_t reg = regs_.GetHL();
            const uint16_t sum = reg + word;

            regs_.SetCarry(reg > 0xFFFF - word);
            regs_.SetSubtract(false);
            regs_.SetHalf((reg & 0x07FF) + (word & 0x07FF) > 0x07FF);
            regs_.SetHL(sum);

            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    template<Register source>
    bool ADDRegister(CPUType &cpu) {
        constexpr auto sourceReg = GetRegisterPtr<source>();
        const uint8_t value = regs_.*sourceReg;
        const uint8_t a = regs_.a;
        const uint8_t new_value = a + value;
        regs_.SetCarry(static_cast<uint16_t>(a) + static_cast<uint16_t>(value) > 0xFF);
        regs_.SetZero(new_value == 0);
        regs_.SetSubtract(false);
        regs_.SetHalf((regs_.a & 0xF) + (value & 0xF) > 0xF);
        regs_.a = new_value;
        cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
        return true;
    }

    bool ADDIndirect(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(regs_.GetHL(), ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            const uint8_t a = regs_.a;
            const uint8_t new_value = a + byte;
            regs_.SetCarry(static_cast<uint16_t>(a) + static_cast<uint16_t>(byte) > 0xFF);
            regs_.SetZero(new_value == 0);
            regs_.SetSubtract(false);
            regs_.SetHalf((regs_.a & 0xF) + (byte & 0xF) > 0xF);
            regs_.a = new_value;
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool ADDImmediate(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            byte = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            const uint8_t a = regs_.a;
            const uint8_t new_value = a + byte;
            regs_.SetCarry(static_cast<uint16_t>(a) + static_cast<uint16_t>(byte) > 0xFF);
            regs_.SetZero(new_value == 0);
            regs_.SetSubtract(false);
            regs_.SetHalf((regs_.a & 0xF) + (byte & 0xF) > 0xF);
            regs_.a = new_value;
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    bool ADDSigned(CPUType &cpu) {
        if (cpu.mCycleCounter() == 2) {
            word = static_cast<uint16_t>(static_cast<int16_t>(static_cast<int8_t>(
                cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU))));
            return false;
        }
        if (cpu.mCycleCounter() == 3) {
            word2 = cpu.sp();
            return false;
        }
        if (cpu.mCycleCounter() == 4) {
            regs_.SetCarry(((word2 & 0xFF) + (word & 0xFF)) > 0xFF);
            regs_.SetHalf(((word2 & 0xF) + (word & 0xF)) > 0xF);
            regs_.SetSubtract(false);
            regs_.SetZero(false);
            return false;
        }
        if (cpu.mCycleCounter() == 5) {
            cpu.sp() = word2 + word;
            cpu.nextInstruction() = cpu.bus_.ReadByte(cpu.pc()++, ComponentSource::CPU);
            return true;
        }
        return false;
    }

    static constexpr std::array<WrappedFunction, 256> prefixedTable = {{
        /* 0x00 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RLC<Register::B>(cpu); },
        /* 0x01 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RLC<Register::C>(cpu); },
        /* 0x02 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RLC<Register::D>(cpu); },
        /* 0x03 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RLC<Register::E>(cpu); },
        /* 0x04 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RLC<Register::H>(cpu); },
        /* 0x05 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RLC<Register::L>(cpu); },
        /* 0x06 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RLCAddr(cpu); },
        /* 0x07 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RLC<Register::A>(cpu); },
        /* 0x08 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RRC<Register::B>(cpu); },
        /* 0x09 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RRC<Register::C>(cpu); },
        /* 0x0A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RRC<Register::D>(cpu); },
        /* 0x0B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RRC<Register::E>(cpu); },
        /* 0x0C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RRC<Register::H>(cpu); },
        /* 0x0D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RRC<Register::L>(cpu); },
        /* 0x0E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RRCAddr(cpu); },
        /* 0x0F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RRC<Register::A>(cpu); },
        /* 0x10 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RL<Register::B>(cpu); },
        /* 0x11 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RL<Register::C>(cpu); },
        /* 0x12 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RL<Register::D>(cpu); },
        /* 0x13 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RL<Register::E>(cpu); },
        /* 0x14 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RL<Register::H>(cpu); },
        /* 0x15 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RL<Register::L>(cpu); },
        /* 0x16 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RLAddr(cpu); },
        /* 0x17 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RL<Register::A>(cpu); },
        /* 0x18 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RR<Register::B>(cpu); },
        /* 0x19 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RR<Register::C>(cpu); },
        /* 0x1A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RR<Register::D>(cpu); },
        /* 0x1B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RR<Register::E>(cpu); },
        /* 0x1C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RR<Register::H>(cpu); },
        /* 0x1D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RR<Register::L>(cpu); },
        /* 0x1E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RRAddr(cpu); },
        /* 0x1F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RR<Register::A>(cpu); },
        /* 0x20 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SLA<Register::B>(cpu); },
        /* 0x21 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SLA<Register::C>(cpu); },
        /* 0x22 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SLA<Register::D>(cpu); },
        /* 0x23 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SLA<Register::E>(cpu); },
        /* 0x24 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SLA<Register::H>(cpu); },
        /* 0x25 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SLA<Register::L>(cpu); },
        /* 0x26 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SLAAddr(cpu); },
        /* 0x27 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SLA<Register::A>(cpu); },
        /* 0x28 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SRA<Register::B>(cpu); },
        /* 0x29 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SRA<Register::C>(cpu); },
        /* 0x2A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SRA<Register::D>(cpu); },
        /* 0x2B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SRA<Register::E>(cpu); },
        /* 0x2C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SRA<Register::H>(cpu); },
        /* 0x2D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SRA<Register::L>(cpu); },
        /* 0x2E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SRAAddr(cpu); },
        /* 0x2F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SRA<Register::A>(cpu); },
        /* 0x30 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SWAP<Register::B>(cpu); },
        /* 0x31 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SWAP<Register::C>(cpu); },
        /* 0x32 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SWAP<Register::D>(cpu); },
        /* 0x33 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SWAP<Register::E>(cpu); },
        /* 0x34 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SWAP<Register::H>(cpu); },
        /* 0x35 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SWAP<Register::L>(cpu); },
        /* 0x36 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SWAPAddr(cpu); },
        /* 0x37 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SWAP<Register::A>(cpu); },
        /* 0x38 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SRL<Register::B>(cpu); },
        /* 0x39 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SRL<Register::C>(cpu); },
        /* 0x3A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SRL<Register::D>(cpu); },
        /* 0x3B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SRL<Register::E>(cpu); },
        /* 0x3C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SRL<Register::H>(cpu); },
        /* 0x3D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SRL<Register::L>(cpu); },
        /* 0x3E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SRLAddr(cpu); },
        /* 0x3F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SRL<Register::A>(cpu); },
        /* 0x40 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::B, 0>(cpu); },
        /* 0x41 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::C, 0>(cpu); },
        /* 0x42 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::D, 0>(cpu); },
        /* 0x43 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::E, 0>(cpu); },
        /* 0x44 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::H, 0>(cpu); },
        /* 0x45 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::L, 0>(cpu); },
        /* 0x46 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BITAddr<0>(cpu); },
        /* 0x47 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::A, 0>(cpu); },
        /* 0x48 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::B, 1>(cpu); },
        /* 0x49 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::C, 1>(cpu); },
        /* 0x4A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::D, 1>(cpu); },
        /* 0x4B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::E, 1>(cpu); },
        /* 0x4C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::H, 1>(cpu); },
        /* 0x4D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::L, 1>(cpu); },
        /* 0x4E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BITAddr<1>(cpu); },
        /* 0x4F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::A, 1>(cpu); },
        /* 0x50 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::B, 2>(cpu); },
        /* 0x51 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::C, 2>(cpu); },
        /* 0x52 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::D, 2>(cpu); },
        /* 0x53 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::E, 2>(cpu); },
        /* 0x54 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::H, 2>(cpu); },
        /* 0x55 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::L, 2>(cpu); },
        /* 0x56 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BITAddr<2>(cpu); },
        /* 0x57 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::A, 2>(cpu); },
        /* 0x58 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::B, 3>(cpu); },
        /* 0x59 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::C, 3>(cpu); },
        /* 0x5A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::D, 3>(cpu); },
        /* 0x5B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::E, 3>(cpu); },
        /* 0x5C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::H, 3>(cpu); },
        /* 0x5D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::L, 3>(cpu); },
        /* 0x5E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BITAddr<3>(cpu); },
        /* 0x5F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::A, 3>(cpu); },
        /* 0x60 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::B, 4>(cpu); },
        /* 0x61 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::C, 4>(cpu); },
        /* 0x62 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::D, 4>(cpu); },
        /* 0x63 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::E, 4>(cpu); },
        /* 0x64 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::H, 4>(cpu); },
        /* 0x65 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::L, 4>(cpu); },
        /* 0x66 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BITAddr<4>(cpu); },
        /* 0x67 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::A, 4>(cpu); },
        /* 0x68 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::B, 5>(cpu); },
        /* 0x69 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::C, 5>(cpu); },
        /* 0x6A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::D, 5>(cpu); },
        /* 0x6B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::E, 5>(cpu); },
        /* 0x6C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::H, 5>(cpu); },
        /* 0x6D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::L, 5>(cpu); },
        /* 0x6E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BITAddr<5>(cpu); },
        /* 0x6F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::A, 5>(cpu); },
        /* 0x70 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::B, 6>(cpu); },
        /* 0x71 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::C, 6>(cpu); },
        /* 0x72 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::D, 6>(cpu); },
        /* 0x73 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::E, 6>(cpu); },
        /* 0x74 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::H, 6>(cpu); },
        /* 0x75 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::L, 6>(cpu); },
        /* 0x76 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BITAddr<6>(cpu); },
        /* 0x77 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::A, 6>(cpu); },
        /* 0x78 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::B, 7>(cpu); },
        /* 0x79 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::C, 7>(cpu); },
        /* 0x7A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::D, 7>(cpu); },
        /* 0x7B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::E, 7>(cpu); },
        /* 0x7C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::H, 7>(cpu); },
        /* 0x7D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::L, 7>(cpu); },
        /* 0x7E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BITAddr<7>(cpu); },
        /* 0x7F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.BIT<Register::A, 7>(cpu); },
        /* 0x80 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::B, 0>(cpu); },
        /* 0x81 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::C, 0>(cpu); },
        /* 0x82 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::D, 0>(cpu); },
        /* 0x83 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::E, 0>(cpu); },
        /* 0x84 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::H, 0>(cpu); },
        /* 0x85 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::L, 0>(cpu); },
        /* 0x86 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RESAddr<0>(cpu); },
        /* 0x87 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::A, 0>(cpu); },
        /* 0x88 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::B, 1>(cpu); },
        /* 0x89 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::C, 1>(cpu); },
        /* 0x8A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::D, 1>(cpu); },
        /* 0x8B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::E, 1>(cpu); },
        /* 0x8C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::H, 1>(cpu); },
        /* 0x8D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::L, 1>(cpu); },
        /* 0x8E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RESAddr<1>(cpu); },
        /* 0x8F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::A, 1>(cpu); },
        /* 0x90 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::B, 2>(cpu); },
        /* 0x91 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::C, 2>(cpu); },
        /* 0x92 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::D, 2>(cpu); },
        /* 0x93 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::E, 2>(cpu); },
        /* 0x94 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::H, 2>(cpu); },
        /* 0x95 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::L, 2>(cpu); },
        /* 0x96 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RESAddr<2>(cpu); },
        /* 0x97 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::A, 2>(cpu); },
        /* 0x98 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::B, 3>(cpu); },
        /* 0x99 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::C, 3>(cpu); },
        /* 0x9A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::D, 3>(cpu); },
        /* 0x9B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::E, 3>(cpu); },
        /* 0x9C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::H, 3>(cpu); },
        /* 0x9D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::L, 3>(cpu); },
        /* 0x9E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RESAddr<3>(cpu); },
        /* 0x9F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::A, 3>(cpu); },
        /* 0xA0 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::B, 4>(cpu); },
        /* 0xA1 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::C, 4>(cpu); },
        /* 0xA2 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::D, 4>(cpu); },
        /* 0xA3 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::E, 4>(cpu); },
        /* 0xA4 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::H, 4>(cpu); },
        /* 0xA5 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::L, 4>(cpu); },
        /* 0xA6 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RESAddr<4>(cpu); },
        /* 0xA7 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::A, 4>(cpu); },
        /* 0xA8 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::B, 5>(cpu); },
        /* 0xA9 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::C, 5>(cpu); },
        /* 0xAA */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::D, 5>(cpu); },
        /* 0xAB */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::E, 5>(cpu); },
        /* 0xAC */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::H, 5>(cpu); },
        /* 0xAD */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::L, 5>(cpu); },
        /* 0xAE */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RESAddr<5>(cpu); },
        /* 0xAF */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::A, 5>(cpu); },
        /* 0xB0 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::B, 6>(cpu); },
        /* 0xB1 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::C, 6>(cpu); },
        /* 0xB2 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::D, 6>(cpu); },
        /* 0xB3 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::E, 6>(cpu); },
        /* 0xB4 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::H, 6>(cpu); },
        /* 0xB5 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::L, 6>(cpu); },
        /* 0xB6 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RESAddr<6>(cpu); },
        /* 0xB7 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::A, 6>(cpu); },
        /* 0xB8 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::B, 7>(cpu); },
        /* 0xB9 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::C, 7>(cpu); },
        /* 0xBA */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::D, 7>(cpu); },
        /* 0xBB */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::E, 7>(cpu); },
        /* 0xBC */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::H, 7>(cpu); },
        /* 0xBD */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::L, 7>(cpu); },
        /* 0xBE */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RESAddr<7>(cpu); },
        /* 0xBF */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RES<Register::A, 7>(cpu); },
        /* 0xC0 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::B, 0>(cpu); },
        /* 0xC1 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::C, 0>(cpu); },
        /* 0xC2 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::D, 0>(cpu); },
        /* 0xC3 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::E, 0>(cpu); },
        /* 0xC4 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::H, 0>(cpu); },
        /* 0xC5 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::L, 0>(cpu); },
        /* 0xC6 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SETAddr<0>(cpu); },
        /* 0xC7 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::A, 0>(cpu); },
        /* 0xC8 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::B, 1>(cpu); },
        /* 0xC9 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::C, 1>(cpu); },
        /* 0xCA */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::D, 1>(cpu); },
        /* 0xCB */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::E, 1>(cpu); },
        /* 0xCC */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::H, 1>(cpu); },
        /* 0xCD */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::L, 1>(cpu); },
        /* 0xCE */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SETAddr<1>(cpu); },
        /* 0xCF */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::A, 1>(cpu); },
        /* 0xD0 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::B, 2>(cpu); },
        /* 0xD1 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::C, 2>(cpu); },
        /* 0xD2 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::D, 2>(cpu); },
        /* 0xD3 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::E, 2>(cpu); },
        /* 0xD4 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::H, 2>(cpu); },
        /* 0xD5 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::L, 2>(cpu); },
        /* 0xD6 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SETAddr<2>(cpu); },
        /* 0xD7 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::A, 2>(cpu); },
        /* 0xD8 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::B, 3>(cpu); },
        /* 0xD9 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::C, 3>(cpu); },
        /* 0xDA */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::D, 3>(cpu); },
        /* 0xDB */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::E, 3>(cpu); },
        /* 0xDC */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::H, 3>(cpu); },
        /* 0xDD */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::L, 3>(cpu); },
        /* 0xDE */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SETAddr<3>(cpu); },
        /* 0xDF */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::A, 3>(cpu); },
        /* 0xE0 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::B, 4>(cpu); },
        /* 0xE1 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::C, 4>(cpu); },
        /* 0xE2 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::D, 4>(cpu); },
        /* 0xE3 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::E, 4>(cpu); },
        /* 0xE4 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::H, 4>(cpu); },
        /* 0xE5 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::L, 4>(cpu); },
        /* 0xE6 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SETAddr<4>(cpu); },
        /* 0xE7 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::A, 4>(cpu); },
        /* 0xE8 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::B, 5>(cpu); },
        /* 0xE9 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::C, 5>(cpu); },
        /* 0xEA */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::D, 5>(cpu); },
        /* 0xEB */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::E, 5>(cpu); },
        /* 0xEC */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::H, 5>(cpu); },
        /* 0xED */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::L, 5>(cpu); },
        /* 0xEE */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SETAddr<5>(cpu); },
        /* 0xEF */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::A, 5>(cpu); },
        /* 0xF0 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::B, 6>(cpu); },
        /* 0xF1 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::C, 6>(cpu); },
        /* 0xF2 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::D, 6>(cpu); },
        /* 0xF3 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::E, 6>(cpu); },
        /* 0xF4 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::H, 6>(cpu); },
        /* 0xF5 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::L, 6>(cpu); },
        /* 0xF6 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SETAddr<6>(cpu); },
        /* 0xF7 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::A, 6>(cpu); },
        /* 0xF8 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::B, 7>(cpu); },
        /* 0xF9 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::C, 7>(cpu); },
        /* 0xFA */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::D, 7>(cpu); },
        /* 0xFB */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::E, 7>(cpu); },
        /* 0xFC */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::H, 7>(cpu); },
        /* 0xFD */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::L, 7>(cpu); },
        /* 0xFE */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SETAddr<7>(cpu); },
        /* 0xFF */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SET<Register::A, 7>(cpu); },
    }};

    static constexpr std::array<WrappedFunction, 256> nonPrefixedTable = {{
        /* 0x00 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.NOP(cpu); },
        /* 0x01 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LD16Register<LoadWordTarget::BC>(cpu); },
        /* 0x02 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDFromAccBC(cpu); },
        /* 0x03 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.INC16<Arithmetic16Target::BC>(cpu); },
        /* 0x04 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.INCRegister<Register::B>(cpu); },
        /* 0x05 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.DECRegister<Register::B>(cpu); },
        /* 0x06 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegisterImmediate<Register::B>(cpu); },
        /* 0x07 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RLCA(cpu); },
        /* 0x08 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LD16FromStack(cpu); },
        /* 0x09 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADD16<Arithmetic16Target::BC>(cpu); },
        /* 0x0A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDAccumulatorBC(cpu); },
        /* 0x0B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.DEC16<Arithmetic16Target::BC>(cpu); },
        /* 0x0C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.INCRegister<Register::C>(cpu); },
        /* 0x0D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.DECRegister<Register::C>(cpu); },
        /* 0x0E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegisterImmediate<Register::C>(cpu); },
        /* 0x0F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RRCA(cpu); },
        /* 0x10 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.STOP(cpu); },
        /* 0x11 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LD16Register<LoadWordTarget::DE>(cpu); },
        /* 0x12 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDFromAccDE(cpu); },
        /* 0x13 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.INC16<Arithmetic16Target::DE>(cpu); },
        /* 0x14 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.INCRegister<Register::D>(cpu); },
        /* 0x15 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.DECRegister<Register::D>(cpu); },
        /* 0x16 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegisterImmediate<Register::D>(cpu); },
        /* 0x17 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RLA(cpu); },
        /* 0x18 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.JRUnconditional(cpu); },
        /* 0x19 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADD16<Arithmetic16Target::DE>(cpu); },
        /* 0x1A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDAccumulatorDE(cpu); },
        /* 0x1B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.DEC16<Arithmetic16Target::DE>(cpu); },
        /* 0x1C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.INCRegister<Register::E>(cpu); },
        /* 0x1D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.DECRegister<Register::E>(cpu); },
        /* 0x1E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegisterImmediate<Register::E>(cpu); },
        /* 0x1F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RRA(cpu); },
        /* 0x20 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.JR<JumpTest::NotZero>(cpu); },
        /* 0x21 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LD16Register<LoadWordTarget::HL>(cpu); },
        /* 0x22 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDFromAccumulatorIndirectInc(cpu); },
        /* 0x23 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.INC16<Arithmetic16Target::HL>(cpu); },
        /* 0x24 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.INCRegister<Register::H>(cpu); },
        /* 0x25 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.DECRegister<Register::H>(cpu); },
        /* 0x26 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegisterImmediate<Register::H>(cpu); },
        /* 0x27 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.DAA(cpu); },
        /* 0x28 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.JR<JumpTest::Zero>(cpu); },
        /* 0x29 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADD16<Arithmetic16Target::HL>(cpu); },
        /* 0x2A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDAccumulatorIndirectInc(cpu); },
        /* 0x2B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.DEC16<Arithmetic16Target::HL>(cpu); },
        /* 0x2C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.INCRegister<Register::L>(cpu); },
        /* 0x2D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.DECRegister<Register::L>(cpu); },
        /* 0x2E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegisterImmediate<Register::L>(cpu); },
        /* 0x2F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.CPL(cpu); },
        /* 0x30 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.JR<JumpTest::NotCarry>(cpu); },
        /* 0x31 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LD16Register<LoadWordTarget::SP>(cpu); },
        /* 0x32 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDFromAccumulatorIndirectDec(cpu); },
        /* 0x33 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.INC16<Arithmetic16Target::SP>(cpu); },
        /* 0x34 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.INCIndirect(cpu); },
        /* 0x35 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.DECIndirect(cpu); },
        /* 0x36 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDAddrImmediate(cpu); },
        /* 0x37 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SCF(cpu); },
        /* 0x38 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.JR<JumpTest::Carry>(cpu); },
        /* 0x39 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADD16<Arithmetic16Target::SP>(cpu); },
        /* 0x3A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDAccumulatorIndirectDec(cpu); },
        /* 0x3B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.DEC16<Arithmetic16Target::SP>(cpu); },
        /* 0x3C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.INCRegister<Register::A>(cpu); },
        /* 0x3D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.DECRegister<Register::A>(cpu); },
        /* 0x3E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegisterImmediate<Register::A>(cpu); },
        /* 0x3F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.CCF(cpu); },
        /* 0x40 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::B, Register::B>(cpu); },
        /* 0x41 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::B, Register::C>(cpu); },
        /* 0x42 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::B, Register::D>(cpu); },
        /* 0x43 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::B, Register::E>(cpu); },
        /* 0x44 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::B, Register::H>(cpu); },
        /* 0x45 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::B, Register::L>(cpu); },
        /* 0x46 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegisterIndirect<Register::B>(cpu); },
        /* 0x47 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::B, Register::A>(cpu); },
        /* 0x48 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::C, Register::B>(cpu); },
        /* 0x49 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::C, Register::C>(cpu); },
        /* 0x4A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::C, Register::D>(cpu); },
        /* 0x4B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::C, Register::E>(cpu); },
        /* 0x4C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::C, Register::H>(cpu); },
        /* 0x4D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::C, Register::L>(cpu); },
        /* 0x4E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegisterIndirect<Register::C>(cpu); },
        /* 0x4F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::C, Register::A>(cpu); },
        /* 0x50 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::D, Register::B>(cpu); },
        /* 0x51 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::D, Register::C>(cpu); },
        /* 0x52 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::D, Register::D>(cpu); },
        /* 0x53 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::D, Register::E>(cpu); },
        /* 0x54 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::D, Register::H>(cpu); },
        /* 0x55 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::D, Register::L>(cpu); },
        /* 0x56 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegisterIndirect<Register::D>(cpu); },
        /* 0x57 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::D, Register::A>(cpu); },
        /* 0x58 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::E, Register::B>(cpu); },
        /* 0x59 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::E, Register::C>(cpu); },
        /* 0x5A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::E, Register::D>(cpu); },
        /* 0x5B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::E, Register::E>(cpu); },
        /* 0x5C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::E, Register::H>(cpu); },
        /* 0x5D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::E, Register::L>(cpu); },
        /* 0x5E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegisterIndirect<Register::E>(cpu); },
        /* 0x5F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::E, Register::A>(cpu); },
        /* 0x60 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::H, Register::B>(cpu); },
        /* 0x61 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::H, Register::C>(cpu); },
        /* 0x62 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::H, Register::D>(cpu); },
        /* 0x63 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::H, Register::E>(cpu); },
        /* 0x64 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::H, Register::H>(cpu); },
        /* 0x65 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::H, Register::L>(cpu); },
        /* 0x66 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegisterIndirect<Register::H>(cpu); },
        /* 0x67 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::H, Register::A>(cpu); },
        /* 0x68 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::L, Register::B>(cpu); },
        /* 0x69 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::L, Register::C>(cpu); },
        /* 0x6A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::L, Register::D>(cpu); },
        /* 0x6B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::L, Register::E>(cpu); },
        /* 0x6C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::L, Register::H>(cpu); },
        /* 0x6D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::L, Register::L>(cpu); },
        /* 0x6E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegisterIndirect<Register::L>(cpu); },
        /* 0x6F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::L, Register::A>(cpu); },
        /* 0x70 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDAddrRegister<Register::B>(cpu); },
        /* 0x71 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDAddrRegister<Register::C>(cpu); },
        /* 0x72 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDAddrRegister<Register::D>(cpu); },
        /* 0x73 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDAddrRegister<Register::E>(cpu); },
        /* 0x74 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDAddrRegister<Register::H>(cpu); },
        /* 0x75 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDAddrRegister<Register::L>(cpu); },
        /* 0x76 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.HALT(cpu); },
        /* 0x77 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDAddrRegister<Register::A>(cpu); },
        /* 0x78 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::A, Register::B>(cpu); },
        /* 0x79 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::A, Register::C>(cpu); },
        /* 0x7A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::A, Register::D>(cpu); },
        /* 0x7B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::A, Register::E>(cpu); },
        /* 0x7C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::A, Register::H>(cpu); },
        /* 0x7D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::A, Register::L>(cpu); },
        /* 0x7E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegisterIndirect<Register::A>(cpu); },
        /* 0x7F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDRegister<Register::A, Register::A>(cpu); },
        /* 0x80 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADDRegister<Register::B>(cpu); },
        /* 0x81 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADDRegister<Register::C>(cpu); },
        /* 0x82 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADDRegister<Register::D>(cpu); },
        /* 0x83 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADDRegister<Register::E>(cpu); },
        /* 0x84 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADDRegister<Register::H>(cpu); },
        /* 0x85 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADDRegister<Register::L>(cpu); },
        /* 0x86 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADDIndirect(cpu); },
        /* 0x87 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADDRegister<Register::A>(cpu); },
        /* 0x88 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADCRegister<Register::B>(cpu); },
        /* 0x89 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADCRegister<Register::C>(cpu); },
        /* 0x8A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADCRegister<Register::D>(cpu); },
        /* 0x8B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADCRegister<Register::E>(cpu); },
        /* 0x8C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADCRegister<Register::H>(cpu); },
        /* 0x8D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADCRegister<Register::L>(cpu); },
        /* 0x8E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADCIndirect(cpu); },
        /* 0x8F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADCRegister<Register::A>(cpu); },
        /* 0x90 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SUB<Register::B>(cpu); },
        /* 0x91 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SUB<Register::C>(cpu); },
        /* 0x92 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SUB<Register::D>(cpu); },
        /* 0x93 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SUB<Register::E>(cpu); },
        /* 0x94 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SUB<Register::H>(cpu); },
        /* 0x95 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SUB<Register::L>(cpu); },
        /* 0x96 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SUBIndirect(cpu); },
        /* 0x97 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SUB<Register::A>(cpu); },
        /* 0x98 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SBCRegister<Register::B>(cpu); },
        /* 0x99 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SBCRegister<Register::C>(cpu); },
        /* 0x9A */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SBCRegister<Register::D>(cpu); },
        /* 0x9B */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SBCRegister<Register::E>(cpu); },
        /* 0x9C */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SBCRegister<Register::H>(cpu); },
        /* 0x9D */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SBCRegister<Register::L>(cpu); },
        /* 0x9E */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SBCIndirect(cpu); },
        /* 0x9F */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SBCRegister<Register::A>(cpu); },
        /* 0xA0 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.AND<Register::B>(cpu); },
        /* 0xA1 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.AND<Register::C>(cpu); },
        /* 0xA2 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.AND<Register::D>(cpu); },
        /* 0xA3 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.AND<Register::E>(cpu); },
        /* 0xA4 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.AND<Register::H>(cpu); },
        /* 0xA5 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.AND<Register::L>(cpu); },
        /* 0xA6 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ANDIndirect(cpu); },
        /* 0xA7 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.AND<Register::A>(cpu); },
        /* 0xA8 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.XORRegister<Register::B>(cpu); },
        /* 0xA9 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.XORRegister<Register::C>(cpu); },
        /* 0xAA */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.XORRegister<Register::D>(cpu); },
        /* 0xAB */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.XORRegister<Register::E>(cpu); },
        /* 0xAC */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.XORRegister<Register::H>(cpu); },
        /* 0xAD */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.XORRegister<Register::L>(cpu); },
        /* 0xAE */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.XORIndirect(cpu); },
        /* 0xAF */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.XORRegister<Register::A>(cpu); },
        /* 0xB0 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ORRegister<Register::B>(cpu); },
        /* 0xB1 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ORRegister<Register::C>(cpu); },
        /* 0xB2 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ORRegister<Register::D>(cpu); },
        /* 0xB3 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ORRegister<Register::E>(cpu); },
        /* 0xB4 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ORRegister<Register::H>(cpu); },
        /* 0xB5 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ORRegister<Register::L>(cpu); },
        /* 0xB6 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ORIndirect(cpu); },
        /* 0xB7 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ORRegister<Register::A>(cpu); },
        /* 0xB8 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.CPRegister<Register::B>(cpu); },
        /* 0xB9 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.CPRegister<Register::C>(cpu); },
        /* 0xBA */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.CPRegister<Register::D>(cpu); },
        /* 0xBB */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.CPRegister<Register::E>(cpu); },
        /* 0xBC */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.CPRegister<Register::H>(cpu); },
        /* 0xBD */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.CPRegister<Register::L>(cpu); },
        /* 0xBE */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.CPIndirect(cpu); },
        /* 0xBF */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.CPRegister<Register::A>(cpu); },
        /* 0xC0 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RETConditional<JumpTest::NotZero>(cpu); },
        /* 0xC1 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.POP<StackTarget::BC>(cpu); },
        /* 0xC2 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.JP<JumpTest::NotZero>(cpu); },
        /* 0xC3 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.JPUnconditional(cpu); },
        /* 0xC4 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.CALL<JumpTest::NotZero>(cpu); },
        /* 0xC5 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.PUSH<StackTarget::BC>(cpu); },
        /* 0xC6 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADDImmediate(cpu); },
        /* 0xC7 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RST<RSTTarget::H00>(cpu); },
        /* 0xC8 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RETConditional<JumpTest::Zero>(cpu); },
        /* 0xC9 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RETUnconditional(cpu); },
        /* 0xCA */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.JP<JumpTest::Zero>(cpu); },
        /* 0xCB */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.PREFIX(cpu); },
        /* 0xCC */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.CALL<JumpTest::Zero>(cpu); },
        /* 0xCD */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.CALLUnconditional(cpu); },
        /* 0xCE */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADCImmediate(cpu); },
        /* 0xCF */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RST<RSTTarget::H08>(cpu); },
        /* 0xD0 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RETConditional<JumpTest::NotCarry>(cpu); },
        /* 0xD1 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.POP<StackTarget::DE>(cpu); },
        /* 0xD2 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.JP<JumpTest::NotCarry>(cpu); },
        /* 0xD3 */ &Instructions::IllegalOpcode,
        /* 0xD4 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.CALL<JumpTest::NotCarry>(cpu); },
        /* 0xD5 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.PUSH<StackTarget::DE>(cpu); },
        /* 0xD6 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SUBImmediate(cpu); },
        /* 0xD7 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RST<RSTTarget::H10>(cpu); },
        /* 0xD8 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RETConditional<JumpTest::Carry>(cpu); },
        /* 0xD9 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RETI(cpu); },
        /* 0xDA */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.JP<JumpTest::Carry>(cpu); },
        /* 0xDB */ &Instructions::IllegalOpcode,
        /* 0xDC */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.CALL<JumpTest::Carry>(cpu); },
        /* 0xDD */ &Instructions::IllegalOpcode,
        /* 0xDE */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.SBCImmediate(cpu); },
        /* 0xDF */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RST<RSTTarget::H18>(cpu); },
        /* 0xE0 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LoadFromAccumulatorDirectA(cpu); },
        /* 0xE1 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.POP<StackTarget::HL>(cpu); },
        /* 0xE2 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LoadFromAccumulatorIndirectC(cpu); },
        /* 0xE3 */ &Instructions::IllegalOpcode,
        /* 0xE4 */ &Instructions::IllegalOpcode,
        /* 0xE5 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.PUSH<StackTarget::HL>(cpu); },
        /* 0xE6 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ANDImmediate(cpu); },
        /* 0xE7 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RST<RSTTarget::H20>(cpu); },
        /* 0xE8 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ADDSigned(cpu); },
        /* 0xE9 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.JPHL(cpu); },
        /* 0xEA */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDFromAccumulatorDirect(cpu); },
        /* 0xEB */ &Instructions::IllegalOpcode,
        /* 0xEC */ &Instructions::IllegalOpcode,
        /* 0xED */ &Instructions::IllegalOpcode,
        /* 0xEE */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.XORImmediate(cpu); },
        /* 0xEF */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RST<RSTTarget::H28>(cpu); },
        /* 0xF0 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LoadAccumulatorA(cpu); },
        /* 0xF1 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.POP<StackTarget::AF>(cpu); },
        /* 0xF2 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LoadAccumulatorIndirectC(cpu); },
        /* 0xF3 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.DI(cpu); },
        /* 0xF4 */ &Instructions::IllegalOpcode,
        /* 0xF5 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.PUSH<StackTarget::AF>(cpu); },
        /* 0xF6 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.ORImmediate(cpu); },
        /* 0xF7 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RST<RSTTarget::H30>(cpu); },
        /* 0xF8 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LD16StackAdjusted(cpu); },
        /* 0xF9 */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LD16Stack(cpu); },
        /* 0xFA */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.LDAccumulatorDirect(cpu); },
        /* 0xFB */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.EI(cpu); },
        /* 0xFC */ &Instructions::IllegalOpcode,
        /* 0xFD */ &Instructions::IllegalOpcode,
        /* 0xFE */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.CPImmediate(cpu); },
        /* 0xFF */ [](Instructions &instr, CPUType &cpu) -> bool { return instr.RST<RSTTarget::H38>(cpu); },
    }};

    static inline const std::array<std::string, 256> prefixedInstructions = {
        "RLC B", "RLC C", "RLC D", "RLC E", "RLC H", "RLC L", "RLC (HL)", "RLC A", "RRC B", "RRC C", "RRC D", "RRC E",
        "RRC H", "RRC L", "RRC (HL)", "RRC A", "RL B", "RL C", "RL D", "RL E", "RL H", "RL L", "RL (HL)", "RL A",
        "RR B", "RR C", "RR D", "RR E", "RR H", "RR L", "RR (HL)", "RR A", "SLA B", "SLA C", "SLA D", "SLA E", "SLA H",
        "SLA L", "SLA (HL)", "SLA A", "SRA B", "SRA C", "SRA D", "SRA E", "SRA H", "SRA L", "SRA (HL)", "SRA A",
        "SWAP B", "SWAP C", "SWAP D", "SWAP E", "SWAP H", "SWAP L", "SWAP (HL)", "SWAP A", "SRL B", "SRL C", "SRL D",
        "SRL E", "SRL H", "SRL L", "SRL (HL)", "SRL A", "BIT 0,B", "BIT 0,C", "BIT 0,D", "BIT 0,E", "BIT 0,H",
        "BIT 0,L", "BIT 0,(HL)", "BIT 0,A", "BIT 1,B", "BIT 1,C", "BIT 1,D", "BIT 1,E", "BIT 1,H", "BIT 1,L",
        "BIT 1,(HL)", "BIT 1,A", "BIT 2,B", "BIT 2,C", "BIT 2,D", "BIT 2,E", "BIT 2,H", "BIT 2,L", "BIT 2,(HL)",
        "BIT 2,A", "BIT 3,B", "BIT 3,C", "BIT 3,D", "BIT 3,E", "BIT 3,H", "BIT 3,L", "BIT 3,(HL)", "BIT 3,A", "BIT 4,B",
        "BIT 4,C", "BIT 4,D", "BIT 4,E", "BIT 4,H", "BIT 4,L", "BIT 4,(HL)", "BIT 4,A", "BIT 5,B", "BIT 5,C", "BIT 5,D",
        "BIT 5,E", "BIT 5,H", "BIT 5,L", "BIT 5,(HL)", "BIT 5,A", "BIT 6,B", "BIT 6,C", "BIT 6,D", "BIT 6,E", "BIT 6,H",
        "BIT 6,L", "BIT 6,(HL)", "BIT 6,A", "BIT 7,B", "BIT 7,C", "BIT 7,D", "BIT 7,E", "BIT 7,H", "BIT 7,L",
        "BIT 7,(HL)", "BIT 7,A", "RES 0,B", "RES 0,C", "RES 0,D", "RES 0,E", "RES 0,H", "RES 0,L", "RES 0,(HL)",
        "RES 0,A", "RES 1,B", "RES 1,C", "RES 1,D", "RES 1,E", "RES 1,H", "RES 1,L", "RES 1,(HL)", "RES 1,A", "RES 2,B",
        "RES 2,C", "RES 2,D", "RES 2,E", "RES 2,H", "RES 2,L", "RES 2,(HL)", "RES 2,A", "RES 3,B", "RES 3,C", "RES 3,D",
        "RES 3,E", "RES 3,H", "RES 3,L", "RES 3,(HL)", "RES 3,A", "RES 4,B", "RES 4,C", "RES 4,D", "RES 4,E", "RES 4,H",
        "RES 4,L", "RES 4,(HL)", "RES 4,A", "RES 5,B", "RES 5,C", "RES 5,D", "RES 5,E", "RES 5,H", "RES 5,L",
        "RES 5,(HL)", "RES 5,A", "RES 6,B", "RES 6,C", "RES 6,D", "RES 6,E", "RES 6,H", "RES 6,L", "RES 6,(HL)",
        "RES 6,A", "RES 7,B", "RES 7,C", "RES 7,D", "RES 7,E", "RES 7,H", "RES 7,L", "RES 7,(HL)", "RES 7,A", "SET 0,B",
        "SET 0,C", "SET 0,D", "SET 0,E", "SET 0,H", "SET 0,L", "SET 0,(HL)", "SET 0,A", "SET 1,B", "SET 1,C", "SET 1,D",
        "SET 1,E", "SET 1,H", "SET 1,L", "SET 1,(HL)", "SET 1,A", "SET 2,B", "SET 2,C", "SET 2,D", "SET 2,E", "SET 2,H",
        "SET 2,L", "SET 2,(HL)", "SET 2,A", "SET 3,B", "SET 3,C", "SET 3,D", "SET 3,E", "SET 3,H", "SET 3,L",
        "SET 3,(HL)", "SET 3,A", "SET 4,B", "SET 4,C", "SET 4,D", "SET 4,E", "SET 4,H", "SET 4,L", "SET 4,(HL)",
        "SET 4,A", "SET 5,B", "SET 5,C", "SET 5,D", "SET 5,E", "SET 5,H", "SET 5,L", "SET 5,(HL)", "SET 5,A", "SET 6,B",
        "SET 6,C", "SET 6,D", "SET 6,E", "SET 6,H", "SET 6,L", "SET 6,(HL)", "SET 6,A", "SET 7,B", "SET 7,C", "SET 7,D",
        "SET 7,E", "SET 7,H", "SET 7,L", "SET 7,(HL)", "SET 7,A"
    };

    static inline const std::array<std::string, 256> nonPrefixedInstructions = {
        "NOP", "LD BC,d16", "LD (BC),A", "INC BC", "INC B", "DEC B", "LD B,d8", "RLCA", "LD (a16),SP", "ADD HL,BC",
        "LD A,(BC)", "DEC BC", "INC C", "DEC C", "LD C,d8", "RRCA", "STOP", "LD DE,d16", "LD (DE),A", "INC DE", "INC D",
        "DEC D", "LD D,d8", "RLA", "JR r8", "ADD HL,DE", "LD A,(DE)", "DEC DE", "INC E", "DEC E", "LD E,d8", "RRA",
        "JR NZ,r8", "LD HL,d16", "LD (HL+),A", "INC HL", "INC H", "DEC H", "LD H,d8", "DAA", "JR Z,r8", "ADD HL,HL",
        "LD A,(HL+)", "DEC HL", "INC L", "DEC L", "LD L,d8", "CPL", "JR NC,r8", "LD SP,d16", "LD (HL-),A", "INC SP",
        "INC (HL)", "DEC (HL)", "LD (HL),d8", "SCF", "JR C,r8", "ADD HL,SP", "LD A,(HL-)", "DEC SP", "INC A", "DEC A",
        "LD A,d8", "CCF", "LD B,B", "LD B,C", "LD B,D", "LD B,E", "LD B,H", "LD B,L", "LD B,(HL)", "LD B,A", "LD C,B",
        "LD C,C", "LD C,D", "LD C,E", "LD C,H", "LD C,L", "LD C,(HL)", "LD C,A", "LD D,B", "LD D,C", "LD D,D", "LD D,E",
        "LD D,H", "LD D,L", "LD D,(HL)", "LD D,A", "LD E,B", "LD E,C", "LD E,D", "LD E,E", "LD E,H", "LD E,L",
        "LD E,(HL)", "LD E,A", "LD H,B", "LD H,C", "LD H,D", "LD H,E", "LD H,H", "LD H,L", "LD H,(HL)", "LD H,A",
        "LD L,B", "LD L,C", "LD L,D", "LD L,E", "LD L,H", "LD L,L", "LD L,(HL)", "LD L,A", "LD (HL),B", "LD (HL),C",
        "LD (HL),D", "LD (HL),E", "LD (HL),H", "LD (HL),L", "HALT", "LD (HL),A", "LD A,B", "LD A,C", "LD A,D", "LD A,E",
        "LD A,H", "LD A,L", "LD A,(HL)", "LD A,A", "ADD A,B", "ADD A,C", "ADD A,D", "ADD A,E", "ADD A,H", "ADD A,L",
        "ADD A,(HL)", "ADD A,A", "ADC A,B", "ADC A,C", "ADC A,D", "ADC A,E", "ADC A,H", "ADC A,L", "ADC A,(HL)",
        "ADC A,A", "SUB B", "SUB C", "SUB D", "SUB E", "SUB H", "SUB L", "SUB (HL)", "SUB A", "SBC A,B", "SBC A,C",
        "SBC A,D", "SBC A,E", "SBC A,H", "SBC A,L", "SBC A,(HL)", "SBC A,A", "AND B", "AND C", "AND D", "AND E",
        "AND H", "AND L", "AND (HL)", "AND A", "XOR B", "XOR C", "XOR D", "XOR E", "XOR H", "XOR L", "XOR (HL)",
        "XOR A", "OR B", "OR C", "OR D", "OR E", "OR H", "OR L", "OR (HL)", "OR A", "CP B", "CP C", "CP D", "CP E",
        "CP H", "CP L", "CP (HL)", "CP A", "RET NZ", "POP BC", "JP NZ,a16", "JP a16", "CALL NZ,a16", "PUSH BC",
        "ADD A,d8", "RST 00H", "RET Z", "RET", "JP Z,a16", "PREFIX", "CALL Z,a16", "CALL a16", "ADC A,d8", "RST 08H",
        "RET NC", "POP DE", "JP NC,a16", "INVALID", "CALL NC,a16", "PUSH DE", "SUB d8", "RST 10H", "RET C", "RETI", "JP C,a16",
        "INVALID", "CALL C,a16", "INVALID", "SBC A,d8", "RST 18H", "LD (FF00+d8),A", "POP HL", "LD (FF00+C),A", "INVALID", "INVALID",
        "PUSH HL", "AND d8", "RST 20H", "ADD SP,d8", "JP (HL)", "LD (a16),A", "INVALID", "INVALID", "INVALID", "XOR d8", "RST 28H",
        "LD A,(FF00+d8)", "POP AF", "LD A,(FF00+C)", "DI", "INVALID", "PUSH AF", "OR A,d8", "RST 30H", "LD HL,d8", "LD SP,HL", "LD A,(a16)",
        "EI", "INVALID", "INVALID", "CP A,d8", "RST 38H"
    };
};
