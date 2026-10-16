#include "Interrupts.h"

void Interrupts::acknowledge(const uint8_t mask, const bool cgb) {
    const uint8_t visible = interrupt_visible_pending & mask;
    interrupt_flag &= ~mask;
    interrupt_visible_pending &= ~mask;
    interrupt_flag_delayed &= ~visible;
    if (visible) {
        for (auto i{0u}; i < interrupt_set_delays.size(); ++i) {
            if (visible & (1u << i))
                interrupt_set_delays[i] = interrupt_visible_delays[i] = 0;
        }
        recompute_pending();
    }
    acknowledge_cycles = (mask == 0x08 || mask == 0x04) ? 3 + cgb : (mask == 0x02 || mask == 0x01) ? 3 : 0;
    acknowledge_mask = acknowledge_cycles ? mask : 0;
}

void Interrupts::set(const InterruptType interrupt, const bool delayed) {
    const uint8_t mask = 0x01 << static_cast<uint8_t>(interrupt);
    if (!delayed) {
        if (observations)
            observations->Emit(diagnostics::Component::Interrupt, diagnostics::EventKind::InterruptRequest, 0xFF0F, mask);
        interrupt_flag |= mask & ~acknowledge_mask;
        if (observations)
            observations->Emit(diagnostics::Component::Interrupt, diagnostics::EventKind::InterruptVisible, 0xFF0F,
                               interrupt_flag | interrupt_visible_pending | 0xE0);
    } else {
        set_after(interrupt, 4);
    }
}

void Interrupts::set_after(const InterruptType interrupt, const uint8_t dots, const bool visibleEarly, const uint8_t visibleDelay) {
    if (dots == 0) {
        set(interrupt, false);
        return;
    }
    const uint8_t mask = 0x01 << static_cast<uint8_t>(interrupt);
    const auto index = static_cast<uint8_t>(interrupt);
    if (observations)
        observations->Emit(diagnostics::Component::Interrupt, diagnostics::EventKind::InterruptRequest, 0xFF0F, mask,
                           dots | (visibleEarly << 8) | (visibleDelay << 16));
    if (visibleEarly && !visibleDelay && (acknowledge_mask & mask))
        return;
    if (visibleEarly) {
        if (visibleDelay) {
            interrupt_visible_delays[index] = visibleDelay;
        } else {
            interrupt_visible_pending |= mask;
            if (observations)
                observations->Emit(diagnostics::Component::Interrupt, diagnostics::EventKind::InterruptVisible, 0xFF0F,
                                   interrupt_flag | interrupt_visible_pending | 0xE0);
        }
    }
    if (interrupt_set_delays[index] == 0)
        interrupt_set_delays[index] = dots;
    interrupt_flag_delayed |= mask;
    delays_pending = true;
}

void Interrupts::tick_pending() {
    uint8_t remaining = 0;
    for (unsigned i = 0; i < interrupt_set_delays.size(); ++i) {
        const auto previous_flags = interrupt_flag | interrupt_visible_pending;
        const uint8_t mask = 1u << i;
        if (interrupt_visible_delays[i] && --interrupt_visible_delays[i] == 0) {
            if (acknowledge_mask & mask) {
                interrupt_flag_delayed &= ~mask;
                interrupt_set_delays[i] = 0;
            } else {
                interrupt_visible_pending |= mask;
            }
        }
        if (interrupt_set_delays[i] && --interrupt_set_delays[i] == 0) {
            interrupt_flag |= interrupt_flag_delayed & mask & ~acknowledge_mask;
            interrupt_flag_delayed &= ~mask;
            interrupt_visible_pending &= ~mask;
        }
        remaining |= interrupt_set_delays[i] | interrupt_visible_delays[i];
        if (observations && previous_flags != (interrupt_flag | interrupt_visible_pending))
            observations->Emit(diagnostics::Component::Interrupt, diagnostics::EventKind::InterruptVisible, 0xFF0F,
                               interrupt_flag | interrupt_visible_pending | 0xE0);
    }
    delays_pending = remaining != 0;
}

void Interrupts::cancel_pending(const InterruptType interrupt) {
    const auto index = static_cast<uint8_t>(interrupt);
    const uint8_t mask = 1u << index;
    const auto previous_flags = interrupt_flag | interrupt_visible_pending;
    interrupt_flag_delayed &= ~mask;
    interrupt_visible_pending &= ~mask;
    interrupt_set_delays[index] = interrupt_visible_delays[index] = 0;
    recompute_pending();
    if (observations && previous_flags != (interrupt_flag | interrupt_visible_pending))
        observations->Emit(diagnostics::Component::Interrupt, diagnostics::EventKind::InterruptVisible, INTERRUPT_FLAG,
                           interrupt_flag | interrupt_visible_pending | 0xE0);
}

bool Interrupts::is_set(InterruptType interrupt) const {
    const uint8_t mask = 0x01 << static_cast<uint8_t>(interrupt);
    return (interrupt_flag & mask) != 0;
}
