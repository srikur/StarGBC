#pragma once

#include <charconv>
#include <cmath>
#include <concepts>
#include <meta>
#include <optional>
#include <ostream>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace diagnostics {
    inline void WriteJson(std::ostream &out, const std::string_view value) {
        constexpr std::string_view hex = "0123456789abcdef";
        out.put('"');
        for (const unsigned char c : value) {
            if (c == '"' || c == '\\') {
                out.put('\\');
                out.put(c);
            } else if (c < 0x20) {
                out << "\\u00" << hex[c >> 4] << hex[c & 15];
            } else {
                out.put(c);
            }
        }
        out.put('"');
    }

    template<class T>
    void WriteJson(std::ostream &out, const T &value) {
        if constexpr (std::convertible_to<T, std::string_view>) {
            WriteJson(out, std::string_view(value));
        } else if constexpr (std::same_as<T, bool>) {
            out << (value ? "true" : "false");
        } else if constexpr (std::is_arithmetic_v<T>) {
            if constexpr (std::floating_point<T>) {
                if (!std::isfinite(value))
                    throw std::invalid_argument("nonfinite JSON number");
            }
            char buffer[128];
            const auto [end, error] = std::to_chars(std::begin(buffer), std::end(buffer), value);
            if (error != std::errc{})
                throw std::runtime_error("JSON number conversion failed");
            out.write(buffer, end - buffer);
        } else if constexpr (std::is_enum_v<T>) {
            template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^T))) {
                if (value == [:e:]) {
                    WriteJson(out, std::meta::identifier_of(e));
                    return;
                }
            }
            throw std::invalid_argument("unknown JSON enum value");
        } else if constexpr (requires {
                                 value.has_value();
                                 *value;
                             }) {
            if (value)
                WriteJson(out, *value);
            else
                out << "null";
        } else if constexpr (std::ranges::input_range<T>) {
            out.put('[');
            bool separator = false;
            for (const auto &entry : value) {
                if (std::exchange(separator, true))
                    out.put(',');
                WriteJson(out, entry);
            }
            out.put(']');
        } else {
            static_assert(std::is_aggregate_v<T>);
            out.put('{');
            bool separator = false;
            template for (constexpr auto m :
                          std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()))) {
                if (std::exchange(separator, true))
                    out.put(',');
                WriteJson(out, std::meta::identifier_of(m));
                out.put(':');
                WriteJson(out, value.[:m:]);
            }
            out.put('}');
        }
    }
} // namespace diagnostics
