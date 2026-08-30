#include <stddef.h>
#include <stdint.h>

#include "photometer.h"
#include "ads1220.h"
#include "driver/gpio.h"

#define PHOTOMETER_LIGHT_GPIO 27
#define PHOTOMETER_MOVING_AVERAGE_SIZE 8

static int32_t ch1_buffer[PHOTOMETER_MOVING_AVERAGE_SIZE] = {0};
static int32_t ch2_buffer[PHOTOMETER_MOVING_AVERAGE_SIZE] = {0};

static uint32_t moving_average_index = 0;
static uint32_t moving_average_count = 0;

static int64_t ch1_sum = 0;
static int64_t ch2_sum = 0;

static int32_t channel_1_dark_offset_raw = 0;
static int32_t channel_2_dark_offset_raw = 0;

static void photometer_update_moving_average(
    int32_t ch1_value,
    int32_t ch2_value,
    int32_t *ch1_average,
    int32_t *ch2_average
)
{
    if (moving_average_count < PHOTOMETER_MOVING_AVERAGE_SIZE) {
        ch1_buffer[moving_average_index] = ch1_value;
        ch2_buffer[moving_average_index] = ch2_value;

        ch1_sum += ch1_value;
        ch2_sum += ch2_value;

        moving_average_count++;
    } else {
        ch1_sum -= ch1_buffer[moving_average_index];
        ch2_sum -= ch2_buffer[moving_average_index];

        ch1_buffer[moving_average_index] = ch1_value;
        ch2_buffer[moving_average_index] = ch2_value;

        ch1_sum += ch1_value;
        ch2_sum += ch2_value;
    }

    moving_average_index++;
    if (moving_average_index >= PHOTOMETER_MOVING_AVERAGE_SIZE) {
        moving_average_index = 0;
    }

    *ch1_average = (int32_t)(ch1_sum / moving_average_count);
    *ch2_average = (int32_t)(ch2_sum / moving_average_count);
}

void photometer_set_dark_offsets(int32_t channel_1_offset_raw, int32_t channel_2_offset_raw)
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
    if (sample_count == 0 || timeout_ms == 0 || channel_1_offset_raw == NULL || channel_2_offset_raw == NULL) {
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

    photometer_set_dark_offsets(new_channel_1_offset_raw, new_channel_2_offset_raw);

    *channel_1_offset_raw = new_channel_1_offset_raw;
    *channel_2_offset_raw = new_channel_2_offset_raw;

    return ESP_OK;
}

esp_err_t photometer_get_dark_offsets(int32_t *channel_1_offset_raw, int32_t *channel_2_offset_raw)
{
    if (channel_1_offset_raw == NULL || channel_2_offset_raw == NULL) {
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

    new_measurement.channel_1_corrected_raw = new_measurement.channel_1_raw - channel_1_dark_offset_raw;
    new_measurement.channel_2_corrected_raw = new_measurement.channel_2_raw - channel_2_dark_offset_raw;

    photometer_update_moving_average(
        new_measurement.channel_1_corrected_raw,
        new_measurement.channel_2_corrected_raw,
        &new_measurement.channel_1_filtered_raw,
        &new_measurement.channel_2_filtered_raw
    );

    new_measurement.channel_1_microvolts = ads1220_raw_to_microvolts(new_measurement.channel_1_corrected_raw);
    new_measurement.channel_2_microvolts = ads1220_raw_to_microvolts(new_measurement.channel_2_corrected_raw);

    new_measurement.channel_1_filtered_microvolts = ads1220_raw_to_microvolts(new_measurement.channel_1_filtered_raw);
    new_measurement.channel_2_filtered_microvolts = ads1220_raw_to_microvolts(new_measurement.channel_2_filtered_raw);

    new_measurement.ratio_valid = !new_measurement.channel_1_saturated &&
                                  !new_measurement.channel_2_saturated &&
                                  new_measurement.channel_2_corrected_raw != 0;

    if (new_measurement.ratio_valid) {
        new_measurement.channel_ratio = new_measurement.channel_1_microvolts / new_measurement.channel_2_microvolts;
    }

    new_measurement.filtered_ratio_valid = !new_measurement.channel_1_saturated &&
                                           !new_measurement.channel_2_saturated &&
                                           new_measurement.channel_2_filtered_raw != 0;

    if (new_measurement.filtered_ratio_valid) {
        new_measurement.filtered_ratio = new_measurement.channel_1_filtered_microvolts / new_measurement.channel_2_filtered_microvolts;
    }

    *measurement = new_measurement;

    return ESP_OK;
}

esp_err_t photometer_light_on(void)
{
    return gpio_set_level(PHOTOMETER_LIGHT_GPIO, 1);
}

esp_err_t photometer_light_off(void)
{
    return gpio_set_level(PHOTOMETER_LIGHT_GPIO, 0);
}

esp_err_t photometer_light_init(void)
{
    esp_err_t result = gpio_set_direction(PHOTOMETER_LIGHT_GPIO, GPIO_MODE_OUTPUT);
    if (result != ESP_OK) {
        return result;
    }

    return gpio_set_level(PHOTOMETER_LIGHT_GPIO, 0);
}