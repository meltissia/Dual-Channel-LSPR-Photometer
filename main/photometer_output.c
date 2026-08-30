#include <stdio.h>
#include "photometer_output.h"

void photometer_output_print_csv_header(void)
{
    printf(
        "timestamp_us,measurement_number,"
        "ch1_raw,ch1_dark,ch1_corrected,ch1_uv,"
        "ch1_filtered,ch1_filtered_uv,"
        "ch2_raw,ch2_dark,ch2_corrected,ch2_uv,"
        "ch2_filtered,ch2_filtered_uv,"
        "ratio,filtered_ratio,"
        "ratio_valid,filtered_ratio_valid,"
        "ch1_saturated,ch2_saturated\n"
    );
}

void photometer_output_print_csv_measurement(
    int64_t timestamp_us,
    uint32_t measurement_number,
    int32_t channel_1_dark_offset_raw,
    int32_t channel_2_dark_offset_raw,
    const photometer_measurement_t *measurement
)
{
    printf(
        "DATA,%lld,%lu,"
        "%ld,%ld,%ld,%.3f,%ld,%.3f,"
        "%ld,%ld,%ld,%.3f,%ld,%.3f,"
        "%.6f,%.6f,"
        "%d,%d,%d,%d\n",
        (long long)timestamp_us,
        (unsigned long)measurement_number,
        (long)measurement->channel_1_raw,
        (long)channel_1_dark_offset_raw,
        (long)measurement->channel_1_corrected_raw,
        measurement->channel_1_microvolts,
        (long)measurement->channel_1_filtered_raw,
        measurement->channel_1_filtered_microvolts,
        (long)measurement->channel_2_raw,
        (long)channel_2_dark_offset_raw,
        (long)measurement->channel_2_corrected_raw,
        measurement->channel_2_microvolts,
        (long)measurement->channel_2_filtered_raw,
        measurement->channel_2_filtered_microvolts,
        measurement->channel_ratio,
        measurement->filtered_ratio,
        measurement->ratio_valid,
        measurement->filtered_ratio_valid,
        measurement->channel_1_saturated,
        measurement->channel_2_saturated
    );
}