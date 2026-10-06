#pragma once
#include "types.h"

struct DecisionEngine {
    int unknown_streak = 0;
    uint32_t clean_day = 0;
    int cleans_today = 0;

    // Lookup, then the CLEAN guards. Revalidation after a clean is handled by the caller.
    Decision update(const FeatureSet& f, const WaterState& w, const Diagnosis& d, bool water_event_pending);
};
