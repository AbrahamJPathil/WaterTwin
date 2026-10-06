#pragma once
#include "types.h"
#include "baseline.h"

struct SensorIntegrityEngine {
    SensorHealth h[N_SENSORS];
    float inst[N_SENSORS] = {};       // this cycle's unsmoothed health, used by revalidation
    float short_ema[N_SENSORS] = {};
    bool  short_seeded[N_SENSORS] = {};
    Baseline usual_noise[N_SENSORS];
    Baseline dark;                    // LDR dark reading, for light-leak detection
    int fail_count[N_SENSORS] = {}, flat_count[N_SENSORS] = {};
    int acoustic_fail_count = 0;
    bool acoustic_failed = false;
    bool prev_wet = false;
    int cycles = 0;

    SensorIntegrityEngine();
    // chem = the water engine's long baselines (drift reference)
    void update(const FeatureSet& f, const WaterState& w, const Baseline chem[]);
    void learn(const FeatureSet& f, bool frozen);
    // After a clean: forget the fouled history so the next reading is judged on its own.
    void resetSensor(int i) { short_seeded[i] = false; }
};
