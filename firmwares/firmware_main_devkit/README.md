# OxyPulse Firmware — главный узел

ESP32-DEVKIT CH340: **WiFi AP/STA + HTTP + I2C + HRV + ИГТ**. BLE нет — Wellue/COOSPO приходят с BLE-узла ([`firmware_ble_s3/`](../firmware_ble_s3/) или [`firmware_ble_devkit/`](../firmware_ble_devkit/)) по UART.

Обзор — [`README.md`](../../README.md). API — [`docs/API.md`](../../docs/API.md).  
Распиновка — [`docs/HARDWARE.md`](../../docs/HARDWARE.md). Монолит на одной плате — [`firmware_alone_devkit/`](../firmware_alone_devkit/).

## Плата

- **ESP32-DEVKIT USB Type-C CH340** (`esp32dev`)
- UART-мост: **RX=GPIO16, TX=GPIO17** (`Serial2`, 921600). RX0/TX0 не трогать (CH340).
- I2C: 18/19 (рабочий), 22/23 (буферы гипоксии/гипероксии); клапаны V1–V6, мотор O₂, Servo1 — [`docs/HARDWARE.md`](../../docs/HARDWARE.md)

## Сборка

```bash
cd firmwares/firmware_main_devkit
pio run -t uploadfs   # web/ → LittleFS
pio run -t upload
pio device monitor
```

UI берётся из [`web/`](../../web/) (`data_dir = ../../web` в `platformio.ini`).

Открывайте веб по STA-IP или AP `192.168.4.1`. На главном узле нет BLE coexistence — HTTP должен быть заметно стабильнее, чем на монолите.

`app.js` грузится сразу; `hrv.js` и `igt.js` — вместе с UI (`uploadfs`).

## Подключение

- **I2C буферы гипоксии/гипероксии:** SDA=GPIO**22**, SCL=GPIO**23** (датчики обоих буферов, разные адреса)
- **I2C рабочий:** SDA=GPIO**18**, SCL=GPIO**19**
- **Клапаны V1–V6:** GPIO **25, 26, 27, 13, 14, 21** — полярность реле: `GPIO_VALVE_ACTIVE_LOW` в `platformio.ini` (сейчас **1** = active LOW)
- **Мотор концентратора O₂:** GPIO **33**
- **Servo1** (гипоксия/гипероксия): **D4** = GPIO **4**
- **UART к S3:** GPIO**16** RX ← S3 TX, GPIO**17** TX → S3 RX, общая GND
- **OCS-3RL2.0 (рабочий O₂):** GPIO**34** RX ← датчик TX, GPIO**32** TX → датчик RX, питание **5–12 V**
- **WiFi AP:** `ESP32-Tester` / `12345678` / `192.168.4.1`

Не использовать GPIO **16/17** (мост BLE) и **32/34** (OCS) как реле/servo.

| Датчик | Шина | Адрес / порт | Опрос |
|---|---|---|---|
| ADS1115 (O₂ гипоксия) | 22/23 | `0x48` | 50 ms |
| DPS310 (гипоксия) | 22/23 | `0x76` / `0x77` | 100 ms |
| ADS1115 (O₂ гипероксия) | 22/23 | свой адрес | 50 ms |
| DPS310 (гипероксия) | 22/23 | свой адрес | 100 ms |
| OCS-3RL2.0 (O₂ рабочий) | UART | 9600, RX34/TX32 | ~500 ms кадр |
| DPS310/BMP390 (рабочий) | 18/19 | `0x76` / `0x77` | 100 ms |
| SFM3300-250-D | 18/19 | `0x40` | 10 ms |
| STC31-C | 18/19 | `0x29` | 1000 ms |

При старте — I2C-скан в Serial. В `/api/status` поле `sensors.bridge.ok` — жив ли UART с S3.

## Флаги сборки (`platformio.ini`)

| Флаг | По умолчанию | Смысл |
|---|---|---|
| `WORKING_PRESSURE_BMP390` | `1` | Рабочий I2C: BMP390 вместо DPS310 |
| `STC31_ASC_ENABLED` | `0` | ASC STC31: `1` = вкл (открытый воздух), `0` = выкл (маска/контур) |
| `IGT_PRESSURE_DROP_HPA` | `10` | Падение P_гипо от **baseline** (hPa) — завершение выкачки |
| `IGT_WORK_PRESSURE_DELTA_HPA` | `3` | P_раб − P_гипо ≥ порога — альтернативное завершение выкачки |
| `IGT_PRESSURE_BASELINE_LIMIT_HPA` | `25` | Аварийный стоп подготовки: P ниже своего baseline на этот порог (гипо или раб.) |
| `IGT_PRIMARY_FILL_SEC` | `120` | Длительность первичного наполнения буфера гипоксии |
| `IGT_PRESSURE_WAIT_SEC` | `60` | Таймаут выкачки буфера |
| `IGT_RECIRC_TIMEOUT_SEC` | `600` | Таймаут рециркуляции до целевого O₂ |
| `IGT_SERVO_HYPOXIA_DEG` | `180` | Servo1 в фазе гипоксии |
| `IGT_SERVO_NORMOXIA_DEG` | `0` | Servo1 в нормоксии / после abort / после последнего цикла |

## Калибровка

Константы в [`src/config.h`](src/config.h). O₂ гипоксии — `O2_HYPOXIA_*`. Рабочий O₂ — цифровой OCS (калибровка мВ не нужна).

**CO₂ STC31-C:** FRC — кнопка в веб-UI или `{"cmd":"co2_frc","target_ppm":400}`. ASC — `STC31_ASC_ENABLED` в `platformio.ini`.

## ИГТ

FSM в `src/igt_session.cpp`: подготовка буфера гипоксии (V1, V4, O2_MOTOR, датчики O₂/давления/потока) и циклы гипоксия/нормоксия (Servo1). API — [`docs/API.md`](../../docs/API.md) (`/api/igt/*`). Процесс не зависит от веба. После последнего цикла и при abort Servo1 = 0°.
