#include <doctest/doctest.h>
#include <sstream>
#include "diagnostics/Contracts.h"
#include "diagnostics/Hash.h"
#include "diagnostics/Json.h"

TEST_CASE("diagnostics: hashes match SHA-256 vectors") {
    CHECK(diagnostics::TextHash("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(diagnostics::TextHash("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(diagnostics::TextHash(std::string(1'000'000, 'a')) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE("diagnostics: JSON escapes strings and reflects typed contracts") {
    std::ostringstream text;
    diagnostics::WriteJson(text, std::string("\"\\\n\0", 4));
    CHECK(text.str() == "\"\\\"\\\\\\u000a\\u0000\"");
    std::ostringstream position;
    diagnostics::WriteJson(position, diagnostics::Position{.tick = 9'000'000'000});
    CHECK(position.str() == "{\"tick\":9000000000,\"frame\":0,\"dot\":0,\"ly\":0,\"cpuPhase\":0}");
}

TEST_CASE("diagnostics: stable case identity excludes hashes and escapes separators") {
    diagnostics::CaseIdentity test{.suite = "suite", .rom = "a|b", .model = "DMGB", .startup = "skip", .expectationVariant = "screen"};
    CHECK(diagnostics::StableCaseId(test) == "suite|a%7cb|DMGB|skip|screen");
    test.rom = "a%7cb";
    CHECK(diagnostics::StableCaseId(test) == "suite|a%257cb|DMGB|skip|screen");
}
