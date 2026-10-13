#include "diagnostics/Report.h"

#include <chrono>

namespace diagnostics {
    Report::Report(const std::filesystem::path &path, const RunIdentity &run) {
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());
        stream_.exceptions(std::ios::failbit | std::ios::badbit);
        stream_.open(path);
        Write(run);
    }

    std::filesystem::path DefaultReportPath() {
        const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
        const std::filesystem::path root{"build/diagnostics/runs"};
        std::filesystem::create_directories(root);
        for (unsigned attempt = 0; attempt < 1000; ++attempt) {
            const auto directory = root / (std::to_string(stamp) + "-" + std::to_string(attempt));
            if (std::filesystem::create_directory(directory))
                return directory / "results.jsonl";
        }
        throw std::runtime_error("Cannot allocate diagnostics run directory");
    }
} // namespace diagnostics
