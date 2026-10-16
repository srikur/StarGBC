#pragma once

template<typename T>
consteval std::meta::info get_member(const std::string_view name) {
    for (const auto member : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))
        if (std::meta::identifier_of(member) == name)
            return member;
    throw std::out_of_range("Could not find class member");
}
