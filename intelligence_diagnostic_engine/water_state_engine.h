#pragma once
#include "types.h"
#include "baseline.h"

struct WaterStateEngine {
    Baseline base[N_CHEM];
    float prev_temp = NAN;

    // health = last cycle's sensor health (unhealthy sensors don't vote)
    WaterState update(const FeatureSet& f, const SensorHealth health[]);
    // Baseline update, frozen while abnormal so an event never becomes the new normal.
    void learn(const FeatureSet& f, const WaterState& w, const SensorHealth health[]);
};
