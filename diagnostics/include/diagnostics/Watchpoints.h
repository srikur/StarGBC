#pragma once

#include <array>
#include "Trace.h"

namespace diagnostics {
    struct Watchpoint {
        EventFilter events;
        std::optional<uint32_t> value;
        uint32_t mask{0xFFFFFFFF};

        [[nodiscard]] bool Matches(const Event &event) const noexcept {
            return events.Matches(event) && (!value || (event.value & mask) == (*value & mask));
        }
    };

    class Watchpoints {
        ObservationContext &context_;
        std::array<Watchpoint, 16> conditions_;
        size_t size_{};

    public:
        explicit Watchpoints(ObservationContext &context);

        ~Watchpoints();

        Watchpoints(const Watchpoints &) = delete;

        Watchpoints &operator=(const Watchpoints &) = delete;

        void Add(Watchpoint condition);

        void BreakAt(uint16_t pc, std::optional<uint16_t> bank = {});

        void ReadAt(Range<uint16_t> addresses);

        void WriteAt(Range<uint16_t> addresses);
    };
}
