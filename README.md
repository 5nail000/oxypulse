# OxyPulse

Портативная система мониторинга и управления замкнутым дыхательным контуром:
две ESP32 + датчики на I2C + веб-интерфейс (основной клиент).

**На текущем этапе:** основная работа в **`firmwares/firmware_main_devkit/`** + BLE-узел (`firmware_ble_s3` или `firmware_ble_devkit`) + **`web/`**.  
Android-клиент (`android/`) — **в последнем приоритете**; на сплите используйте **WiFi HTTP**.

## Структура репозитория

| Путь | Назначение | Приоритет |
|---|---|---|
| [`firmwares/`](firmwares/) | Все прошивки (main, ble, common, alone) | |
| [`firmwares/firmware_main_devkit/`](firmwares/firmware_main_devkit/) | ESP32-DEVKIT: WiFi, HTTP API, I2C, клапан, HRV, тренды | **основной** |
| [`firmwares/firmware_ble_s3/`](firmwares/firmware_ble_s3/) | ESP32-S3-N16R8: BLE central (Wellue, COOSPO) → UART | **основной** |
| [`firmwares/firmware_ble_devkit/`](firmwares/firmware_ble_devkit/) | Вторая ESP32-DEVKIT: BLE central → UART | альтернатива S3 |
| [`firmwares/firmware_common/`](firmwares/firmware_common/) | UART NDJSON, snapshot-структуры, logger | |
| [`firmwares/firmware_alone_devkit/`](firmwares/firmware_alone_devkit/) | Монолит на одной ESP32 (откат, не развиваем) | архив |
| [`web/`](web/) | Веб-UI (Vanilla JS); LittleFS с `firmware_main_devkit` | **основной** |
| [`docs/API.md`](docs/API.md) | HTTP API, JSON status, команды | |
| [`docs/HARDWARE.md`](docs/HARDWARE.md) | Распиновка, I²C, UART-мост, датчики, BLE, питание | |
| [`web/README.md`](web/README.md) | Веб-UI, тренды, WiFi-настройка | |
| [`android/`](android/) | Android-клиент (WiFi HTTP; GATT только на `firmware_alone_devkit`) | низкий |
| [`PROJECT_CONTEXT.md`](PROJECT_CONTEXT.md) | Цели, архитектура, дорожная карта | |
| [`TESTING.md`](TESTING.md) | Проверка на железе | |

## Быстрый старт

### Две платы (рабочая схема)

1. Соединить UART: **GPIO16↔16**, **GPIO17↔17**, общая **GND** (прямая коммутация, см. [`docs/HARDWARE.md`](docs/HARDWARE.md)).
2. Прошить BLE-узел, затем главный:

```bash
# S3-N16R8
cd firmwares/firmware_ble_s3
pio run -t upload

# или вторая DevKit CH340
cd firmwares/firmware_ble_devkit
pio run -t upload

cd ../firmware_main_devkit
pio run -t uploadfs   # web/ → LittleFS
pio run -t upload
pio device monitor
```

Подключиться к AP **`ESP32-Tester`** (пароль `12345678`) → **`http://192.168.4.1/`**  
или по IP в LAN, если настроен WiFi STA (кнопка **WiFi** в интерфейсе).

Подробнее — [`firmwares/firmware_main_devkit/README.md`](firmwares/firmware_main_devkit/README.md), [`firmwares/firmware_ble_s3/README.md`](firmwares/firmware_ble_s3/README.md), [`firmwares/firmware_ble_devkit/README.md`](firmwares/firmware_ble_devkit/README.md), [`web/README.md`](web/README.md).

### Android (опционально)

```bash
cd android
gradlew.bat assembleDebug
```

APK: `android/app/build/outputs/apk/debug/app-debug.apk`. На сплите — **только WiFi HTTP**. См. [`android/README.md`](android/README.md).

## Железо (кратко)

- **Главный узел:** ESP32-DEVKIT CH340 — I2C, WiFi, клапан GPIO26
- **BLE-узел:** ESP32-S3-N16R8 или вторая ESP32-DEVKIT — Wellue / COOSPO, без WiFi
- **UART:** 921600, прямая коммутация **GPIO16↔16**, **GPIO17↔17**
- **I2C гипоксия (O₂ + давление):** SDA=GPIO22, SCL=GPIO23 (DevKit)
- **I2C рабочий (O₂, давление, поток, CO₂):** SDA=GPIO18, SCL=GPIO19 (DevKit)
- **I2C-датчики:** ADS1115+AO-02 (`0x48`), DPS310 (`0x76`/`0x77`) — на обеих шинах; SFM3300 (`0x40`) и SCD41 (`0x62`) — только рабочий

GPIO34/35 на `esp32dev` — input-only, под I2C не использовать. GPIO **16/17** на DevKit — UART-мост, не реле.

## API

JSON over HTTP (`/api/status`, `/api/cmd`) с главного узла.  
Полная спецификация — [`docs/API.md`](docs/API.md).

## Ближайшие шаги

1. E2E на двух платах: UART `sensors.bridge.ok`, затем Wellue/COOSPO/HRV
2. Калибровка O₂ (`O2_HYPOXIA_*` / `O2_WORKING_*` в `firmwares/firmware_main_devkit/src/config.h`)
3. Логика клапана по порогам CO₂ / SpO₂ / ЧСС
4. Android — по мере необходимости, после firmware + web
