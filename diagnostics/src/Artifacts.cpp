#include "diagnostics/Artifacts.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include "diagnostics/Hash.h"
#include "diagnostics/Json.h"

namespace diagnostics {
    PixelDifference ComparePixels(const std::span<const uint32_t> actual, const std::span<const uint32_t> expected, const uint32_t width,
                                  const uint32_t height, const uint32_t mask) {
        if (!width || !height || actual.size() != static_cast<uint64_t>(width) * height || expected.size() != actual.size())
            throw std::invalid_argument("Pixel buffers must match image dimensions");
        PixelDifference result{.width = width, .height = height, .mask = mask};
        for (size_t i = 0; i < actual.size(); ++i) {
            if ((actual[i] & mask) == (expected[i] & mask))
                continue;
            const PixelCoordinate pixel{static_cast<uint32_t>(i % width), static_cast<uint32_t>(i / width)};
            if (!result.first) {
                result.first = result.minimum = result.maximum = pixel;
            } else {
                result.minimum->x = std::min(result.minimum->x, pixel.x);
                result.minimum->y = std::min(result.minimum->y, pixel.y);
                result.maximum->x = std::max(result.maximum->x, pixel.x);
                result.maximum->y = std::max(result.maximum->y, pixel.y);
            }
            ++result.mismatches;
        }
        return result;
    }

    void WriteBitmap(const std::filesystem::path &path, const std::span<const uint32_t> pixels, const uint32_t width, const uint32_t height) {
        const uint64_t stride = (static_cast<uint64_t>(width) * 3 + 3) & ~uint64_t{3};
        const uint64_t size = 54 + stride * height;
        if (!width || !height || pixels.size() != static_cast<uint64_t>(width) * height || size > std::numeric_limits<uint32_t>::max())
            throw std::invalid_argument("Invalid bitmap dimensions");
        std::ofstream file;
        file.exceptions(std::ios::failbit | std::ios::badbit);
        file.open(path, std::ios::binary);
        const auto littleEndian = [&](uint32_t value, const unsigned bytes) {
            for (unsigned i = 0; i < bytes; ++i) {
                file.put(static_cast<char>(value & 255));
                value >>= 8;
            }
        };
        file << "BM";
        for (const uint32_t value : {static_cast<uint32_t>(size), 0u, 54u, 40u, width, height})
            littleEndian(value, 4);
        littleEndian(1, 2);
        littleEndian(24, 2);
        for (const uint32_t value : {0u, static_cast<uint32_t>(stride * height), 0u, 0u, 0u, 0u})
            littleEndian(value, 4);
        for (uint32_t y = height; y > 0; --y) {
            for (uint32_t x = 0; x < width; ++x)
                littleEndian(pixels[(y - 1) * width + x], 3);
            for (uint64_t padding = width * 3; padding < stride; ++padding)
                file.put(0);
        }
    }

    void WriteFailureBundle(const std::filesystem::path &directory, Result &result, Reproduction reproduction, const TerminalState &state,
                            const std::span<const uint32_t> actual, const std::span<const uint32_t> expected, const uint32_t mask) {
        if (!std::filesystem::create_directories(directory))
            throw std::runtime_error("Failure bundle already exists: " + directory.string());
        const auto json = [&](const std::string &name, const auto &value) {
            std::ofstream file;
            file.exceptions(std::ios::failbit | std::ios::badbit);
            file.open(directory / name);
            WriteJson(file, value);
            file << '\n';
        };
        const auto copyInput = [&](std::string &path, const std::string &name, const std::string &hash) {
            if (path.empty())
                return;
            if (hash.empty()) {
                path = "missing-" + name;
                return;
            }
            if (FileHash(path) != hash)
                throw std::runtime_error("Reproduction input changed during run: " + path);
            std::filesystem::copy_file(path, directory / name);
            path = name;
        };
        copyInput(reproduction.rom, "rom.gb", result.inputs.rom);
        copyInput(reproduction.bootrom, "bootrom.bin", result.inputs.bootrom);
        copyInput(reproduction.reference, "reference.screen", result.inputs.reference);
        copyInput(reproduction.cartridgeSave, "rom.sav", result.inputs.cartridgeSave);
        json("reproduction.json", reproduction);
        json("terminal.json", state);
        if (!actual.empty()) {
            WriteBitmap(directory / "actual.bmp", actual, 160, 144);
            if (!expected.empty()) {
                const auto difference = ComparePixels(actual, expected, 160, 144, mask);
                json("pixels.json", difference);
                WriteBitmap(directory / "expected.bmp", expected, 160, 144);
                std::vector<uint32_t> pixels(actual.size());
                for (size_t i = 0; i < pixels.size(); ++i)
                    pixels[i] = (actual[i] & mask) == (expected[i] & mask) ? 0x202020 : 0xFF0066;
                WriteBitmap(directory / "difference.bmp", pixels, 160, 144);
            }
        }
        result.artifacts.push_back(directory.string());
        json("result.json", result);
    }
} // namespace diagnostics
