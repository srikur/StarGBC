#pragma once

#include <functional>

#include "CPU.h"
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

template<BusLike BusT>
class Instructions {
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

    enum class CorruptionType {
        Write,
        Read,
        ReadWrite,
    };

    void HandleOAMCorruption(const CPU<BusT> &, uint16_t location, CorruptionType type) const;

    bool DAA(CPU<BusT> &) const;

    bool RETI(CPU<BusT> &);

    bool DI(CPU<BusT> &) const;

    bool EI(CPU<BusT> &) const;

    bool HALT(CPU<BusT> &) const;

    template<RSTTarget target>
    bool RST(CPU<BusT> &);

    bool CALLUnconditional(CPU<BusT> &);

    template<JumpTest test>
    bool CALL(CPU<BusT> &);

    bool RLCA(CPU<BusT> &) const;

    bool RLA(CPU<BusT> &) const;

    bool CCF(CPU<BusT> &) const;

    bool CPL(CPU<BusT> &) const;

    bool SCF(CPU<BusT> &) const;

    bool RRCA(CPU<BusT> &) const;

    bool RRA(CPU<BusT> &) const;

    bool RETUnconditional(CPU<BusT> &);

    template<JumpTest test>
    bool RETConditional(CPU<BusT> &);

    bool JRUnconditional(CPU<BusT> &);

    template<JumpTest test>
    bool JR(CPU<BusT> &);

    bool JPUnconditional(CPU<BusT> &);

    template<JumpTest test>
    bool JP(CPU<BusT> &);

    bool JPHL(CPU<BusT> &) const;

    bool NOP(CPU<BusT> &) const;

    bool PREFIX(CPU<BusT> &) const;

    bool STOP(CPU<BusT> &) const;

    template<Register source>
    bool DECRegister(CPU<BusT> &) const;

    bool DECIndirect(CPU<BusT> &);

    template<Arithmetic16Target target>
    bool DEC16(CPU<BusT> &);

    template<Register source>
    bool INCRegister(CPU<BusT> &) const;

    bool INCIndirect(CPU<BusT> &);

    template<Arithmetic16Target target>
    bool INC16(CPU<BusT> &);

    template<Register target, Register source>
    bool LDRegister(CPU<BusT> &);

    template<Register source>
    bool LDRegisterImmediate(CPU<BusT> &);

    template<Register source>
    bool LDRegisterIndirect(CPU<BusT> &);

    template<Register source>
    bool LDAddrRegister(CPU<BusT> &);

    bool LDAddrImmediate(CPU<BusT> &);

    bool LDAccumulatorBC(CPU<BusT> &);

    bool LDAccumulatorDE(CPU<BusT> &);

    bool LDFromAccBC(CPU<BusT> &) const;

    bool LDFromAccDE(CPU<BusT> &) const;

    bool LDAccumulatorDirect(CPU<BusT> &);

    bool LDFromAccumulatorDirect(CPU<BusT> &);

    bool LDAccumulatorIndirectDec(CPU<BusT> &);

    bool LDFromAccumulatorIndirectDec(CPU<BusT> &);

    bool LDAccumulatorIndirectInc(CPU<BusT> &);

    bool LDFromAccumulatorIndirectInc(CPU<BusT> &);

    bool LoadFromAccumulatorIndirectC(CPU<BusT> &) const;

    bool LoadFromAccumulatorDirectA(CPU<BusT> &);

    bool LoadAccumulatorA(CPU<BusT> &);

    bool LoadAccumulatorIndirectC(CPU<BusT> &);

    template<LoadWordTarget target>
    bool LD16Register(CPU<BusT> &);

    bool LD16FromStack(CPU<BusT> &);

    bool LD16StackAdjusted(CPU<BusT> &);

    bool LD16Stack(CPU<BusT> &) const;

    template<StackTarget target>
    bool PUSH(CPU<BusT> &);

    template<StackTarget target>
    bool POP(CPU<BusT> &);

    template<Register source>
    bool CPRegister(CPU<BusT> &);

    bool CPIndirect(CPU<BusT> &);

    bool CPImmediate(CPU<BusT> &);

    template<Register source>
    bool ORRegister(CPU<BusT> &);

    bool ORIndirect(CPU<BusT> &);

    bool ORImmediate(CPU<BusT> &);

    template<Register source>
    bool XORRegister(CPU<BusT> &);

    bool XORIndirect(CPU<BusT> &);

    bool XORImmediate(CPU<BusT> &);

    template<Register source>
    bool AND(CPU<BusT> &);

    bool ANDIndirect(CPU<BusT> &);

    bool ANDImmediate(CPU<BusT> &);

    template<Register source>
    bool SUB(CPU<BusT> &);

    bool SUBIndirect(CPU<BusT> &);

    bool SUBImmediate(CPU<BusT> &);

    template<Register source>
    bool RRC(CPU<BusT> &);

    bool RRCAddr(CPU<BusT> &);

    template<Register source>
    bool RLC(CPU<BusT> &);

    bool RLCAddr(CPU<BusT> &);

    template<Register source>
    bool RR(CPU<BusT> &);

    bool RRAddr(CPU<BusT> &);

    template<Register source>
    bool RL(CPU<BusT> &);

    bool RLAddr(CPU<BusT> &);

    template<Register source>
    bool SLA(CPU<BusT> &);

    bool SLAAddr(CPU<BusT> &);

    template<Register source>
    bool SRA(CPU<BusT> &);

    bool SRAAddr(CPU<BusT> &);

    template<Register source>
    bool SWAP(CPU<BusT> &);

    bool SWAPAddr(CPU<BusT> &);

    template<Register source>
    bool SRL(CPU<BusT> &);

    bool SRLAddr(CPU<BusT> &);

    template<Register source, int bit>
    bool BIT(CPU<BusT> &);

    template<int bit>
    bool BITAddr(CPU<BusT> &);

    template<Register source, int bit>
    bool RES(CPU<BusT> &);

    template<int bit>
    bool RESAddr(CPU<BusT> &);

    template<Register source, int bit>
    bool SET(CPU<BusT> &);

    template<int bit>
    bool SETAddr(CPU<BusT> &);

    template<Register source>
    bool SBCRegister(CPU<BusT> &);

    bool SBCIndirect(CPU<BusT> &);

    bool SBCImmediate(CPU<BusT> &);

    template<Register source>
    bool ADCRegister(CPU<BusT> &);

    bool ADCIndirect(CPU<BusT> &);

    bool ADCImmediate(CPU<BusT> &);

    template<Arithmetic16Target target>
    bool ADD16(CPU<BusT> &);

    template<Register source>
    bool ADDRegister(CPU<BusT> &);

    bool ADDIndirect(CPU<BusT> &);

    bool ADDImmediate(CPU<BusT> &);

    bool ADDSigned(CPU<BusT> &);

public:
    explicit Instructions(Registers &regs, BusT &bus, Interrupts &interrupts) : regs_(regs), bus_(bus), interrupts_(interrupts) {
    }

    Registers &regs_;
    BusT &bus_;
    Interrupts &interrupts_;
    int8_t signedByte{0};
    uint8_t byte{0};
    uint16_t word{0};
    uint16_t word2{0};
    bool jumpCondition{false};

    using WrappedFunction = std::function<bool(CPU<BusT> &)>;

    template<typename... Args>
    bool prefixedInstr(const uint8_t opcode, Args &&... args) {
        return prefixedTable[opcode](std::forward<Args>(args)...);
    }

    template<typename... Args>
    bool nonPrefixedInstr(const uint8_t opcode, Args &&... args) {
        return nonPrefixedTable[opcode](std::forward<Args>(args)...);
    }

    void ResetState() {
        word = word2 = byte = signedByte = 0;
        jumpCondition = false;
    }

    [[nodiscard]] std::string GetMnemonic(uint16_t instruction) const {
        const bool prefixed = instruction >> 8 == 0xCB;
        instruction &= 0xFF;
        return prefixed ? prefixedInstructions[instruction] : nonPrefixedInstructions[instruction];
    }

    const std::array<WrappedFunction, 256> prefixedTable = [this] {
        std::array<WrappedFunction, 256> table{};
        table[0x00] = [this](CPU<BusT> &cpu) -> bool { return this->RLC<Register::B>(cpu); };
        table[0x01] = [this](CPU<BusT> &cpu) -> bool { return this->RLC<Register::C>(cpu); };
        table[0x02] = [this](CPU<BusT> &cpu) -> bool { return this->RLC<Register::D>(cpu); };
        table[0x03] = [this](CPU<BusT> &cpu) -> bool { return this->RLC<Register::E>(cpu); };
        table[0x04] = [this](CPU<BusT> &cpu) -> bool { return this->RLC<Register::H>(cpu); };
        table[0x05] = [this](CPU<BusT> &cpu) -> bool { return this->RLC<Register::L>(cpu); };
        table[0x06] = [this](CPU<BusT> &cpu) -> bool { return this->RLCAddr(cpu); };
        table[0x07] = [this](CPU<BusT> &cpu) -> bool { return this->RLC<Register::A>(cpu); };
        table[0x08] = [this](CPU<BusT> &cpu) -> bool { return this->RRC<Register::B>(cpu); };
        table[0x09] = [this](CPU<BusT> &cpu) -> bool { return this->RRC<Register::C>(cpu); };
        table[0x0A] = [this](CPU<BusT> &cpu) -> bool { return this->RRC<Register::D>(cpu); };
        table[0x0B] = [this](CPU<BusT> &cpu) -> bool { return this->RRC<Register::E>(cpu); };
        table[0x0C] = [this](CPU<BusT> &cpu) -> bool { return this->RRC<Register::H>(cpu); };
        table[0x0D] = [this](CPU<BusT> &cpu) -> bool { return this->RRC<Register::L>(cpu); };
        table[0x0E] = [this](CPU<BusT> &cpu) -> bool { return this->RRCAddr(cpu); };
        table[0x0F] = [this](CPU<BusT> &cpu) -> bool { return this->RRC<Register::A>(cpu); };
        table[0x10] = [this](CPU<BusT> &cpu) -> bool { return this->RL<Register::B>(cpu); };
        table[0x11] = [this](CPU<BusT> &cpu) -> bool { return this->RL<Register::C>(cpu); };
        table[0x12] = [this](CPU<BusT> &cpu) -> bool { return this->RL<Register::D>(cpu); };
        table[0x13] = [this](CPU<BusT> &cpu) -> bool { return this->RL<Register::E>(cpu); };
        table[0x14] = [this](CPU<BusT> &cpu) -> bool { return this->RL<Register::H>(cpu); };
        table[0x15] = [this](CPU<BusT> &cpu) -> bool { return this->RL<Register::L>(cpu); };
        table[0x16] = [this](CPU<BusT> &cpu) -> bool { return this->RLAddr(cpu); };
        table[0x17] = [this](CPU<BusT> &cpu) -> bool { return this->RL<Register::A>(cpu); };
        table[0x18] = [this](CPU<BusT> &cpu) -> bool { return this->RR<Register::B>(cpu); };
        table[0x19] = [this](CPU<BusT> &cpu) -> bool { return this->RR<Register::C>(cpu); };
        table[0x1A] = [this](CPU<BusT> &cpu) -> bool { return this->RR<Register::D>(cpu); };
        table[0x1B] = [this](CPU<BusT> &cpu) -> bool { return this->RR<Register::E>(cpu); };
        table[0x1C] = [this](CPU<BusT> &cpu) -> bool { return this->RR<Register::H>(cpu); };
        table[0x1D] = [this](CPU<BusT> &cpu) -> bool { return this->RR<Register::L>(cpu); };
        table[0x1E] = [this](CPU<BusT> &cpu) -> bool { return this->RRAddr(cpu); };
        table[0x1F] = [this](CPU<BusT> &cpu) -> bool { return this->RR<Register::A>(cpu); };
        table[0x20] = [this](CPU<BusT> &cpu) -> bool { return this->SLA<Register::B>(cpu); };
        table[0x21] = [this](CPU<BusT> &cpu) -> bool { return this->SLA<Register::C>(cpu); };
        table[0x22] = [this](CPU<BusT> &cpu) -> bool { return this->SLA<Register::D>(cpu); };
        table[0x23] = [this](CPU<BusT> &cpu) -> bool { return this->SLA<Register::E>(cpu); };
        table[0x24] = [this](CPU<BusT> &cpu) -> bool { return this->SLA<Register::H>(cpu); };
        table[0x25] = [this](CPU<BusT> &cpu) -> bool { return this->SLA<Register::L>(cpu); };
        table[0x26] = [this](CPU<BusT> &cpu) -> bool { return this->SLAAddr(cpu); };
        table[0x27] = [this](CPU<BusT> &cpu) -> bool { return this->SLA<Register::A>(cpu); };
        table[0x28] = [this](CPU<BusT> &cpu) -> bool { return this->SRA<Register::B>(cpu); };
        table[0x29] = [this](CPU<BusT> &cpu) -> bool { return this->SRA<Register::C>(cpu); };
        table[0x2A] = [this](CPU<BusT> &cpu) -> bool { return this->SRA<Register::D>(cpu); };
        table[0x2B] = [this](CPU<BusT> &cpu) -> bool { return this->SRA<Register::E>(cpu); };
        table[0x2C] = [this](CPU<BusT> &cpu) -> bool { return this->SRA<Register::H>(cpu); };
        table[0x2D] = [this](CPU<BusT> &cpu) -> bool { return this->SRA<Register::L>(cpu); };
        table[0x2E] = [this](CPU<BusT> &cpu) -> bool { return this->SRAAddr(cpu); };
        table[0x2F] = [this](CPU<BusT> &cpu) -> bool { return this->SRA<Register::A>(cpu); };
        table[0x30] = [this](CPU<BusT> &cpu) -> bool { return this->SWAP<Register::B>(cpu); };
        table[0x31] = [this](CPU<BusT> &cpu) -> bool { return this->SWAP<Register::C>(cpu); };
        table[0x32] = [this](CPU<BusT> &cpu) -> bool { return this->SWAP<Register::D>(cpu); };
        table[0x33] = [this](CPU<BusT> &cpu) -> bool { return this->SWAP<Register::E>(cpu); };
        table[0x34] = [this](CPU<BusT> &cpu) -> bool { return this->SWAP<Register::H>(cpu); };
        table[0x35] = [this](CPU<BusT> &cpu) -> bool { return this->SWAP<Register::L>(cpu); };
        table[0x36] = [this](CPU<BusT> &cpu) -> bool { return this->SWAPAddr(cpu); };
        table[0x37] = [this](CPU<BusT> &cpu) -> bool { return this->SWAP<Register::A>(cpu); };
        table[0x38] = [this](CPU<BusT> &cpu) -> bool { return this->SRL<Register::B>(cpu); };
        table[0x39] = [this](CPU<BusT> &cpu) -> bool { return this->SRL<Register::C>(cpu); };
        table[0x3A] = [this](CPU<BusT> &cpu) -> bool { return this->SRL<Register::D>(cpu); };
        table[0x3B] = [this](CPU<BusT> &cpu) -> bool { return this->SRL<Register::E>(cpu); };
        table[0x3C] = [this](CPU<BusT> &cpu) -> bool { return this->SRL<Register::H>(cpu); };
        table[0x3D] = [this](CPU<BusT> &cpu) -> bool { return this->SRL<Register::L>(cpu); };
        table[0x3E] = [this](CPU<BusT> &cpu) -> bool { return this->SRLAddr(cpu); };
        table[0x3F] = [this](CPU<BusT> &cpu) -> bool { return this->SRL<Register::A>(cpu); };
        table[0x40] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::B, 0>(cpu); };
        table[0x41] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::C, 0>(cpu); };
        table[0x42] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::D, 0>(cpu); };
        table[0x43] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::E, 0>(cpu); };
        table[0x44] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::H, 0>(cpu); };
        table[0x45] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::L, 0>(cpu); };
        table[0x46] = [this](CPU<BusT> &cpu) -> bool { return this->BITAddr<0>(cpu); };
        table[0x47] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::A, 0>(cpu); };
        table[0x48] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::B, 1>(cpu); };
        table[0x49] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::C, 1>(cpu); };
        table[0x4A] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::D, 1>(cpu); };
        table[0x4B] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::E, 1>(cpu); };
        table[0x4C] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::H, 1>(cpu); };
        table[0x4D] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::L, 1>(cpu); };
        table[0x4E] = [this](CPU<BusT> &cpu) -> bool { return this->BITAddr<1>(cpu); };
        table[0x4F] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::A, 1>(cpu); };
        table[0x50] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::B, 2>(cpu); };
        table[0x51] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::C, 2>(cpu); };
        table[0x52] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::D, 2>(cpu); };
        table[0x53] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::E, 2>(cpu); };
        table[0x54] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::H, 2>(cpu); };
        table[0x55] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::L, 2>(cpu); };
        table[0x56] = [this](CPU<BusT> &cpu) -> bool { return this->BITAddr<2>(cpu); };
        table[0x57] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::A, 2>(cpu); };
        table[0x58] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::B, 3>(cpu); };
        table[0x59] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::C, 3>(cpu); };
        table[0x5A] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::D, 3>(cpu); };
        table[0x5B] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::E, 3>(cpu); };
        table[0x5C] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::H, 3>(cpu); };
        table[0x5D] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::L, 3>(cpu); };
        table[0x5E] = [this](CPU<BusT> &cpu) -> bool { return this->BITAddr<3>(cpu); };
        table[0x5F] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::A, 3>(cpu); };
        table[0x60] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::B, 4>(cpu); };
        table[0x61] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::C, 4>(cpu); };
        table[0x62] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::D, 4>(cpu); };
        table[0x63] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::E, 4>(cpu); };
        table[0x64] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::H, 4>(cpu); };
        table[0x65] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::L, 4>(cpu); };
        table[0x66] = [this](CPU<BusT> &cpu) -> bool { return this->BITAddr<4>(cpu); };
        table[0x67] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::A, 4>(cpu); };
        table[0x68] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::B, 5>(cpu); };
        table[0x69] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::C, 5>(cpu); };
        table[0x6A] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::D, 5>(cpu); };
        table[0x6B] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::E, 5>(cpu); };
        table[0x6C] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::H, 5>(cpu); };
        table[0x6D] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::L, 5>(cpu); };
        table[0x6E] = [this](CPU<BusT> &cpu) -> bool { return this->BITAddr<5>(cpu); };
        table[0x6F] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::A, 5>(cpu); };
        table[0x70] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::B, 6>(cpu); };
        table[0x71] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::C, 6>(cpu); };
        table[0x72] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::D, 6>(cpu); };
        table[0x73] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::E, 6>(cpu); };
        table[0x74] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::H, 6>(cpu); };
        table[0x75] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::L, 6>(cpu); };
        table[0x76] = [this](CPU<BusT> &cpu) -> bool { return this->BITAddr<6>(cpu); };
        table[0x77] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::A, 6>(cpu); };
        table[0x78] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::B, 7>(cpu); };
        table[0x79] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::C, 7>(cpu); };
        table[0x7A] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::D, 7>(cpu); };
        table[0x7B] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::E, 7>(cpu); };
        table[0x7C] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::H, 7>(cpu); };
        table[0x7D] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::L, 7>(cpu); };
        table[0x7E] = [this](CPU<BusT> &cpu) -> bool { return this->BITAddr<7>(cpu); };
        table[0x7F] = [this](CPU<BusT> &cpu) -> bool { return this->BIT<Register::A, 7>(cpu); };
        table[0x80] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::B, 0>(cpu); };
        table[0x81] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::C, 0>(cpu); };
        table[0x82] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::D, 0>(cpu); };
        table[0x83] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::E, 0>(cpu); };
        table[0x84] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::H, 0>(cpu); };
        table[0x85] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::L, 0>(cpu); };
        table[0x86] = [this](CPU<BusT> &cpu) -> bool { return this->RESAddr<0>(cpu); };
        table[0x87] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::A, 0>(cpu); };
        table[0x88] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::B, 1>(cpu); };
        table[0x89] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::C, 1>(cpu); };
        table[0x8A] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::D, 1>(cpu); };
        table[0x8B] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::E, 1>(cpu); };
        table[0x8C] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::H, 1>(cpu); };
        table[0x8D] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::L, 1>(cpu); };
        table[0x8E] = [this](CPU<BusT> &cpu) -> bool { return this->RESAddr<1>(cpu); };
        table[0x8F] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::A, 1>(cpu); };
        table[0x90] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::B, 2>(cpu); };
        table[0x91] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::C, 2>(cpu); };
        table[0x92] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::D, 2>(cpu); };
        table[0x93] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::E, 2>(cpu); };
        table[0x94] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::H, 2>(cpu); };
        table[0x95] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::L, 2>(cpu); };
        table[0x96] = [this](CPU<BusT> &cpu) -> bool { return this->RESAddr<2>(cpu); };
        table[0x97] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::A, 2>(cpu); };
        table[0x98] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::B, 3>(cpu); };
        table[0x99] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::C, 3>(cpu); };
        table[0x9A] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::D, 3>(cpu); };
        table[0x9B] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::E, 3>(cpu); };
        table[0x9C] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::H, 3>(cpu); };
        table[0x9D] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::L, 3>(cpu); };
        table[0x9E] = [this](CPU<BusT> &cpu) -> bool { return this->RESAddr<3>(cpu); };
        table[0x9F] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::A, 3>(cpu); };
        table[0xA0] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::B, 4>(cpu); };
        table[0xA1] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::C, 4>(cpu); };
        table[0xA2] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::D, 4>(cpu); };
        table[0xA3] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::E, 4>(cpu); };
        table[0xA4] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::H, 4>(cpu); };
        table[0xA5] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::L, 4>(cpu); };
        table[0xA6] = [this](CPU<BusT> &cpu) -> bool { return this->RESAddr<4>(cpu); };
        table[0xA7] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::A, 4>(cpu); };
        table[0xA8] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::B, 5>(cpu); };
        table[0xA9] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::C, 5>(cpu); };
        table[0xAA] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::D, 5>(cpu); };
        table[0xAB] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::E, 5>(cpu); };
        table[0xAC] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::H, 5>(cpu); };
        table[0xAD] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::L, 5>(cpu); };
        table[0xAE] = [this](CPU<BusT> &cpu) -> bool { return this->RESAddr<5>(cpu); };
        table[0xAF] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::A, 5>(cpu); };
        table[0xB0] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::B, 6>(cpu); };
        table[0xB1] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::C, 6>(cpu); };
        table[0xB2] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::D, 6>(cpu); };
        table[0xB3] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::E, 6>(cpu); };
        table[0xB4] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::H, 6>(cpu); };
        table[0xB5] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::L, 6>(cpu); };
        table[0xB6] = [this](CPU<BusT> &cpu) -> bool { return this->RESAddr<6>(cpu); };
        table[0xB7] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::A, 6>(cpu); };
        table[0xB8] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::B, 7>(cpu); };
        table[0xB9] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::C, 7>(cpu); };
        table[0xBA] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::D, 7>(cpu); };
        table[0xBB] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::E, 7>(cpu); };
        table[0xBC] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::H, 7>(cpu); };
        table[0xBD] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::L, 7>(cpu); };
        table[0xBE] = [this](CPU<BusT> &cpu) -> bool { return this->RESAddr<7>(cpu); };
        table[0xBF] = [this](CPU<BusT> &cpu) -> bool { return this->RES<Register::A, 7>(cpu); };
        table[0xC0] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::B, 0>(cpu); };
        table[0xC1] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::C, 0>(cpu); };
        table[0xC2] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::D, 0>(cpu); };
        table[0xC3] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::E, 0>(cpu); };
        table[0xC4] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::H, 0>(cpu); };
        table[0xC5] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::L, 0>(cpu); };
        table[0xC6] = [this](CPU<BusT> &cpu) -> bool { return this->SETAddr<0>(cpu); };
        table[0xC7] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::A, 0>(cpu); };
        table[0xC8] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::B, 1>(cpu); };
        table[0xC9] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::C, 1>(cpu); };
        table[0xCA] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::D, 1>(cpu); };
        table[0xCB] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::E, 1>(cpu); };
        table[0xCC] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::H, 1>(cpu); };
        table[0xCD] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::L, 1>(cpu); };
        table[0xCE] = [this](CPU<BusT> &cpu) -> bool { return this->SETAddr<1>(cpu); };
        table[0xCF] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::A, 1>(cpu); };
        table[0xD0] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::B, 2>(cpu); };
        table[0xD1] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::C, 2>(cpu); };
        table[0xD2] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::D, 2>(cpu); };
        table[0xD3] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::E, 2>(cpu); };
        table[0xD4] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::H, 2>(cpu); };
        table[0xD5] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::L, 2>(cpu); };
        table[0xD6] = [this](CPU<BusT> &cpu) -> bool { return this->SETAddr<2>(cpu); };
        table[0xD7] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::A, 2>(cpu); };
        table[0xD8] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::B, 3>(cpu); };
        table[0xD9] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::C, 3>(cpu); };
        table[0xDA] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::D, 3>(cpu); };
        table[0xDB] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::E, 3>(cpu); };
        table[0xDC] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::H, 3>(cpu); };
        table[0xDD] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::L, 3>(cpu); };
        table[0xDE] = [this](CPU<BusT> &cpu) -> bool { return this->SETAddr<3>(cpu); };
        table[0xDF] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::A, 3>(cpu); };
        table[0xE0] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::B, 4>(cpu); };
        table[0xE1] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::C, 4>(cpu); };
        table[0xE2] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::D, 4>(cpu); };
        table[0xE3] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::E, 4>(cpu); };
        table[0xE4] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::H, 4>(cpu); };
        table[0xE5] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::L, 4>(cpu); };
        table[0xE6] = [this](CPU<BusT> &cpu) -> bool { return this->SETAddr<4>(cpu); };
        table[0xE7] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::A, 4>(cpu); };
        table[0xE8] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::B, 5>(cpu); };
        table[0xE9] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::C, 5>(cpu); };
        table[0xEA] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::D, 5>(cpu); };
        table[0xEB] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::E, 5>(cpu); };
        table[0xEC] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::H, 5>(cpu); };
        table[0xED] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::L, 5>(cpu); };
        table[0xEE] = [this](CPU<BusT> &cpu) -> bool { return this->SETAddr<5>(cpu); };
        table[0xEF] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::A, 5>(cpu); };
        table[0xF0] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::B, 6>(cpu); };
        table[0xF1] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::C, 6>(cpu); };
        table[0xF2] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::D, 6>(cpu); };
        table[0xF3] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::E, 6>(cpu); };
        table[0xF4] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::H, 6>(cpu); };
        table[0xF5] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::L, 6>(cpu); };
        table[0xF6] = [this](CPU<BusT> &cpu) -> bool { return this->SETAddr<6>(cpu); };
        table[0xF7] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::A, 6>(cpu); };
        table[0xF8] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::B, 7>(cpu); };
        table[0xF9] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::C, 7>(cpu); };
        table[0xFA] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::D, 7>(cpu); };
        table[0xFB] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::E, 7>(cpu); };
        table[0xFC] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::H, 7>(cpu); };
        table[0xFD] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::L, 7>(cpu); };
        table[0xFE] = [this](CPU<BusT> &cpu) -> bool { return this->SETAddr<7>(cpu); };
        table[0xFF] = [this](CPU<BusT> &cpu) -> bool { return this->SET<Register::A, 7>(cpu); };
        return table;
    }();

    const std::array<WrappedFunction, 256> nonPrefixedTable = [this] {
        std::array<WrappedFunction, 256> table{};
        table[0x00] = [this](CPU<BusT> &cpu) -> bool { return this->NOP(cpu); };
        table[0x01] = [this](CPU<BusT> &cpu) -> bool { return this->LD16Register<LoadWordTarget::BC>(cpu); };
        table[0x02] = [this](CPU<BusT> &cpu) -> bool { return this->LDFromAccBC(cpu); };
        table[0x03] = [this](CPU<BusT> &cpu) -> bool { return this->INC16<Arithmetic16Target::BC>(cpu); };
        table[0x04] = [this](CPU<BusT> &cpu) -> bool { return this->INCRegister<Register::B>(cpu); };
        table[0x05] = [this](CPU<BusT> &cpu) -> bool { return this->DECRegister<Register::B>(cpu); };
        table[0x06] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegisterImmediate<Register::B>(cpu); };
        table[0x07] = [this](CPU<BusT> &cpu) -> bool { return this->RLCA(cpu); };
        table[0x08] = [this](CPU<BusT> &cpu) -> bool { return this->LD16FromStack(cpu); };
        table[0x09] = [this](CPU<BusT> &cpu) -> bool { return this->ADD16<Arithmetic16Target::BC>(cpu); };
        table[0x10] = [this](CPU<BusT> &cpu) -> bool { return this->STOP(cpu); };
        table[0x0A] = [this](CPU<BusT> &cpu) -> bool { return this->LDAccumulatorBC(cpu); };
        table[0x0B] = [this](CPU<BusT> &cpu) -> bool { return this->DEC16<Arithmetic16Target::BC>(cpu); };
        table[0x0C] = [this](CPU<BusT> &cpu) -> bool { return this->INCRegister<Register::C>(cpu); };
        table[0x0D] = [this](CPU<BusT> &cpu) -> bool { return this->DECRegister<Register::C>(cpu); };
        table[0x0E] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegisterImmediate<Register::C>(cpu); };
        table[0x0F] = [this](CPU<BusT> &cpu) -> bool { return this->RRCA(cpu); };
        table[0x11] = [this](CPU<BusT> &cpu) -> bool { return this->LD16Register<LoadWordTarget::DE>(cpu); };
        table[0x12] = [this](CPU<BusT> &cpu) -> bool { return this->LDFromAccDE(cpu); };
        table[0x13] = [this](CPU<BusT> &cpu) -> bool { return this->INC16<Arithmetic16Target::DE>(cpu); };
        table[0x14] = [this](CPU<BusT> &cpu) -> bool { return this->INCRegister<Register::D>(cpu); };
        table[0x15] = [this](CPU<BusT> &cpu) -> bool { return this->DECRegister<Register::D>(cpu); };
        table[0x16] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegisterImmediate<Register::D>(cpu); };
        table[0x17] = [this](CPU<BusT> &cpu) -> bool { return this->RLA(cpu); };
        table[0x18] = [this](CPU<BusT> &cpu) -> bool { return this->JRUnconditional(cpu); };
        table[0x19] = [this](CPU<BusT> &cpu) -> bool { return this->ADD16<Arithmetic16Target::DE>(cpu); };
        table[0x1A] = [this](CPU<BusT> &cpu) -> bool { return this->LDAccumulatorDE(cpu); };
        table[0x1B] = [this](CPU<BusT> &cpu) -> bool { return this->DEC16<Arithmetic16Target::DE>(cpu); };
        table[0x1C] = [this](CPU<BusT> &cpu) -> bool { return this->INCRegister<Register::E>(cpu); };
        table[0x1D] = [this](CPU<BusT> &cpu) -> bool { return this->DECRegister<Register::E>(cpu); };
        table[0x1E] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegisterImmediate<Register::E>(cpu); };
        table[0x1F] = [this](CPU<BusT> &cpu) -> bool { return this->RRA(cpu); };
        table[0x20] = [this](CPU<BusT> &cpu) -> bool { return this->JR<JumpTest::NotZero>(cpu); };
        table[0x21] = [this](CPU<BusT> &cpu) -> bool { return this->LD16Register<LoadWordTarget::HL>(cpu); };
        table[0x22] = [this](CPU<BusT> &cpu) -> bool { return this->LDFromAccumulatorIndirectInc(cpu); };
        table[0x23] = [this](CPU<BusT> &cpu) -> bool { return this->INC16<Arithmetic16Target::HL>(cpu); };
        table[0x24] = [this](CPU<BusT> &cpu) -> bool { return this->INCRegister<Register::H>(cpu); };
        table[0x25] = [this](CPU<BusT> &cpu) -> bool { return this->DECRegister<Register::H>(cpu); };
        table[0x26] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegisterImmediate<Register::H>(cpu); };
        table[0x27] = [this](CPU<BusT> &cpu) -> bool { return this->DAA(cpu); };
        table[0x28] = [this](CPU<BusT> &cpu) -> bool { return this->JR<JumpTest::Zero>(cpu); };
        table[0x29] = [this](CPU<BusT> &cpu) -> bool { return this->ADD16<Arithmetic16Target::HL>(cpu); };
        table[0x2A] = [this](CPU<BusT> &cpu) -> bool { return this->LDAccumulatorIndirectInc(cpu); };
        table[0x2B] = [this](CPU<BusT> &cpu) -> bool { return this->DEC16<Arithmetic16Target::HL>(cpu); };
        table[0x2C] = [this](CPU<BusT> &cpu) -> bool { return this->INCRegister<Register::L>(cpu); };
        table[0x2D] = [this](CPU<BusT> &cpu) -> bool { return this->DECRegister<Register::L>(cpu); };
        table[0x2E] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegisterImmediate<Register::L>(cpu); };
        table[0x2F] = [this](CPU<BusT> &cpu) -> bool { return this->CPL(cpu); };
        table[0x30] = [this](CPU<BusT> &cpu) -> bool { return this->JR<JumpTest::NotCarry>(cpu); };
        table[0x31] = [this](CPU<BusT> &cpu) -> bool { return this->LD16Register<LoadWordTarget::SP>(cpu); };
        table[0x32] = [this](CPU<BusT> &cpu) -> bool { return this->LDFromAccumulatorIndirectDec(cpu); };
        table[0x33] = [this](CPU<BusT> &cpu) -> bool { return this->INC16<Arithmetic16Target::SP>(cpu); };
        table[0x34] = [this](CPU<BusT> &cpu) -> bool { return this->INCIndirect(cpu); };
        table[0x35] = [this](CPU<BusT> &cpu) -> bool { return this->DECIndirect(cpu); };
        table[0x36] = [this](CPU<BusT> &cpu) -> bool { return this->LDAddrImmediate(cpu); };
        table[0x37] = [this](CPU<BusT> &cpu) -> bool { return this->SCF(cpu); };
        table[0x38] = [this](CPU<BusT> &cpu) -> bool { return this->JR<JumpTest::Carry>(cpu); };
        table[0x39] = [this](CPU<BusT> &cpu) -> bool { return this->ADD16<Arithmetic16Target::SP>(cpu); };
        table[0x3A] = [this](CPU<BusT> &cpu) -> bool { return this->LDAccumulatorIndirectDec(cpu); };
        table[0x3B] = [this](CPU<BusT> &cpu) -> bool { return this->DEC16<Arithmetic16Target::SP>(cpu); };
        table[0x3C] = [this](CPU<BusT> &cpu) -> bool { return this->INCRegister<Register::A>(cpu); };
        table[0x3D] = [this](CPU<BusT> &cpu) -> bool { return this->DECRegister<Register::A>(cpu); };
        table[0x3E] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegisterImmediate<Register::A>(cpu); };
        table[0x3F] = [this](CPU<BusT> &cpu) -> bool { return this->CCF(cpu); };
        table[0x40] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::B, Register::B>(cpu); };
        table[0x41] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::B, Register::C>(cpu); };
        table[0x42] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::B, Register::D>(cpu); };
        table[0x43] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::B, Register::E>(cpu); };
        table[0x44] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::B, Register::H>(cpu); };
        table[0x45] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::B, Register::L>(cpu); };
        table[0x46] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegisterIndirect<Register::B>(cpu); };
        table[0x47] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::B, Register::A>(cpu); };
        table[0x48] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::C, Register::B>(cpu); };
        table[0x49] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::C, Register::C>(cpu); };
        table[0x4A] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::C, Register::D>(cpu); };
        table[0x4B] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::C, Register::E>(cpu); };
        table[0x4C] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::C, Register::H>(cpu); };
        table[0x4D] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::C, Register::L>(cpu); };
        table[0x4E] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegisterIndirect<Register::C>(cpu); };
        table[0x4F] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::C, Register::A>(cpu); };
        table[0x50] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::D, Register::B>(cpu); };
        table[0x51] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::D, Register::C>(cpu); };
        table[0x52] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::D, Register::D>(cpu); };
        table[0x53] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::D, Register::E>(cpu); };
        table[0x54] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::D, Register::H>(cpu); };
        table[0x55] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::D, Register::L>(cpu); };
        table[0x56] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegisterIndirect<Register::D>(cpu); };
        table[0x57] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::D, Register::A>(cpu); };
        table[0x58] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::E, Register::B>(cpu); };
        table[0x59] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::E, Register::C>(cpu); };
        table[0x5A] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::E, Register::D>(cpu); };
        table[0x5B] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::E, Register::E>(cpu); };
        table[0x5C] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::E, Register::H>(cpu); };
        table[0x5D] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::E, Register::L>(cpu); };
        table[0x5E] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegisterIndirect<Register::E>(cpu); };
        table[0x5F] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::E, Register::A>(cpu); };
        table[0x60] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::H, Register::B>(cpu); };
        table[0x61] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::H, Register::C>(cpu); };
        table[0x62] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::H, Register::D>(cpu); };
        table[0x63] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::H, Register::E>(cpu); };
        table[0x64] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::H, Register::H>(cpu); };
        table[0x65] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::H, Register::L>(cpu); };
        table[0x66] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegisterIndirect<Register::H>(cpu); };
        table[0x67] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::H, Register::A>(cpu); };
        table[0x68] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::L, Register::B>(cpu); };
        table[0x69] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::L, Register::C>(cpu); };
        table[0x6A] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::L, Register::D>(cpu); };
        table[0x6B] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::L, Register::E>(cpu); };
        table[0x6C] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::L, Register::H>(cpu); };
        table[0x6D] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::L, Register::L>(cpu); };
        table[0x6E] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegisterIndirect<Register::L>(cpu); };
        table[0x6F] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::L, Register::A>(cpu); };
        table[0x70] = [this](CPU<BusT> &cpu) -> bool { return this->LDAddrRegister<Register::B>(cpu); };
        table[0x71] = [this](CPU<BusT> &cpu) -> bool { return this->LDAddrRegister<Register::C>(cpu); };
        table[0x72] = [this](CPU<BusT> &cpu) -> bool { return this->LDAddrRegister<Register::D>(cpu); };
        table[0x73] = [this](CPU<BusT> &cpu) -> bool { return this->LDAddrRegister<Register::E>(cpu); };
        table[0x74] = [this](CPU<BusT> &cpu) -> bool { return this->LDAddrRegister<Register::H>(cpu); };
        table[0x75] = [this](CPU<BusT> &cpu) -> bool { return this->LDAddrRegister<Register::L>(cpu); };
        table[0x76] = [this](CPU<BusT> &cpu) -> bool { return this->HALT(cpu); };
        table[0x77] = [this](CPU<BusT> &cpu) -> bool { return this->LDAddrRegister<Register::A>(cpu); };
        table[0x78] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::A, Register::B>(cpu); };
        table[0x79] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::A, Register::C>(cpu); };
        table[0x7A] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::A, Register::D>(cpu); };
        table[0x7B] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::A, Register::E>(cpu); };
        table[0x7C] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::A, Register::H>(cpu); };
        table[0x7D] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::A, Register::L>(cpu); };
        table[0x7E] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegisterIndirect<Register::A>(cpu); };
        table[0x7F] = [this](CPU<BusT> &cpu) -> bool { return this->LDRegister<Register::A, Register::A>(cpu); };
        table[0x80] = [this](CPU<BusT> &cpu) -> bool { return this->ADDRegister<Register::B>(cpu); };
        table[0x81] = [this](CPU<BusT> &cpu) -> bool { return this->ADDRegister<Register::C>(cpu); };
        table[0x82] = [this](CPU<BusT> &cpu) -> bool { return this->ADDRegister<Register::D>(cpu); };
        table[0x83] = [this](CPU<BusT> &cpu) -> bool { return this->ADDRegister<Register::E>(cpu); };
        table[0x84] = [this](CPU<BusT> &cpu) -> bool { return this->ADDRegister<Register::H>(cpu); };
        table[0x85] = [this](CPU<BusT> &cpu) -> bool { return this->ADDRegister<Register::L>(cpu); };
        table[0x86] = [this](CPU<BusT> &cpu) -> bool { return this->ADDIndirect(cpu); };
        table[0x87] = [this](CPU<BusT> &cpu) -> bool { return this->ADDRegister<Register::A>(cpu); };
        table[0x88] = [this](CPU<BusT> &cpu) -> bool { return this->ADCRegister<Register::B>(cpu); };
        table[0x89] = [this](CPU<BusT> &cpu) -> bool { return this->ADCRegister<Register::C>(cpu); };
        table[0x8A] = [this](CPU<BusT> &cpu) -> bool { return this->ADCRegister<Register::D>(cpu); };
        table[0x8B] = [this](CPU<BusT> &cpu) -> bool { return this->ADCRegister<Register::E>(cpu); };
        table[0x8C] = [this](CPU<BusT> &cpu) -> bool { return this->ADCRegister<Register::H>(cpu); };
        table[0x8D] = [this](CPU<BusT> &cpu) -> bool { return this->ADCRegister<Register::L>(cpu); };
        table[0x8E] = [this](CPU<BusT> &cpu) -> bool { return this->ADCIndirect(cpu); };
        table[0x8F] = [this](CPU<BusT> &cpu) -> bool { return this->ADCRegister<Register::A>(cpu); };
        table[0x90] = [this](CPU<BusT> &cpu) -> bool { return this->SUB<Register::B>(cpu); };
        table[0x91] = [this](CPU<BusT> &cpu) -> bool { return this->SUB<Register::C>(cpu); };
        table[0x92] = [this](CPU<BusT> &cpu) -> bool { return this->SUB<Register::D>(cpu); };
        table[0x93] = [this](CPU<BusT> &cpu) -> bool { return this->SUB<Register::E>(cpu); };
        table[0x94] = [this](CPU<BusT> &cpu) -> bool { return this->SUB<Register::H>(cpu); };
        table[0x95] = [this](CPU<BusT> &cpu) -> bool { return this->SUB<Register::L>(cpu); };
        table[0x96] = [this](CPU<BusT> &cpu) -> bool { return this->SUBIndirect(cpu); };
        table[0x97] = [this](CPU<BusT> &cpu) -> bool { return this->SUB<Register::A>(cpu); };
        table[0x98] = [this](CPU<BusT> &cpu) -> bool { return this->SBCRegister<Register::B>(cpu); };
        table[0x99] = [this](CPU<BusT> &cpu) -> bool { return this->SBCRegister<Register::C>(cpu); };
        table[0x9A] = [this](CPU<BusT> &cpu) -> bool { return this->SBCRegister<Register::D>(cpu); };
        table[0x9B] = [this](CPU<BusT> &cpu) -> bool { return this->SBCRegister<Register::E>(cpu); };
        table[0x9C] = [this](CPU<BusT> &cpu) -> bool { return this->SBCRegister<Register::H>(cpu); };
        table[0x9D] = [this](CPU<BusT> &cpu) -> bool { return this->SBCRegister<Register::L>(cpu); };
        table[0x9E] = [this](CPU<BusT> &cpu) -> bool { return this->SBCIndirect(cpu); };
        table[0x9F] = [this](CPU<BusT> &cpu) -> bool { return this->SBCRegister<Register::A>(cpu); };
        table[0xA0] = [this](CPU<BusT> &cpu) -> bool { return this->AND<Register::B>(cpu); };
        table[0xA1] = [this](CPU<BusT> &cpu) -> bool { return this->AND<Register::C>(cpu); };
        table[0xA2] = [this](CPU<BusT> &cpu) -> bool { return this->AND<Register::D>(cpu); };
        table[0xA3] = [this](CPU<BusT> &cpu) -> bool { return this->AND<Register::E>(cpu); };
        table[0xA4] = [this](CPU<BusT> &cpu) -> bool { return this->AND<Register::H>(cpu); };
        table[0xA5] = [this](CPU<BusT> &cpu) -> bool { return this->AND<Register::L>(cpu); };
        table[0xA6] = [this](CPU<BusT> &cpu) -> bool { return this->ANDIndirect(cpu); };
        table[0xA7] = [this](CPU<BusT> &cpu) -> bool { return this->AND<Register::A>(cpu); };
        table[0xA8] = [this](CPU<BusT> &cpu) -> bool { return this->XORRegister<Register::B>(cpu); };
        table[0xA9] = [this](CPU<BusT> &cpu) -> bool { return this->XORRegister<Register::C>(cpu); };
        table[0xAA] = [this](CPU<BusT> &cpu) -> bool { return this->XORRegister<Register::D>(cpu); };
        table[0xAB] = [this](CPU<BusT> &cpu) -> bool { return this->XORRegister<Register::E>(cpu); };
        table[0xAC] = [this](CPU<BusT> &cpu) -> bool { return this->XORRegister<Register::H>(cpu); };
        table[0xAD] = [this](CPU<BusT> &cpu) -> bool { return this->XORRegister<Register::L>(cpu); };
        table[0xAE] = [this](CPU<BusT> &cpu) -> bool { return this->XORIndirect(cpu); };
        table[0xAF] = [this](CPU<BusT> &cpu) -> bool { return this->XORRegister<Register::A>(cpu); };
        table[0xB0] = [this](CPU<BusT> &cpu) -> bool { return this->ORRegister<Register::B>(cpu); };
        table[0xB1] = [this](CPU<BusT> &cpu) -> bool { return this->ORRegister<Register::C>(cpu); };
        table[0xB2] = [this](CPU<BusT> &cpu) -> bool { return this->ORRegister<Register::D>(cpu); };
        table[0xB3] = [this](CPU<BusT> &cpu) -> bool { return this->ORRegister<Register::E>(cpu); };
        table[0xB4] = [this](CPU<BusT> &cpu) -> bool { return this->ORRegister<Register::H>(cpu); };
        table[0xB5] = [this](CPU<BusT> &cpu) -> bool { return this->ORRegister<Register::L>(cpu); };
        table[0xB6] = [this](CPU<BusT> &cpu) -> bool { return this->ORIndirect(cpu); };
        table[0xB7] = [this](CPU<BusT> &cpu) -> bool { return this->ORRegister<Register::A>(cpu); };
        table[0xB8] = [this](CPU<BusT> &cpu) -> bool { return this->CPRegister<Register::B>(cpu); };
        table[0xB9] = [this](CPU<BusT> &cpu) -> bool { return this->CPRegister<Register::C>(cpu); };
        table[0xBA] = [this](CPU<BusT> &cpu) -> bool { return this->CPRegister<Register::D>(cpu); };
        table[0xBB] = [this](CPU<BusT> &cpu) -> bool { return this->CPRegister<Register::E>(cpu); };
        table[0xBC] = [this](CPU<BusT> &cpu) -> bool { return this->CPRegister<Register::H>(cpu); };
        table[0xBD] = [this](CPU<BusT> &cpu) -> bool { return this->CPRegister<Register::L>(cpu); };
        table[0xBE] = [this](CPU<BusT> &cpu) -> bool { return this->CPIndirect(cpu); };
        table[0xBF] = [this](CPU<BusT> &cpu) -> bool { return this->CPRegister<Register::A>(cpu); };
        table[0xC0] = [this](CPU<BusT> &cpu) -> bool { return this->RETConditional<JumpTest::NotZero>(cpu); };
        table[0xC1] = [this](CPU<BusT> &cpu) -> bool { return this->POP<StackTarget::BC>(cpu); };
        table[0xC2] = [this](CPU<BusT> &cpu) -> bool { return this->JP<JumpTest::NotZero>(cpu); };
        table[0xC3] = [this](CPU<BusT> &cpu) -> bool { return this->JPUnconditional(cpu); };
        table[0xC4] = [this](CPU<BusT> &cpu) -> bool { return this->CALL<JumpTest::NotZero>(cpu); };
        table[0xC5] = [this](CPU<BusT> &cpu) -> bool { return this->PUSH<StackTarget::BC>(cpu); };
        table[0xC6] = [this](CPU<BusT> &cpu) -> bool { return this->ADDImmediate(cpu); };
        table[0xC7] = [this](CPU<BusT> &cpu) -> bool { return this->RST<RSTTarget::H00>(cpu); };
        table[0xC8] = [this](CPU<BusT> &cpu) -> bool { return this->RETConditional<JumpTest::Zero>(cpu); };
        table[0xC9] = [this](CPU<BusT> &cpu) -> bool { return this->RETUnconditional(cpu); };
        table[0xCA] = [this](CPU<BusT> &cpu) -> bool { return this->JP<JumpTest::Zero>(cpu); };
        table[0xCB] = [this](CPU<BusT> &cpu) -> bool { return this->PREFIX(cpu); };
        table[0xCC] = [this](CPU<BusT> &cpu) -> bool { return this->CALL<JumpTest::Zero>(cpu); };
        table[0xCD] = [this](CPU<BusT> &cpu) -> bool { return this->CALLUnconditional(cpu); };
        table[0xCE] = [this](CPU<BusT> &cpu) -> bool { return this->ADCImmediate(cpu); };
        table[0xCF] = [this](CPU<BusT> &cpu) -> bool { return this->RST<RSTTarget::H08>(cpu); };
        table[0xD0] = [this](CPU<BusT> &cpu) -> bool { return this->RETConditional<JumpTest::NotCarry>(cpu); };
        table[0xD1] = [this](CPU<BusT> &cpu) -> bool { return this->POP<StackTarget::DE>(cpu); };
        table[0xD2] = [this](CPU<BusT> &cpu) -> bool { return this->JP<JumpTest::NotCarry>(cpu); };
        table[0xD4] = [this](CPU<BusT> &cpu) -> bool { return this->CALL<JumpTest::NotCarry>(cpu); };
        table[0xD5] = [this](CPU<BusT> &cpu) -> bool { return this->PUSH<StackTarget::DE>(cpu); };
        table[0xD6] = [this](CPU<BusT> &cpu) -> bool { return this->SUBImmediate(cpu); };
        table[0xD7] = [this](CPU<BusT> &cpu) -> bool { return this->RST<RSTTarget::H10>(cpu); };
        table[0xD8] = [this](CPU<BusT> &cpu) -> bool { return this->RETConditional<JumpTest::Carry>(cpu); };
        table[0xD9] = [this](CPU<BusT> &cpu) -> bool { return this->RETI(cpu); };
        table[0xDA] = [this](CPU<BusT> &cpu) -> bool { return this->JP<JumpTest::Carry>(cpu); };
        table[0xDC] = [this](CPU<BusT> &cpu) -> bool { return this->CALL<JumpTest::Carry>(cpu); };
        table[0xDE] = [this](CPU<BusT> &cpu) -> bool { return this->SBCImmediate(cpu); };
        table[0xDF] = [this](CPU<BusT> &cpu) -> bool { return this->RST<RSTTarget::H18>(cpu); };
        table[0xE0] = [this](CPU<BusT> &cpu) -> bool { return this->LoadFromAccumulatorDirectA(cpu); };
        table[0xE1] = [this](CPU<BusT> &cpu) -> bool { return this->POP<StackTarget::HL>(cpu); };
        table[0xE2] = [this](CPU<BusT> &cpu) -> bool { return this->LoadFromAccumulatorIndirectC(cpu); };
        table[0xE5] = [this](CPU<BusT> &cpu) -> bool { return this->PUSH<StackTarget::HL>(cpu); };
        table[0xE6] = [this](CPU<BusT> &cpu) -> bool { return this->ANDImmediate(cpu); };
        table[0xE7] = [this](CPU<BusT> &cpu) -> bool { return this->RST<RSTTarget::H20>(cpu); };
        table[0xE8] = [this](CPU<BusT> &cpu) -> bool { return this->ADDSigned(cpu); };
        table[0xE9] = [this](CPU<BusT> &cpu) -> bool { return this->JPHL(cpu); };
        table[0xEA] = [this](CPU<BusT> &cpu) -> bool { return this->LDFromAccumulatorDirect(cpu); };
        table[0xEE] = [this](CPU<BusT> &cpu) -> bool { return this->XORImmediate(cpu); };
        table[0xEF] = [this](CPU<BusT> &cpu) -> bool { return this->RST<RSTTarget::H28>(cpu); };
        table[0xF0] = [this](CPU<BusT> &cpu) -> bool { return this->LoadAccumulatorA(cpu); };
        table[0xF1] = [this](CPU<BusT> &cpu) -> bool { return this->POP<StackTarget::AF>(cpu); };
        table[0xF2] = [this](CPU<BusT> &cpu) -> bool { return this->LoadAccumulatorIndirectC(cpu); };
        table[0xF3] = [this](CPU<BusT> &cpu) -> bool { return this->DI(cpu); };
        table[0xF5] = [this](CPU<BusT> &cpu) -> bool { return this->PUSH<StackTarget::AF>(cpu); };
        table[0xF6] = [this](CPU<BusT> &cpu) -> bool { return this->ORImmediate(cpu); };
        table[0xF7] = [this](CPU<BusT> &cpu) -> bool { return this->RST<RSTTarget::H30>(cpu); };
        table[0xF8] = [this](CPU<BusT> &cpu) -> bool { return this->LD16StackAdjusted(cpu); };
        table[0xF9] = [this](CPU<BusT> &cpu) -> bool { return this->LD16Stack(cpu); };
        table[0xFA] = [this](CPU<BusT> &cpu) -> bool { return this->LDAccumulatorDirect(cpu); };
        table[0xFB] = [this](CPU<BusT> &cpu) -> bool { return this->EI(cpu); };
        table[0xFE] = [this](CPU<BusT> &cpu) -> bool { return this->CPImmediate(cpu); };
        table[0xFF] = [this](CPU<BusT> &cpu) -> bool { return this->RST<RSTTarget::H38>(cpu); };
        return table;
    }();

    const std::array<std::string, 256> prefixedInstructions = {
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

    const std::array<std::string, 256> nonPrefixedInstructions = {
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
