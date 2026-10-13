#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include "Contracts.h"
#include "Json.h"

namespace diagnostics {
    struct AssertionResult {
        std::string record{"assertion"};
        std::string name;
        std::string suite;
        Outcome outcome{};
        unsigned assertions{};
        unsigned failures{};
    };

    struct RunCompletion {
        std::string record{"complete"};
        unsigned cases{};
        unsigned failed{};
    };

    class Report {
        std::ofstream stream_;
        std::mutex mutex_;

    public:
        explicit Report(const std::filesystem::path &path, const RunIdentity &run);

        template<class T>
        void Write(const T &value) {
            const std::scoped_lock lock(mutex_);
            WriteJson(stream_, value);
            stream_ << '\n';
            stream_.flush();
        }
    };

    [[nodiscard]] std::filesystem::path DefaultReportPath();
} // namespace diagnostics
