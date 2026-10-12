// Deterministic gate-grid composition. UI/message thread only; no JUCE or allocation.
#pragma once
#include "Types.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace br {
enum class RhythmStyle { Euclidean, Backbeat, Syncopated, Sparse, Dense };
struct RhythmRequest {
    int steps = 16;
    int pulses = 5;
    int rotation = 0;
    uint64_t seed = 1;
    float variation = 0;
    float accent = 0.5f;
    RhythmStyle style = RhythmStyle::Euclidean;
};

// Does not touch gate entries outside the selected range or any other lane.
// The caller must publish the updated Patterns via the existing message-thread state path.
inline void generateRhythm(const RhythmRequest& req, Patterns& pattern)
{
    const int n = std::clamp(req.steps, 1, kMaxSteps);
    const int pulses = std::clamp(req.pulses, 0, n);
    const int rotation = ((req.rotation % n) + n) % n;
    const float variation = std::isfinite(req.variation) ? clampf(req.variation, 0.f, 1.f) : 0.f;
    const float accent = std::isfinite(req.accent) ? clampf(req.accent, 0.f, 1.f) : 0.5f;
    int enabled = 0;
    for (int i = 0; i < n; ++i) {
        const int at = (i + n - rotation) % n;
        const bool quarter = (4 * at) % n == 0;
        const bool eighthOffbeat = (8 * at) % n == 4 * (n % 2 == 0 ? 1 : 0);
        const double noise = hashUnit(req.seed, dGate + 700, at);
        bool on = false;
        switch (req.style) {
            case RhythmStyle::Euclidean:
                on = ((at + 1) * pulses / n) != (at * pulses / n);
                break;
            case RhythmStyle::Backbeat:
                on = quarter || ((8 * at / n) % 8 == 2 || (8 * at / n) % 8 == 6) && ((8 * at) % n == 0);
                break;
            case RhythmStyle::Syncopated:
                on = !quarter && ((8 * at) % n == 0 || noise < 0.25);
                break;
            case RhythmStyle::Sparse:
                on = quarter && noise < 0.9 || noise < 0.10;
                break;
            case RhythmStyle::Dense:
                on = quarter || noise < 0.70;
                break;
        }
        if (req.style != RhythmStyle::Euclidean && !quarter && variation > 0 &&
            hashUnit(req.seed, dGate + 701, at) < variation * 0.35)
            on = !on;
        GateStep& cell = pattern.gate[i];
        cell.on = static_cast<uint8_t>(on);
        cell.dir = 0;
        cell.fx = 0;
        cell.shape = -1;
        cell.width = on ? clampf(0.75f + (quarter ? 0.25f : -0.25f) * accent, 0.15f, 1.f) : -1.f;
        cell.depth = -1.f;
        cell.fadeIn = 0.f;
        cell.fadeOut = 0.f;
        enabled += on ? 1 : 0;
    }
    if (req.style != RhythmStyle::Euclidean && !enabled)
        pattern.gate[rotation].on = 1;
}
} // namespace br
