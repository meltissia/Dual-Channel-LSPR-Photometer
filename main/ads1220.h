#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    ADS1220_INPUT_AIN0_AIN1 = 0x00,
    ADS1220_INPUT_AIN2_AIN3 = 0x05
} ads1220_input_t;

#define ADS1220_REG0_GAIN_1                      0x00U
#define ADS1220_REG0_PGA_ENABLED                 0x00U

#define ADS1220_REG1_DATA_RATE_20_SPS            0x00U
#define ADS1220_REG1_NORMAL_MODE                 0x00U
#define ADS1220_REG1_SINGLE_SHOT                 0x00U
#define ADS1220_REG1_TEMPERATURE_SENSOR_DISABLED 0x00U
#define ADS1220_REG1_BURNOUT_SOURCES_DISABLED    0x00U

#define ADS1220_REG2_INTERNAL_REFERENCE          0x00U
#define ADS1220_REG2_FILTER_50_60_HZ             0x10U
#define ADS1220_REG2_LOW_SIDE_SWITCH_OPEN        0x00U
#define ADS1220_REG2_IDAC_DISABLED               0x00U

#define ADS1220_REG3_IDAC_ROUTING_DISABLED       0x00U
#define ADS1220_REG3_DOUT_DRDY_ENABLED           0x02U

esp_err_t ads1220_init(void);
esp_err_t ads1220_reset(void);
esp_err_t ads1220_power_down(void);
esp_err_t ads1220_configure(const uint8_t register_values[4]);
esp_err_t ads1220_recover(const uint8_t register_values[4]);

esp_err_t ads1220_select_input(ads1220_input_t input);
esp_err_t ads1220_start_conversion(void);
esp_err_t ads1220_wait_ready(uint32_t timeout_ms);

esp_err_t ads1220_read_register(uint8_t register_address, uint8_t *register_value);
esp_err_t ads1220_write_register(uint8_t register_address, uint8_t register_value);

esp_err_t ads1220_read_data(int32_t *conversion_value);
esp_err_t ads1220_read_single(int32_t *conversion_value, uint32_t timeout_ms);
esp_err_t ads1220_read_average(ads1220_input_t input, int32_t *average_value, uint32_t sample_count, uint32_t timeout_ms, bool *saturated);

double ads1220_raw_to_microvolts(int32_t raw_value);