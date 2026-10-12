#include <array>
#include <cstdlib>
#include <stdexcept>
#include "Reference.h"
#include <Core/gb.h>

extern "C" uint64_t StarGBC_SameBoyRunTicks(const GB_gameboy_t *machine);
extern "C" bool StarGBC_SameBoyBootromActive(const GB_gameboy_t *machine);
extern "C" void StarGBC_SameBoyInitializeRtc(GB_gameboy_t *machine);

namespace diagnostics {
    namespace {
        class SameBoyAdapter final : public ReferenceAdapter {
            struct Release {
                void operator()(GB_gameboy_t *machine) const { GB_free(machine); std::free(machine); }
            };
            std::unique_ptr<GB_gameboy_t, Release> machine_;
            std::array<uint32_t, 160 * 144> pixels_{};
            ObservationContext *context_{};
            uint64_t ticks_{};
            uint64_t frames_{};
            uint16_t instructionAddress_{};

            static SameBoyAdapter &Owner(GB_gameboy_t *machine) { return *static_cast<SameBoyAdapter *>(GB_get_user_data(machine)); }
            uint16_t Bank(const uint16_t address) const {
                if ((address < 0x100 || (address >= 0x200 && address < 0x900 && GB_is_cgb(machine_.get()))) &&
                    StarGBC_SameBoyBootromActive(machine_.get()))
                    return UINT16_MAX;
                GB_direct_access_t area = address < 0x4000 ? GB_DIRECT_ACCESS_ROM0 : address < 0x8000 ? GB_DIRECT_ACCESS_ROM :
                                          address < 0xA000 ? GB_DIRECT_ACCESS_VRAM : address < 0xC000 ? GB_DIRECT_ACCESS_CART_RAM : GB_DIRECT_ACCESS_RAM;
                uint16_t bank{};
                if (address >= 0xFE00 || (address >= 0xC000 && address < 0xD000))
                    return 0;
                static_cast<void>(GB_get_direct_access(machine_.get(), area, nullptr, &bank));
                return bank;
            }
            void EventAt(const EventKind kind, const uint16_t address, const uint8_t value) {
                if (!context_)
                    return;
                const bool instruction = kind == EventKind::InstructionBegin;
                if (instruction)
                    instructionAddress_ = address;
                context_->Dispatch({.position = {.tick = ticks_ + StarGBC_SameBoyRunTicks(machine_.get()), .frame = UINT64_MAX,
                                    .dot = UINT16_MAX, .ly = UINT8_MAX, .cpuPhase = UINT8_MAX},
                                    .pc = instructionAddress_, .pcBank = Bank(instructionAddress_), .address = address, .bank = Bank(address),
                                    .component = instruction ? Component::CPU : Component::Bus, .kind = kind,
                                    .initiator = Initiator::Peripheral, .value = value});
            }
        public:
            void Load(const ReferenceSettings &settings) override {
                const auto model = settings.model == "DMGB" ? GB_MODEL_DMG_B : settings.model == "CGBC" ? GB_MODEL_CGB_C : GB_MODEL_CGB_E;
                GB_random_seed(0x53544152474243ULL);
                machine_.reset(GB_init(GB_alloc(), model));
                GB_set_user_data(machine_.get(), this);
                GB_set_turbo_mode(machine_.get(), true, false);
                if (GB_load_rom(machine_.get(), settings.rom.c_str()) || GB_load_boot_rom(machine_.get(), settings.bios.c_str()))
                    throw std::runtime_error("SameBoy ROM or boot ROM load failed");
                StarGBC_SameBoyInitializeRtc(machine_.get());
                size_t size{};
                auto *ram = static_cast<uint8_t *>(GB_get_direct_access(machine_.get(), GB_DIRECT_ACCESS_CART_RAM, &size, nullptr));
                if (ram)
                    std::fill_n(ram, size, 0);
                GB_set_pixels_output(machine_.get(), pixels_.data());
                GB_set_rgb_encode_callback(machine_.get(), +[](GB_gameboy_t *, const uint8_t red, const uint8_t green, const uint8_t blue) -> uint32_t {
                    return 0xFF000000 | uint32_t{red} << 16 | uint32_t{green} << 8 | blue;
                });
                GB_set_color_correction_mode(machine_.get(), GB_COLOR_CORRECTION_DISABLED);
                static constexpr GB_palette_t palette{{{255, 255, 255}, {192, 192, 192}, {96, 96, 96}, {0, 0, 0}, {255, 255, 255}}};
                GB_set_palette(machine_.get(), &palette);
                GB_set_vblank_callback(machine_.get(), +[](GB_gameboy_t *machine, const GB_vblank_type_t type) {
                    if (type == GB_VBLANK_TYPE_NORMAL_FRAME)
                        ++Owner(machine).frames_;
                });
            }
            void ObserveWith(ObservationContext *context) override {
                context_ = context;
                GB_set_execution_callback(machine_.get(), context ? +[](GB_gameboy_t *machine, const uint16_t address, const uint8_t opcode) {
                    Owner(machine).EventAt(EventKind::InstructionBegin, address, opcode);
                } : nullptr);
                GB_set_read_memory_callback(machine_.get(), context ? +[](GB_gameboy_t *machine, const uint16_t address, const uint8_t value) -> uint8_t {
                    Owner(machine).EventAt(EventKind::BusRead, address, value);
                    return value;
                } : nullptr);
                GB_set_write_memory_callback(machine_.get(), context ? +[](GB_gameboy_t *machine, const uint16_t address, const uint8_t value) -> bool {
                    Owner(machine).EventAt(EventKind::BusWrite, address, value);
                    return true;
                } : nullptr);
            }
            uint64_t AdvanceTo(const uint64_t deadline) override {
                while (ticks_ < deadline) {
                    const auto elapsed = GB_run(machine_.get());
                    if (!elapsed)
                        throw std::runtime_error("SameBoy made no progress");
                    ticks_ += elapsed;
                }
                return ticks_;
            }
            ReferenceRegisters Registers() override {
                const auto &registers = *GB_get_registers(machine_.get());
                return {registers.pc, registers.sp, registers.af, registers.bc, registers.de, registers.hl};
            }
            std::span<const uint32_t> Pixels() const override { return pixels_; }
            uint64_t Frames() const override { return frames_; }
        };
    }
    ReferenceCapabilities Capabilities() {
        return {.name = "SameBoy", .repository = "https://github.com/LIJI32/SameBoy", .revision = "c71f110eb1b1e87914fddd9c0e68712dddce787d",
                .models = {"DMGB", "CGBC", "CGBE"}, .startup = {"ExplicitBootrom"}, .resolutionTicks = 1,
                .executionGranularity = "GB_run operation; elapsed 8 MHz ticks and overshoot are reported",
                .events = {EventKind::InstructionBegin, EventKind::BusRead, EventKind::BusWrite}, .busValues = true,
                .instructionValues = true, .banks = true, .instructionPhase = "Execution callback after opcode read and before its operation",
                .audio = "Core resampling is not compared with another core's resampler",
                .limitations = {"Bus callbacks may include DMA and remapped addresses; initiator and blocked status are unavailable",
                                "Callback ticks use the pinned core's cycles_since_run through a small C bridge",
                                "Power-on randomness uses seed 0x53544152474243; other cores initialize RAM differently",
                                "No skip-boot, external inputs or cartridge saves in this adapter protocol"}};
    }
    std::unique_ptr<ReferenceAdapter> MakeReferenceAdapter() { return std::make_unique<SameBoyAdapter>(); }
}
