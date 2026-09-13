#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

enum class Model : uint8_t {
    Auto,
    DMG0, DMGA, DMGB, DMGC,
    MGB, SGB, SGB2,
    CGB0, CGBA, CGBB, CGBC, CGBD, CGBE,
    AGB0, AGBA, AGBAE, AGBB, AGBBE,
};

inline constexpr std::array<std::string_view, 19> ModelNames{
    std::string_view{"auto"}, "dmg0", "dmga", "dmgb", "dmgc",
    "mgb", "sgb", "sgb2", "cgb0", "cgba", "cgbb", "cgbc", "cgbd", "cgbe",
    "agb0", "agba", "agbae", "agbb", "agbbe",
};

constexpr bool IsValidModel(const Model model) {
    return static_cast<unsigned>(model) < ModelNames.size();
}

constexpr std::string_view ModelName(const Model model) {
    return IsValidModel(model) ? ModelNames[static_cast<unsigned>(model)] : "invalid";
}

constexpr std::optional<Model> ParseModel(const std::string_view name) {
    if (name == "dmg") return Model::DMGB;
    if (name == "cgb") return Model::CGBE;
    if (name == "agb") return Model::AGBA;
    if (name == "ags") return Model::AGBB;
    for (unsigned i = 0; i < ModelNames.size(); ++i) {
        if (name == ModelNames[i]) return static_cast<Model>(i);
    }
    return std::nullopt;
}

constexpr Model ResolveModel(const Model requested, const bool colorCartridge) {
    return requested == Model::Auto ? (colorCartridge ? Model::CGBE : Model::DMGB) : requested;
}

constexpr bool IsAgb(const Model model) {
    return model >= Model::AGB0 && model <= Model::AGBBE;
}

constexpr bool IsCgb(const Model model) {
    return (model >= Model::CGB0 && model <= Model::CGBE) || IsAgb(model);
}

constexpr bool IsDmg(const Model model) {
    return model >= Model::DMG0 && model <= Model::SGB2;
}

constexpr bool IsSgb(const Model model) {
    return model == Model::SGB || model == Model::SGB2;
}

constexpr bool HasEarlyCgbLengthClocking(const Model model) {
    return model >= Model::CGB0 && model <= Model::CGBB;
}

constexpr bool HasLateCgbPulseTiming(const Model model) {
    return model == Model::CGBD || model == Model::CGBE;
}

constexpr bool HasEarlyCgbPulseTiming(const Model model) {
    return model >= Model::CGB0 && model <= Model::CGBC;
}

constexpr bool HasEarlyCgbPcmGlitch(const Model model) {
    return model >= Model::CGB0 && model <= Model::CGBC;
}

constexpr bool HasIntermediateApuWrites(const Model model) {
    return IsDmg(model) || HasEarlyCgbPcmGlitch(model);
}

constexpr bool HasLatchedTileRow(const Model model) {
    return model == Model::CGBD || model == Model::CGBE || IsAgb(model);
}

enum class UnusableOamBehavior { Zero, EarlyCgbRam, CgbDRam, AddressPattern };

constexpr UnusableOamBehavior UnusableOamFor(const Model model) {
    if (model >= Model::CGB0 && model <= Model::CGBC) return UnusableOamBehavior::EarlyCgbRam;
    if (model == Model::CGBD) return UnusableOamBehavior::CgbDRam;
    if (model == Model::CGBE || IsAgb(model)) return UnusableOamBehavior::AddressPattern;
    return UnusableOamBehavior::Zero;
}
