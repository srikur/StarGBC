#include "diagnostics/Hash.h"

#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <vector>

namespace diagnostics {
    std::string Sha256(const std::span<const std::byte> bytes) {
        static constexpr std::array<uint32_t, 64> constants{
                0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
                0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
                0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
                0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
                0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
                0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        std::array<uint32_t, 8> hash{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        const auto size = ((bytes.size() + 9 + 63) / 64) * 64;
        for (size_t block = 0; block < size; block += 64) {
            std::array<uint32_t, 64> words{};
            for (size_t i = 0; i < 64; ++i) {
                const size_t offset = block + i;
                uint8_t byte{};
                if (offset < bytes.size())
                    byte = std::to_integer<uint8_t>(bytes[offset]);
                else if (offset == bytes.size())
                    byte = 0x80;
                else if (offset >= size - 8)
                    byte = static_cast<uint8_t>((static_cast<uint64_t>(bytes.size()) * 8) >> ((size - 1 - offset) * 8));
                words[i / 4] |= static_cast<uint32_t>(byte) << ((3 - i % 4) * 8);
            }
            for (size_t i = 16; i < words.size(); ++i) {
                const auto x = words[i - 15];
                const auto y = words[i - 2];
                words[i] = words[i - 16] + (std::rotr(x, 7) ^ std::rotr(x, 18) ^ (x >> 3)) + words[i - 7] +
                           (std::rotr(y, 17) ^ std::rotr(y, 19) ^ (y >> 10));
            }
            auto [a, b, c, d, e, f, g, h] = hash;
            for (size_t i = 0; i < words.size(); ++i) {
                const auto t1 = h + (std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25)) + ((e & f) ^ (~e & g)) + constants[i] + words[i];
                const auto t2 = (std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
                h = g;
                g = f;
                f = e;
                e = d + t1;
                d = c;
                c = b;
                b = a;
                a = t1 + t2;
            }
            const std::array state{a, b, c, d, e, f, g, h};
            for (size_t i = 0; i < hash.size(); ++i)
                hash[i] += state[i];
        }
        std::string result;
        constexpr std::string_view hex = "0123456789abcdef";
        for (const auto word : hash)
            for (int shift = 28; shift >= 0; shift -= 4)
                result += hex[(word >> shift) & 15];
        return result;
    }

    std::string FileHash(const std::filesystem::path &path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            throw std::runtime_error("Cannot hash file: " + path.string());
        const auto size = file.tellg();
        if (size < 0)
            throw std::runtime_error("Cannot determine file size: " + path.string());
        std::vector<std::byte> bytes(static_cast<size_t>(size));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char *>(bytes.data()), size))
            throw std::runtime_error("Cannot read file: " + path.string());
        return Sha256(bytes);
    }
} // namespace diagnostics
