#pragma once
#include <cstdint>

// ACOUSTIC is only ever a Diagnosis suspect (transducer/LM358 fault); health[] covers the first COUNT.
enum class SensorId { PH, TDS, TURBIDITY, TEMP, COUNT, ACOUSTIC };
constexpr int N_SENSORS = (int)SensorId::COUNT;
constexpr int N_CHEM = 3;  // PH, TDS, TURBIDITY vote on the water; TEMP is context only

struct FeatureSet {
    uint32_t timestamp_ms;

    float ph;              // compensated, 0-14
    float tds_ppm;         // compensated, 0-1000
    float turbidity;       // 0-1 attenuation = 1 - (lit - dark) / clear_ref
    float temp_c;          // DS18B20

    float noise[4];        // std-dev within this cycle's sample burst, per SensorId
    bool  valid[4];        // false = upstream read error for that sensor

    bool  chamber_wet;     // sidecar currently holds water (mirrors the tank; no valve)
    float wet_duration_ms; // how long it's been continuously wet, for staleness checks

    bool  acoustic_valid;  // false while the ping circuit doesn't exist, chamber is dry, or during cleaning
    float ringdown_us;     // time LM358 envelope stays above threshold

    float ldr_lit, ldr_dark; // raw LDR counts, for the LED-dead / light-leak signatures
};

struct WaterState {
    float anomaly_score, confidence; bool abnormal;
    float z[N_CHEM];        // per chemistry sensor, vs baseline
    int   agreeing;         // sensors with |z| > Z_DEV
    uint8_t skipped;        // bit per SensorId: invalid or unhealthy, left out of the vote
    bool  tds_saturated;    // TDS >= TDS_SAT: counted high, z capped
    bool  temp_jump;        // confidence halved this cycle
};
struct SensorHealth { float health_score, drift_score, noise_score, confidence; bool failed; };

enum class DiagnosisType { NORMAL, WATER_EVENT, SENSOR_FOULING, SENSOR_DRIFT,
                           SENSOR_FAILURE, UNKNOWN };

enum class Decision { SLEEP, PRESERVE_SAMPLE_AND_ALERT, CLEAN, RECALIBRATE,
                      MAINTENANCE_ALERT, RESAMPLE };

struct Diagnosis {
    DiagnosisType type;
    SensorId      suspect;          // which sensor, for DRIFT / FOULING / FAILURE; COUNT = none
    float         confidence;
    float         ev_water, ev_degradation, ev_temporal, ev_cross_sensor, ev_acoustic;
    uint32_t      timestamp_ms;
};

struct Result { WaterState water; SensorHealth health[4]; Diagnosis diagnosis; Decision decision; };
Result process(const FeatureSet&);   // the only call the hardware team makes

inline float sensorValue(const FeatureSet& f, int i) {
    switch ((SensorId)i) {
        case SensorId::PH: return f.ph;
        case SensorId::TDS: return f.tds_ppm;
        case SensorId::TURBIDITY: return f.turbidity;
        default: return f.temp_c;
    }
}
inline float clamp01(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
