#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    int32_t channel_1_raw;
    int32_t channel_2_raw;

    int32_t channel_1_corrected_raw;
    int32_t channel_2_corrected_raw;

    int32_t channel_1_filtered_raw;
    int32_t channel_2_filtered_raw;

    double channel_1_microvolts;
    double channel_2_microvolts;

    double channel_1_filtered_microvolts;
    double channel_2_filtered_microvolts;

    bool channel_1_saturated;
    bool channel_2_saturated;

    bool ratio_valid;
    double channel_ratio;

    bool filtered_ratio_valid;
    double filtered_ratio;
} photometer_measurement_t;

esp_err_t photometer_light_init(void);
esp_err_t photometer_light_on(void);
esp_err_t photometer_light_off(void);

void photometer_set_dark_offsets(int32_t channel_1_offset_raw, int32_t channel_2_offset_raw);
esp_err_t photometer_get_dark_offsets(int32_t *channel_1_offset_raw, int32_t *channel_2_offset_raw);
void photometer_clear_dark_offsets(void);

esp_err_t photometer_calibrate_dark(
    uint32_t sample_count,
    uint32_t timeout_ms,
    int32_t *channel_1_offset_raw,
    int32_t *channel_2_offset_raw
);

esp_err_t photometer_measure(
    uint32_t sample_count,
    uint32_t timeout_ms,
    photometer_measurement_t *measurement
);