#define DOCTEST_CONFIG_IMPLEMENT
#include "TestRoms.h"

int main(const int argc, char **argv) {
    if (argc == 1) { ExecuteTestRoms(argc, argv); } else {
        std::fprintf(stderr, "USAGE: StarGBC_Tests\n");
        return -1;
    }
}
