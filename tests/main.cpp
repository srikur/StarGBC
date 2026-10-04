#define DOCTEST_CONFIG_IMPLEMENT
#include "AgeTestRoms.h"
#include "GambatteTests.gen.h"
#include "GbMicrotest.gen.h"
#include "GraphicsRoms.h"
#include "TestRoms.h"

int main(const int argc, char **argv) { return ExecuteTestRoms(argc, argv); }
