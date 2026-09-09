#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <JuceHeader.h>
#include "Art/TrackRenderer.h"
#include "Geometry/PositionMath.h"

class TrackImageCache
{
public:
    TrackImageCache() : alive(std::make_shared<std::atomic<bool>>(true)) {}
    ~TrackImageCache() { shutdown(); }

    struct BakedTrack {
        juce::Image fadedTrack;
        juce::Image layers[TrackRenderer::NUM_LAYERS];
        int overflow = 0;
        bool valid = false;
    };

    BakedTrack& get(bool isDrums) { return isDrums ? drums : guitar; }
    const BakedTrack& get(bool isDrums) const { return isDrums ? drums : guitar; }

    /** Request an async bake on a background thread.
        Drops the request if a bake is already in flight.
        onComplete is called on the message thread after staging is ready. */
    void requestBake(TrackRenderer& renderer, int width, int height, int overflowGuitar, int overflowDrums,
                     float farFadeEnd, float farFadeLen, float farFadeCurve, float posEnd,
                     const PositionConstants::NormalizedCoordinates* guitarLaneCoords, int guitarLaneCount,
                     const PositionConstants::NormalizedCoordinates* drumLaneCoords, int drumLaneCount,
                     float texScale, float texOpacity,
                     std::function<void()> onComplete)
    {
        // Don't overlap bakes
        bool expected = false;
        if (!baking.compare_exchange_strong(expected, true))
            return;

        // Snapshot generation at request time
        int requestGen = gen.load(std::memory_order_relaxed);

        // Copy lane coords so background thread doesn't alias GUI-thread arrays
        std::array<PositionConstants::NormalizedCoordinates, PositionConstants::GUITAR_LANE_COUNT> gLanes;
        std::array<PositionConstants::NormalizedCoordinates, PositionConstants::DRUM_LANE_COUNT> dLanes;
        std::copy_n(guitarLaneCoords, guitarLaneCount, gLanes.begin());
        std::copy_n(drumLaneCoords, drumLaneCount, dLanes.begin());

        // Copy texture settings to scratch renderer before spawning thread
        renderer.textureScale = texScale;
        renderer.textureOpacity = texOpacity;

        // Join any previous thread
        if (bakeThread.joinable())
            bakeThread.join();

        auto aliveFlag = alive;  // shared_ptr copy for the lambda chain

        bakeThread = std::thread([this, &renderer, width, height, overflowGuitar, overflowDrums,
                                  farFadeEnd, farFadeLen, farFadeCurve, posEnd,
                                  gLanes, dLanes, guitarLaneCount, drumLaneCount,
                                  requestGen, aliveFlag,
                                  onComplete = std::move(onComplete)]() {
            BakedTrack stagedG, stagedD;
            bakeInto(renderer, stagedG, stagedD, width, height, overflowGuitar, overflowDrums,
                     farFadeEnd, farFadeLen, farFadeCurve, posEnd,
                     gLanes.data(), guitarLaneCount, dLanes.data(), drumLaneCount);

            int stagedW = width, stagedH = height, stagedOG = overflowGuitar, stagedOD = overflowDrums;

            juce::MessageManager::callAsync([this, requestGen, onComplete, aliveFlag,
                                             sg = std::move(stagedG), sd = std::move(stagedD),
                                             stagedW, stagedH, stagedOG, stagedOD]() mutable {
                // Owner destroyed — discard silently
                if (!aliveFlag->load(std::memory_order_relaxed))
                    return;

                baking.store(false, std::memory_order_relaxed);

                // Discard if a newer invalidation happened since we started
                if (requestGen != gen.load(std::memory_order_relaxed))
                {
                    if (onComplete)
                        onComplete();
                    return;
                }

                // Commit staged → active
                guitar = std::move(sg);
                drums = std::move(sd);
                bakedWidth = stagedW;
                bakedHeight = stagedH;
                bakedOverflowGuitar = stagedOG;
                bakedOverflowDrums = stagedOD;
                dirty = false;

                if (onComplete)
                    onComplete();
            });
        });
    }

    void invalidate()
    {
        dirty = true;
        gen.fetch_add(1, std::memory_order_relaxed);
    }

    bool isDirty() const { return dirty; }
    bool isValid() const { return !dirty && guitar.valid && drums.valid; }

    int getBakedWidth() const { return bakedWidth; }
    int getBakedHeight() const { return bakedHeight; }
    int getBakedOverflow(bool isDrums) const { return isDrums ? bakedOverflowDrums : bakedOverflowGuitar; }

    void shutdown()
    {
        alive->store(false, std::memory_order_relaxed);
        if (bakeThread.joinable())
            bakeThread.join();
    }

private:
    /** Bake both guitar and drums into the provided output structs. */
    static void bakeInto(TrackRenderer& renderer, BakedTrack& outGuitar, BakedTrack& outDrums,
                         int width, int height, int overflowGuitar, int overflowDrums,
                         float farFadeEnd, float farFadeLen, float farFadeCurve, float posEnd,
                         const PositionConstants::NormalizedCoordinates* guitarLaneCoords, int guitarLaneCount,
                         const PositionConstants::NormalizedCoordinates* drumLaneCoords, int drumLaneCount)
    {
        auto bakeOne = [&](bool isDrums, BakedTrack& out) {
            renderer.activePart = isDrums ? Part::DRUMS : Part::GUITAR;
            auto* coords = isDrums ? drumLaneCoords : guitarLaneCoords;
            int count = isDrums ? drumLaneCount : guitarLaneCount;
            int ov = isDrums ? overflowDrums : overflowGuitar;
            renderer.setLaneCoords(coords, count);
            renderer.invalidate();
            renderer.rebuild(width, height, ov, farFadeEnd, farFadeLen, farFadeCurve, posEnd);

            out.fadedTrack = renderer.getFadedTrackImage().createCopy();
            for (int i = 0; i < TrackRenderer::NUM_LAYERS; i++)
                out.layers[i] = renderer.getLayerImage((TrackRenderer::Layer)i).createCopy();
            out.overflow = ov;
            out.valid = true;
        };

        bakeOne(false, outGuitar);
        bakeOne(true, outDrums);
    }

    BakedTrack guitar, drums;
    bool dirty = true;
    std::atomic<int> gen{0};
    std::atomic<bool> baking{false};
    std::shared_ptr<std::atomic<bool>> alive;
    std::thread bakeThread;
    int bakedWidth = 0, bakedHeight = 0, bakedOverflowGuitar = 0, bakedOverflowDrums = 0;
};
