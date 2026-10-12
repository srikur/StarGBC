#include "Reference.h"
#include "Gameboy.h"

namespace diagnostics {
    namespace {
        class StarGBCAdapter final : public ReferenceAdapter {
            std::unique_ptr<Gameboy> machine_;
        public:
            void Load(const ReferenceSettings &settings) override {
                Model model{Model::Auto};
                template for (constexpr auto value : std::define_static_array(std::meta::enumerators_of(^^Model))) {
                    if (settings.model == std::meta::identifier_of(value))
                        model = [:value:];
                }
                if (model == Model::Auto)
                    throw std::invalid_argument("Unsupported StarGBC model");
                auto save = std::filesystem::path(settings.rom);
                save.replace_extension(".sav");
                if (std::filesystem::exists(save))
                    throw std::invalid_argument("Reference comparison requires an isolated ROM without cartridge save data");
                machine_ = std::make_unique<Gameboy>(GameboySettings{.romName = settings.rom, .biosPath = settings.bios, .model = model,
                           .noBootrom = settings.noBootrom, .colorCorrection = false, .executionPolicy = ExecutionPolicy::Ordinary});
            }
            void ObserveWith(ObservationContext *context) override { machine_->ObserveWith(context); }
            uint64_t AdvanceTo(const uint64_t deadline) override {
                static_cast<void>(machine_->AdvanceTicks(deadline - machine_->TimelineTicks()));
                return machine_->TimelineTicks();
            }
            ReferenceRegisters Registers() override {
                const auto cpu = machine_->InspectCpu();
                return {cpu.pc, cpu.sp, cpu.af, cpu.bc, cpu.de, cpu.hl};
            }
            std::span<const uint32_t> Pixels() const override { return {machine_->GetScreenData(), 160 * 144}; }
            uint64_t Frames() const override { return machine_->ExecutionPosition().frame; }
        };
    }
    ReferenceCapabilities Capabilities() {
        return {.name = "StarGBC", .repository = "local", .revision = std::string(Gameboy::CoreBuildIdentity()),
                .models = {"DMGB", "CGBC", "CGBE"}, .startup = {"ExplicitBootrom", "SkipBootrom"}, .resolutionTicks = 1,
                .executionGranularity = "MasterTick", .maximumOvershootTicks = 0,
                .events = {EventKind::InstructionBegin, EventKind::BusRead, EventKind::BusWrite}, .busValues = true,
                .instructionValues = true, .banks = true, .instructionPhase = "First execution machine cycle after opcode prefetch",
                .audio = "48 kHz stereo resampling; not equated with another core's resampler",
                .limitations = {"No external inputs or cartridge saves in this adapter protocol", "Power-on RAM and skip-boot state are core-specific"}};
    }
    std::unique_ptr<ReferenceAdapter> MakeReferenceAdapter() { return std::make_unique<StarGBCAdapter>(); }
}
