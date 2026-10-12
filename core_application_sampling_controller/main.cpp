```cpp
#include <cstdio>
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "types.h"
#include "intelligence.h"

static const char* TAG = "WaterTwin";

// Sampling state machine
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

static SamplingState currentState = SamplingState::IDLE;

static FeatureSet features{};
static Result result{};

// Mock implementation until Person 4 provides hardware interfaces.
static bool waterPresent()
{
    return true;  // Simulate water being present
}

// Mock feature data until the real sensor interface is ready.
static FeatureSet readFeatures()
{
    FeatureSet f{};

    f.timestamp_ms =
        static_cast<uint32_t>(esp_timer_get_time() / 1000);

    f.ph = 7.0f;
    f.tds_ppm = 300.0f;
    f.turbidity = 0.10f;
    f.temp_c = 22.0f;

    f.noise[0] = 0.02f;
    f.noise[1] = 2.0f;
    f.noise[2] = 0.003f;
    f.noise[3] = 0.05f;

    for (int i = 0; i < 4; i++) {
        f.valid[i] = true;
    }

    f.chamber_wet = waterPresent();
    f.wet_duration_ms = 10000.0f;

    f.acoustic_valid = false;
    f.ringdown_us = 0.0f;

    f.ldr_lit = 2000.0f;
    f.ldr_dark = 100.0f;

    return f;
}

static bool allSensorsInvalid(const FeatureSet& f)
{
    for (int i = 0; i < 4; i++) {
        if (f.valid[i]) {
            return false;
        }
    }

    return true;
}

static void printDecision(Decision decision)
{
    switch (decision) {
        case Decision::SLEEP:
            ESP_LOGI(TAG, "Decision: SLEEP");
            break;

        case Decision::CLEAN:
            ESP_LOGI(TAG, "Decision: CLEAN");
            break;

        case Decision::RESAMPLE:
            ESP_LOGI(TAG, "Decision: RESAMPLE");
            break;

        case Decision::RECALIBRATE:
            ESP_LOGI(TAG, "Decision: RECALIBRATE");
            break;

        case Decision::PRESERVE_SAMPLE_AND_ALERT:
            ESP_LOGI(TAG, "Decision: PRESERVE SAMPLE AND ALERT");
            break;

        case Decision::MAINTENANCE_ALERT:
            ESP_LOGI(TAG, "Decision: MAINTENANCE ALERT");
            break;
    }
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "WaterTwin controller started");

    while (true) {
        switch (currentState) {

            case SamplingState::IDLE:
                ESP_LOGI(TAG, "IDLE");
                currentState = SamplingState::PREPARE;
                break;

            case SamplingState::PREPARE:
                ESP_LOGI(TAG, "PREPARE: checking water presence");

                if (waterPresent()) {
                    ESP_LOGI(TAG, "Water detected");
                    currentState = SamplingState::MEASUREMENT;
                } else {
                    ESP_LOGW(TAG, "No water detected; waiting");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                break;

            case SamplingState::MEASUREMENT:
                ESP_LOGI(TAG, "MEASUREMENT");

                features = readFeatures();

                if (!features.chamber_wet) {
                    ESP_LOGW(TAG, "Water disappeared");
                    currentState = SamplingState::PREPARE;
                    break;
                }

                if (allSensorsInvalid(features)) {
                    ESP_LOGE(TAG, "All sensor readings are invalid");
                    currentState = SamplingState::ERROR;
                    break;
                }

                currentState = SamplingState::ANALYSIS;
                break;

            case SamplingState::ANALYSIS:
                ESP_LOGI(TAG, "ANALYSIS");

                // Call Person 3's intelligence engine.
                result = process(features);

                ESP_LOGI(
                    TAG,
                    "Water anomaly score: %.3f",
                    result.water.anomaly_score
                );

                ESP_LOGI(
                    TAG,
                    "Diagnosis confidence: %.3f",
                    result.diagnosis.confidence
                );

                printDecision(result.decision);

                currentState = SamplingState::ACTION;
                break;

            case SamplingState::ACTION:
                ESP_LOGI(TAG, "ACTION");

                switch (result.decision) {
                    case Decision::CLEAN:
                        // Mock only: no physical cleaning happens here.
                        ESP_LOGI(TAG, "Mock cleaning action requested");
                        currentState = SamplingState::REVALIDATION;
                        break;

                    case Decision::RESAMPLE:
                        ESP_LOGI(TAG, "Requesting a fresh measurement");
                        currentState = SamplingState::MEASUREMENT;
                        break;

                    case Decision::PRESERVE_SAMPLE_AND_ALERT:
                        ESP_LOGW(TAG, "Preserve sample and send alert");
                        currentState = SamplingState::COMPLETE;
                        break;

                    case Decision::RECALIBRATE:
                        ESP_LOGW(TAG, "Recalibration required");
                        currentState = SamplingState::COMPLETE;
                        break;

                    case Decision::MAINTENANCE_ALERT:
                        ESP_LOGW(TAG, "Maintenance required");
                        currentState = SamplingState::COMPLETE;
                        break;

                    case Decision::SLEEP:
                        ESP_LOGI(TAG, "No action required");
                        currentState = SamplingState::COMPLETE;
                        break;
                }
                break;

            case SamplingState::REVALIDATION:
                ESP_LOGI(TAG, "REVALIDATION: taking a fresh reading");

                // Allow time before the next measurement.
                vTaskDelay(pdMS_TO_TICKS(1000));

                // Person 3's engine tracks post-clean revalidation
                // across subsequent calls to process().
                currentState = SamplingState::MEASUREMENT;
                break;

            case SamplingState::COMPLETE:
                ESP_LOGI(TAG, "Sampling cycle complete");

                vTaskDelay(pdMS_TO_TICKS(2000));
                currentState = SamplingState::IDLE;
                break;

            case SamplingState::ERROR:
                ESP_LOGE(TAG, "Error; retrying after delay");

                vTaskDelay(pdMS_TO_TICKS(2000));
                currentState = SamplingState::PREPARE;
                break;
        }

        // Prevent the controller loop from spinning continuously.
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```
