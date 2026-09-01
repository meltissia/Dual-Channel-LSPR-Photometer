# UART Bring-up Logs

Референсний набір даних: [`rev1_ads1220_.bringup_log.csv`](rev1_ads1220_.bringup_log.csv).

Цей набір даних збережено як підтвердження роботи цифрового acquisition pipeline Rev.1.0 після стабілізації SPI-інтерфейсу. Він містить двоканальне зчитування ADC, dark correction, перерахунок ADC-коду, moving-average processing, розрахунок співвідношення каналів та структурований UART output.

На стабільних ділянках логу відсутні раніше зафіксовані структуровані пошкодження SPI-даних, зсуви байтів та великі стрибки до некоректних ADC-кодів.

Зафіксовані фізичні значення сигналів не є метрологічно валідованими та не повинні інтерпретуватися як характеристика точності фотометра, noise floor, чутливості або довготривалої стабільності.

---

## Схема CSV Output

```text
record_type, timestamp_us, measurement_number,
ch1_raw, ch1_dark, ch1_corrected, ch1_uv, ch1_filtered, ch1_filtered_uv,
ch2_raw, ch2_dark, ch2_corrected, ch2_uv, ch2_filtered, ch2_filtered_uv,
ratio, filtered_ratio, ratio_valid, filtered_ratio_valid, ch1_saturated, ch2_saturated
```

### Приклад рядка

```text
DATA,40004171,26,1043353,1031889,11464,2798.828,11595,2830.811,204278,682426,-478148,-116735.352,-473754,-115662.598,-0.023976,-0.024475,1,1,0,0
```

> **Примітка:** Значення є даними bring-up та не являють собою абсолютну метрологічну калібровку.