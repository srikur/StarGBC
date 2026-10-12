#include "diagnostics/Contracts.h"

namespace diagnostics {
    std::string StableCaseId(const CaseIdentity &test) {
        constexpr std::string_view hex = "0123456789abcdef";
        std::string result;
        for (const auto &part : {test.suite, test.rom, test.model, test.startup, test.expectationVariant}) {
            if (!result.empty())
                result += '|';
            for (const unsigned char c : part) {
                if (c == '%' || c == '|' || c < 0x20) {
                    result += '%';
                    result += hex[c >> 4];
                    result += hex[c & 15];
                } else {
                    result += c;
                }
            }
        }
        return result;
    }
}
