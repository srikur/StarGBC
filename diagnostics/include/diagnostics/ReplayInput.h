#pragma once

#include <cstdint>

namespace diagnostics {
    struct InputSink {
        void *owner{};
        void (*applied)(void *, uint64_t tick, uint8_t buttons) noexcept{};
    };

    struct AppliedInput {
        uint64_t tick{};
        uint64_t sequence{};
        uint8_t buttons{};
    };
}
