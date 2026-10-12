#include "diagnostics/Trace.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include "diagnostics/Json.h"

namespace diagnostics {
    bool EventFilter::Matches(const Event &event) const noexcept {
        return (components & (uint64_t{1} << static_cast<unsigned>(event.component))) &&
               (kinds & (uint64_t{1} << static_cast<unsigned>(event.kind))) && ticks.Contains(event.position.tick) &&
               (!addresses || addresses->Contains(event.address)) && (!pcs || pcs->Contains(event.pc)) &&
               (!pcBank || *pcBank == event.pcBank) && (!lines || lines->Contains(event.position.ly)) &&
               (!dots || dots->Contains(event.position.dot));
    }

    TraceRecorder::TraceRecorder(const size_t capacity, EventFilter filter) : events_(capacity), filter_(std::move(filter)) {
        if (!capacity)
            throw std::invalid_argument("Trace capacity must be positive");
        context_.owner = this;
        context_.callback = [](void *owner, const Event &event) noexcept { static_cast<TraceRecorder *>(owner)->Record(event); };
    }

    void TraceRecorder::Record(const Event &event) noexcept {
        ++observed_;
        if (!enabled_ || captureComplete_)
            return;
        if (trigger_ && !triggerEvent_ && trigger_->match.Matches(event)) {
            triggerEvent_ = event;
            followingRemaining_ = trigger_->followingEvents;
            Append(event, events_.size());
            captureComplete_ = followingRemaining_ == 0;
            return;
        }
        if (!filter_.Matches(event))
            return;
        Append(event, trigger_ && !triggerEvent_ ? trigger_->historyEvents : events_.size());
        if (triggerEvent_ && --followingRemaining_ == 0)
            captureComplete_ = true;
    }

    void TraceRecorder::Append(const Event &event, const size_t limit) noexcept {
        if (!limit) {
            ++overwritten_;
            return;
        }
        if (size_ == limit) {
            begin_ = (begin_ + 1) % events_.size();
            ++overwritten_;
        } else {
            ++size_;
        }
        events_[(begin_ + size_ - 1) % events_.size()] = event;
    }

    void TraceRecorder::Arm(CaptureTrigger trigger) {
        if (observed_)
            throw std::logic_error("Arm a trigger before recording");
        if (trigger.historyEvents >= events_.size() || trigger.followingEvents > events_.size() - trigger.historyEvents - 1)
            throw std::invalid_argument("Trace capacity must hold history, trigger and following events");
        trigger_ = std::move(trigger);
    }

    TraceMetadata TraceRecorder::Metadata() const {
        return {.capacity = events_.size(), .retained = size_, .observed = observed_, .overwritten = overwritten_, .filter = filter_,
                .executableHash = executableHash_, .romHash = romHash_, .configuration = configuration_,
                .trigger = trigger_, .triggerEvent = triggerEvent_, .captureComplete = captureComplete_};
    }

    void TraceRecorder::Identify(std::string executableHash, std::string romHash, std::string configuration) {
        executableHash_ = std::move(executableHash);
        romHash_ = std::move(romHash);
        configuration_ = std::move(configuration);
    }

    std::vector<Event> TraceRecorder::Events() const {
        std::vector<Event> result;
        result.reserve(size_ + (overwritten_ != 0));
        if (overwritten_ && size_) {
            auto gap = events_[begin_];
            gap.component = Component::Control;
            gap.kind = EventKind::Overflow;
            --gap.sequence;
            gap.detail = static_cast<uint32_t>(std::min<uint64_t>(overwritten_, std::numeric_limits<uint32_t>::max()));
            result.push_back(gap);
        }
        for (size_t i = 0; i < size_; ++i)
            result.push_back(events_[(begin_ + i) % events_.size()]);
        return result;
    }

    void TraceRecorder::WriteJsonLines(const std::filesystem::path &path) const {
        std::ofstream output;
        output.exceptions(std::ios::failbit | std::ios::badbit);
        output.open(path);
        WriteJson(output, Metadata());
        output << '\n';
        for (const auto &event : Events()) {
            WriteJson(output, event);
            output << '\n';
        }
    }

    void TraceRecorder::WriteBinary(const std::filesystem::path &path) const {
        std::ofstream output;
        output.exceptions(std::ios::failbit | std::ios::badbit);
        output.open(path, std::ios::binary);
        const auto write = [&](uint64_t value, const unsigned bytes) {
            for (unsigned i = 0; i < bytes; ++i) {
                output.put(static_cast<char>(value & 255));
                value >>= 8;
            }
        };
        std::ostringstream header;
        WriteJson(header, Metadata());
        const auto metadata = header.str();
        const auto events = Events();
        output << "SGTRACE1";
        write(1, 4);
        write(metadata.size(), 4);
        write(events.size(), 8);
        output << metadata;
        for (const auto &event : events) {
            write(event.position.tick, 8);
            write(event.position.frame, 8);
            write(event.position.dot, 2);
            write(event.position.ly, 1);
            write(event.position.cpuPhase, 1);
            write(event.sequence, 8);
            write(event.branch, 8);
            write(event.pc, 2);
            write(event.pcBank, 2);
            write(event.address, 2);
            write(event.bank, 2);
            write(static_cast<uint8_t>(event.component), 1);
            write(static_cast<uint8_t>(event.kind), 1);
            write(static_cast<uint8_t>(event.initiator), 1);
            write(event.blocked, 1);
            write(event.value, 4);
            write(event.detail, 4);
            write(event.relatedTick, 8);
            write(event.parentBranch, 8);
        }
    }
}
