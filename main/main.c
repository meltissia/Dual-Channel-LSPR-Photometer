#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ads1220.h"
#include "photometer.h"
#include "photometer_output.h"

void app_main(void)
{
    esp_err_t result;

    result = ads1220_init();
    if (result != ESP_OK) {
        printf("ADS1220 init failed: %s\n", esp_err_to_name(result));
        return;
    }

    const uint8_t register_values[4] = {
        (uint8_t)(((uint8_t)ADS1220_INPUT_AIN0_AIN1 << 4) | ADS1220_REG0_GAIN_1 | ADS1220_REG0_PGA_ENABLED),
        (uint8_t)(ADS1220_REG1_DATA_RATE_20_SPS | ADS1220_REG1_NORMAL_MODE | ADS1220_REG1_SINGLE_SHOT | ADS1220_REG1_TEMPERATURE_SENSOR_DISABLED | ADS1220_REG1_BURNOUT_SOURCES_DISABLED),
        (uint8_t)(ADS1220_REG2_INTERNAL_REFERENCE | ADS1220_REG2_FILTER_50_60_HZ | ADS1220_REG2_LOW_SIDE_SWITCH_OPEN | ADS1220_REG2_IDAC_DISABLED),
        (uint8_t)(ADS1220_REG3_IDAC_ROUTING_DISABLED)
    };

    result = ads1220_configure(register_values);
    if (result != ESP_OK) {
        printf("ADS1220 configure failed: %s\n", esp_err_to_name(result));
        return;
    }

    result = photometer_light_init();
    if (result != ESP_OK) {
        printf("Light init failed: %s\n", esp_err_to_name(result));
        return;
    }

    const uint32_t sample_count = 4;
    const uint32_t timeout_ms = 200;
    uint32_t measurement_number = 0;

    photometer_output_print_csv_header();

    while (1) {
        int32_t channel_1_dark_offset_raw = 0;
        int32_t channel_2_dark_offset_raw = 0;

        result = photometer_light_off();
        if (result != ESP_OK) {
            printf("Light off failed: %s\n", esp_err_to_name(result));
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(250));

        result = photometer_calibrate_dark(sample_count, timeout_ms, &channel_1_dark_offset_raw, &channel_2_dark_offset_raw);
        if (result != ESP_OK) {
            printf("Dark calibration failed: %s\n", esp_err_to_name(result));
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        result = photometer_light_on();
        if (result != ESP_OK) {
            printf("Light on failed: %s\n", esp_err_to_name(result));
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(250));

        photometer_measurement_t measurement = {0};
        result = photometer_measure(sample_count, timeout_ms, &measurement);

        esp_err_t light_off_result = photometer_light_off();
        if (light_off_result != ESP_OK) {
            printf("Light off failed: %s\n", esp_err_to_name(light_off_result));
            return;
        }

        if (result != ESP_OK) {
            printf("Measurement failed: %s\n", esp_err_to_name(result));
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        measurement_number++;
        const int64_t timestamp_us = esp_timer_get_time();

        photometer_output_print_csv_measurement(
            timestamp_us,
            measurement_number,
            channel_1_dark_offset_raw,
            channel_2_dark_offset_raw,
            &measurement
        );

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}