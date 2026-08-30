#include <stddef.h>
#include <stdint.h>

#include "photometer.h"

#include "ads1220.h"
#include "driver/gpio.h"

#define PHOTOMETER_LIGHT_GPIO 27

static int32_t channel_1_dark_offset_raw = 0;
static int32_t channel_2_dark_offset_raw = 0;

void photometer_set_dark_offsets(
    int32_t channel_1_offset_raw,
    int32_t channel_2_offset_raw
)
{
    channel_1_dark_offset_raw = channel_1_offset_raw;
    channel_2_dark_offset_raw = channel_2_offset_raw;
}

esp_err_t photometer_calibrate_dark(
    uint32_t sample_count,
    uint32_t timeout_ms,
    int32_t *channel_1_offset_raw,
    int32_t *channel_2_offset_raw
)
{
    if (
        sample_count == 0 ||
        timeout_ms == 0 ||
        channel_1_offset_raw == NULL ||
        channel_2_offset_raw == NULL
    ) {
        return ESP_ERR_INVALID_ARG;
    }

    int32_t new_channel_1_offset_raw = 0;
    int32_t new_channel_2_offset_raw = 0;

    bool channel_1_saturated = false;
    bool channel_2_saturated = false;

    esp_err_t result = ads1220_read_average(
        ADS1220_INPUT_AIN0_AIN1,
        &new_channel_1_offset_raw,
        sample_count,
        timeout_ms,
        &channel_1_saturated
    );

    if (result != ESP_OK) {
        return result;
    }

    result = ads1220_read_average(
        ADS1220_INPUT_AIN2_AIN3,
        &new_channel_2_offset_raw,
        sample_count,
        timeout_ms,
        &channel_2_saturated
    );

    if (result != ESP_OK) {
        return result;
    }

    if (channel_1_saturated || channel_2_saturated) {
        return ESP_ERR_INVALID_STATE;
    }

    photometer_set_dark_offsets(
        new_channel_1_offset_raw,
        new_channel_2_offset_raw
    );

    *channel_1_offset_raw = new_channel_1_offset_raw;
    *channel_2_offset_raw = new_channel_2_offset_raw;

    return ESP_OK;
}

esp_err_t photometer_get_dark_offsets(
    int32_t *channel_1_offset_raw,
    int32_t *channel_2_offset_raw
)
{
    if (
        channel_1_offset_raw == NULL ||
        channel_2_offset_raw == NULL
    ) {
        return ESP_ERR_INVALID_ARG;
    }

    *channel_1_offset_raw = channel_1_dark_offset_raw;
    *channel_2_offset_raw = channel_2_dark_offset_raw;

    return ESP_OK;
}

esp_err_t photometer_measure(
    uint32_t sample_count,
    uint32_t timeout_ms,
    photometer_measurement_t *measurement
)
{
    if (measurement == NULL || sample_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    photometer_measurement_t new_measurement = {0};

    esp_err_t result = ads1220_read_average(
        ADS1220_INPUT_AIN0_AIN1,
        &new_measurement.channel_1_raw,
        sample_count,
        timeout_ms,
        &new_measurement.channel_1_saturated
    );

    if (result != ESP_OK) {
        return result;
    }

    result = ads1220_read_average(
        ADS1220_INPUT_AIN2_AIN3,
        &new_measurement.channel_2_raw,
        sample_count,
        timeout_ms,
        &new_measurement.channel_2_saturated
    );

    if (result != ESP_OK) {
        return result;
    }

    new_measurement.channel_1_corrected_raw =
        new_measurement.channel_1_raw -
        channel_1_dark_offset_raw;

    new_measurement.channel_2_corrected_raw =
        new_measurement.channel_2_raw -
        channel_2_dark_offset_raw;

    new_measurement.channel_1_microvolts =
        ads1220_raw_to_microvolts(
            new_measurement.channel_1_corrected_raw
        );

    new_measurement.channel_2_microvolts =
        ads1220_raw_to_microvolts(
            new_measurement.channel_2_corrected_raw
        );

    new_measurement.ratio_valid =
        !new_measurement.channel_1_saturated &&
        !new_measurement.channel_2_saturated &&
        new_measurement.channel_2_corrected_raw != 0;

    if (new_measurement.ratio_valid) {
        new_measurement.channel_ratio =
            new_measurement.channel_1_microvolts /
            new_measurement.channel_2_microvolts;
    }

    *measurement = new_measurement;

    return ESP_OK;
}

esp_err_t photometer_light_on(void)
{
    esp_err_t result = gpio_set_level(
        PHOTOMETER_LIGHT_GPIO,
        1
    );

    if (result != ESP_OK) {
        return result;
    }

    return ESP_OK;
}

esp_err_t photometer_light_off(void)
{
    esp_err_t result = gpio_set_level(
        PHOTOMETER_LIGHT_GPIO,
        0
    );

    if (result != ESP_OK) {
        return result;
    }

    return ESP_OK;
}

esp_err_t photometer_light_init(void)
{
    esp_err_t result = gpio_set_direction(
        PHOTOMETER_LIGHT_GPIO,
        GPIO_MODE_OUTPUT
    );

    if (result != ESP_OK) {
        return result;
    }

    result = gpio_set_level(
        PHOTOMETER_LIGHT_GPIO,
        0
    );

    if (result != ESP_OK) {
        return result;
    }

    return ESP_OK;
}