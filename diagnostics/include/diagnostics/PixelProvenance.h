#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include "Observation.h"

namespace diagnostics {
    enum class PixelSource : uint8_t { Unknown, Background, Window, Sprite, Backdrop, WindowGlitch };
    enum class SpriteDecision : uint8_t { Transparent, Replaced, KeptEarlier, Aborted };
    enum class PixelDecision : uint8_t {
        SpriteTransparent,
        ObjectsDisabled,
        BackgroundDisabled,
        BackgroundPriority,
        SpriteBehindBackground,
        SpriteForeground,
        DisplayCleared
    };
    enum class PipelineReason : uint8_t {
        FineScroll,
        WindowClipping,
        WindowRestart,
        SpriteClipping,
        SpriteFetch,
        SpriteAlignment,
        FifoStarvation,
        RestoredWithoutHistory,
        WindowFetch
    };

    struct PixelOrigin {
        bool valid{};
        PixelSource source{PixelSource::Unknown};
        uint16_t mapAddress{};
        uint8_t tile{};
        uint8_t attributes{};
        uint8_t bank{};
        uint16_t lowAddress{};
        uint16_t highAddress{};
        uint8_t lowRead{};
        uint8_t highRead{};
        uint8_t lowData{};
        uint8_t highData{};
        uint8_t lowRow{};
        uint8_t highRow{};
        uint8_t bit{};
        uint8_t color{};
        uint8_t palette{};
        uint8_t oamIndex{255};
        int16_t spriteX{};
        int16_t spriteY{};
        bool priority{};
        uint64_t mapTick{};
        uint64_t lowTick{};
        uint64_t highTick{};
        uint64_t overrideTick{};
    };

    struct SpriteCandidate {
        PixelOrigin origin;
        SpriteDecision decision{};
        bool selected{};
    };

    struct SpriteOrigins {
        std::array<SpriteCandidate, 10> candidates;
        uint8_t count{};
        bool overflow{};
        PixelOrigin selected;

        void Merge(const PixelOrigin &origin, const SpriteDecision decision) noexcept {
            const bool replace = decision == SpriteDecision::Replaced;
            if (replace) {
                for (auto &candidate : candidates)
                    candidate.selected = false;
                selected = origin;
            }
            if (count == candidates.size()) {
                overflow = true;
                return;
            }
            candidates[count++] = {origin, decision, replace};
        }
    };

    struct PixelExplanation {
        ExecutionPosition emission;
        uint64_t branch{};
        uint16_t x{};
        uint16_t y{};
        PixelOrigin background;
        SpriteOrigins sprites;
        PixelOrigin selected;
        PixelDecision decision{};
        PixelDecision emittedDecision{};
        PixelSource emittedSource{};
        uint32_t emittedColor{};
        uint32_t finalColor{};
        uint64_t finalColorTick{};
        uint8_t lcdc{};
        bool backgroundEnabled{};
        bool objectsEnabled{};
        uint8_t backgroundPalette{};
        uint8_t objectPalette0{};
        uint8_t objectPalette1{};
        bool recolored{};
    };

    struct PixelPipelineEvent {
        ExecutionPosition position;
        uint64_t branch{};
        uint16_t x{};
        uint16_t y{};
        PipelineReason reason{};
        PixelOrigin origin;
    };

    struct PixelRecolor {
        ExecutionPosition position;
        uint64_t branch{};
        uint16_t offset{};
        uint16_t registerAddress{};
        uint8_t registerValue{};
        bool selectionChanged{};
        uint32_t color{};
        PixelSource selectedSource{};
        PixelDecision decision{};
        uint8_t rawColor{};
        uint8_t palette{};
    };

    struct PixelProvenanceContext {
        void *owner{};
        void (*output)(void *, const PixelExplanation &) noexcept {};
        void (*pipeline)(void *, const PixelPipelineEvent &) noexcept {};
        void (*recolor)(void *, const PixelRecolor &) noexcept {};
        ExecutionPosition position;
        uint64_t branch{};
        PixelOrigin backgroundFetch;
        PixelOrigin savedBackgroundFetch;
        PixelOrigin spriteFetch;
        std::array<PixelOrigin, 16> backgroundQueue;
        uint8_t backgroundHead{};
        uint8_t backgroundCount{};
        std::array<SpriteOrigins, 8> sprites;

        void ClearBackground() noexcept { backgroundHead = backgroundCount = 0; }

        void ResetPipeline() noexcept {
            ClearBackground();
            backgroundFetch = savedBackgroundFetch = spriteFetch = {};
            sprites = {};
        }

        void PushBackground(const PixelOrigin &origin, const bool front = false) noexcept {
            if (backgroundCount == backgroundQueue.size())
                ClearBackground();
            if (front) {
                backgroundHead = (backgroundHead + 15) & 15;
                backgroundQueue[backgroundHead] = origin;
            } else {
                backgroundQueue[(backgroundHead + backgroundCount) & 15] = origin;
            }
            ++backgroundCount;
        }

        [[nodiscard]] PixelOrigin PopBackground() noexcept {
            if (!backgroundCount)
                return {};
            const auto result = backgroundQueue[backgroundHead];
            backgroundHead = (backgroundHead + 1) & 15;
            --backgroundCount;
            return result;
        }

        [[nodiscard]] SpriteOrigins PopSprites(const uint8_t slot) noexcept {
            const auto result = sprites[slot];
            sprites[slot] = {};
            return result;
        }

        void Pipeline(const uint16_t x, const uint16_t y, const PipelineReason reason, const PixelOrigin &origin = {}) const noexcept {
            if (pipeline)
                pipeline(owner, {position, branch, x, y, reason, origin});
        }
    };
} // namespace diagnostics
