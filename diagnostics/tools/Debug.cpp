#include <fstream>
#include <iostream>
#include <optional>
#include <print>
#include <starparse/starparse.hpp>
#include "Gameboy.h"
#include "diagnostics/Contracts.h"
#include "diagnostics/Hash.h"
#include "diagnostics/Json.h"

namespace {
    using namespace StarParse;

    struct[[=Program{"StarGBC_Debug", "Headless accuracy diagnostics", "1"}]] Arguments {
        [[=Positional{0}, =Required]] std::string rom;
        [[=Opt{"Hardware revision"}]] Model model{Model::Auto};
        [[=Opt{"Boot ROM path"}]] std::string bios;
        [[=Opt{"Skip boot ROM execution"}]] bool no_bootrom{};
        [[=Opt{"Disable color correction"}]] bool raw_color{};
        [[=Opt{"Fixed RunFrame intervals before inspection"}, =Range{0, 1'000'000}]] unsigned intervals{};
        [[=Opt{"Register address to inspect"}]] std::optional<uint16_t> register_address;
        [[=Opt{"JSON output file; stdout when omitted"}]] std::string output;
    };

    struct InspectionReport {
        uint32_t schemaVersion{diagnostics::SchemaVersion};
        std::string romHash;
        MachineInspection machine;
        std::optional<RegisterInspection> selectedRegister;
    };
}

int main(const int argc, char **argv) {
    const auto arguments = StarParse::parse_or_exit<Arguments>(argc, argv, {.allow_case_insensitivity = true});
    if (arguments.help_requested()) {
        std::print("{}", arguments.help());
        return 0;
    }
    if (arguments.version_requested()) {
        std::print("{}", arguments.version());
        return 0;
    }
    try {
        Gameboy machine({.romName = arguments->rom, .biosPath = arguments->bios, .model = arguments->model,
                         .noBootrom = arguments->no_bootrom, .colorCorrection = !arguments->raw_color});
        for (unsigned interval = 0; interval < arguments->intervals; ++interval)
            machine.RunFrame();
        InspectionReport report{.romHash = diagnostics::FileHash(arguments->rom), .machine = machine.Inspect()};
        if (arguments->register_address)
            report.selectedRegister = machine.InspectRegister(*arguments->register_address);
        if (arguments->output.empty()) {
            diagnostics::WriteJson(std::cout, report);
            std::cout << '\n';
        } else {
            std::ofstream output;
            output.exceptions(std::ios::failbit | std::ios::badbit);
            output.open(arguments->output);
            diagnostics::WriteJson(output, report);
            output << '\n';
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Diagnostics error: " << error.what() << '\n';
        return 2;
    }
}
