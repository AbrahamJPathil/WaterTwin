#include "decision_engine.h"
#include "config.h"

using namespace cfg;
using DT = DiagnosisType;

Decision DecisionEngine::update(const FeatureSet& f, const WaterState& w, const Diagnosis& d, bool water_event_pending) {
    if (d.type == DT::UNKNOWN) {
        if (++unknown_streak < MAX_RESAMPLE) return Decision::RESAMPLE;
        unknown_streak = 0;
        return Decision::MAINTENANCE_ALERT;
    }
    unknown_streak = 0;
    if (d.confidence < CONF_ACT) return Decision::RESAMPLE;

    switch (d.type) {
        case DT::NORMAL:         return Decision::SLEEP;
        case DT::WATER_EVENT:    return Decision::PRESERVE_SAMPLE_AND_ALERT;
        case DT::SENSOR_DRIFT:   return Decision::RECALIBRATE;
        case DT::SENSOR_FAILURE: return Decision::MAINTENANCE_ALERT;
        default: break;  // SENSOR_FOULING: guards below
    }

    // Guard 1: cleaning would destroy evidence of a real contamination event.
    if (w.abnormal || water_event_pending) return Decision::RESAMPLE;
    // Guard 2: the transducer must be submerged. The controller re-checks right before firing too.
    if (!f.chamber_wet) return Decision::SLEEP;
    // Guard 3: daily clean budget. ponytail: day = timestamp/24h, so the uint32 ms wrap (~49.7 d) also resets it.
    uint32_t day = f.timestamp_ms / MS_PER_DAY;
    if (day != clean_day) { clean_day = day; cleans_today = 0; }
    if (cleans_today >= MAX_CLEANS_PER_DAY) return Decision::MAINTENANCE_ALERT;
    cleans_today++;
    return Decision::CLEAN;
}
