#pragma once

#include <filesystem>
#include <optional>
#include <vector>
#include "PixelProvenance.h"
#include "Trace.h"

namespace diagnostics {
    struct PixelRegion {
        Range<uint16_t> x{0, 159};
        Range<uint16_t> y{0, 0};
        Range<uint64_t> frames;
        Range<uint64_t> ticks;

        [[nodiscard]] bool Contains(const ExecutionPosition &position, uint16_t px, uint16_t py) const noexcept;
    };

    struct ProvenanceMetadata {
        uint32_t schemaVersion{1};
        std::string record{"pixel-provenance"};
        PixelRegion region;
        uint64_t pixelCapacity{};
        uint64_t eventCapacity{};
        uint64_t overwrittenPixels{};
        uint64_t overwrittenEvents{};
        std::string build;
        std::string romHash;
        uint64_t branch{};
    };

    class ProvenanceRecorder {
        PixelRegion region_;
        PixelProvenanceContext context_;
        std::vector<PixelExplanation> pixels_;
        std::vector<PixelPipelineEvent> pipeline_;
        std::vector<PixelRecolor> recolors_;
        size_t pixelsWritten_{};
        size_t pipelineWritten_{};
        size_t recolorsWritten_{};
        std::string build_;
        std::string romHash_;

        void Output(const PixelExplanation &pixel) noexcept;
        void Pipeline(const PixelPipelineEvent &event) noexcept;
        void Recolor(const PixelRecolor &event) noexcept;

    public:
        explicit ProvenanceRecorder(PixelRegion region = {}, size_t pixelCapacity = 160, size_t eventCapacity = 4096);
        ProvenanceRecorder(const ProvenanceRecorder &) = delete;
        ProvenanceRecorder &operator=(const ProvenanceRecorder &) = delete;
        [[nodiscard]] PixelProvenanceContext &Context() { return context_; }
        void Identify(std::string build, std::string romHash);
        [[nodiscard]] ProvenanceMetadata Metadata() const;
        [[nodiscard]] std::vector<PixelExplanation> Pixels() const;
        [[nodiscard]] std::vector<PixelPipelineEvent> PipelineEvents() const;
        [[nodiscard]] std::vector<PixelRecolor> Recolors() const;
        [[nodiscard]] std::optional<PixelExplanation> Query(uint16_t x, uint16_t y, std::optional<uint64_t> frame = {}, std::optional<uint64_t> branch = {}) const;
        void WriteJson(const std::filesystem::path &path) const;
    };
}
