#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

// Fixed-capacity, inline-storage stand-ins for std::deque / std::vector where
// the logical size has a hard hardware bound. Storage lives in a std::array so
// the reflection serializer can walk the contents; overflows are bugs

template<class T, std::size_t Capacity>
class FixedDeque {
public:
    [[nodiscard]] constexpr bool empty() const { return count_ == 0; }
    [[nodiscard]] constexpr std::size_t size() const { return count_; }

    constexpr void clear() {
        head_ = 0;
        count_ = 0;
    }

    constexpr T &front() {
        assert(count_ > 0);
        return data_[head_];
    }

    constexpr const T &front() const {
        assert(count_ > 0);
        return data_[head_];
    }

    // Indices stay below 2*Capacity, so a conditional subtract replaces the
    // modulo (a real division for non-power-of-two capacities)
    constexpr void push_back(const T &value) {
        assert(count_ < Capacity);
        std::size_t idx = head_ + count_;
        if (idx >= Capacity) idx -= Capacity;
        data_[idx] = value;
        ++count_;
    }

    constexpr void push_front(const T &value) {
        assert(count_ < Capacity);
        head_ = head_ == 0 ? Capacity - 1 : head_ - 1;
        data_[head_] = value;
        ++count_;
    }

    constexpr void pop_front() {
        assert(count_ > 0);
        head_ = head_ + 1 == Capacity ? 0 : head_ + 1;
        --count_;
    }

    // A raw-restored deque is safe to use iff these hold
    [[nodiscard]] constexpr bool valid() const { return head_ < Capacity && count_ <= Capacity; }

private:
    std::array<T, Capacity> data_{};
    uint8_t head_{0};
    uint8_t count_{0};
};

template<class T, std::size_t Capacity>
class FixedVector {
public:
    [[nodiscard]] constexpr bool empty() const { return count_ == 0; }
    [[nodiscard]] constexpr std::size_t size() const { return count_; }

    constexpr void clear() { count_ = 0; }

    constexpr void push_back(const T &value) {
        assert(count_ < Capacity);
        data_[count_++] = value;
    }

    constexpr T *begin() { return data_.data(); }
    constexpr T *end() { return data_.data() + count_; }
    constexpr const T *begin() const { return data_.data(); }
    constexpr const T *end() const { return data_.data() + count_; }

    [[nodiscard]] constexpr bool valid() const { return count_ <= Capacity; }

private:
    std::array<T, Capacity> data_{};
    uint8_t count_{0};
};
