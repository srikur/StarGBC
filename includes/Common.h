#pragma once
#include <exception>
#include <print>
#include <utility>
#include <vector>
#include <array>
#include <memory>
#include <fstream>
#include <chrono>
#include <string>
#include <cstdint>

template<uint8_t bit>
static constexpr bool Bit(const uint8_t value) {
    static_assert(bit < 8);
    return value >> bit & 0x01;
}

enum class Keys {
    Right = 0x01,
    Left = 0x02,
    Up = 0x04,
    Down = 0x08,
    A = 0x10,
    B = 0x20,
    Select = 0x40,
    Start = 0x80,
};

class GameboyException : public std::exception {
    virtual void message() const noexcept {
        std::printf("Gameboy Exception!\n");
        std::printf("------------------\n");
    }
};

class UnreachableCodeException final : public GameboyException {
    void message() const noexcept override {
        std::printf("Type: Unreachable Code\n");
        std::printf("----------------------\n");
        std::printf("Message: %s\n", message_.c_str());
    }

    std::string message_;

    UnreachableCodeException() = default;

public:
    explicit UnreachableCodeException(std::string message) : message_(std::move(message)) {
        this->message();
    }
};

class FatalErrorException final : public GameboyException {
    void message() const noexcept override {
        std::printf("Type: Fatal Error");
        std::printf("-----------------");
        std::printf("Message: %s\n", message_.c_str());
    }

    std::string message_;

    FatalErrorException() = default;

public:
    explicit FatalErrorException(std::string message) : message_(std::move(message)) {
        this->message();
    }
};
