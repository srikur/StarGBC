#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>

namespace diagnostics {
    inline void WriteUnsigned(std::ostream &output, uint64_t value, const unsigned bytes = 8) {
        for (unsigned i = 0; i < bytes; ++i, value >>= 8)
            output.put(static_cast<char>(value & 255));
        if (!output)
            throw std::runtime_error("Cannot write diagnostics data");
    }

    inline uint64_t ReadUnsigned(std::istream &input, const unsigned bytes = 8) {
        uint64_t value{};
        for (unsigned i = 0; i < bytes; ++i) {
            const auto byte = input.get();
            if (byte == std::char_traits<char>::eof())
                throw std::runtime_error("Truncated diagnostics data");
            value |= static_cast<uint64_t>(static_cast<uint8_t>(byte)) << (i * 8);
        }
        return value;
    }

    inline void WriteString(std::ostream &output, const std::string &text) {
        if (text.size() > 1'000'000)
            throw std::invalid_argument("Diagnostics string exceeds size limit");
        WriteUnsigned(output, text.size(), 4);
        output.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    inline std::string ReadString(std::istream &input) {
        const auto size = ReadUnsigned(input, 4);
        if (size > 1'000'000)
            throw std::invalid_argument("Diagnostics string exceeds size limit");
        std::string text(size, '\0');
        if (!input.read(text.data(), static_cast<std::streamsize>(size)))
            throw std::runtime_error("Truncated diagnostics string");
        return text;
    }
}
