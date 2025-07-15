#pragma once

#include "Gameboy.h"

class Gameboy;

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

    // used for storing data between cycles
    bool jumpCondition{false};
    int8_t signedByte{0};
    uint8_t byte{0};
    uint16_t word{0};
    uint16_t word2{0};

    bool DAA(Gameboy &gameboy);

    bool RETI(Gameboy &gameboy);

    bool DI(Gameboy &gameboy);

    bool EI(Gameboy &gameboy);

    bool HALT(Gameboy &gameboy);

    template<RSTTarget target>
    bool RST(Gameboy &gameboy);

    bool CALLUnconditional(Gameboy &gameboy);

    template<JumpTest test>
    bool CALL(Gameboy &gameboy);

    bool RLCA(Gameboy &gameboy);

    bool RLA(Gameboy &gameboy);

    bool CCF(Gameboy &gameboy);

    bool CPL(Gameboy &gameboy);

    bool SCF(Gameboy &gameboy);

    bool RRCA(Gameboy &gameboy);

    bool RRA(Gameboy &gameboy);

    bool RETUnconditional(Gameboy &gameboy);

    template<JumpTest test>
    bool RETConditional(Gameboy &gameboy);

    bool JRUnconditional(Gameboy &gameboy);

    template<JumpTest test>
    bool JR(Gameboy &gameboy);

    bool JPUnconditional(Gameboy &gameboy);

    template<JumpTest test>
    bool JP(Gameboy &gameboy);

    bool JPHL(Gameboy &gameboy);

    bool NOP(Gameboy &gameboy);

    bool PREFIX(Gameboy &gameboy) const;

    bool STOP(Gameboy &gameboy);

    template<Register source>
    bool DECRegister(Gameboy &gameboy) const;

    bool DECIndirect(Gameboy &gameboy);

    template<Arithmetic16Target target>
    bool DEC16(Gameboy &gameboy);

    template<Register source>
    bool INCRegister(Gameboy &gameboy) const;

    bool INCIndirect(Gameboy &gameboy);

    template<Arithmetic16Target target>
    bool INC16(Gameboy &gameboy);

    template<Register target, Register source>
    bool LDRegister(Gameboy &gameboy);

    template<Register source>
    bool LDRegisterImmediate(Gameboy &gameboy);

    template<Register source>
    bool LDRegisterIndirect(Gameboy &gameboy);

    template<Register source>
    bool LDAddrRegister(Gameboy &gameboy);

    bool LDAddrImmediate(Gameboy &gameboy);

    bool LDAccumulatorBC(Gameboy &gameboy);

    bool LDAccumulatorDE(Gameboy &gameboy);

    bool LDFromAccBC(Gameboy &gameboy);

    bool LDFromAccDE(Gameboy &gameboy);

    bool LDAccumulatorDirect(Gameboy &gameboy);

    bool LDFromAccumulatorDirect(Gameboy &gameboy);

    bool LDAccumulatorIndirectDec(Gameboy &gameboy);

    bool LDFromAccumulatorIndirectDec(Gameboy &gameboy);

    bool LDAccumulatorIndirectInc(Gameboy &gameboy);

    bool LDFromAccumulatorIndirectInc(Gameboy &gameboy);

    bool LoadFromAccumulatorIndirectC(Gameboy &gameboy);

    bool LoadFromAccumulatorDirectA(Gameboy &gameboy);

    bool LoadAccumulatorA(Gameboy &gameboy);

    bool LoadAccumulatorIndirectC(Gameboy &gameboy);

    template<LoadWordTarget target>
    bool LD16Register(Gameboy &gameboy);

    bool LD16FromStack(Gameboy &gameboy);

    bool LD16StackAdjusted(Gameboy &gameboy);

    bool LD16Stack(Gameboy &gameboy);

    template<StackTarget target>
    bool PUSH(Gameboy &gameboy);

    template<StackTarget target>
    bool POP(Gameboy &gameboy);

    template<Register source>
    bool CPRegister(Gameboy &gameboy);

    bool CPIndirect(Gameboy &gameboy);

    bool CPImmediate(Gameboy &gameboy);

    template<Register source>
    bool ORRegister(Gameboy &gameboy);

    bool ORIndirect(Gameboy &gameboy);

    bool ORImmediate(Gameboy &gameboy);

    template<Register source>
    bool XORRegister(Gameboy &gameboy);

    bool XORIndirect(Gameboy &gameboy);

    bool XORImmediate(Gameboy &gameboy);

    template<Register source>
    bool AND(Gameboy &gameboy);

    bool ANDIndirect(Gameboy &gameboy);

    bool ANDImmediate(Gameboy &gameboy);

    template<Register source>
    bool SUB(Gameboy &gameboy);

    bool SUBIndirect(Gameboy &gameboy);

    bool SUBImmediate(Gameboy &gameboy);

    template<Register source>
    bool RRC(Gameboy &gameboy);

    bool RRCAddr(Gameboy &gameboy);

    template<Register source>
    bool RLC(Gameboy &gameboy);

    bool RLCAddr(Gameboy &gameboy);

    template<Register source>
    bool RR(Gameboy &gameboy);

    bool RRAddr(Gameboy &gameboy);

    template<Register source>
    bool RL(Gameboy &gameboy);

    bool RLAddr(Gameboy &gameboy);

    template<Register source>
    bool SLA(Gameboy &gameboy);

    bool SLAAddr(Gameboy &gameboy);

    template<Register source>
    bool SRA(Gameboy &gameboy);

    bool SRAAddr(Gameboy &gameboy);

    template<Register source>
    bool SWAP(Gameboy &gameboy);

    bool SWAPAddr(Gameboy &gameboy);

    template<Register source>
    bool SRL(Gameboy &gameboy);

    bool SRLAddr(Gameboy &gameboy);

    template<Register source, int bit>
    bool BIT(Gameboy &gameboy);

    template<int bit>
    bool BITAddr(Gameboy &gameboy);

    template<Register source, int bit>
    bool RES(Gameboy &gameboy);

    template<int bit>
    bool RESAddr(Gameboy &gameboy);

    template<Register source, int bit>
    bool SET(Gameboy &gameboy);

    template<int bit>
    bool SETAddr(Gameboy &gameboy);

    template<Register source>
    bool SBCRegister(Gameboy &gameboy);

    bool SBCIndirect(Gameboy &gameboy);

    bool SBCImmediate(Gameboy &gameboy);

    template<Register source>
    bool ADCRegister(Gameboy &gameboy);

    bool ADCIndirect(Gameboy &gameboy);

    bool ADCImmediate(Gameboy &gameboy);

    template<Arithmetic16Target target>
    bool ADD16(Gameboy &gameboy);

    template<Register source>
    bool ADDRegister(Gameboy &gameboy);

    bool ADDIndirect(Gameboy &gameboy);

    bool ADDImmediate(Gameboy &gameboy);

    bool ADDSigned(Gameboy &gameboy);

public:
    using WrappedFunction = std::function<bool(Gameboy &)>;

    template<typename... Args>
    bool prefixedInstr(const uint8_t opcode, Args &&... args) {
        return prefixedTable[opcode](std::forward<Args>(args)...);
    }

    template<typename... Args>
    bool nonPrefixedInstr(const uint8_t opcode, Args &&... args) {
        return nonPrefixedTable[opcode](std::forward<Args>(args)...);
    }

    std::string GetMnemonic(uint16_t instruction) {
        const bool prefixed = instruction >> 8 == 0xCB;
        instruction &= 0xFF;
        return prefixed ? prefixedInstructions[instruction] : nonPrefixedInstructions[instruction];
    }

    const std::array<WrappedFunction, 256> prefixedTable = [this] {
        std::array<WrappedFunction, 256> table{};
        table[0x00] = [this](Gameboy &gb) -> bool { return this->RLC<Register::B>(gb); };
        table[0x01] = [this](Gameboy &gb) -> bool { return this->RLC<Register::C>(gb); };
        table[0x02] = [this](Gameboy &gb) -> bool { return this->RLC<Register::D>(gb); };
        table[0x03] = [this](Gameboy &gb) -> bool { return this->RLC<Register::E>(gb); };
        table[0x04] = [this](Gameboy &gb) -> bool { return this->RLC<Register::H>(gb); };
        table[0x05] = [this](Gameboy &gb) -> bool { return this->RLC<Register::L>(gb); };
        table[0x06] = [this](Gameboy &gb) -> bool { return this->RLCAddr(gb); };
        table[0x07] = [this](Gameboy &gb) -> bool { return this->RLC<Register::A>(gb); };
        table[0x08] = [this](Gameboy &gb) -> bool { return this->RRC<Register::B>(gb); };
        table[0x09] = [this](Gameboy &gb) -> bool { return this->RRC<Register::C>(gb); };
        table[0x0A] = [this](Gameboy &gb) -> bool { return this->RRC<Register::D>(gb); };
        table[0x0B] = [this](Gameboy &gb) -> bool { return this->RRC<Register::E>(gb); };
        table[0x0C] = [this](Gameboy &gb) -> bool { return this->RRC<Register::H>(gb); };
        table[0x0D] = [this](Gameboy &gb) -> bool { return this->RRC<Register::L>(gb); };
        table[0x0E] = [this](Gameboy &gb) -> bool { return this->RRCAddr(gb); };
        table[0x0F] = [this](Gameboy &gb) -> bool { return this->RRC<Register::A>(gb); };
        table[0x10] = [this](Gameboy &gb) -> bool { return this->RL<Register::B>(gb); };
        table[0x11] = [this](Gameboy &gb) -> bool { return this->RL<Register::C>(gb); };
        table[0x12] = [this](Gameboy &gb) -> bool { return this->RL<Register::D>(gb); };
        table[0x13] = [this](Gameboy &gb) -> bool { return this->RL<Register::E>(gb); };
        table[0x14] = [this](Gameboy &gb) -> bool { return this->RL<Register::H>(gb); };
        table[0x15] = [this](Gameboy &gb) -> bool { return this->RL<Register::L>(gb); };
        table[0x16] = [this](Gameboy &gb) -> bool { return this->RLAddr(gb); };
        table[0x17] = [this](Gameboy &gb) -> bool { return this->RL<Register::A>(gb); };
        table[0x18] = [this](Gameboy &gb) -> bool { return this->RR<Register::B>(gb); };
        table[0x19] = [this](Gameboy &gb) -> bool { return this->RR<Register::C>(gb); };
        table[0x1A] = [this](Gameboy &gb) -> bool { return this->RR<Register::D>(gb); };
        table[0x1B] = [this](Gameboy &gb) -> bool { return this->RR<Register::E>(gb); };
        table[0x1C] = [this](Gameboy &gb) -> bool { return this->RR<Register::H>(gb); };
        table[0x1D] = [this](Gameboy &gb) -> bool { return this->RR<Register::L>(gb); };
        table[0x1E] = [this](Gameboy &gb) -> bool { return this->RRAddr(gb); };
        table[0x1F] = [this](Gameboy &gb) -> bool { return this->RR<Register::A>(gb); };
        table[0x20] = [this](Gameboy &gb) -> bool { return this->SLA<Register::B>(gb); };
        table[0x21] = [this](Gameboy &gb) -> bool { return this->SLA<Register::C>(gb); };
        table[0x22] = [this](Gameboy &gb) -> bool { return this->SLA<Register::D>(gb); };
        table[0x23] = [this](Gameboy &gb) -> bool { return this->SLA<Register::E>(gb); };
        table[0x24] = [this](Gameboy &gb) -> bool { return this->SLA<Register::H>(gb); };
        table[0x25] = [this](Gameboy &gb) -> bool { return this->SLA<Register::L>(gb); };
        table[0x26] = [this](Gameboy &gb) -> bool { return this->SLAAddr(gb); };
        table[0x27] = [this](Gameboy &gb) -> bool { return this->SLA<Register::A>(gb); };
        table[0x28] = [this](Gameboy &gb) -> bool { return this->SRA<Register::B>(gb); };
        table[0x29] = [this](Gameboy &gb) -> bool { return this->SRA<Register::C>(gb); };
        table[0x2A] = [this](Gameboy &gb) -> bool { return this->SRA<Register::D>(gb); };
        table[0x2B] = [this](Gameboy &gb) -> bool { return this->SRA<Register::E>(gb); };
        table[0x2C] = [this](Gameboy &gb) -> bool { return this->SRA<Register::H>(gb); };
        table[0x2D] = [this](Gameboy &gb) -> bool { return this->SRA<Register::L>(gb); };
        table[0x2E] = [this](Gameboy &gb) -> bool { return this->SRAAddr(gb); };
        table[0x2F] = [this](Gameboy &gb) -> bool { return this->SRA<Register::A>(gb); };
        table[0x30] = [this](Gameboy &gb) -> bool { return this->SWAP<Register::B>(gb); };
        table[0x31] = [this](Gameboy &gb) -> bool { return this->SWAP<Register::C>(gb); };
        table[0x32] = [this](Gameboy &gb) -> bool { return this->SWAP<Register::D>(gb); };
        table[0x33] = [this](Gameboy &gb) -> bool { return this->SWAP<Register::E>(gb); };
        table[0x34] = [this](Gameboy &gb) -> bool { return this->SWAP<Register::H>(gb); };
        table[0x35] = [this](Gameboy &gb) -> bool { return this->SWAP<Register::L>(gb); };
        table[0x36] = [this](Gameboy &gb) -> bool { return this->SWAPAddr(gb); };
        table[0x37] = [this](Gameboy &gb) -> bool { return this->SWAP<Register::A>(gb); };
        table[0x38] = [this](Gameboy &gb) -> bool { return this->SRL<Register::B>(gb); };
        table[0x39] = [this](Gameboy &gb) -> bool { return this->SRL<Register::C>(gb); };
        table[0x3A] = [this](Gameboy &gb) -> bool { return this->SRL<Register::D>(gb); };
        table[0x3B] = [this](Gameboy &gb) -> bool { return this->SRL<Register::E>(gb); };
        table[0x3C] = [this](Gameboy &gb) -> bool { return this->SRL<Register::H>(gb); };
        table[0x3D] = [this](Gameboy &gb) -> bool { return this->SRL<Register::L>(gb); };
        table[0x3E] = [this](Gameboy &gb) -> bool { return this->SRLAddr(gb); };
        table[0x3F] = [this](Gameboy &gb) -> bool { return this->SRL<Register::A>(gb); };
        table[0x40] = [this](Gameboy &gb) -> bool { return this->BIT<Register::B, 0>(gb); };
        table[0x41] = [this](Gameboy &gb) -> bool { return this->BIT<Register::C, 0>(gb); };
        table[0x42] = [this](Gameboy &gb) -> bool { return this->BIT<Register::D, 0>(gb); };
        table[0x43] = [this](Gameboy &gb) -> bool { return this->BIT<Register::E, 0>(gb); };
        table[0x44] = [this](Gameboy &gb) -> bool { return this->BIT<Register::H, 0>(gb); };
        table[0x45] = [this](Gameboy &gb) -> bool { return this->BIT<Register::L, 0>(gb); };
        table[0x46] = [this](Gameboy &gb) -> bool { return this->BITAddr<0>(gb); };
        table[0x47] = [this](Gameboy &gb) -> bool { return this->BIT<Register::A, 0>(gb); };
        table[0x48] = [this](Gameboy &gb) -> bool { return this->BIT<Register::B, 1>(gb); };
        table[0x49] = [this](Gameboy &gb) -> bool { return this->BIT<Register::C, 1>(gb); };
        table[0x4A] = [this](Gameboy &gb) -> bool { return this->BIT<Register::D, 1>(gb); };
        table[0x4B] = [this](Gameboy &gb) -> bool { return this->BIT<Register::E, 1>(gb); };
        table[0x4C] = [this](Gameboy &gb) -> bool { return this->BIT<Register::H, 1>(gb); };
        table[0x4D] = [this](Gameboy &gb) -> bool { return this->BIT<Register::L, 1>(gb); };
        table[0x4E] = [this](Gameboy &gb) -> bool { return this->BITAddr<1>(gb); };
        table[0x4F] = [this](Gameboy &gb) -> bool { return this->BIT<Register::A, 1>(gb); };
        table[0x50] = [this](Gameboy &gb) -> bool { return this->BIT<Register::B, 2>(gb); };
        table[0x51] = [this](Gameboy &gb) -> bool { return this->BIT<Register::C, 2>(gb); };
        table[0x52] = [this](Gameboy &gb) -> bool { return this->BIT<Register::D, 2>(gb); };
        table[0x53] = [this](Gameboy &gb) -> bool { return this->BIT<Register::E, 2>(gb); };
        table[0x54] = [this](Gameboy &gb) -> bool { return this->BIT<Register::H, 2>(gb); };
        table[0x55] = [this](Gameboy &gb) -> bool { return this->BIT<Register::L, 2>(gb); };
        table[0x56] = [this](Gameboy &gb) -> bool { return this->BITAddr<2>(gb); };
        table[0x57] = [this](Gameboy &gb) -> bool { return this->BIT<Register::A, 2>(gb); };
        table[0x58] = [this](Gameboy &gb) -> bool { return this->BIT<Register::B, 3>(gb); };
        table[0x59] = [this](Gameboy &gb) -> bool { return this->BIT<Register::C, 3>(gb); };
        table[0x5A] = [this](Gameboy &gb) -> bool { return this->BIT<Register::D, 3>(gb); };
        table[0x5B] = [this](Gameboy &gb) -> bool { return this->BIT<Register::E, 3>(gb); };
        table[0x5C] = [this](Gameboy &gb) -> bool { return this->BIT<Register::H, 3>(gb); };
        table[0x5D] = [this](Gameboy &gb) -> bool { return this->BIT<Register::L, 3>(gb); };
        table[0x5E] = [this](Gameboy &gb) -> bool { return this->BITAddr<3>(gb); };
        table[0x5F] = [this](Gameboy &gb) -> bool { return this->BIT<Register::A, 3>(gb); };
        table[0x60] = [this](Gameboy &gb) -> bool { return this->BIT<Register::B, 4>(gb); };
        table[0x61] = [this](Gameboy &gb) -> bool { return this->BIT<Register::C, 4>(gb); };
        table[0x62] = [this](Gameboy &gb) -> bool { return this->BIT<Register::D, 4>(gb); };
        table[0x63] = [this](Gameboy &gb) -> bool { return this->BIT<Register::E, 4>(gb); };
        table[0x64] = [this](Gameboy &gb) -> bool { return this->BIT<Register::H, 4>(gb); };
        table[0x65] = [this](Gameboy &gb) -> bool { return this->BIT<Register::L, 4>(gb); };
        table[0x66] = [this](Gameboy &gb) -> bool { return this->BITAddr<4>(gb); };
        table[0x67] = [this](Gameboy &gb) -> bool { return this->BIT<Register::A, 4>(gb); };
        table[0x68] = [this](Gameboy &gb) -> bool { return this->BIT<Register::B, 5>(gb); };
        table[0x69] = [this](Gameboy &gb) -> bool { return this->BIT<Register::C, 5>(gb); };
        table[0x6A] = [this](Gameboy &gb) -> bool { return this->BIT<Register::D, 5>(gb); };
        table[0x6B] = [this](Gameboy &gb) -> bool { return this->BIT<Register::E, 5>(gb); };
        table[0x6C] = [this](Gameboy &gb) -> bool { return this->BIT<Register::H, 5>(gb); };
        table[0x6D] = [this](Gameboy &gb) -> bool { return this->BIT<Register::L, 5>(gb); };
        table[0x6E] = [this](Gameboy &gb) -> bool { return this->BITAddr<5>(gb); };
        table[0x6F] = [this](Gameboy &gb) -> bool { return this->BIT<Register::A, 5>(gb); };
        table[0x70] = [this](Gameboy &gb) -> bool { return this->BIT<Register::B, 6>(gb); };
        table[0x71] = [this](Gameboy &gb) -> bool { return this->BIT<Register::C, 6>(gb); };
        table[0x72] = [this](Gameboy &gb) -> bool { return this->BIT<Register::D, 6>(gb); };
        table[0x73] = [this](Gameboy &gb) -> bool { return this->BIT<Register::E, 6>(gb); };
        table[0x74] = [this](Gameboy &gb) -> bool { return this->BIT<Register::H, 6>(gb); };
        table[0x75] = [this](Gameboy &gb) -> bool { return this->BIT<Register::L, 6>(gb); };
        table[0x76] = [this](Gameboy &gb) -> bool { return this->BITAddr<6>(gb); };
        table[0x77] = [this](Gameboy &gb) -> bool { return this->BIT<Register::A, 6>(gb); };
        table[0x78] = [this](Gameboy &gb) -> bool { return this->BIT<Register::B, 7>(gb); };
        table[0x79] = [this](Gameboy &gb) -> bool { return this->BIT<Register::C, 7>(gb); };
        table[0x7A] = [this](Gameboy &gb) -> bool { return this->BIT<Register::D, 7>(gb); };
        table[0x7B] = [this](Gameboy &gb) -> bool { return this->BIT<Register::E, 7>(gb); };
        table[0x7C] = [this](Gameboy &gb) -> bool { return this->BIT<Register::H, 7>(gb); };
        table[0x7D] = [this](Gameboy &gb) -> bool { return this->BIT<Register::L, 7>(gb); };
        table[0x7E] = [this](Gameboy &gb) -> bool { return this->BITAddr<7>(gb); };
        table[0x7F] = [this](Gameboy &gb) -> bool { return this->BIT<Register::A, 7>(gb); };
        table[0x80] = [this](Gameboy &gb) -> bool { return this->RES<Register::B, 0>(gb); };
        table[0x81] = [this](Gameboy &gb) -> bool { return this->RES<Register::C, 0>(gb); };
        table[0x82] = [this](Gameboy &gb) -> bool { return this->RES<Register::D, 0>(gb); };
        table[0x83] = [this](Gameboy &gb) -> bool { return this->RES<Register::E, 0>(gb); };
        table[0x84] = [this](Gameboy &gb) -> bool { return this->RES<Register::H, 0>(gb); };
        table[0x85] = [this](Gameboy &gb) -> bool { return this->RES<Register::L, 0>(gb); };
        table[0x86] = [this](Gameboy &gb) -> bool { return this->RESAddr<0>(gb); };
        table[0x87] = [this](Gameboy &gb) -> bool { return this->RES<Register::A, 0>(gb); };
        table[0x88] = [this](Gameboy &gb) -> bool { return this->RES<Register::B, 1>(gb); };
        table[0x89] = [this](Gameboy &gb) -> bool { return this->RES<Register::C, 1>(gb); };
        table[0x8A] = [this](Gameboy &gb) -> bool { return this->RES<Register::D, 1>(gb); };
        table[0x8B] = [this](Gameboy &gb) -> bool { return this->RES<Register::E, 1>(gb); };
        table[0x8C] = [this](Gameboy &gb) -> bool { return this->RES<Register::H, 1>(gb); };
        table[0x8D] = [this](Gameboy &gb) -> bool { return this->RES<Register::L, 1>(gb); };
        table[0x8E] = [this](Gameboy &gb) -> bool { return this->RESAddr<1>(gb); };
        table[0x8F] = [this](Gameboy &gb) -> bool { return this->RES<Register::A, 1>(gb); };
        table[0x90] = [this](Gameboy &gb) -> bool { return this->RES<Register::B, 2>(gb); };
        table[0x91] = [this](Gameboy &gb) -> bool { return this->RES<Register::C, 2>(gb); };
        table[0x92] = [this](Gameboy &gb) -> bool { return this->RES<Register::D, 2>(gb); };
        table[0x93] = [this](Gameboy &gb) -> bool { return this->RES<Register::E, 2>(gb); };
        table[0x94] = [this](Gameboy &gb) -> bool { return this->RES<Register::H, 2>(gb); };
        table[0x95] = [this](Gameboy &gb) -> bool { return this->RES<Register::L, 2>(gb); };
        table[0x96] = [this](Gameboy &gb) -> bool { return this->RESAddr<2>(gb); };
        table[0x97] = [this](Gameboy &gb) -> bool { return this->RES<Register::A, 2>(gb); };
        table[0x98] = [this](Gameboy &gb) -> bool { return this->RES<Register::B, 3>(gb); };
        table[0x99] = [this](Gameboy &gb) -> bool { return this->RES<Register::C, 3>(gb); };
        table[0x9A] = [this](Gameboy &gb) -> bool { return this->RES<Register::D, 3>(gb); };
        table[0x9B] = [this](Gameboy &gb) -> bool { return this->RES<Register::E, 3>(gb); };
        table[0x9C] = [this](Gameboy &gb) -> bool { return this->RES<Register::H, 3>(gb); };
        table[0x9D] = [this](Gameboy &gb) -> bool { return this->RES<Register::L, 3>(gb); };
        table[0x9E] = [this](Gameboy &gb) -> bool { return this->RESAddr<3>(gb); };
        table[0x9F] = [this](Gameboy &gb) -> bool { return this->RES<Register::A, 3>(gb); };
        table[0xA0] = [this](Gameboy &gb) -> bool { return this->RES<Register::B, 4>(gb); };
        table[0xA1] = [this](Gameboy &gb) -> bool { return this->RES<Register::C, 4>(gb); };
        table[0xA2] = [this](Gameboy &gb) -> bool { return this->RES<Register::D, 4>(gb); };
        table[0xA3] = [this](Gameboy &gb) -> bool { return this->RES<Register::E, 4>(gb); };
        table[0xA4] = [this](Gameboy &gb) -> bool { return this->RES<Register::H, 4>(gb); };
        table[0xA5] = [this](Gameboy &gb) -> bool { return this->RES<Register::L, 4>(gb); };
        table[0xA6] = [this](Gameboy &gb) -> bool { return this->RESAddr<4>(gb); };
        table[0xA7] = [this](Gameboy &gb) -> bool { return this->RES<Register::A, 4>(gb); };
        table[0xA8] = [this](Gameboy &gb) -> bool { return this->RES<Register::B, 5>(gb); };
        table[0xA9] = [this](Gameboy &gb) -> bool { return this->RES<Register::C, 5>(gb); };
        table[0xAA] = [this](Gameboy &gb) -> bool { return this->RES<Register::D, 5>(gb); };
        table[0xAB] = [this](Gameboy &gb) -> bool { return this->RES<Register::E, 5>(gb); };
        table[0xAC] = [this](Gameboy &gb) -> bool { return this->RES<Register::H, 5>(gb); };
        table[0xAD] = [this](Gameboy &gb) -> bool { return this->RES<Register::L, 5>(gb); };
        table[0xAE] = [this](Gameboy &gb) -> bool { return this->RESAddr<5>(gb); };
        table[0xAF] = [this](Gameboy &gb) -> bool { return this->RES<Register::A, 5>(gb); };
        table[0xB0] = [this](Gameboy &gb) -> bool { return this->RES<Register::B, 6>(gb); };
        table[0xB1] = [this](Gameboy &gb) -> bool { return this->RES<Register::C, 6>(gb); };
        table[0xB2] = [this](Gameboy &gb) -> bool { return this->RES<Register::D, 6>(gb); };
        table[0xB3] = [this](Gameboy &gb) -> bool { return this->RES<Register::E, 6>(gb); };
        table[0xB4] = [this](Gameboy &gb) -> bool { return this->RES<Register::H, 6>(gb); };
        table[0xB5] = [this](Gameboy &gb) -> bool { return this->RES<Register::L, 6>(gb); };
        table[0xB6] = [this](Gameboy &gb) -> bool { return this->RESAddr<6>(gb); };
        table[0xB7] = [this](Gameboy &gb) -> bool { return this->RES<Register::A, 6>(gb); };
        table[0xB8] = [this](Gameboy &gb) -> bool { return this->RES<Register::B, 7>(gb); };
        table[0xB9] = [this](Gameboy &gb) -> bool { return this->RES<Register::C, 7>(gb); };
        table[0xBA] = [this](Gameboy &gb) -> bool { return this->RES<Register::D, 7>(gb); };
        table[0xBB] = [this](Gameboy &gb) -> bool { return this->RES<Register::E, 7>(gb); };
        table[0xBC] = [this](Gameboy &gb) -> bool { return this->RES<Register::H, 7>(gb); };
        table[0xBD] = [this](Gameboy &gb) -> bool { return this->RES<Register::L, 7>(gb); };
        table[0xBE] = [this](Gameboy &gb) -> bool { return this->RESAddr<7>(gb); };
        table[0xBF] = [this](Gameboy &gb) -> bool { return this->RES<Register::A, 7>(gb); };
        table[0xC0] = [this](Gameboy &gb) -> bool { return this->SET<Register::B, 0>(gb); };
        table[0xC1] = [this](Gameboy &gb) -> bool { return this->SET<Register::C, 0>(gb); };
        table[0xC2] = [this](Gameboy &gb) -> bool { return this->SET<Register::D, 0>(gb); };
        table[0xC3] = [this](Gameboy &gb) -> bool { return this->SET<Register::E, 0>(gb); };
        table[0xC4] = [this](Gameboy &gb) -> bool { return this->SET<Register::H, 0>(gb); };
        table[0xC5] = [this](Gameboy &gb) -> bool { return this->SET<Register::L, 0>(gb); };
        table[0xC6] = [this](Gameboy &gb) -> bool { return this->SETAddr<0>(gb); };
        table[0xC7] = [this](Gameboy &gb) -> bool { return this->SET<Register::A, 0>(gb); };
        table[0xC8] = [this](Gameboy &gb) -> bool { return this->SET<Register::B, 1>(gb); };
        table[0xC9] = [this](Gameboy &gb) -> bool { return this->SET<Register::C, 1>(gb); };
        table[0xCA] = [this](Gameboy &gb) -> bool { return this->SET<Register::D, 1>(gb); };
        table[0xCB] = [this](Gameboy &gb) -> bool { return this->SET<Register::E, 1>(gb); };
        table[0xCC] = [this](Gameboy &gb) -> bool { return this->SET<Register::H, 1>(gb); };
        table[0xCD] = [this](Gameboy &gb) -> bool { return this->SET<Register::L, 1>(gb); };
        table[0xCE] = [this](Gameboy &gb) -> bool { return this->SETAddr<1>(gb); };
        table[0xCF] = [this](Gameboy &gb) -> bool { return this->SET<Register::A, 1>(gb); };
        table[0xD0] = [this](Gameboy &gb) -> bool { return this->SET<Register::B, 2>(gb); };
        table[0xD1] = [this](Gameboy &gb) -> bool { return this->SET<Register::C, 2>(gb); };
        table[0xD2] = [this](Gameboy &gb) -> bool { return this->SET<Register::D, 2>(gb); };
        table[0xD3] = [this](Gameboy &gb) -> bool { return this->SET<Register::E, 2>(gb); };
        table[0xD4] = [this](Gameboy &gb) -> bool { return this->SET<Register::H, 2>(gb); };
        table[0xD5] = [this](Gameboy &gb) -> bool { return this->SET<Register::L, 2>(gb); };
        table[0xD6] = [this](Gameboy &gb) -> bool { return this->SETAddr<2>(gb); };
        table[0xD7] = [this](Gameboy &gb) -> bool { return this->SET<Register::A, 2>(gb); };
        table[0xD8] = [this](Gameboy &gb) -> bool { return this->SET<Register::B, 3>(gb); };
        table[0xD9] = [this](Gameboy &gb) -> bool { return this->SET<Register::C, 3>(gb); };
        table[0xDA] = [this](Gameboy &gb) -> bool { return this->SET<Register::D, 3>(gb); };
        table[0xDB] = [this](Gameboy &gb) -> bool { return this->SET<Register::E, 3>(gb); };
        table[0xDC] = [this](Gameboy &gb) -> bool { return this->SET<Register::H, 3>(gb); };
        table[0xDD] = [this](Gameboy &gb) -> bool { return this->SET<Register::L, 3>(gb); };
        table[0xDE] = [this](Gameboy &gb) -> bool { return this->SETAddr<3>(gb); };
        table[0xDF] = [this](Gameboy &gb) -> bool { return this->SET<Register::A, 3>(gb); };
        table[0xE0] = [this](Gameboy &gb) -> bool { return this->SET<Register::B, 4>(gb); };
        table[0xE1] = [this](Gameboy &gb) -> bool { return this->SET<Register::C, 4>(gb); };
        table[0xE2] = [this](Gameboy &gb) -> bool { return this->SET<Register::D, 4>(gb); };
        table[0xE3] = [this](Gameboy &gb) -> bool { return this->SET<Register::E, 4>(gb); };
        table[0xE4] = [this](Gameboy &gb) -> bool { return this->SET<Register::H, 4>(gb); };
        table[0xE5] = [this](Gameboy &gb) -> bool { return this->SET<Register::L, 4>(gb); };
        table[0xE6] = [this](Gameboy &gb) -> bool { return this->SETAddr<4>(gb); };
        table[0xE7] = [this](Gameboy &gb) -> bool { return this->SET<Register::A, 4>(gb); };
        table[0xE8] = [this](Gameboy &gb) -> bool { return this->SET<Register::B, 5>(gb); };
        table[0xE9] = [this](Gameboy &gb) -> bool { return this->SET<Register::C, 5>(gb); };
        table[0xEA] = [this](Gameboy &gb) -> bool { return this->SET<Register::D, 5>(gb); };
        table[0xEB] = [this](Gameboy &gb) -> bool { return this->SET<Register::E, 5>(gb); };
        table[0xEC] = [this](Gameboy &gb) -> bool { return this->SET<Register::H, 5>(gb); };
        table[0xED] = [this](Gameboy &gb) -> bool { return this->SET<Register::L, 5>(gb); };
        table[0xEE] = [this](Gameboy &gb) -> bool { return this->SETAddr<5>(gb); };
        table[0xEF] = [this](Gameboy &gb) -> bool { return this->SET<Register::A, 5>(gb); };
        table[0xF0] = [this](Gameboy &gb) -> bool { return this->SET<Register::B, 6>(gb); };
        table[0xF1] = [this](Gameboy &gb) -> bool { return this->SET<Register::C, 6>(gb); };
        table[0xF2] = [this](Gameboy &gb) -> bool { return this->SET<Register::D, 6>(gb); };
        table[0xF3] = [this](Gameboy &gb) -> bool { return this->SET<Register::E, 6>(gb); };
        table[0xF4] = [this](Gameboy &gb) -> bool { return this->SET<Register::H, 6>(gb); };
        table[0xF5] = [this](Gameboy &gb) -> bool { return this->SET<Register::L, 6>(gb); };
        table[0xF6] = [this](Gameboy &gb) -> bool { return this->SETAddr<6>(gb); };
        table[0xF7] = [this](Gameboy &gb) -> bool { return this->SET<Register::A, 6>(gb); };
        table[0xF8] = [this](Gameboy &gb) -> bool { return this->SET<Register::B, 7>(gb); };
        table[0xF9] = [this](Gameboy &gb) -> bool { return this->SET<Register::C, 7>(gb); };
        table[0xFA] = [this](Gameboy &gb) -> bool { return this->SET<Register::D, 7>(gb); };
        table[0xFB] = [this](Gameboy &gb) -> bool { return this->SET<Register::E, 7>(gb); };
        table[0xFC] = [this](Gameboy &gb) -> bool { return this->SET<Register::H, 7>(gb); };
        table[0xFD] = [this](Gameboy &gb) -> bool { return this->SET<Register::L, 7>(gb); };
        table[0xFE] = [this](Gameboy &gb) -> bool { return this->SETAddr<7>(gb); };
        table[0xFF] = [this](Gameboy &gb) -> bool { return this->SET<Register::A, 7>(gb); };
        return table;
    }();

    const std::array<WrappedFunction, 256> nonPrefixedTable = [this] {
        std::array<WrappedFunction, 256> table{};
        table[0x00] = [this](Gameboy &gb) -> bool { return this->NOP(gb); };
        table[0x01] = [this](Gameboy &gb) -> bool { return this->LD16Register<LoadWordTarget::BC>(gb); };
        table[0x02] = [this](Gameboy &gb) -> bool { return this->LDFromAccBC(gb); };
        table[0x03] = [this](Gameboy &gb) -> bool { return this->INC16<Arithmetic16Target::BC>(gb); };
        table[0x04] = [this](Gameboy &gb) -> bool { return this->INCRegister<Register::B>(gb); };
        table[0x05] = [this](Gameboy &gb) -> bool { return this->DECRegister<Register::B>(gb); };
        table[0x06] = [this](Gameboy &gb) -> bool { return this->LDRegisterImmediate<Register::B>(gb); };
        table[0x07] = [this](Gameboy &gb) -> bool { return this->RLCA(gb); };
        table[0x08] = [this](Gameboy &gb) -> bool { return this->LD16FromStack(gb); };
        table[0x09] = [this](Gameboy &gb) -> bool { return this->ADD16<Arithmetic16Target::BC>(gb); };
        table[0x10] = [this](Gameboy &gb) -> bool { return this->STOP(gb); };
        table[0x0A] = [this](Gameboy &gb) -> bool { return this->LDAccumulatorBC(gb); };
        table[0x0B] = [this](Gameboy &gb) -> bool { return this->DEC16<Arithmetic16Target::BC>(gb); };
        table[0x0C] = [this](Gameboy &gb) -> bool { return this->INCRegister<Register::C>(gb); };
        table[0x0D] = [this](Gameboy &gb) -> bool { return this->DECRegister<Register::C>(gb); };
        table[0x0E] = [this](Gameboy &gb) -> bool { return this->LDRegisterImmediate<Register::C>(gb); };
        table[0x0F] = [this](Gameboy &gb) -> bool { return this->RRCA(gb); };
        table[0x11] = [this](Gameboy &gb) -> bool { return this->LD16Register<LoadWordTarget::DE>(gb); };
        table[0x12] = [this](Gameboy &gb) -> bool { return this->LDFromAccDE(gb); };
        table[0x13] = [this](Gameboy &gb) -> bool { return this->INC16<Arithmetic16Target::DE>(gb); };
        table[0x14] = [this](Gameboy &gb) -> bool { return this->INCRegister<Register::D>(gb); };
        table[0x15] = [this](Gameboy &gb) -> bool { return this->DECRegister<Register::D>(gb); };
        table[0x16] = [this](Gameboy &gb) -> bool { return this->LDRegisterImmediate<Register::D>(gb); };
        table[0x17] = [this](Gameboy &gb) -> bool { return this->RLA(gb); };
        table[0x18] = [this](Gameboy &gb) -> bool { return this->JRUnconditional(gb); };
        table[0x19] = [this](Gameboy &gb) -> bool { return this->ADD16<Arithmetic16Target::DE>(gb); };
        table[0x1A] = [this](Gameboy &gb) -> bool { return this->LDAccumulatorDE(gb); };
        table[0x1B] = [this](Gameboy &gb) -> bool { return this->DEC16<Arithmetic16Target::DE>(gb); };
        table[0x1C] = [this](Gameboy &gb) -> bool { return this->INCRegister<Register::E>(gb); };
        table[0x1D] = [this](Gameboy &gb) -> bool { return this->DECRegister<Register::E>(gb); };
        table[0x1E] = [this](Gameboy &gb) -> bool { return this->LDRegisterImmediate<Register::E>(gb); };
        table[0x1F] = [this](Gameboy &gb) -> bool { return this->RRA(gb); };
        table[0x20] = [this](Gameboy &gb) -> bool { return this->JR<JumpTest::NotZero>(gb); };
        table[0x21] = [this](Gameboy &gb) -> bool { return this->LD16Register<LoadWordTarget::HL>(gb); };
        table[0x22] = [this](Gameboy &gb) -> bool { return this->LDFromAccumulatorIndirectInc(gb); };
        table[0x23] = [this](Gameboy &gb) -> bool { return this->INC16<Arithmetic16Target::HL>(gb); };
        table[0x24] = [this](Gameboy &gb) -> bool { return this->INCRegister<Register::H>(gb); };
        table[0x25] = [this](Gameboy &gb) -> bool { return this->DECRegister<Register::H>(gb); };
        table[0x26] = [this](Gameboy &gb) -> bool { return this->LDRegisterImmediate<Register::H>(gb); };
        table[0x27] = [this](Gameboy &gb) -> bool { return this->DAA(gb); };
        table[0x28] = [this](Gameboy &gb) -> bool { return this->JR<JumpTest::Zero>(gb); };
        table[0x29] = [this](Gameboy &gb) -> bool { return this->ADD16<Arithmetic16Target::HL>(gb); };
        table[0x2A] = [this](Gameboy &gb) -> bool { return this->LDAccumulatorIndirectInc(gb); };
        table[0x2B] = [this](Gameboy &gb) -> bool { return this->DEC16<Arithmetic16Target::HL>(gb); };
        table[0x2C] = [this](Gameboy &gb) -> bool { return this->INCRegister<Register::L>(gb); };
        table[0x2D] = [this](Gameboy &gb) -> bool { return this->DECRegister<Register::L>(gb); };
        table[0x2E] = [this](Gameboy &gb) -> bool { return this->LDRegisterImmediate<Register::L>(gb); };
        table[0x2F] = [this](Gameboy &gb) -> bool { return this->CPL(gb); };
        table[0x30] = [this](Gameboy &gb) -> bool { return this->JR<JumpTest::NotCarry>(gb); };
        table[0x31] = [this](Gameboy &gb) -> bool { return this->LD16Register<LoadWordTarget::SP>(gb); };
        table[0x32] = [this](Gameboy &gb) -> bool { return this->LDFromAccumulatorIndirectDec(gb); };
        table[0x33] = [this](Gameboy &gb) -> bool { return this->INC16<Arithmetic16Target::SP>(gb); };
        table[0x34] = [this](Gameboy &gb) -> bool { return this->INCIndirect(gb); };
        table[0x35] = [this](Gameboy &gb) -> bool { return this->DECIndirect(gb); };
        table[0x36] = [this](Gameboy &gb) -> bool { return this->LDAddrImmediate(gb); };
        table[0x37] = [this](Gameboy &gb) -> bool { return this->SCF(gb); };
        table[0x38] = [this](Gameboy &gb) -> bool { return this->JR<JumpTest::Carry>(gb); };
        table[0x39] = [this](Gameboy &gb) -> bool { return this->ADD16<Arithmetic16Target::SP>(gb); };
        table[0x3A] = [this](Gameboy &gb) -> bool { return this->LDAccumulatorIndirectDec(gb); };
        table[0x3B] = [this](Gameboy &gb) -> bool { return this->DEC16<Arithmetic16Target::SP>(gb); };
        table[0x3C] = [this](Gameboy &gb) -> bool { return this->INCRegister<Register::A>(gb); };
        table[0x3D] = [this](Gameboy &gb) -> bool { return this->DECRegister<Register::A>(gb); };
        table[0x3E] = [this](Gameboy &gb) -> bool { return this->LDRegisterImmediate<Register::A>(gb); };
        table[0x3F] = [this](Gameboy &gb) -> bool { return this->CCF(gb); };
        table[0x40] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::B, Register::B>(gb); };
        table[0x41] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::B, Register::C>(gb); };
        table[0x42] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::B, Register::D>(gb); };
        table[0x43] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::B, Register::E>(gb); };
        table[0x44] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::B, Register::H>(gb); };
        table[0x45] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::B, Register::L>(gb); };
        table[0x46] = [this](Gameboy &gb) -> bool { return this->LDRegisterIndirect<Register::B>(gb); };
        table[0x47] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::B, Register::A>(gb); };
        table[0x48] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::C, Register::B>(gb); };
        table[0x49] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::C, Register::C>(gb); };
        table[0x4A] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::C, Register::D>(gb); };
        table[0x4B] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::C, Register::E>(gb); };
        table[0x4C] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::C, Register::H>(gb); };
        table[0x4D] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::C, Register::L>(gb); };
        table[0x4E] = [this](Gameboy &gb) -> bool { return this->LDRegisterIndirect<Register::C>(gb); };
        table[0x4F] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::C, Register::A>(gb); };
        table[0x50] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::D, Register::B>(gb); };
        table[0x51] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::D, Register::C>(gb); };
        table[0x52] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::D, Register::D>(gb); };
        table[0x53] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::D, Register::E>(gb); };
        table[0x54] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::D, Register::H>(gb); };
        table[0x55] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::D, Register::L>(gb); };
        table[0x56] = [this](Gameboy &gb) -> bool { return this->LDRegisterIndirect<Register::D>(gb); };
        table[0x57] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::D, Register::A>(gb); };
        table[0x58] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::E, Register::B>(gb); };
        table[0x59] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::E, Register::C>(gb); };
        table[0x5A] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::E, Register::D>(gb); };
        table[0x5B] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::E, Register::E>(gb); };
        table[0x5C] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::E, Register::H>(gb); };
        table[0x5D] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::E, Register::L>(gb); };
        table[0x5E] = [this](Gameboy &gb) -> bool { return this->LDRegisterIndirect<Register::E>(gb); };
        table[0x5F] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::E, Register::A>(gb); };
        table[0x60] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::H, Register::B>(gb); };
        table[0x61] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::H, Register::C>(gb); };
        table[0x62] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::H, Register::D>(gb); };
        table[0x63] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::H, Register::E>(gb); };
        table[0x64] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::H, Register::H>(gb); };
        table[0x65] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::H, Register::L>(gb); };
        table[0x66] = [this](Gameboy &gb) -> bool { return this->LDRegisterIndirect<Register::H>(gb); };
        table[0x67] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::H, Register::A>(gb); };
        table[0x68] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::L, Register::B>(gb); };
        table[0x69] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::L, Register::C>(gb); };
        table[0x6A] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::L, Register::D>(gb); };
        table[0x6B] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::L, Register::E>(gb); };
        table[0x6C] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::L, Register::H>(gb); };
        table[0x6D] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::L, Register::L>(gb); };
        table[0x6E] = [this](Gameboy &gb) -> bool { return this->LDRegisterIndirect<Register::L>(gb); };
        table[0x6F] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::L, Register::A>(gb); };
        table[0x70] = [this](Gameboy &gb) -> bool { return this->LDAddrRegister<Register::B>(gb); };
        table[0x71] = [this](Gameboy &gb) -> bool { return this->LDAddrRegister<Register::C>(gb); };
        table[0x72] = [this](Gameboy &gb) -> bool { return this->LDAddrRegister<Register::D>(gb); };
        table[0x73] = [this](Gameboy &gb) -> bool { return this->LDAddrRegister<Register::E>(gb); };
        table[0x74] = [this](Gameboy &gb) -> bool { return this->LDAddrRegister<Register::H>(gb); };
        table[0x75] = [this](Gameboy &gb) -> bool { return this->LDAddrRegister<Register::L>(gb); };
        table[0x76] = [this](Gameboy &gb) -> bool { return this->HALT(gb); };
        table[0x77] = [this](Gameboy &gb) -> bool { return this->LDAddrRegister<Register::A>(gb); };
        table[0x78] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::A, Register::B>(gb); };
        table[0x79] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::A, Register::C>(gb); };
        table[0x7A] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::A, Register::D>(gb); };
        table[0x7B] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::A, Register::E>(gb); };
        table[0x7C] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::A, Register::H>(gb); };
        table[0x7D] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::A, Register::L>(gb); };
        table[0x7E] = [this](Gameboy &gb) -> bool { return this->LDRegisterIndirect<Register::A>(gb); };
        table[0x7F] = [this](Gameboy &gb) -> bool { return this->LDRegister<Register::A, Register::A>(gb); };
        table[0x80] = [this](Gameboy &gb) -> bool { return this->ADDRegister<Register::B>(gb); };
        table[0x81] = [this](Gameboy &gb) -> bool { return this->ADDRegister<Register::C>(gb); };
        table[0x82] = [this](Gameboy &gb) -> bool { return this->ADDRegister<Register::D>(gb); };
        table[0x83] = [this](Gameboy &gb) -> bool { return this->ADDRegister<Register::E>(gb); };
        table[0x84] = [this](Gameboy &gb) -> bool { return this->ADDRegister<Register::H>(gb); };
        table[0x85] = [this](Gameboy &gb) -> bool { return this->ADDRegister<Register::L>(gb); };
        table[0x86] = [this](Gameboy &gb) -> bool { return this->ADDIndirect(gb); };
        table[0x87] = [this](Gameboy &gb) -> bool { return this->ADDRegister<Register::A>(gb); };
        table[0x88] = [this](Gameboy &gb) -> bool { return this->ADCRegister<Register::B>(gb); };
        table[0x89] = [this](Gameboy &gb) -> bool { return this->ADCRegister<Register::C>(gb); };
        table[0x8A] = [this](Gameboy &gb) -> bool { return this->ADCRegister<Register::D>(gb); };
        table[0x8B] = [this](Gameboy &gb) -> bool { return this->ADCRegister<Register::E>(gb); };
        table[0x8C] = [this](Gameboy &gb) -> bool { return this->ADCRegister<Register::H>(gb); };
        table[0x8D] = [this](Gameboy &gb) -> bool { return this->ADCRegister<Register::L>(gb); };
        table[0x8E] = [this](Gameboy &gb) -> bool { return this->ADCIndirect(gb); };
        table[0x8F] = [this](Gameboy &gb) -> bool { return this->ADCRegister<Register::A>(gb); };
        table[0x90] = [this](Gameboy &gb) -> bool { return this->SUB<Register::B>(gb); };
        table[0x91] = [this](Gameboy &gb) -> bool { return this->SUB<Register::C>(gb); };
        table[0x92] = [this](Gameboy &gb) -> bool { return this->SUB<Register::D>(gb); };
        table[0x93] = [this](Gameboy &gb) -> bool { return this->SUB<Register::E>(gb); };
        table[0x94] = [this](Gameboy &gb) -> bool { return this->SUB<Register::H>(gb); };
        table[0x95] = [this](Gameboy &gb) -> bool { return this->SUB<Register::L>(gb); };
        table[0x96] = [this](Gameboy &gb) -> bool { return this->SUBIndirect(gb); };
        table[0x97] = [this](Gameboy &gb) -> bool { return this->SUB<Register::A>(gb); };
        table[0x98] = [this](Gameboy &gb) -> bool { return this->SBCRegister<Register::B>(gb); };
        table[0x99] = [this](Gameboy &gb) -> bool { return this->SBCRegister<Register::C>(gb); };
        table[0x9A] = [this](Gameboy &gb) -> bool { return this->SBCRegister<Register::D>(gb); };
        table[0x9B] = [this](Gameboy &gb) -> bool { return this->SBCRegister<Register::E>(gb); };
        table[0x9C] = [this](Gameboy &gb) -> bool { return this->SBCRegister<Register::H>(gb); };
        table[0x9D] = [this](Gameboy &gb) -> bool { return this->SBCRegister<Register::L>(gb); };
        table[0x9E] = [this](Gameboy &gb) -> bool { return this->SBCIndirect(gb); };
        table[0x9F] = [this](Gameboy &gb) -> bool { return this->SBCRegister<Register::A>(gb); };
        table[0xA0] = [this](Gameboy &gb) -> bool { return this->AND<Register::B>(gb); };
        table[0xA1] = [this](Gameboy &gb) -> bool { return this->AND<Register::C>(gb); };
        table[0xA2] = [this](Gameboy &gb) -> bool { return this->AND<Register::D>(gb); };
        table[0xA3] = [this](Gameboy &gb) -> bool { return this->AND<Register::E>(gb); };
        table[0xA4] = [this](Gameboy &gb) -> bool { return this->AND<Register::H>(gb); };
        table[0xA5] = [this](Gameboy &gb) -> bool { return this->AND<Register::L>(gb); };
        table[0xA6] = [this](Gameboy &gb) -> bool { return this->ANDIndirect(gb); };
        table[0xA7] = [this](Gameboy &gb) -> bool { return this->AND<Register::A>(gb); };
        table[0xA8] = [this](Gameboy &gb) -> bool { return this->XORRegister<Register::B>(gb); };
        table[0xA9] = [this](Gameboy &gb) -> bool { return this->XORRegister<Register::C>(gb); };
        table[0xAA] = [this](Gameboy &gb) -> bool { return this->XORRegister<Register::D>(gb); };
        table[0xAB] = [this](Gameboy &gb) -> bool { return this->XORRegister<Register::E>(gb); };
        table[0xAC] = [this](Gameboy &gb) -> bool { return this->XORRegister<Register::H>(gb); };
        table[0xAD] = [this](Gameboy &gb) -> bool { return this->XORRegister<Register::L>(gb); };
        table[0xAE] = [this](Gameboy &gb) -> bool { return this->XORIndirect(gb); };
        table[0xAF] = [this](Gameboy &gb) -> bool { return this->XORRegister<Register::A>(gb); };
        table[0xB0] = [this](Gameboy &gb) -> bool { return this->ORRegister<Register::B>(gb); };
        table[0xB1] = [this](Gameboy &gb) -> bool { return this->ORRegister<Register::C>(gb); };
        table[0xB2] = [this](Gameboy &gb) -> bool { return this->ORRegister<Register::D>(gb); };
        table[0xB3] = [this](Gameboy &gb) -> bool { return this->ORRegister<Register::E>(gb); };
        table[0xB4] = [this](Gameboy &gb) -> bool { return this->ORRegister<Register::H>(gb); };
        table[0xB5] = [this](Gameboy &gb) -> bool { return this->ORRegister<Register::L>(gb); };
        table[0xB6] = [this](Gameboy &gb) -> bool { return this->ORIndirect(gb); };
        table[0xB7] = [this](Gameboy &gb) -> bool { return this->ORRegister<Register::A>(gb); };
        table[0xB8] = [this](Gameboy &gb) -> bool { return this->CPRegister<Register::B>(gb); };
        table[0xB9] = [this](Gameboy &gb) -> bool { return this->CPRegister<Register::C>(gb); };
        table[0xBA] = [this](Gameboy &gb) -> bool { return this->CPRegister<Register::D>(gb); };
        table[0xBB] = [this](Gameboy &gb) -> bool { return this->CPRegister<Register::E>(gb); };
        table[0xBC] = [this](Gameboy &gb) -> bool { return this->CPRegister<Register::H>(gb); };
        table[0xBD] = [this](Gameboy &gb) -> bool { return this->CPRegister<Register::L>(gb); };
        table[0xBE] = [this](Gameboy &gb) -> bool { return this->CPIndirect(gb); };
        table[0xBF] = [this](Gameboy &gb) -> bool { return this->CPRegister<Register::A>(gb); };
        table[0xC0] = [this](Gameboy &gb) -> bool { return this->RETConditional<JumpTest::NotZero>(gb); };
        table[0xC1] = [this](Gameboy &gb) -> bool { return this->POP<StackTarget::BC>(gb); };
        table[0xC2] = [this](Gameboy &gb) -> bool { return this->JP<JumpTest::NotZero>(gb); };
        table[0xC3] = [this](Gameboy &gb) -> bool { return this->JPUnconditional(gb); };
        table[0xC4] = [this](Gameboy &gb) -> bool { return this->CALL<JumpTest::NotZero>(gb); };
        table[0xC5] = [this](Gameboy &gb) -> bool { return this->PUSH<StackTarget::BC>(gb); };
        table[0xC6] = [this](Gameboy &gb) -> bool { return this->ADDImmediate(gb); };
        table[0xC7] = [this](Gameboy &gb) -> bool { return this->RST<RSTTarget::H00>(gb); };
        table[0xC8] = [this](Gameboy &gb) -> bool { return this->RETConditional<JumpTest::Zero>(gb); };
        table[0xC9] = [this](Gameboy &gb) -> bool { return this->RETUnconditional(gb); };
        table[0xCA] = [this](Gameboy &gb) -> bool { return this->JP<JumpTest::Zero>(gb); };
        table[0xCB] = [this](Gameboy &gb) -> bool { return this->PREFIX(gb); };
        table[0xCC] = [this](Gameboy &gb) -> bool { return this->CALL<JumpTest::Zero>(gb); };
        table[0xCD] = [this](Gameboy &gb) -> bool { return this->CALLUnconditional(gb); };
        table[0xCE] = [this](Gameboy &gb) -> bool { return this->ADCImmediate(gb); };
        table[0xCF] = [this](Gameboy &gb) -> bool { return this->RST<RSTTarget::H08>(gb); };
        table[0xD0] = [this](Gameboy &gb) -> bool { return this->RETConditional<JumpTest::NotCarry>(gb); };
        table[0xD1] = [this](Gameboy &gb) -> bool { return this->POP<StackTarget::DE>(gb); };
        table[0xD2] = [this](Gameboy &gb) -> bool { return this->JP<JumpTest::NotCarry>(gb); };
        table[0xD4] = [this](Gameboy &gb) -> bool { return this->CALL<JumpTest::NotCarry>(gb); };
        table[0xD5] = [this](Gameboy &gb) -> bool { return this->PUSH<StackTarget::DE>(gb); };
        table[0xD6] = [this](Gameboy &gb) -> bool { return this->SUBImmediate(gb); };
        table[0xD7] = [this](Gameboy &gb) -> bool { return this->RST<RSTTarget::H10>(gb); };
        table[0xD8] = [this](Gameboy &gb) -> bool { return this->RETConditional<JumpTest::Carry>(gb); };
        table[0xD9] = [this](Gameboy &gb) -> bool { return this->RETI(gb); };
        table[0xDA] = [this](Gameboy &gb) -> bool { return this->JP<JumpTest::Carry>(gb); };
        table[0xDC] = [this](Gameboy &gb) -> bool { return this->CALL<JumpTest::Carry>(gb); };
        table[0xDE] = [this](Gameboy &gb) -> bool { return this->SBCImmediate(gb); };
        table[0xDF] = [this](Gameboy &gb) -> bool { return this->RST<RSTTarget::H18>(gb); };
        table[0xE0] = [this](Gameboy &gb) -> bool { return this->LoadFromAccumulatorDirectA(gb); };
        table[0xE1] = [this](Gameboy &gb) -> bool { return this->POP<StackTarget::HL>(gb); };
        table[0xE2] = [this](Gameboy &gb) -> bool { return this->LoadFromAccumulatorIndirectC(gb); };
        table[0xE5] = [this](Gameboy &gb) -> bool { return this->PUSH<StackTarget::HL>(gb); };
        table[0xE6] = [this](Gameboy &gb) -> bool { return this->ANDImmediate(gb); };
        table[0xE7] = [this](Gameboy &gb) -> bool { return this->RST<RSTTarget::H20>(gb); };
        table[0xE8] = [this](Gameboy &gb) -> bool { return this->ADDSigned(gb); };
        table[0xE9] = [this](Gameboy &gb) -> bool { return this->JPHL(gb); };
        table[0xEA] = [this](Gameboy &gb) -> bool { return this->LDFromAccumulatorDirect(gb); };
        table[0xEE] = [this](Gameboy &gb) -> bool { return this->XORImmediate(gb); };
        table[0xEF] = [this](Gameboy &gb) -> bool { return this->RST<RSTTarget::H28>(gb); };
        table[0xF0] = [this](Gameboy &gb) -> bool { return this->LoadAccumulatorA(gb); };
        table[0xF1] = [this](Gameboy &gb) -> bool { return this->POP<StackTarget::AF>(gb); };
        table[0xF2] = [this](Gameboy &gb) -> bool { return this->LoadAccumulatorIndirectC(gb); };
        table[0xF3] = [this](Gameboy &gb) -> bool { return this->DI(gb); };
        table[0xF5] = [this](Gameboy &gb) -> bool { return this->PUSH<StackTarget::AF>(gb); };
        table[0xF6] = [this](Gameboy &gb) -> bool { return this->ORImmediate(gb); };
        table[0xF7] = [this](Gameboy &gb) -> bool { return this->RST<RSTTarget::H30>(gb); };
        table[0xF8] = [this](Gameboy &gb) -> bool { return this->LD16StackAdjusted(gb); };
        table[0xF9] = [this](Gameboy &gb) -> bool { return this->LD16Stack(gb); };
        table[0xFA] = [this](Gameboy &gb) -> bool { return this->LDAccumulatorDirect(gb); };
        table[0xFB] = [this](Gameboy &gb) -> bool { return this->EI(gb); };
        table[0xFE] = [this](Gameboy &gb) -> bool { return this->CPImmediate(gb); };
        table[0xFF] = [this](Gameboy &gb) -> bool { return this->RST<RSTTarget::H38>(gb); };
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
