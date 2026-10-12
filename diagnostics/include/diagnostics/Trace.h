#pragma once

#include <filesystem>
#include <limits>
#include <optional>
#include <vector>
#include "Observation.h"

namespace diagnostics {
    template<class T>
    struct Range {
        T first{};
        T last{std::numeric_limits<T>::max()};
        [[nodiscard]] bool Contains(const T value) const noexcept { return value >= first && value <= last; }
    };

    struct EventFilter {
        uint64_t components{~uint64_t{0}};
        uint64_t kinds{~uint64_t{0}};
        Range<uint64_t> ticks;
        std::optional<Range<uint16_t>> addresses;
        std::optional<Range<uint16_t>> pcs;
        std::optional<uint16_t> pcBank;
        std::optional<Range<uint8_t>> lines;
        std::optional<Range<uint16_t>> dots;

        [[nodiscard]] bool Matches(const Event &event) const noexcept;
    };

    struct CaptureTrigger {
        EventFilter match;
        size_t historyEvents{};
        size_t followingEvents{};
    };

    struct TraceMetadata {
        uint32_t schemaVersion{1};
        std::string record{"trace"};
        uint64_t capacity{};
        uint64_t retained{};
        uint64_t observed{};
        uint64_t overwritten{};
        EventFilter filter;
        std::string executableHash;
        std::string romHash;
        std::string configuration;
        std::optional<CaptureTrigger> trigger;
        std::optional<Event> triggerEvent;
        bool captureComplete{};
    };

    class TraceRecorder {
        std::vector<Event> events_;
        size_t begin_{};
        size_t size_{};
        uint64_t observed_{};
        uint64_t overwritten_{};
        EventFilter filter_;
        bool enabled_{true};
        ObservationContext context_;
        std::string executableHash_;
        std::string romHash_;
        std::string configuration_;
        std::optional<CaptureTrigger> trigger_;
        std::optional<Event> triggerEvent_;
        size_t followingRemaining_{};
        bool captureComplete_{};

        void Record(const Event &event) noexcept;
        void Append(const Event &event, size_t limit) noexcept;

    public:
        explicit TraceRecorder(size_t capacity, EventFilter filter = {});
        TraceRecorder(const TraceRecorder &) = delete;
        TraceRecorder &operator=(const TraceRecorder &) = delete;
        [[nodiscard]] ObservationContext &Context() { return context_; }
        [[nodiscard]] TraceMetadata Metadata() const;
        [[nodiscard]] std::vector<Event> Events() const;
        void SetCaptureEnabled(const bool enabled) { enabled_ = enabled; }
        void Clear() {
            begin_ = size_ = observed_ = overwritten_ = 0;
            trigger_.reset();
            triggerEvent_.reset();
            followingRemaining_ = 0;
            captureComplete_ = false;
        }
        void Identify(std::string executableHash, std::string romHash, std::string configuration);
        void Arm(CaptureTrigger trigger);
        [[nodiscard]] bool CaptureComplete() const { return captureComplete_; }
        void WriteJsonLines(const std::filesystem::path &path) const;
        void WriteBinary(const std::filesystem::path &path) const;
    };
}
