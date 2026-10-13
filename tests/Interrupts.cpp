#include "Interrupts.h"
#include <doctest/doctest.h>

TEST_CASE("interrupts: simultaneous sources retain independent propagation times") {
    Interrupts interrupts;
    interrupts.interruptFlag = 0;
    const auto vblank = [&] { interrupts.SetAfter(InterruptType::VBlank, 8, true, 4); };
    const auto stat = [&] { interrupts.SetAfter(InterruptType::LCDStat, 2); };
    SUBCASE("VBlank scheduled first") {
        vblank();
        stat();
    }
    SUBCASE("STAT scheduled first") {
        stat();
        vblank();
    }

    interrupts.Tick();
    CHECK(interrupts.interruptFlag == 0);
    interrupts.Tick();
    CHECK(interrupts.IsSet(InterruptType::LCDStat));
    CHECK_FALSE(interrupts.IsSet(InterruptType::VBlank));
    interrupts.Tick();
    CHECK(interrupts.interruptVisiblePending == 0);
    interrupts.Tick();
    CHECK(interrupts.interruptVisiblePending == 1);
    CHECK_FALSE(interrupts.IsSet(InterruptType::VBlank));
    for (unsigned dot = 4; dot < 8; ++dot)
        interrupts.Tick();
    CHECK(interrupts.IsSet(InterruptType::VBlank));
    CHECK(interrupts.IsSet(InterruptType::LCDStat));
    CHECK(interrupts.interruptVisiblePending == 0);
}
