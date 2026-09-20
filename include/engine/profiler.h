#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

// Rolling frame-time statistics.
//
// Averages hide the frames people actually notice: one 40ms hitch inside a
// second of 2ms frames still reads as an average of ~2.6ms while feeling like
// a stutter. Percentiles are what make that hitch visible, so this keeps a
// window of raw samples rather than a running mean.
class Profiler {
public:
    static constexpr std::size_t kWindow = 240;   // ~4s at 60fps

    struct FrameStats {
        float avgMs = 0.0f;
        float p50Ms = 0.0f;
        float p95Ms = 0.0f;
        float p99Ms = 0.0f;
        float maxMs = 0.0f;
        float fps = 0.0f;
        std::size_t samples = 0;
    };

    struct Counters {
        int drawCalls = 0;
        int culled = 0;
        int considered = 0;
    };

    void addSample(float ms) {
        if (samples_.size() < kWindow) samples_.push_back(ms);
        else { samples_[cursor_] = ms; }
        cursor_ = (cursor_ + 1) % kWindow;
    }

    void beginFrame() { counters_ = {}; }
    void noteDraw() { ++counters_.drawCalls; }
    void noteCulled() { ++counters_.culled; }
    void noteConsidered() { ++counters_.considered; }

    const Counters& counters() const { return counters_; }

    FrameStats stats() const {
        FrameStats s;
        if (samples_.empty()) return s;

        std::vector<float> sorted(samples_);
        std::sort(sorted.begin(), sorted.end());

        double sum = 0.0;
        for (float v : sorted) sum += v;

        auto pick = [&sorted](double q) {
            std::size_t i = (std::size_t)(q * (double)(sorted.size() - 1) + 0.5);
            return sorted[std::min(i, sorted.size() - 1)];
        };

        s.samples = sorted.size();
        s.avgMs = (float)(sum / (double)sorted.size());
        s.p50Ms = pick(0.50);
        s.p95Ms = pick(0.95);
        s.p99Ms = pick(0.99);
        s.maxMs = sorted.back();
        s.fps = s.avgMs > 0.0f ? 1000.0f / s.avgMs : 0.0f;
        return s;
    }

    void reset() { samples_.clear(); cursor_ = 0; }

private:
    std::vector<float> samples_;
    std::size_t cursor_ = 0;
    Counters counters_{};
};
