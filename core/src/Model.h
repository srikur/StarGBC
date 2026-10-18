#pragma once

#include <meta>
#include <string_view>

enum class Model : uint8_t {
    Auto,
    DMG0,
    DMGA,
    DMGB,
    DMGC,
    MGB,
    SGB,
    SGB2,
    CGB0,
    CGBA,
    CGBB,
    CGBC,
    CGBD,
    CGBE,
    AGB0,
    AGBA,
    AGBAE,
    AGBB,
    AGBBE,
};

enum class DmgPaletteSampling : uint8_t { BitwiseOr, NewValue };

template<typename E>
    requires std::is_enum_v<E>
constexpr std::string_view model_name(E value) {
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
        if (value == [:e:]) {
            return std::meta::identifier_of(e);
        }
    }
    return "invalid";
}

template<typename E>
    requires std::is_enum_v<E>
constexpr std::optional<E> model_from_name(const std::string_view value) {
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
        if (value == std::meta::identifier_of(e)) {
            return [:e:];
        }
    }
    return std::nullopt;
}

constexpr std::string_view ModelName(const Model model) { return model_name(model); }

constexpr Model ResolveModel(const Model requested, const bool colorCartridge) {
    return requested == Model::Auto ? (colorCartridge ? Model::CGBE : Model::DMGB) : requested;
}

constexpr bool IsAgb(const Model model) { return model >= Model::AGB0 && model <= Model::AGBBE; }

constexpr bool IsCgb(const Model model) { return (model >= Model::CGB0 && model <= Model::CGBE) || IsAgb(model); }

constexpr bool IsDmg(const Model model) { return model >= Model::DMG0 && model <= Model::SGB2; }

constexpr bool IsSgb(const Model model) { return model == Model::SGB || model == Model::SGB2; }

constexpr bool HasEarlyCgbLengthClocking(const Model model) { return model >= Model::CGB0 && model <= Model::CGBB; }

constexpr bool HasLateCgbPulseTiming(const Model model) { return model == Model::CGBD || model == Model::CGBE; }

constexpr bool HasEarlyCgbPulseTiming(const Model model) { return model >= Model::CGB0 && model <= Model::CGBC; }

constexpr bool HasEarlyCgbPcmGlitch(const Model model) { return model >= Model::CGB0 && model <= Model::CGBC; }

constexpr bool HasIntermediateApuWrites(const Model model) { return IsDmg(model) || HasEarlyCgbPcmGlitch(model); }

constexpr bool HasLatchedTileRow(const Model model) { return model == Model::CGBD || model == Model::CGBE || IsAgb(model); }

enum class UnusableOamBehavior { Zero, EarlyCgbRam, CgbDRam, AddressPattern };

constexpr UnusableOamBehavior UnusableOamFor(const Model model) {
    if (model >= Model::CGB0 && model <= Model::CGBC)
        return UnusableOamBehavior::EarlyCgbRam;
    if (model == Model::CGBD)
        return UnusableOamBehavior::CgbDRam;
    if (model == Model::CGBE || IsAgb(model))
        return UnusableOamBehavior::AddressPattern;
    return UnusableOamBehavior::Zero;
}
