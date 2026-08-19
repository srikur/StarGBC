#pragma once

#include <algorithm>
#include <vector>
#include <meta>
#include <ranges>

// Annotation used on member variables that should NOT be accounted for in save state files
struct NotStateAwareTag {
};

constexpr NotStateAwareTag NotStateAware{};

consteval bool IsStateAware(const std::meta::info m) {
    return std::meta::annotations_of_with_type(m, ^^NotStateAwareTag).empty();
}

consteval bool IsNotReferenceType(const std::meta::info m) {
    return !std::meta::is_reference_type(std::meta::type_of(m));
}

consteval bool IsSerializableLeaf(const std::meta::info type) {
    return std::meta::has_unique_object_representations(type)
           || std::meta::is_floating_point_type(type);
}

consteval std::vector<std::meta::info> StateMembersOf(const std::meta::info type) {
    if (!std::meta::is_class_type(type) && !std::meta::is_union_type(type))
        return {};
    return std::meta::nonstatic_data_members_of(type, std::meta::access_context::unchecked())
           | std::views::filter(IsStateAware)
           | std::views::filter(IsNotReferenceType)
           | std::ranges::to<std::vector>();
}

consteval std::size_t StateSizeOf(const std::meta::info type) {
    if (std::meta::is_array_type(type)) {
        return std::meta::extent(type) * StateSizeOf(std::meta::remove_extent(type));
    }
    const std::vector<std::meta::info> members = StateMembersOf(type);
    if (members.empty()) {
        if (!IsSerializableLeaf(type)) {
            throw std::invalid_argument("leaf state type has padding or is not trivially copyable");
        }
        return std::meta::size_of(type);
    }
    return std::ranges::fold_left(
        members | std::views::transform([](const std::meta::info m) {
            return StateSizeOf(std::meta::type_of(m));
        }),
        0uz, std::plus{});
}
