#include "diagnostics/Watchpoints.h"

#include <algorithm>
#include <stdexcept>
#include <span>

namespace diagnostics {
    Watchpoints::Watchpoints(ObservationContext &context) : context_(context) {
        if (context_.condition)
            throw std::logic_error("Observation context already has execution conditions");
        context_.conditionOwner = this;
        context_.condition = [](void *owner, const Event &event) noexcept {
            const auto &watchpoints = *static_cast<Watchpoints *>(owner);
            return std::ranges::any_of(std::span(watchpoints.conditions_).first(watchpoints.size_),
                                       [&](const Watchpoint &condition) { return condition.Matches(event); });
        };
    }

    Watchpoints::~Watchpoints() {
        context_.condition = nullptr;
        context_.conditionOwner = nullptr;
        context_.stopEvent.reset();
    }

    void Watchpoints::Add(Watchpoint condition) {
        if (size_ == conditions_.size())
            throw std::invalid_argument("At most sixteen event watchpoints are supported");
        if (!condition.mask)
            throw std::invalid_argument("Watchpoint masks must be nonzero");
        conditions_[size_++] = std::move(condition);
    }

    void Watchpoints::BreakAt(const uint16_t pc, const std::optional<uint16_t> bank) {
        Add({
            .events = {
                .components = uint64_t{1} << static_cast<unsigned>(Component::CPU),
                .kinds = uint64_t{1} << static_cast<unsigned>(EventKind::InstructionBegin), .pcs = {{pc, pc}}, .pcBank = bank
            }
        });
    }

    void Watchpoints::ReadAt(const Range<uint16_t> addresses) {
        Add({
            .events = {
                .components = uint64_t{1} << static_cast<unsigned>(Component::Bus),
                .kinds = uint64_t{1} << static_cast<unsigned>(EventKind::BusRead), .addresses = addresses
            }
        });
    }

    void Watchpoints::WriteAt(const Range<uint16_t> addresses) {
        Add({
            .events = {
                .components = uint64_t{1} << static_cast<unsigned>(Component::Bus),
                .kinds = uint64_t{1} << static_cast<unsigned>(EventKind::BusWrite), .addresses = addresses
            }
        });
    }
}
