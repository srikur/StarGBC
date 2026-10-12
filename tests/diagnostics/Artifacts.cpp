#include <array>
#include <doctest/doctest.h>
#include "diagnostics/Artifacts.h"

TEST_CASE("diagnostics: pixel differences retain the first mismatch and inclusive bounds") {
    constexpr std::array<uint32_t, 6> actual{0, 1, 2, 3, 4, 5};
    constexpr std::array<uint32_t, 6> expected{0, 0xFF000001, 9, 9, 4, 5};
    const auto difference = diagnostics::ComparePixels(actual, expected, 3, 2, 0xFFFFFF);
    CHECK(difference.mismatches == 2);
    REQUIRE(difference.first);
    CHECK(difference.first->x == 2);
    CHECK(difference.first->y == 0);
    CHECK(difference.minimum->x == 0);
    CHECK(difference.minimum->y == 0);
    CHECK(difference.maximum->x == 2);
    CHECK(difference.maximum->y == 1);
    CHECK_FALSE(diagnostics::ComparePixels(actual, actual, 3, 2).first);
    CHECK_THROWS_AS(static_cast<void>(diagnostics::ComparePixels(actual, expected, 2, 2)), std::invalid_argument);
}
