#include <array>
#include <fstream>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include "Reference.h"
#include <gambatte.h>

namespace diagnostics {
    namespace {
        class GambatteAdapter final : public ReferenceAdapter {
            gambatte::GB machine_;
            std::array<gambatte::uint_least32_t, 160 * 144> pixels_{};
            std::array<gambatte::uint_least32_t, 35112 + 2064> audio_{};
            std::array<unsigned, 32768> colors_{};
            ObservationContext *context_{};
            uint64_t ticks_{};
            uint64_t frames_{};
            static inline thread_local GambatteAdapter *active_{};

            void EventAt(const EventKind kind, const int32_t address, const int64_t offset) {
                if (!context_ || offset < 0)
                    return;
                const auto bank = machine_.getAddrBank(static_cast<unsigned short>(address));
                context_->Dispatch({
                    .position = {
                        .tick = ticks_ + static_cast<uint64_t>(offset) * 4, .frame = UINT64_MAX,
                        .dot = UINT16_MAX, .ly = UINT8_MAX, .cpuPhase = UINT8_MAX
                    },
                    .pc = kind == EventKind::InstructionBegin ? static_cast<uint16_t>(address) : uint16_t{UINT16_MAX},
                    .pcBank = kind == EventKind::InstructionBegin ? static_cast<uint16_t>(bank) : uint16_t{UINT16_MAX},
                    .address = static_cast<uint16_t>(address), .bank = static_cast<uint16_t>(bank),
                    .component = kind == EventKind::InstructionBegin ? Component::CPU : Component::Bus,
                    .kind = kind, .initiator = Initiator::CPU
                });
            }

        public:
            void Load(const ReferenceSettings &settings) override {
                std::ifstream input(settings.rom, std::ios::binary);
                if (!input)
                    throw std::runtime_error("Cannot read reference ROM");
                const std::vector<char> rom{std::istreambuf_iterator<char>(input), {}};
                if (rom.size() < 0x150 || (settings.model == "DMGB" && (rom[0x143] & 0x80)))
                    throw std::invalid_argument("Gambatte cannot force a color-header ROM into DMG mode through this API");
                if (!settings.noBootrom && machine_.loadBios(settings.bios))
                    throw std::runtime_error("Gambatte boot ROM load failed");
                const unsigned flags = gambatte::GB::READONLY_SAV | (settings.noBootrom ? gambatte::GB::NO_BIOS : 0) |
                                       (settings.model == "CGBC" ? gambatte::GB::CGB_MODE : 0);
                if (machine_.load(rom.data(), rom.size(), flags) != 0)
                    throw std::runtime_error("Gambatte ROM load failed");
                machine_.setTimeMode(true);
                machine_.setTime(0);
                unsigned char *ram{};
                int size{};
                if (machine_.getMemoryArea(3, &ram, &size) && ram && size > 0)
                    std::fill_n(ram, size, 0);
                constexpr std::array<unsigned, 4> shades{0xFFFFFFFF, 0xFFC0C0C0, 0xFF606060, 0xFF000000};
                for (int palette = 0; palette < 3; ++palette)
                    for (int color = 0; color < 4; ++color)
                        machine_.setDmgPaletteColor(palette, color, shades[color]);
                const auto expand = [](const unsigned value) { return (value << 3) | (value >> 2); };
                for (unsigned color = 0; color < colors_.size(); ++color)
                    colors_[color] = 0xFF000000 | expand(color & 31) << 16 | expand((color >> 5) & 31) << 8 | expand((color >> 10) & 31);
                machine_.setCgbPalette(colors_.data());
            }

            void ObserveWith(ObservationContext *context) override {
                context_ = context;
                machine_.setExecCallback(context
                                             ? +[](const int32_t address, const int64_t offset) {
                                                 active_->EventAt(EventKind::InstructionBegin, address, offset);
                                             }
                                             : nullptr);
                machine_.setReadCallback(context
                                             ? +[](const int32_t address, const int64_t offset) {
                                                 active_->EventAt(EventKind::BusRead, address, offset);
                                             }
                                             : nullptr);
                machine_.setWriteCallback(context
                                              ? +[](const int32_t address, const int64_t offset) {
                                                  active_->EventAt(EventKind::BusWrite, address, offset);
                                              }
                                              : nullptr);
            }

            uint64_t AdvanceTo(const uint64_t deadline) override {
                active_ = this;
                while (ticks_ < deadline) {
                    size_t samples = std::min<uint64_t>(35112, (deadline - ticks_ + 3) / 4);
                    const auto frame = machine_.runFor(pixels_.data(), 160, audio_.data(), samples);
                    if (!samples)
                        throw std::runtime_error("Gambatte made no progress");
                    ticks_ += samples * 4;
                    if (frame >= 0)
                        ++frames_;
                }
                active_ = nullptr;
                return ticks_;
            }

            ReferenceRegisters Registers() override {
                std::array < int, 10 > values{};
                machine_.getRegs(values.data());
                const auto pair = [&](const unsigned high, const unsigned low) { return static_cast<uint16_t>((values[high] << 8) | values[low]); };
                return {static_cast<uint16_t>(values[0]), static_cast<uint16_t>(values[1]), pair(2, 7), pair(3, 4), pair(5, 6), pair(8, 9)};
            }

            std::span<const uint32_t> Pixels() const override { return pixels_; }
            uint64_t Frames() const override { return frames_; }
        };
    }

    ReferenceCapabilities Capabilities() {
        return {
            .name = "Gambatte", .repository = "https://github.com/pokemon-speedrunning/gambatte-core",
            .revision = "5a41a68c25402421fb1983ddadc9faf2418ddb0f", .models = {"DMGB", "CGBC"},
            .startup = {"ExplicitBootrom", "SkipBootrom"}, .resolutionTicks = 4,
            .executionGranularity = "runFor sample budget; actual samples determine elapsed ticks", .maximumOvershootTicks = 8259,
            .events = {EventKind::InstructionBegin, EventKind::BusRead, EventKind::BusWrite},
            .banks = true, .instructionPhase = "Immediately before the opcode fetch callback; prefetched dispatch may omit a second callback",
            .audio = "2,097,152 Hz stereo PCM; resampler and mixer observations are not shared",
            .limitations = {
                "DMGB and CGBC map to generic DMG/CGB families; this API cannot select exact silicon revisions",
                "DMGB requires a monochrome cartridge header",
                "Bank observations describe cartridge mappings and do not identify boot ROM interception",
                "Bus callbacks supply addresses and timing, but no values; no side-effecting externalRead is used",
                "No external inputs or cartridge saves in this adapter protocol", "Power-on RAM and skip-boot state are core-specific"
            }
        };
    }

    std::unique_ptr<ReferenceAdapter> MakeReferenceAdapter() { return std::make_unique<GambatteAdapter>(); }
}
