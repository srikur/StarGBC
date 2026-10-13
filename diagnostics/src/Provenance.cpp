#include "diagnostics/Provenance.h"

#include <algorithm>
#include <fstream>
#include "diagnostics/Json.h"

namespace diagnostics {
    namespace {
        template<class T>
        std::vector<T> Ordered(const std::vector<T> &ring, const size_t written) {
            std::vector<T> result;
            const auto count = std::min(ring.size(), written);
            result.reserve(count);
            for (size_t i = written - count; i < written; ++i)
                result.push_back(ring[i % ring.size()]);
            return result;
        }
    } // namespace

    bool PixelRegion::Contains(const ExecutionPosition &position, const uint16_t px, const uint16_t py) const noexcept {
        return x.Contains(px) && y.Contains(py) && frames.Contains(position.frame) && ticks.Contains(position.tick);
    }

    ProvenanceRecorder::ProvenanceRecorder(PixelRegion region, const size_t pixelCapacity, const size_t eventCapacity) : region_(region) {
        if (!pixelCapacity || pixelCapacity > 46080 || !eventCapacity || eventCapacity > 1'000'000 || region.x.first > region.x.last ||
            region.x.last > 159 || region.y.first > region.y.last || region.y.last > 143 || region.frames.first > region.frames.last ||
            region.ticks.first > region.ticks.last)
            throw std::invalid_argument("Invalid provenance region or bounded capacity");
        pixels_.resize(pixelCapacity);
        pipeline_.resize(eventCapacity);
        recolors_.resize(eventCapacity);
        context_.owner = this;
        context_.output = [](void *owner, const PixelExplanation &pixel) noexcept { static_cast<ProvenanceRecorder *>(owner)->Output(pixel); };
        context_.pipeline = [](void *owner, const PixelPipelineEvent &event) noexcept { static_cast<ProvenanceRecorder *>(owner)->Pipeline(event); };
        context_.recolor = [](void *owner, const PixelRecolor &event) noexcept { static_cast<ProvenanceRecorder *>(owner)->Recolor(event); };
    }

    void ProvenanceRecorder::Output(const PixelExplanation &pixel) noexcept {
        if (region_.Contains(pixel.emission, pixel.x, pixel.y))
            pixels_[pixelsWritten_++ % pixels_.size()] = pixel;
    }

    void ProvenanceRecorder::Pipeline(const PixelPipelineEvent &event) noexcept {
        if (region_.Contains(event.position, event.x, event.y))
            pipeline_[pipelineWritten_++ % pipeline_.size()] = event;
    }

    void ProvenanceRecorder::Recolor(const PixelRecolor &event) noexcept {
        if (event.offset == UINT16_MAX) {
            if (!region_.frames.Contains(event.position.frame) || !region_.ticks.Contains(event.position.tick))
                return;
            recolors_[recolorsWritten_++ % recolors_.size()] = event;
            std::array<bool, 160 * 144> cleared{};
            for (size_t i = pixelsWritten_; i > pixelsWritten_ - std::min(pixels_.size(), pixelsWritten_); --i) {
                auto &pixel = pixels_[(i - 1) % pixels_.size()];
                const auto offset = pixel.y * 160 + pixel.x;
                if (pixel.branch != event.branch || cleared[offset])
                    continue;
                cleared[offset] = true;
                pixel.recolored = true;
                pixel.finalColor = event.color;
                pixel.finalColorTick = event.position.tick;
                pixel.decision = PixelDecision::DisplayCleared;
                pixel.selected = {.valid = true, .source = PixelSource::Backdrop};
            }
            return;
        }
        if (!region_.Contains(event.position, event.offset % 160, event.offset / 160))
            return;
        recolors_[recolorsWritten_++ % recolors_.size()] = event;
        for (size_t i = pixelsWritten_; i > pixelsWritten_ - std::min(pixels_.size(), pixelsWritten_); --i) {
            auto &pixel = pixels_[(i - 1) % pixels_.size()];
            if (pixel.x != event.offset % 160 || pixel.y != event.offset / 160 || pixel.branch != event.branch ||
                pixel.emission.frame != event.position.frame)
                continue;
            pixel.recolored = true;
            pixel.finalColor = event.color;
            pixel.finalColorTick = event.position.tick;
            if (event.selectionChanged) {
                pixel.decision = event.decision;
                pixel.selected = event.selectedSource == PixelSource::Sprite ? pixel.sprites.selected : pixel.background;
                if (event.selectedSource == PixelSource::Backdrop)
                    pixel.selected = {.valid = true, .source = PixelSource::Backdrop};
                else if (event.selectedSource == PixelSource::Sprite)
                    pixel.selected.source = event.selectedSource;
                pixel.selected.color = event.rawColor;
                pixel.selected.palette = event.palette;
            }
            return;
        }
    }

    void ProvenanceRecorder::Identify(std::string build, std::string romHash) {
        build_ = std::move(build);
        romHash_ = std::move(romHash);
    }

    ProvenanceMetadata ProvenanceRecorder::Metadata() const {
        return {.region = region_,
                .pixelCapacity = pixels_.size(),
                .eventCapacity = pipeline_.size(),
                .overwrittenPixels = pixelsWritten_ - std::min(pixelsWritten_, pixels_.size()),
                .overwrittenEvents = pipelineWritten_ - std::min(pipelineWritten_, pipeline_.size()) + recolorsWritten_ -
                                     std::min(recolorsWritten_, recolors_.size()),
                .build = build_,
                .romHash = romHash_,
                .branch = context_.branch};
    }

    std::vector<PixelExplanation> ProvenanceRecorder::Pixels() const { return Ordered(pixels_, pixelsWritten_); }
    std::vector<PixelPipelineEvent> ProvenanceRecorder::PipelineEvents() const { return Ordered(pipeline_, pipelineWritten_); }
    std::vector<PixelRecolor> ProvenanceRecorder::Recolors() const { return Ordered(recolors_, recolorsWritten_); }

    std::optional<PixelExplanation> ProvenanceRecorder::Query(const uint16_t x, const uint16_t y, const std::optional<uint64_t> frame,
                                                              const std::optional<uint64_t> branch) const {
        for (size_t i = pixelsWritten_; i > pixelsWritten_ - std::min(pixels_.size(), pixelsWritten_); --i) {
            const auto &pixel = pixels_[(i - 1) % pixels_.size()];
            if (pixel.x == x && pixel.y == y && (!frame || pixel.emission.frame == *frame) && pixel.branch == branch.value_or(context_.branch))
                return pixel;
        }
        return {};
    }

    void ProvenanceRecorder::WriteJson(const std::filesystem::path &path) const {
        struct Report {
            ProvenanceMetadata metadata;
            std::vector<PixelExplanation> pixels;
            std::vector<PixelPipelineEvent> pipeline;
            std::vector<PixelRecolor> recolors;
        };
        std::ofstream output(path);
        output.exceptions(std::ios::badbit | std::ios::failbit);
        diagnostics::WriteJson(output, Report{Metadata(), Pixels(), PipelineEvents(), Recolors()});
        output << '\n';
    }
} // namespace diagnostics
