#pragma once

#include <array>
#include "Common.h"
#include "diagnostics/Observation.h"

struct Interrupts {
    u8 interrupt_enable{0x00};
    u8 interrupt_flag{0xE1};
    u8 interrupt_flag_delayed{0x00};
    u8 interrupt_visible_pending{0x00};
    std::array<u8, 5> interrupt_set_delays{};
    std::array<u8, 5> interrupt_visible_delays{};
    bool interrupt_master_enable{false};
    bool interrupt_delay{false};
    u8 acknowledge_mask{};
    u8 acknowledge_cycles{};
    u8 lcd_write_mask{};
    u8 lcd_write_cycles{};
    // True whenever any entry of the two delay arrays is nonzero, so the
    // per-dot Tick can skip the scan in the (overwhelmingly common) idle case
    [[=NotStateAware]] bool delays_pending{false};
    [[=NotStateAware]] diagnostics::ObservationContext *observations{};

    void acknowledge(uint8_t mask, bool cgb);

    void write_flag(uint8_t value, bool double_speed);

    [[nodiscard]] uint8_t blocked_requests() const { return acknowledge_mask | lcd_write_mask; }

    void tick_cpu() {
        if (acknowledge_cycles && --acknowledge_cycles == 0) {
            acknowledge_mask = 0;
        }
        if (lcd_write_cycles && --lcd_write_cycles == 0)
            lcd_write_mask = 0;
    }

    void set(InterruptType, bool);

    void set_after(InterruptType, u8 dots, bool visibleEarly = false, u8 visibleDelay = 0);

    void tick() {
        if (delays_pending)
            tick_pending();
    }

    void tick_pending();

    void cancel_pending(InterruptType interrupt);

    void recompute_pending() {
        u8 any = 0;
        for (const auto i : std::views::indices(interrupt_set_delays.size())) {
            any |= interrupt_set_delays[i] | interrupt_visible_delays[i];
        }
        delays_pending = any != 0;
    }

    [[nodiscard]] bool is_set(InterruptType) const;
};
