
#include <Arduino.h>
#include "types.h"

enum class SamplingState {
    IDLE,
    PREPARE,
    MEASUREMENT,
    ANALYSIS,
    ACTION,
    REVALIDATION,
    COMPLETE,
    ERROR
};

SamplingState currentState = SamplingState::IDLE;

FeatureSet features{};
Result result{};

// Temporary mock data until Person 4 supplies real readings.
FeatureSet readFeatures()
{
    FeatureSet f{};

    f.timestamp_ms = millis();

    f.ph = 7.0f;
    f.tds_ppm = 300.0f;
    f.turbidity = 0.10f;
    f.temp_c = 22.0f;

    f.noise[0] = 0.02f;
    f.noise[1] = 2.0f;
    f.noise[2] = 0.003f;
    f.noise[3] = 0.05f;

    for (int i = 0; i < 4; i++)
        f.valid[i] = true;

    // Mock water-presence signal.
    // Set false to simulate an empty chamber.
    f.chamber_wet = true;
    f.wet_duration_ms = 10000.0f;

    // No acoustic hardware in this mock.
    f.acoustic_valid = false;
    f.ringdown_us = 0.0f;

    f.ldr_lit = 2000.0f;
    f.ldr_dark = 100.0f;

    return f;
}

bool waterPresent()
{
    // Temporary implementation using mock data.
    // Replace with Person 4's water-presence interface.
    return readFeatures().chamber_wet;
}

bool allSensorsInvalid(const FeatureSet& f)
{
    for (int i = 0; i < 4; i++) {
        if (f.valid[i])
            return false;
    }

    return true;
}

void printDecision(Decision d)
{
    switch (d) {
        case Decision::SLEEP:
            Serial.println("Decision: SLEEP");
            break;

        case Decision::CLEAN:
            Serial.println("Decision: CLEAN");
            break;

        case Decision::RESAMPLE:
            Serial.println("Decision: RESAMPLE");
            break;

        case Decision::RECALIBRATE:
            Serial.println("Decision: RECALIBRATE");
            break;

        case Decision::PRESERVE_SAMPLE_AND_ALERT:
            Serial.println("Decision: PRESERVE SAMPLE AND ALERT");
            break;

        case Decision::MAINTENANCE_ALERT:
            Serial.println("Decision: MAINTENANCE ALERT");
            break;
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println("WaterTwin controller started");
}

void loop()
{
    switch (currentState) {

        case SamplingState::IDLE:
            Serial.println("\nIDLE");
            currentState = SamplingState::PREPARE;
            break;

        case SamplingState::PREPARE:
            Serial.println("PREPARE: checking water presence");

            if (waterPresent()) {
                Serial.println("Water detected");
                currentState = SamplingState::MEASUREMENT;
            }
            else {
                Serial.println("No water. Waiting...");
                delay(1000);

                // Remain in PREPARE. No inlet is opened.
            }
            break;

        case SamplingState::MEASUREMENT:
            Serial.println("MEASUREMENT");

            features = readFeatures();

            if (!features.chamber_wet) {
                Serial.println("Water disappeared during measurement");
                currentState = SamplingState::PREPARE;
                break;
            }

            if (allSensorsInvalid(features)) {
                Serial.println("ERROR: all sensor readings invalid");
                currentState = SamplingState::ERROR;
                break;
            }

            currentState = SamplingState::ANALYSIS;
            break;

        case SamplingState::ANALYSIS:
            Serial.println("ANALYSIS");

            // Call your teammate's intelligence engine.
            result = process(features);

            Serial.print("Water anomaly score: ");
            Serial.println(result.water.anomaly_score);

            Serial.print("Diagnosis confidence: ");
            Serial.println(result.diagnosis.confidence);

            printDecision(result.decision);

            currentState = SamplingState::ACTION;
            break;

        case SamplingState::ACTION:
            Serial.println("ACTION");

            switch (result.decision) {

                case Decision::CLEAN:
                    Serial.println("Mock cleaning action executed");

                    // Replace with Person 4's actuator interface.
                    // The intelligence engine handles clean
                    // revalidation across subsequent process() calls.
                    currentState = SamplingState::REVALIDATION;
                    break;

                case Decision::RESAMPLE:
                    Serial.println("Requesting a fresh measurement");
                    currentState = SamplingState::MEASUREMENT;
                    break;

                case Decision::PRESERVE_SAMPLE_AND_ALERT:
                    Serial.println("Preserve sample and send alert");
                    // Alert/sample handling will use the relevant
                    // module when its interface is available.
                    currentState = SamplingState::COMPLETE;
                    break;

                case Decision::RECALIBRATE:
                    Serial.println("Recalibration required");
                    currentState = SamplingState::COMPLETE;
                    break;

                case Decision::MAINTENANCE_ALERT:
                    Serial.println("Maintenance required");
                    currentState = SamplingState::COMPLETE;
                    break;

                case Decision::SLEEP:
                    Serial.println("No action required");
                    currentState = SamplingState::COMPLETE;
                    break;
            }
            break;

        case SamplingState::REVALIDATION:
            Serial.println("REVALIDATION: taking a fresh reading");

            delay(1000);

            // A new reading is acquired on the next pass.
            // process() uses its internal state to assess recovery.
            currentState = SamplingState::MEASUREMENT;
            break;

        case SamplingState::COMPLETE:
            Serial.println("Sampling cycle complete");
            currentState = SamplingState::IDLE;

            // Temporary pause between mock cycles.
            delay(2000);
            break;

        case SamplingState::ERROR:
            Serial.println("ERROR: recovering before retry");

            delay(2000);
            currentState = SamplingState::PREPARE;
            break;
    }
}
