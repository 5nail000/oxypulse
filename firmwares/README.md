# Прошивки OxyPulse

| Папка | Плата | Назначение |
|-------|-------|------------|
| [`firmware_main_devkit/`](firmware_main_devkit/) | ESP32-DEVKIT CH340 | WiFi, HTTP, I2C, клапан, HRV |
| [`firmware_ble_s3/`](firmware_ble_s3/) | ESP32-S3-N16R8 | BLE central → UART |
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

**Главный узел:**

```bash
cd firmwares/firmware_main_devkit
pio run -t uploadfs
pio run -t upload
```
