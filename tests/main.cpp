#define DOCTEST_CONFIG_IMPLEMENT
#include "TestRoms.h"

int main(const int argc, char **argv) {
    const std::string_view arg = argc > 1 ? argv[1] : "";
    if (arg == "--blargg") {
        ExecuteTestRoms(argc, argv);
    } else if (arg == "--all") {
        ExecuteTestRoms(argc, argv);
    } else {
        std::fprintf(stderr, "USAGE: StarGBC_Tests [options]\n"
                     "Options:\n"
                     "  --blargg            blargg test roms\n"
                     "  --all               all tests\n");
        return -1;
    }
}
