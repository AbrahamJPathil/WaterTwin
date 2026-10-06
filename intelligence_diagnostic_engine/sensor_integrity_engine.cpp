#include "sensor_integrity_engine.h"
#include "config.h"
#include <cmath>

using namespace cfg;

SensorIntegrityEngine::SensorIntegrityEngine() {
    for (auto& s : h) s = {1, 0, 0, 0, false};
}

void SensorIntegrityEngine::update(const FeatureSet& f, const WaterState& w, const Baseline chem[]) {
    cycles++;
    for (int i = 0; i < N_SENSORS; i++) {
        SensorHealth& s = h[i];
        float x = sensorValue(f, i);

        // Flat-noise counter (pH, TDS): a live probe always jitters a little.
        bool saturated = i == (int)SensorId::TDS && w.tds_saturated;
        flat_count[i] = (FLAT_NOISE[i] > 0 && !saturated && f.noise[i] < FLAT_NOISE[i]) ? flat_count[i] + 1 : 0;

        // Hard signatures: set failed without smoothing once seen FAIL_CONFIRM cycles running.
        bool sig = false;
        switch ((SensorId)i) {
            case SensorId::TEMP:  // -127 = disconnected, exactly 85 = read before conversion / power fault
                sig = f.temp_c <= TEMP_DISCONNECTED || f.temp_c == TEMP_POWER_ON; break;
            case SensorId::PH:    // pinned at a rail, or dead flat = dry, cracked or unplugged
                sig = f.ph < PH_PIN_LO || f.ph > PH_PIN_HI || flat_count[i] >= FLAT_CYCLES; break;
            case SensorId::TDS:   // ~0 in water that was there last cycle = disconnected; flat = fouled/shorted
                sig = (f.tds_ppm < TDS_ZERO && prev_wet) || flat_count[i] >= FLAT_CYCLES; break;
            case SensorId::TURBIDITY:  // lit ≈ dark = LED dead; dark climbing = light leak
                sig = f.ldr_lit - f.ldr_dark < LED_DEAD_DELTA ||
                      (dark.seeded && f.ldr_dark > dark.mean + LEAK_DELTA); break;
            default: break;
        }
        fail_count[i] = sig ? fail_count[i] + 1 : 0;
        s.failed = fail_count[i] >= FAIL_CONFIRM;

        if (!f.valid[i]) continue;  // no reading, no health update

        // Drift: short EMA vs long reference, counted only while the other chemistry sensors hold.
        // A sensor moving alone is drifting; all moving together is water.
        float drift = 0;
        if (i < N_CHEM) {
            short_ema[i] = short_seeded[i] ? short_ema[i] + SHORT_ALPHA * (x - short_ema[i]) : x;
            short_seeded[i] = true;
            bool others_hold = true;
            for (int j = 0; j < N_CHEM; j++)
                if (j != i && !(w.skipped >> j & 1) && std::fabs(w.z[j]) > Z_DEV) others_hold = false;
            if (others_hold && chem[i].seeded)
                drift = clamp01(std::fabs(short_ema[i] - chem[i].mean) / DRIFT_FULL[i]);
        }

        float usual = usual_noise[i].seeded ? std::fmax(usual_noise[i].mean, NOISE_FLOOR[i]) : NOISE_FLOOR[i];
        float noise = clamp01((f.noise[i] / usual - 1) / (NOISE_FULL_RATIO - 1));

        inst[i] = clamp01(1 - (W_DRIFT * drift + W_NOISE * noise));
        s.health_score += (inst[i] - s.health_score) / HEALTH_WINDOW;
        s.drift_score = drift;
        s.noise_score = noise;
        s.confidence = clamp01((float)cycles / HEALTH_WINDOW);
    }

    // Transducer + LM358: ~0 µs ring-down while the ping claims to be valid.
    acoustic_fail_count = (f.acoustic_valid && f.ringdown_us < RINGDOWN_ZERO_US) ? acoustic_fail_count + 1 : 0;
    acoustic_failed = acoustic_fail_count >= FAIL_CONFIRM;
    prev_wet = f.chamber_wet;
}

void SensorIntegrityEngine::learn(const FeatureSet& f, bool frozen) {
    if (fail_count[(int)SensorId::TURBIDITY] == 0) dark.update(f.ldr_dark, BASE_ALPHA);
    if (frozen) return;
    for (int i = 0; i < N_SENSORS; i++)
        if (f.valid[i] && !h[i].failed) usual_noise[i].update(f.noise[i], BASE_ALPHA);
}
