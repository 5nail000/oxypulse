# OxyPulse Firmware — BLE-узел (ESP32-DEVKIT CH340)

Вторая **ESP32-DEVKIT CH340**: **только NimBLE central** (Wellue Ring O2 S + COOSPO H6M). WiFi выключен.
Снимки и все R-R уходят по UART на [`firmware_main_devkit/`](../firmware_main_devkit/).

Альтернатива — [`firmware_ble_s3/`](../firmware_ble_s3/) (ESP32-S3-N16R8).

Обзор — [`README.md`](../../README.md). Распиновка — [`docs/HARDWARE.md`](../../docs/HARDWARE.md).

## Плата

- **ESP32-DEVKIT CH340** (`esp32dev`)
- UART-мост: **TX=GPIO16, RX=GPIO17** (`Serial1`, 921600) — прямая коммутация с main
- Не использовать **GPIO1/GPIO3** (USB-UART CH340, лог `Serial`)

## Сборка

```bash
cd firmwares/firmware_ble_devkit
pio run -t upload
pio device monitor
```

## Подключение к главному узлу

Прямая коммутация одноимённых пинов:

```
BLE GPIO16  ───────────────────  main GPIO16
BLE GPIO17  ───────────────────  main GPIO17
GND         ───────────────────  GND
```

3.3 V между платами не соединять. Протокол: NDJSON, см. [`firmware_common/src/uart_protocol.h`](../firmware_common/src/uart_protocol.h).

## Поведение

- Имя NimBLE: `OxyPulse-BLE` (не рекламируется, только central)
- Snapshot Wellue/COOSPO ~1 Гц
- Каждый R-R с ремня — отдельная строка `{"t":"rr","ms":…}`
- GATT peripheral для Android **нет** (HTTP на main)
