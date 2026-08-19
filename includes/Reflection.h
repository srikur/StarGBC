#pragma once

#include <algorithm>
#include <stdexcept>
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
    if (!std::meta::is_class_type(type))
        return {};
    return std::meta::nonstatic_data_members_of(type, std::meta::access_context::unchecked())
           | std::views::filter(IsStateAware)
           | std::views::filter(IsNotReferenceType)
           | std::ranges::to<std::vector>();
}

consteval std::vector<std::meta::info> StateBasesOf(const std::meta::info type) {
    if (!std::meta::is_class_type(type))
        return {};
    std::vector<std::meta::info> bases;
    for (const auto base: std::meta::bases_of(type, std::meta::access_context::unchecked())) {
        if (std::meta::is_virtual(base))
            throw std::invalid_argument("state types with virtual base classes are unsupported");
        if (const auto baseType = std::meta::type_of(base); !std::meta::is_empty_type(baseType))
            bases.push_back(baseType);
    }
    return bases;
}

consteval std::size_t StateSizeOf(const std::meta::info type) {
    if (std::meta::is_array_type(type)) {
        return std::meta::extent(type) * StateSizeOf(std::meta::remove_extent(type));
    }
    if (std::meta::is_union_type(type)) {
        // Walking a union would size every alternative and read inactive ones
        throw std::invalid_argument("union state members are unsupported; annotate with NotStateAware");
    }
    const std::vector<std::meta::info> bases = StateBasesOf(type);
    const std::vector<std::meta::info> members = StateMembersOf(type);
    if (bases.empty() && members.empty()) {
        if (std::meta::is_polymorphic_type(type)) {
            throw std::invalid_argument("polymorphic leaf state type would serialize its vptr");
        }
        if (!IsSerializableLeaf(type)) {
            throw std::invalid_argument("leaf state type has padding or is not trivially copyable");
        }
        return std::meta::size_of(type);
    }
    std::size_t total = 0;
    for (const auto base: bases) {total += StateSizeOf(base);}
    for (const auto m: members) {total += StateSizeOf(std::meta::type_of(m));}
    return total;
}
