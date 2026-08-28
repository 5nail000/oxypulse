# OxyPulse Firmware — BLE-узел (ESP32-S3-N16R8)

ESP32-S3-N16R8: **только NimBLE central** (Wellue Ring O2 S + COOSPO H6M). WiFi выключен.
Снимки и все R-R уходят по UART на [`firmware_main_devkit/`](../firmware_main_devkit/).

Альтернатива — [`firmware_ble_devkit/`](../firmware_ble_devkit/) (вторая ESP32-DEVKIT CH340).

Обзор — [`README.md`](../../README.md). Распиновка — [`docs/HARDWARE.md`](../../docs/HARDWARE.md).

## Плата

- **ESP32-S3-N16R8** (`esp32-s3-devkitc-1`, octal flash + OPI PSRAM)
- UART-мост: **TX=GPIO16, RX=GPIO17** (`Serial1`, 921600) — прямая коммутация с main
- Не использовать GPIO **19/20** (USB), **26–37** (flash/PSRAM), **43/44** (UART0)

## Сборка

```bash
cd firmwares/firmware_ble_s3
pio run -t upload
pio device monitor
```

PIO в логе может написать «DevKitC-1-N8» — это имя board JSON. Для N16R8 заданы `qio_opi`, flash 16MB и `BOARD_HAS_PSRAM`. Если плата уходит в boot-loop — проверить эти флаги.

## Подключение к главному узлу

Прямая коммутация одноимённых пинов:

```
S3 GPIO16  ───────────────────  main GPIO16
S3 GPIO17  ───────────────────  main GPIO17
GND        ───────────────────  GND
```

(BLE: TX=16, RX=17; main: RX=16, TX=17 — см. `config.h` / `uart_protocol.h`.)

3.3 V между платами не соединять. Протокол: NDJSON, см. [`firmware_common/src/uart_protocol.h`](../firmware_common/src/uart_protocol.h).

## Поведение

- Имя NimBLE: `OxyPulse-BLE` (не рекламируется, только central)
- Snapshot Wellue/COOSPO ~1 Гц
- Каждый R-R с ремня — отдельная строка `{"t":"rr","ms":…}`
- GATT peripheral для Android **нет** (HTTP на main)
