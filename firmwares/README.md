# Прошивки OxyPulse

| Папка | Плата | Назначение |
|-------|-------|------------|
| [`firmware_valve_devkit/`](firmware_valve_devkit/) | ESP32-DEVKIT CH340 | Поле: ЦУ клапанов, I²C, ИГТ, BLE peripheral `OxyPulse-CU` |
| [`firmware_main_devkit/`](firmware_main_devkit/) | ESP32-DEVKIT CH340 | Стенд: WiFi, HTTP, I2C, клапаны, HRV, ИГТ |
| [`firmware_ble_s3/`](firmware_ble_s3/) | ESP32-S3-N16R8 | Стенд / запас: BLE central → UART |
| [`firmware_ble_devkit/`](firmware_ble_devkit/) | ESP32-DEVKIT CH340 | BLE central → UART (вторая DevKit) |
| [`firmware_common/`](firmware_common/) | — | UART NDJSON, logger, snapshot-структуры |
| [`firmware_alone_devkit/`](firmware_alone_devkit/) | ESP32-DEVKIT | Монолит (архив, откат) |

Обзор — [`../README.md`](../README.md). Железо — [`../docs/HARDWARE.md`](../docs/HARDWARE.md).

## Сборка (сплит)

**BLE-узел** — одна из прошивек:

```bash
# ESP32-S3-N16R8
cd firmwares/firmware_ble_s3
pio run -t upload

# или вторая ESP32-DEVKIT CH340
cd firmwares/firmware_ble_devkit
pio run -t upload
```

## Поле: ЦУ клапанов (`firmware_valve_devkit`)

```bash
cd firmwares/firmware_valve_devkit
pio run -t upload
pio device monitor
```

Телефон ищет BLE-имя **`OxyPulse-CU`**. Веб и WiFi на этой прошивке нет. Стендовый сплит main+S3 не трогать: это откат.

**Главный узел (стенд):**

```bash
cd firmwares/firmware_main_devkit
pio run -t uploadfs
pio run -t upload
```
