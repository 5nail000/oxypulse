# OxyPulse Firmware — ЦУ клапанов

ESP32-DEVKIT CH340: **I²C + GPIO + Servo1 + FSM ИГТ + BLE peripheral `OxyPulse-CU`**.

Нет WiFi, веба, LittleFS, трендов, HRV, UART к S3. Стендовый откат — [`firmware_main_devkit/`](../firmware_main_devkit/).

Драйверы I²C/GPIO/ИГТ копируются из main при сборке (`copy_hw.py` → `src/hw/`, в git не входят). Распиновка та же — [`docs/HARDWARE.md`](../../docs/HARDWARE.md). Протокол — [`docs/API.md`](../../docs/API.md).

## Сборка

```bash
cd firmwares/firmware_valve_devkit
pio run -t upload
pio device monitor
```

`uploadfs` не нужен. Serial: `OxyPulse CU starting`, затем `BLE: advertising as OxyPulse-CU`.

Если `pio run` падает на `bootloader.bin` с `intelhex.HexRecordError` — это окружение esptool/Python, исходники ЦУ уже скомпилированы. Обновить `intelhex` в Python PlatformIO или прошить с другой машины.

Чеклист на железе — [`TESTING.md`](../../TESTING.md) §12.

## BLE

- Имя: **OxyPulse-CU**
- LIVE notify ~1 Гц, JSON < 500 байт
- CMD: `gpio_*` / `servo_*` / `flow_reset` / `co2_frc` / `igt_*` / `ping` / `phys`
- Keep-alive: любая CMD отмечает связь. Обрыв GATT **не** abort ИГТ (протокол идёт дальше; телефон reconnect)

## Подключение

Как у main, без UART 16/17:

- I2C гипоксия: SDA=22, SCL=23
- I2C рабочий: SDA=18, SCL=19 (STC31-C `0x29`, поток, давление)
- OCS-3RL2.0 (рабочий O₂): UART RX=34 / TX=32, питание 5–12 V
- V1–V6: 25, 26, 27, 13, 14, 21 (`GPIO_VALVE_ACTIVE_LOW=1`)
- O2_MOTOR: 33
- Servo1: GPIO 4

Флаги IGT/STC31 в `platformio.ini` совпадают со стендовым main.
