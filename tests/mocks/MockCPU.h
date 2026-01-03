#ifndef STARGBC_MOCKCPU_H
#define STARGBC_MOCKCPU_H

#include "Common.h"
#include "MockBus.h"

template<typename BusT>
struct MockCPU {
    explicit MockCPU(BusT &bus) : bus_(bus) {
    }

    [[nodiscard]] std::add_lvalue_reference_t<uint16_t> pc() {
        return pc_;
    }

    void pc(const uint16_t value) {
        pc_ = value;
    }

    [[nodiscard]] std::add_lvalue_reference_t<uint16_t> sp() {
        return sp_;
    }

    void sp(const uint16_t value) {
        sp_ = value;
    }

    void icount(const uint8_t value) {
        icount_ = value;
    }

    [[nodiscard]] std::add_lvalue_reference_t<uint8_t> mCycleCounter() {
        return mCycleCounter_;
    }

    void mCycleCounter(const uint8_t value) {
        mCycleCounter_ = value;
    }

    [[nodiscard]] std::add_lvalue_reference_t<uint16_t> nextInstruction() {
        return nextInstruction_;
    }

    void nextInstruction(const uint16_t value) {
        nextInstruction_ = value;
    }

    void halted(const bool value) {
        halted_ = value;
    }

    void haltBug(const bool value) {
        haltBug_ = value;
    }

    std::add_lvalue_reference_t<bool> stopped() {
        return stopped_;
    }

    void stopped(const bool value) {
        stopped_ = value;
    }

    Hardware hardware() {
        return Hardware::DMG;
    }

    void Reset() {
        pc_ = 0;
        sp_ = 0;
        icount_ = 0;
        mCycleCounter_ = 1;
        nextInstruction_ = 0;
        halted_ = false;
        haltBug_ = false;
        stopped_ = false;
        currentInstruction = 0;
        prefixed = false;
    }

    bool ExecuteMicroOp(Instructions<MockCPU<MockBus> > &instructions) {
        mCycleCounter_++;
        return prefixed
                   ? instructions.prefixedInstr(currentInstruction, *this)
                   : instructions.nonPrefixedInstr(currentInstruction, *this);
    }

    BusT &bus_;
    uint16_t currentInstruction{};
    bool prefixed{};

private:
    uint16_t pc_{};
    uint16_t sp_{};
    uint8_t icount_{};
    uint8_t mCycleCounter_{0x01};
    uint16_t nextInstruction_{};
    bool halted_{};
    bool haltBug_{};
    bool stopped_{};
};

#endif //STARGBC_MOCKCPU_H
