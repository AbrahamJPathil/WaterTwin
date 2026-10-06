#pragma once
#include <cstdint>
// Every threshold lives here. All values are placeholders until field calibration.
// Per-sensor arrays are indexed by SensorId: {PH, TDS, TURBIDITY, TEMP}.

namespace cfg {

// Baseline
constexpr float BASE_ALPHA  = 0.02f;  // slow EMA: long reference mean/variance
constexpr float SHORT_ALPHA = 0.3f;   // fast EMA compared against it for drift
constexpr float MIN_STD[4]  = {0.05f, 5.0f, 0.01f, 0.2f};  // z-score std floor (pH, ppm, attenuation, °C)

// Water State Engine
constexpr float Z_DEV          = 3.0f;
constexpr float Z_CAP          = 10.0f;  // one wild sensor can't out-vote the others
constexpr float Z_MID          = 2.0f;
constexpr float WATER_ABNORMAL = 0.7f;
constexpr float CONF_BY_COUNT[4] = {0.0f, 0.4f, 0.7f, 1.0f};  // by supporting sensors; one alone caps at 0.4
constexpr float TEMP_JUMP      = 3.0f;   // °C between cycles
constexpr float TDS_SAT        = 990.0f; // ppm; probe clips at 1000
constexpr float Z_SAT          = 4.0f;   // capped z for a saturated TDS reading (> Z_DEV, so it counts as high)
constexpr float HEALTH_SKIP    = 0.4f;

// Sensor Integrity Engine
constexpr float DRIFT_FULL[4]    = {1.0f, 150.0f, 0.2f, 0.0f};  // short-vs-long gap that scores drift 1.0; TEMP has no drift
constexpr float NOISE_FLOOR[4]   = {0.005f, 1.0f, 0.002f, 0.02f}; // lower bound on "usual" noise
constexpr float NOISE_FULL_RATIO = 5.0f;  // noise this many times usual scores 1.0
constexpr float W_DRIFT          = 0.8f;
constexpr float W_NOISE          = 0.4f;
constexpr int   HEALTH_WINDOW    = 5;
constexpr int   FAIL_CONFIRM     = 2;
constexpr float HEALTH_OK        = 0.6f;
constexpr float DRIFT_FLAG       = 0.5f;

// Hard failure signatures
constexpr float TEMP_DISCONNECTED = -126.5f;  // DS18B20 reports -127
constexpr float TEMP_POWER_ON     = 85.0f;    // DS18B20 power-on value, exact match
constexpr float PH_PIN_LO = 0.3f, PH_PIN_HI = 13.7f;
constexpr float FLAT_NOISE[4]     = {1e-4f, 0.05f, 0.0f, 0.0f};  // "near-zero noise" for pH, TDS
constexpr int   FLAT_CYCLES       = 10;
constexpr float TDS_ZERO          = 5.0f;     // ppm
constexpr float LED_DEAD_DELTA    = 20.0f;    // lit - dark counts
constexpr float LEAK_DELTA        = 200.0f;   // dark counts above its baseline
constexpr float RINGDOWN_ZERO_US  = 5.0f;

// Diagnosis
constexpr int   N_CONFIRM       = 2;
constexpr int   TEMPORAL_WINDOW = 3;
constexpr float DAMP_FRAC       = 0.25f;
constexpr float NO_ACOUSTIC_CONF_CAP = 0.6f;

// Decision
constexpr float    CONF_ACT           = 0.6f;
constexpr int      MAX_RESAMPLE       = 3;
constexpr int      MAX_CLEANS_PER_DAY = 3;
constexpr float    RECOVERY_DELTA     = 0.3f;
constexpr float    RINGDOWN_RECOVERED = 0.10f;  // within 10% of clean baseline
constexpr uint32_t MS_PER_DAY         = 86400000u;

}  // namespace cfg
