#pragma once

#include <fstream>
#include "Common.h"
#include "Interrupts.h"

struct Serial {
    explicit Serial(Interrupts &interrupts) :
        interrupts(interrupts) {}

    [[nodiscard]] u8 read_serial(u16) const;

    void write_serial(u16, u8, bool);

    void shift_one_bit();

    // Inline idle fast path: nothing to do unless a transfer is shifting
    void update(const u16 divider) {
        if (active && (divider & (ticks_per_bit / 2 - 1)) == 0)
            clock_edge();
    }

    void divider_reset(const u16 divider) {
        if (active && divider & ticks_per_bit / 4)
            clock_edge();
    }

    void clock_edge();

    bool save_state(std::ofstream &) const;

    bool load_state(std::ifstream &);

    u16 ticks_per_bit{512};
    bool clock_high{false};
    u8 data{0}; // SB
    u8 control{0}; // SC
    u8 bits_shifted{0};
    bool active{false};
    Interrupts &interrupts;
};
