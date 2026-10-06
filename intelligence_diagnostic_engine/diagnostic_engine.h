#pragma once
#include "types.h"
#include "sensor_integrity_engine.h"

struct DiagnosticEngine {
    float ring_base = 0;  // ring-down clean baseline (µs); learned from the first valid ping, reset after each recovered clean
    DiagnosisType raw_type = DiagnosisType::UNKNOWN;
    SensorId raw_suspect = SensorId::COUNT;
    int streak = 0;

    // Output is UNKNOWN until the same diagnosis repeats N_CONFIRM cycles running.
    Diagnosis update(const FeatureSet& f, const WaterState& w, const SensorIntegrityEngine& s);
    bool waterEventPending() const;
};
