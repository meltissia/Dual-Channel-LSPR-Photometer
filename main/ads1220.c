#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "ads1220.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ADS1220_CMD_RDATA               0x10
#define ADS1220_CMD_POWERDOWN           0x02
#define ADS1220_CMD_RESET               0x06
#define ADS1220_CMD_RREG                0x20
#define ADS1220_CMD_START_SYNC          0x08
#define ADS1220_CMD_WREG                0x40

#define ADS1220_REGISTER_COUNT          4
#define ADS1220_REGISTER_0              0
#define ADS1220_REG0_NON_MUX_MASK       0x0F
#define ADS1220_REG0_MUX_SHIFT          4

#define ADS1220_CS_GPIO                 5
#define ADS1220_DRDY_GPIO               16
#define ADS1220_SCLK_GPIO               18
#define ADS1220_MISO_GPIO               19
#define ADS1220_MOSI_GPIO               23

#define ADS1220_RESET_DELAY_US          200
#define ADS1220_READY_POLL_DELAY_US     100

#define ADS1220_INTERNAL_REFERENCE_UV   2048000.0
#define ADS1220_FULL_SCALE_COUNTS       8388608.0
#define ADS1220_CONFIGURED_GAIN         1.0

#define ADS1220_RAW_POSITIVE_FULL_SCALE 8388607
#define ADS1220_RAW_NEGATIVE_FULL_SCALE (-8388608)

static uint8_t ads1220_bitbang_transfer_byte(uint8_t tx_byte)
{
    uint8_t rx_byte = 0;
    for (int bit = 7; bit >= 0; bit--) {
        gpio_set_level(ADS1220_MOSI_GPIO, (tx_byte >> bit) & 0x01);
        esp_rom_delay_us(30);
        gpio_set_level(ADS1220_SCLK_GPIO, 1);
        esp_rom_delay_us(30);
        gpio_set_level(ADS1220_SCLK_GPIO, 0);

        if (gpio_get_level(ADS1220_MISO_GPIO)) {
            rx_byte |= (uint8_t)(1U << bit);
        }
        esp_rom_delay_us(30);
    }
    return rx_byte;
}

static esp_err_t ads1220_wait_drdy(uint32_t timeout_ms)
{
    const int64_t start_us = esp_timer_get_time();
    const int64_t timeout_us = (int64_t)timeout_ms * 1000;

    while (gpio_get_level(ADS1220_DRDY_GPIO) == 0) {
        if ((esp_timer_get_time() - start_us) > timeout_us) {
            return ESP_ERR_TIMEOUT;
        }
        esp_rom_delay_us(10);
    }

    while (gpio_get_level(ADS1220_DRDY_GPIO) != 0) {
        if ((esp_timer_get_time() - start_us) > timeout_us) {
            return ESP_ERR_TIMEOUT;
        }
        esp_rom_delay_us(50);
    }

    return ESP_OK;
}

static esp_err_t ads1220_send_command(uint8_t command)
{
    gpio_set_level(ADS1220_CS_GPIO, 0);
    esp_rom_delay_us(10);
    ads1220_bitbang_transfer_byte(command);
    esp_rom_delay_us(10);
    gpio_set_level(ADS1220_CS_GPIO, 1);
    return ESP_OK;
}

esp_err_t ads1220_start_conversion(void)
{
    return ads1220_send_command(ADS1220_CMD_START_SYNC);
}

esp_err_t ads1220_power_down(void)
{
    return ads1220_send_command(ADS1220_CMD_POWERDOWN);
}

esp_err_t ads1220_reset(void)
{
    esp_err_t result = ads1220_send_command(ADS1220_CMD_RESET);
    if (result != ESP_OK) {
        return result;
    }
    esp_rom_delay_us(ADS1220_RESET_DELAY_US);
    return ESP_OK;
}

esp_err_t ads1220_recover(const uint8_t register_values[4])
{
    if (register_values == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t result = ads1220_reset();
    if (result != ESP_OK) {
        return result;
    }
    return ads1220_configure(register_values);
}

esp_err_t ads1220_wait_ready(uint32_t timeout_ms)
{
    const int64_t start_time_us = esp_timer_get_time();
    const int64_t timeout_us = (int64_t)timeout_ms * 1000;

    while (1) {
        if (gpio_get_level(ADS1220_DRDY_GPIO) == 0) {
            return ESP_OK;
        }
        if ((esp_timer_get_time() - start_time_us) >= timeout_us) {
            return ESP_ERR_TIMEOUT;
        }
        esp_rom_delay_us(ADS1220_READY_POLL_DELAY_US);
    }
}

esp_err_t ads1220_read_register(uint8_t register_address, uint8_t *register_value)
{
    if (register_value == NULL || register_address >= ADS1220_REGISTER_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t command = (uint8_t)(ADS1220_CMD_RREG | (register_address << 2));

    gpio_set_level(ADS1220_CS_GPIO, 0);
    esp_rom_delay_us(10);
    ads1220_bitbang_transfer_byte(command);
    *register_value = ads1220_bitbang_transfer_byte(0x00);
    esp_rom_delay_us(10);
    gpio_set_level(ADS1220_CS_GPIO, 1);

    return ESP_OK;
}

esp_err_t ads1220_write_register(uint8_t register_address, uint8_t register_value)
{
    if (register_address >= ADS1220_REGISTER_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t command = (uint8_t)(ADS1220_CMD_WREG | (register_address << 2));

    gpio_set_level(ADS1220_CS_GPIO, 0);
    esp_rom_delay_us(10);
    ads1220_bitbang_transfer_byte(command);
    ads1220_bitbang_transfer_byte(register_value);
    esp_rom_delay_us(10);
    gpio_set_level(ADS1220_CS_GPIO, 1);

    return ESP_OK;
}

esp_err_t ads1220_select_input(ads1220_input_t input)
{
    if (input != ADS1220_INPUT_AIN0_AIN1 && input != ADS1220_INPUT_AIN2_AIN3) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t reg_val = 0;
    esp_err_t result = ads1220_read_register(ADS1220_REGISTER_0, &reg_val);
    if (result != ESP_OK) {
        return result;
    }

    reg_val = (uint8_t)((reg_val & ADS1220_REG0_NON_MUX_MASK) | ((uint8_t)input << ADS1220_REG0_MUX_SHIFT));
    result = ads1220_write_register(ADS1220_REGISTER_0, reg_val);
    if (result != ESP_OK) {
        return result;
    }

    uint8_t readback_val = 0;
    result = ads1220_read_register(ADS1220_REGISTER_0, &readback_val);
    if (result != ESP_OK) {
        return result;
    }
    if (readback_val != reg_val) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}

esp_err_t ads1220_read_data(int32_t *conversion_value)
{
    if (conversion_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const int max_reads = 6;
    int32_t previous_value = 0;
    bool have_previous = false;

    for (int read_index = 0; read_index < max_reads; read_index++) {
        gpio_set_level(ADS1220_CS_GPIO, 0);
        esp_rom_delay_us(10);

        ads1220_bitbang_transfer_byte(ADS1220_CMD_RDATA);
        esp_rom_delay_us(10);

        const uint8_t byte_2 = ads1220_bitbang_transfer_byte(0x00);
        const uint8_t byte_1 = ads1220_bitbang_transfer_byte(0x00);
        const uint8_t byte_0 = ads1220_bitbang_transfer_byte(0x00);

        esp_rom_delay_us(10);
        gpio_set_level(ADS1220_CS_GPIO, 1);

        uint32_t raw_value = ((uint32_t)byte_2 << 16) |
                             ((uint32_t)byte_1 << 8)  |
                             (uint32_t)byte_0;

        if ((raw_value & 0x00800000U) != 0U) {
            raw_value |= 0xFF000000U;
        }

        const int32_t current_value = (int32_t)raw_value;

        if (have_previous && current_value == previous_value) {
            *conversion_value = current_value;
            return ESP_OK;
        }

        previous_value = current_value;
        have_previous = true;

        esp_rom_delay_us(100);
    }

    return ESP_ERR_INVALID_RESPONSE;
}

esp_err_t ads1220_read_single(int32_t *conversion_value, uint32_t timeout_ms)
{
    if (conversion_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = ads1220_start_conversion();
    if (result != ESP_OK) {
        return result;
    }

    result = ads1220_wait_drdy(timeout_ms);
    if (result != ESP_OK) {
        return result;
    }

    esp_rom_delay_us(1000);
    return ads1220_read_data(conversion_value);
}

esp_err_t ads1220_read_average(ads1220_input_t input, int32_t *average_value, uint32_t sample_count, uint32_t timeout_ms, bool *saturated)
{
    if (average_value == NULL || saturated == NULL || sample_count == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    *saturated = false;
    esp_err_t result = ads1220_select_input(input);
    if (result != ESP_OK) {
        return result;
    }

    int64_t sample_sum = 0;
    for (uint32_t i = 0; i < sample_count; i++) {
        int32_t sample_val = 0;
        result = ads1220_read_single(&sample_val, timeout_ms);
        if (result != ESP_OK) {
            return result;
        }

        if (sample_val == ADS1220_RAW_POSITIVE_FULL_SCALE || sample_val == ADS1220_RAW_NEGATIVE_FULL_SCALE) {
            *saturated = true;
        }
        sample_sum += sample_val;
    }

    *average_value = (int32_t)(sample_sum / (int64_t)sample_count);
    return ESP_OK;
}

double ads1220_raw_to_microvolts(int32_t raw_value)
{
    return ((double)raw_value * ADS1220_INTERNAL_REFERENCE_UV) / 
           (ADS1220_CONFIGURED_GAIN * ADS1220_FULL_SCALE_COUNTS);
}

esp_err_t ads1220_configure(const uint8_t register_values[4])
{
    if (register_values == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint8_t addr = 0; addr < ADS1220_REGISTER_COUNT; addr++) {
        esp_err_t result = ads1220_write_register(addr, register_values[addr]);
        if (result != ESP_OK) {
            return result;
        }

        uint8_t readback = 0;
        result = ads1220_read_register(addr, &readback);
        if (result != ESP_OK) {
            return result;
        }

        if (readback != register_values[addr]) {
            printf("ADS1220 register %u mismatch: expected 0x%02X, read 0x%02X\n",
                   (unsigned int)addr, (unsigned int)register_values[addr], (unsigned int)readback);
            return ESP_ERR_INVALID_RESPONSE;
        }

        printf("ADS1220 register %u verified: 0x%02X\n", (unsigned int)addr, (unsigned int)readback);
    }
    return ESP_OK;
}

esp_err_t ads1220_init(void)
{
    esp_err_t res;

    const struct { gpio_num_t pin; gpio_mode_t mode; } pins[] = {
        { ADS1220_CS_GPIO,   GPIO_MODE_INPUT_OUTPUT },
        { ADS1220_DRDY_GPIO, GPIO_MODE_INPUT        },
        { ADS1220_SCLK_GPIO, GPIO_MODE_OUTPUT       },
        { ADS1220_MOSI_GPIO, GPIO_MODE_OUTPUT       },
        { ADS1220_MISO_GPIO, GPIO_MODE_INPUT        }
    };

    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
        res = gpio_reset_pin(pins[i].pin);
        if (res != ESP_OK) return res;
        res = gpio_set_direction(pins[i].pin, pins[i].mode);
        if (res != ESP_OK) return res;
    }

    res = gpio_set_level(ADS1220_CS_GPIO, 1);
    if (res != ESP_OK) return res;

    printf("INIT CS BEFORE RESET = %d\n", gpio_get_level(ADS1220_CS_GPIO));

    res = gpio_set_level(ADS1220_SCLK_GPIO, 0);
    if (res != ESP_OK) return res;

    res = gpio_set_level(ADS1220_MOSI_GPIO, 0);
    if (res != ESP_OK) return res;

    esp_rom_delay_us(1000);

    res = ads1220_reset();

    printf("INIT CS AFTER RESET = %d\n", gpio_get_level(ADS1220_CS_GPIO));

    return res;
}