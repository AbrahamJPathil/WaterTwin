#include "diagnostic_engine.h"
#include "config.h"
#include <cmath>
#include <initializer_list>

using namespace cfg;
using DT = DiagnosisType;

static float mean(std::initializer_list<float> v) {
    float s = 0;
    for (float x : v) s += x;
    return s / v.size();
}

Diagnosis DiagnosticEngine::update(const FeatureSet& f, const WaterState& w, const SensorIntegrityEngine& s) {
    const SensorHealth* h = s.h;
    bool ping_ok = f.acoustic_valid && !s.acoustic_failed;
    if (ping_ok && ring_base == 0) ring_base = f.ringdown_us;  // assumes the chamber is installed clean
    bool acoustic = ping_ok && ring_base > 0;
    float damp = acoustic ? 1 - f.ringdown_us / ring_base : 0;

    int weak = 0, drifting = 0;
    bool all_healthy = true;
    for (int i = 0; i < N_CHEM; i++) {
        if (h[i].health_score < h[weak].health_score) weak = i;
        if (h[i].health_score < HEALTH_OK) all_healthy = false;
        if (h[i].drift_score > DRIFT_FLAG) drifting++;
    }
    int others_steady = 0;
    for (int j = 0; j < N_CHEM; j++)
        if (j != weak && std::fabs(w.z[j]) <= Z_DEV) others_steady++;

    Diagnosis d{};
    d.timestamp_ms = f.timestamp_ms;
    d.suspect = SensorId::COUNT;
    d.ev_water = w.anomaly_score;
    d.ev_degradation = 1 - h[weak].health_score;
    d.ev_cross_sensor = others_steady / float(N_CHEM - 1);
    d.ev_acoustic = clamp01(damp / (2 * DAMP_FRAC));  // 0.5 at the "short" threshold

    int failed = -1;
    for (int i = 0; i < N_SENSORS && failed < 0; i++)
        if (h[i].failed) failed = i;

    bool cap_conf = false;
    if (failed >= 0 || s.acoustic_failed) {
        // 1. A hard signature repeated FAIL_CONFIRM cycles is a broken part; nothing else is trustworthy.
        d.type = DT::SENSOR_FAILURE;
        d.suspect = failed >= 0 ? (SensorId)failed : SensorId::ACOUSTIC;
        d.ev_degradation = 1;
    } else if (w.abnormal && all_healthy) {
        // 2. Several healthy sensors moved off baseline together: the water changed.
        d.type = DT::WATER_EVENT;
        d.ev_cross_sensor = w.agreeing / 3.0f;
    } else if (h[weak].health_score < HEALTH_OK && acoustic && damp > DAMP_FRAC) {
        // 3. A degraded sensor plus walls damping the ping: physical film, not chemistry.
        d.type = DT::SENSOR_FOULING;
        d.suspect = (SensorId)weak;
    } else if (h[weak].health_score < HEALTH_OK && !acoustic) {
        // 3b. Fits fouling but there's no ping to prove a film: report drift, confidence capped.
        d.type = DT::SENSOR_DRIFT;
        d.suspect = (SensorId)weak;
        cap_conf = true;
    } else if (h[weak].health_score < HEALTH_OK && h[weak].drift_score > DRIFT_FLAG && drifting == 1) {
        // 4. One sensor wandering off its reference while the others hold: calibration drift.
        d.type = DT::SENSOR_DRIFT;
        d.suspect = (SensorId)weak;
    } else if (all_healthy && !w.abnormal && w.agreeing <= 1) {
        // 5. Healthy sensors, at most one stray reading: nothing to act on.
        d.type = DT::NORMAL;
    } else {
        // 6. No rule explains the readings; never force a guess.
        d.type = DT::UNKNOWN;
    }

    if (d.type == raw_type && d.suspect == raw_suspect) streak++;
    else { raw_type = d.type; raw_suspect = d.suspect; streak = 1; }
    d.ev_temporal = float(streak < TEMPORAL_WINDOW ? streak : TEMPORAL_WINDOW) / TEMPORAL_WINDOW;

    // Confidence = mean strength of the conditions that fired.
    switch (d.type) {
        case DT::SENSOR_FAILURE: d.confidence = mean({d.ev_degradation, d.ev_temporal}); break;
        case DT::WATER_EVENT:    d.confidence = mean({d.ev_water, w.confidence, d.ev_cross_sensor, d.ev_temporal}); break;
        case DT::SENSOR_FOULING: d.confidence = mean({d.ev_degradation, d.ev_cross_sensor, d.ev_acoustic, d.ev_temporal}); break;
        case DT::SENSOR_DRIFT:   d.confidence = mean({d.ev_degradation, d.ev_cross_sensor, d.ev_temporal}); break;
        case DT::NORMAL:         d.confidence = mean({1 - d.ev_water, 1 - d.ev_degradation, d.ev_temporal}); break;
        default:                 d.confidence = 0; break;
    }
    if (cap_conf && d.confidence > NO_ACOUSTIC_CONF_CAP) d.confidence = NO_ACOUSTIC_CONF_CAP;

    if (streak < N_CONFIRM) {  // not confirmed yet: keep the evidence, withhold the verdict
        d.type = DT::UNKNOWN;
        d.suspect = SensorId::COUNT;
        d.confidence = 0;
    }
    return d;
}

bool DiagnosticEngine::waterEventPending() const {
    return raw_type == DT::WATER_EVENT && streak < N_CONFIRM;
}
