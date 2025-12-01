#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <fstream>
#include <memory>
#include <print>
#include <string>
#include <utility>
#include <vector>

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

enum class Mode {
    None = 0x00,
    DMG = 0x01,
    MBG = 0x02,
    SGB = 0x03,
    SGB2 = 0x04,
    CGB_DMG = 0x05,
    AGB_DMG = 0x06,
    AGS_DMG = 0x07,
    CGB_GBC = 0x08,
    AGB_GBC = 0x09,
    AGS_GBC = 0x0A,
};

enum class InterruptState {
    M1, M2, M3, M4, M5, M6
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
