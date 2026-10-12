#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

namespace diagnostics {
    [[nodiscard]] std::string Sha256(std::span<const std::byte> bytes);

    [[nodiscard]] std::string FileHash(const std::filesystem::path &path);

    [[nodiscard]] inline std::string TextHash(const std::string_view text) { return Sha256(std::as_bytes(std::span(text))); }
}
