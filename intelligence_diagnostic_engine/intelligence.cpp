#include "intelligence.h"
#include "config.h"
#include <cmath>

using namespace cfg;

Result Intelligence::process(const FeatureSet& in) {
    Result r{};
    for (int i = 0; i < N_SENSORS; i++) r.health[i] = integrity.h[i];

    if (!in.chamber_wet) {
        // Tank drained: expected, not a fault. No diagnosis until it's wet again.
        // Any clean in flight was aborted by the controller, so drop its revalidation too.
        integrity.prev_wet = false;
        post_clean = 0;
        r.diagnosis = {DiagnosisType::UNKNOWN, SensorId::COUNT, 0, 0, 0, 0, 0, 0, in.timestamp_ms};
        r.decision = Decision::SLEEP;
        return r;
    }

    FeatureSet f = in;
    if (post_clean == 2) f.acoustic_valid = false;  // guard 4: cleaning noise must never read as a ring-down

    r.water = water.update(f, integrity.h);
    integrity.update(f, r.water, water.base);
    r.diagnosis = diag.update(f, r.water, integrity);
    if (r.water.abnormal) post_clean = 0;  // a real event outranks revalidation

    if (post_clean == 2) {
        r.decision = Decision::RESAMPLE;  // fresh sample before judging the clean
    } else if (post_clean == 1) {
        // Recovered = suspect health up by RECOVERY_DELTA and ring-down back near the clean baseline.
        int s = clean_suspect;
        bool ring_back = f.acoustic_valid && diag.ring_base > 0 &&
                         std::fabs(f.ringdown_us - diag.ring_base) <= RINGDOWN_RECOVERED * diag.ring_base;
        if (integrity.inst[s] - pre_clean_health >= RECOVERY_DELTA && ring_back) {
            water.base[(int)SensorId::TURBIDITY].reset();  // window wiped: clear reference changed
            diag.ring_base = f.ringdown_us;
            integrity.h[s].health_score = integrity.inst[s];
            r.decision = Decision::SLEEP;
        } else {
            r.decision = Decision::MAINTENANCE_ALERT;
        }
    } else {
        r.decision = decide.update(f, r.water, r.diagnosis, diag.waterEventPending());
    }
    if (post_clean) post_clean--;

    if (r.decision == Decision::CLEAN) {
        post_clean = 2;
        clean_suspect = r.diagnosis.suspect < SensorId::COUNT ? (int)r.diagnosis.suspect : 0;
        pre_clean_health = integrity.h[clean_suspect].health_score;
        integrity.resetSensor(clean_suspect);
    }

    water.learn(f, r.water, integrity.h);
    integrity.learn(f, r.water.abnormal);
    for (int i = 0; i < N_SENSORS; i++) r.health[i] = integrity.h[i];
    return r;
}

Result process(const FeatureSet& f) {
    static Intelligence engine;
    return engine.process(f);
}
