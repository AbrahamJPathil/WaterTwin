#include "water_state_engine.h"
#include "config.h"
#include <cmath>

using namespace cfg;

WaterState WaterStateEngine::update(const FeatureSet& f, const SensorHealth health[]) {
    WaterState w{};
    float sum = 0;
    int used = 0;
    for (int i = 0; i < N_CHEM; i++) {
        w.z[i] = base[i].z(sensorValue(f, i), MIN_STD[i]);
        if (i == (int)SensorId::TDS && f.tds_ppm >= TDS_SAT) {
            w.tds_saturated = true;
            w.z[i] = Z_SAT;  // true value unknown: counts as high, magnitude capped
        }
        if (!f.valid[i] || health[i].failed || health[i].health_score < HEALTH_SKIP) {
            w.skipped |= 1 << i;
            continue;
        }
        float a = std::fmin(std::fabs(w.z[i]), Z_CAP);
        sum += a;
        used++;
        if (a > Z_DEV) w.agreeing++;
    }
    float raw = used ? sum / used * w.agreeing / 3.0f : 0;
    w.anomaly_score = 1 / (1 + std::exp(-(raw - Z_MID)));
    w.abnormal = w.anomaly_score > WATER_ABNORMAL;
    w.confidence = CONF_BY_COUNT[w.agreeing ? w.agreeing : used];

    // A temperature step means pH/TDS compensation may lag: trust the vote less, never more.
    const int T = (int)SensorId::TEMP;
    if (f.valid[T] && !health[T].failed) {
        if (!std::isnan(prev_temp) && std::fabs(f.temp_c - prev_temp) > TEMP_JUMP) {
            w.temp_jump = true;
            w.confidence *= 0.5f;
        }
        prev_temp = f.temp_c;
    }
    return w;
}

void WaterStateEngine::learn(const FeatureSet& f, const WaterState& w, const SensorHealth health[]) {
    if (w.abnormal) return;
    for (int i = 0; i < N_CHEM; i++) {
        if (!f.valid[i] || health[i].failed) continue;
        if (i == (int)SensorId::TDS && w.tds_saturated) continue;
        base[i].update(sensorValue(f, i), BASE_ALPHA);
    }
}
