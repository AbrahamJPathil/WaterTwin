#pragma once
#include "water_state_engine.h"
#include "sensor_integrity_engine.h"
#include "diagnostic_engine.h"
#include "decision_engine.h"

// All cross-cycle state. The free process() in types.h runs one static instance;
// tests construct their own for a clean slate.
struct Intelligence {
    WaterStateEngine water;
    SensorIntegrityEngine integrity;
    DiagnosticEngine diag;
    DecisionEngine decide;

    int post_clean = 0;  // 2 = cycle after CLEAN (acoustic blanked), 1 = revalidation cycle
    int clean_suspect = 0;
    float pre_clean_health = 0;

    Result process(const FeatureSet& f);
};
