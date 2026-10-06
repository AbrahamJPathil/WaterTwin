// PC test harness: g++ -std=c++17 -I. *.cpp -o test && ./test
#include "intelligence.h"
#include <cmath>
#include <cstdio>
#include <functional>

using DT = DiagnosisType;
using D = Decision;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static const uint32_t CYCLE_MS = 600000;  // 10 min

// Steady healthy water with a little deterministic jitter.
static FeatureSet steady(int n) {
    FeatureSet f{};
    f.timestamp_ms = 3600000u + n * CYCLE_MS;
    float j = std::sin(n * 1.7f);
    f.ph = 7.0f + 0.01f * j;
    f.tds_ppm = 300 + 2 * j;
    f.turbidity = 0.10f + 0.002f * j;
    f.temp_c = 22 + 0.1f * j;
    float noise[4] = {0.02f, 2, 0.003f, 0.05f};
    for (int i = 0; i < 4; i++) { f.noise[i] = noise[i]; f.valid[i] = true; }
    f.chamber_wet = true;
    f.wet_duration_ms = 1e6f;
    f.acoustic_valid = true;
    f.ringdown_us = 400;
    f.ldr_lit = 2000;
    f.ldr_dark = 100;
    return f;
}

struct Run {
    Intelligence e;
    int n = 0;
    Result last{};
    Result step(const std::function<void(FeatureSet&)>& tweak = {}) {
        FeatureSet f = steady(n++);
        if (tweak) tweak(f);
        return last = e.process(f);
    }
    void warm(int cycles = 30) { for (int i = 0; i < cycles; i++) step(); }
};

static void test(const char* name, const std::function<void()>& fn) {
    int before = failures;
    fn();
    std::printf("%s %s\n", failures == before ? "ok  " : "FAIL", name);
}

int main() {
    test("NORMAL: steady values, small noise", [] {
        Run r; r.warm();
        CHECK(r.last.diagnosis.type == DT::NORMAL);
        CHECK(r.last.decision == D::SLEEP);
        CHECK(!r.last.water.abnormal);
    });

    test("single pH spike stays NORMAL", [] {
        Run r; r.warm();
        r.step([](FeatureSet& f) { f.ph = 9.5f; });
        CHECK(r.last.diagnosis.type == DT::NORMAL);
        for (int i = 0; i < 5; i++) { r.step(); CHECK(r.last.diagnosis.type == DT::NORMAL); }
    });

    test("WATER_EVENT: pH, TDS, turbidity shift together; never CLEAN", [] {
        Run r; r.warm();
        bool seen = false;
        for (int i = 0; i < 8; i++) {
            r.step([](FeatureSet& f) { f.ph = 8.5f; f.tds_ppm = 600; f.turbidity = 0.4f; });
            CHECK(r.last.decision != D::CLEAN);
            seen |= r.last.diagnosis.type == DT::WATER_EVENT && r.last.decision == D::PRESERVE_SAMPLE_AND_ALERT;
        }
        CHECK(seen);
        CHECK(r.last.diagnosis.type == DT::WATER_EVENT);  // baseline frozen: event doesn't become normal
    });

    test("SENSOR_DRIFT: pH creeps alone over 20 cycles", [] {
        Run r; r.warm();
        bool seen = false;
        for (int i = 1; i <= 20; i++) {
            r.step([i](FeatureSet& f) { f.ph += 0.08f * i; });
            CHECK(r.last.diagnosis.type != DT::WATER_EVENT);
            seen |= r.last.diagnosis.type == DT::SENSOR_DRIFT && r.last.diagnosis.suspect == SensorId::PH
                    && r.last.decision == D::RECALIBRATE;
        }
        CHECK(seen);
    });

    static auto foul = [](Run& r, int i, bool acoustic) {
        return r.step([=](FeatureSet& f) {
            f.turbidity += 0.015f * i;
            f.ringdown_us = 400 * (1 - 0.3f * (i > 10 ? 1 : i / 10.0f));
            f.acoustic_valid = acoustic;
        });
    };

    static auto foulUntilClean = [](Run& r) {
        r.warm();
        for (int i = 1; i <= 30; i++) if (foul(r, i, true).decision == D::CLEAN) return i;
        return 0;
    };

    test("SENSOR_FOULING (turbidity): creeps alone, ring-down shortens", [] {
        Run r;
        int at = foulUntilClean(r);
        CHECK(at > 0);
        CHECK(r.last.diagnosis.type == DT::SENSOR_FOULING);
        CHECK(r.last.diagnosis.suspect == SensorId::TURBIDITY);
        CHECK(r.last.diagnosis.ev_acoustic > 0.5f);
    });

    test("fouling with acoustic_valid false -> DRIFT, confidence <= 0.6", [] {
        Run r; r.warm();
        bool seen = false;
        for (int i = 1; i <= 30; i++) {
            Result x = foul(r, i, false);
            CHECK(x.diagnosis.type != DT::SENSOR_FOULING);
            CHECK(x.decision != D::CLEAN);
            if (x.diagnosis.type == DT::SENSOR_DRIFT) {
                seen = true;
                CHECK(x.diagnosis.suspect == SensorId::TURBIDITY);
                CHECK(x.diagnosis.confidence <= 0.6f);
            }
        }
        CHECK(seen);
    });

    test("SENSOR_FAILURE: DS18B20 -127", [] {
        Run r; r.warm();
        for (int i = 0; i < 4; i++) r.step([](FeatureSet& f) { f.temp_c = -127; });
        CHECK(r.last.diagnosis.type == DT::SENSOR_FAILURE);
        CHECK(r.last.diagnosis.suspect == SensorId::TEMP);
        CHECK(r.last.decision == D::MAINTENANCE_ALERT);
    });

    test("SENSOR_FAILURE: LDR lit ~= dark (LED dead)", [] {
        Run r; r.warm();
        for (int i = 0; i < 4; i++) r.step([](FeatureSet& f) { f.ldr_lit = f.ldr_dark + 5; });
        CHECK(r.last.diagnosis.type == DT::SENSOR_FAILURE);
        CHECK(r.last.diagnosis.suspect == SensorId::TURBIDITY);
    });

    test("SENSOR_FAILURE: TDS ~0 after good wet cycles", [] {
        Run r; r.warm();
        for (int i = 0; i < 4; i++) r.step([](FeatureSet& f) { f.tds_ppm = 0; });
        CHECK(r.last.diagnosis.type == DT::SENSOR_FAILURE);
        CHECK(r.last.diagnosis.suspect == SensorId::TDS);
    });

    test("chamber dry: SLEEP, never failure; CLEAN revalidation abandoned", [] {
        Run r; r.warm();
        for (int i = 0; i < 4; i++) {
            r.step([](FeatureSet& f) { f.chamber_wet = false; f.tds_ppm = 0; f.acoustic_valid = false; });
            CHECK(r.last.decision == D::SLEEP);
            CHECK(r.last.diagnosis.type != DT::SENSOR_FAILURE);
        }
        r.step();  // refill: first wet cycle after dry must not call TDS disconnected
        CHECK(r.last.diagnosis.type != DT::SENSOR_FAILURE);

        Run c;
        CHECK(foulUntilClean(c) > 0);
        CHECK(c.e.post_clean == 2);
        c.step([](FeatureSet& f) { f.chamber_wet = false; });  // drains before/while cleaning
        CHECK(c.last.decision == D::SLEEP);
        CHECK(c.e.post_clean == 0);
    });

    test("temperature jump is not a WATER_EVENT", [] {
        Run r; r.warm();
        for (int i = 0; i < 6; i++) {
            r.step([](FeatureSet& f) { f.temp_c += 6; f.ph += 0.1f; f.tds_ppm += 8; });
            CHECK(r.last.diagnosis.type != DT::WATER_EVENT);
            CHECK(r.last.decision != D::PRESERVE_SAMPLE_AND_ALERT);
        }
    });

    test("TDS saturated at 1000 ppm during an event", [] {
        Run r; r.warm();
        for (int i = 0; i < 4; i++)
            r.step([](FeatureSet& f) { f.ph = 5.5f; f.tds_ppm = 1000; f.turbidity = 0.35f; f.noise[1] = 0; });
        CHECK(r.last.diagnosis.type == DT::WATER_EVENT);
        CHECK(r.last.water.tds_saturated);
        CHECK(r.last.decision == D::PRESERVE_SAMPLE_AND_ALERT);
    });

    test("UNKNOWN: contradictory readings, three in a row escalate", [] {
        Run r; r.warm();
        int alerts = 0;
        for (int i = 0; i < 3; i++) {
            r.step([i](FeatureSet& f) {
                float s = i % 2 ? 1 : -1;
                f.ph += 0.2f * s; f.tds_ppm -= 20 * s;  // two sensors off, opposite ways, not enough to be an event
            });
            CHECK(r.last.diagnosis.type == DT::UNKNOWN);
            CHECK(r.last.decision != D::CLEAN);
            alerts += r.last.decision == D::MAINTENANCE_ALERT;
        }
        CHECK(r.last.decision == D::MAINTENANCE_ALERT);
        CHECK(alerts == 1);
    });

    test("clean then recover: health and ring-down return, baselines reset", [] {
        Run r;
        CHECK(foulUntilClean(r) > 0);
        r.step([](FeatureSet& f) { f.acoustic_valid = true; f.ringdown_us = 50; });  // cleaning noise
        CHECK(r.last.decision == D::RESAMPLE);
        CHECK(r.last.diagnosis.type != DT::SENSOR_FAILURE);
        r.step();  // clean water, ring-down back to 400
        CHECK(r.last.decision == D::SLEEP);
        CHECK(std::fabs(r.e.water.base[(int)SensorId::TURBIDITY].mean - r.e.integrity.short_ema[2]) < 0.01f);  // reseeded from clean reading
        CHECK(std::fabs(r.e.diag.ring_base - 400) < 1);
        bool cleaned_again = false;
        for (int i = 0; i < 10; i++) cleaned_again |= r.step().decision == D::CLEAN;
        CHECK(!cleaned_again);
        CHECK(r.last.diagnosis.type == DT::NORMAL);
    });

    test("clean then no recovery: MAINTENANCE_ALERT", [] {
        Run r;
        int at = foulUntilClean(r);
        CHECK(at > 0);
        foul(r, at + 1, true);
        CHECK(r.last.decision == D::RESAMPLE);
        foul(r, at + 2, true);
        CHECK(r.last.decision == D::MAINTENANCE_ALERT);
    });

    test("clean budget: MAX_CLEANS_PER_DAY then MAINTENANCE_ALERT", [] {
        DecisionEngine d;
        FeatureSet f = steady(0);
        WaterState w{};
        Diagnosis g{}; g.type = DT::SENSOR_FOULING; g.suspect = SensorId::TURBIDITY; g.confidence = 0.9f;
        for (int i = 0; i < 3; i++) CHECK(d.update(f, w, g, false) == D::CLEAN);
        CHECK(d.update(f, w, g, false) == D::MAINTENANCE_ALERT);
        f.timestamp_ms += 86400000u;
        CHECK(d.update(f, w, g, false) == D::CLEAN);
        CHECK(d.update(f, w, g, true) == D::RESAMPLE);   // water event pending
        f.chamber_wet = false;
        CHECK(d.update(f, w, g, false) == D::SLEEP);     // never clean dry
    });

    std::printf(failures ? "\n%d check(s) failed\n" : "\nall passed\n", failures);
    return failures != 0;
}
