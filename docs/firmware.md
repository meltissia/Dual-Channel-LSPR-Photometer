# Firmware Implementation & Bring-up Notes

Built with **ESP-IDF v6.0.2** for target `esp32`.

## Module Breakdown
- `ads1220.c / .h`: Low-level driver, register config via `WREG`/`RREG` with read-back verification, `DRDY` timeout handling, conversion triggering, raw code to µV scaling.
- `photometer.c / .h`: Dual-channel multiplexing, dark offset calibration & correction, boxcar moving average (`N = 8`), saturation checks, ratiometric calculation.
- `photometer_output.c / .h`: Structured CSV UART stream generation with timestamping and status flags.
- `main.c`: Hardware bring-up orchestration and continuous acquisition loop.

## Bring-up Notes & Workarounds
- Hardware SPI communication with ADS1220 was unstable during Rev.1.0 bring-up even at reduced clock rates. The driver was therefore switched to GPIO bit-bang SPI with explicitly controlled `CS`, `SCLK`, `DIN`, and `DOUT` timing.
- Conservative bit timing was required for reliable register access and 24-bit conversion-data reads.
- `DRDY` is polled with a timeout before conversion data are read, preventing indefinite blocking and reads before conversion completion.
- On Rev.1.0, additional repeated `RDATA` validation was required because individual 24-bit reads could still become corrupted despite valid `DRDY`.
