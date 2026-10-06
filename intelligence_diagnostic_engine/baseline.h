#pragma once
#include <cmath>

// EMA mean/variance; the first sample seeds it. Freezing = the caller skips update().
struct Baseline {
    float mean = 0, var = 0;
    bool seeded = false;

    void update(float x, float alpha) {
        if (!seeded) { mean = x; var = 0; seeded = true; return; }
        float d = x - mean;
        mean += alpha * d;
        var = (1 - alpha) * (var + alpha * d * d);
    }
    float z(float x, float min_std) const {
        return seeded ? (x - mean) / std::fmax(std::sqrt(var), min_std) : 0;
    }
    void reset() { seeded = false; }
};
