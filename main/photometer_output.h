#ifndef PHOTOMETER_OUTPUT_H
#define PHOTOMETER_OUTPUT_H

#include <stdint.h>

#include "photometer.h"

void photometer_output_print_csv_header(void);

void photometer_output_print_csv_measurement(
    int64_t timestamp_us,
    uint32_t measurement_number,
    int32_t channel_1_dark_offset_raw,
    int32_t channel_2_dark_offset_raw,
    const photometer_measurement_t *measurement
    
);

#endif