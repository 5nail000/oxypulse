# OxyPulse Firmware (монолит, DevKit)

**Откат / архив.** Одна ESP32-DEVKIT CH340: WiFi + BLE central + BLE GATT + I2C.
Рабочая схема — две платы: [`firmware_main_devkit/`](../firmware_main_devkit/) + BLE-узел ([`firmware_ble_s3/`](../firmware_ble_s3/) или [`firmware_ble_devkit/`](../firmware_ble_devkit/)).
Эту папку не развиваем.

Прошивка: **WiFi AP/STA + HTTP API + BLE peripheral + BLE central**
(I2C-датчики, GPIO/Servo, Wellue Ring O2 S, COOSPO H6M).

Обзор — [`README.md`](../../README.md). API — [`docs/API.md`](../../docs/API.md).  
Распиновка — [`docs/HARDWARE.md`](../../docs/HARDWARE.md).

## Сборка

```bash
cd firmwares/firmware_alone_devkit
pio run -t uploadfs   # web/ → LittleFS (см. board_build.filesystem_dir)
pio run -t upload
pio device monitor
```

UI берётся из [`web/`](../../web/) автоматически (`board_build.filesystem_dir = ../../web` в `platformio.ini`).

**Открывайте веб по STA-IP** (например `http://192.168.31.73/`), если ESP уже в домашней WiFi — AP (`192.168.4.1`) заметно медленнее при активном BLE.

`app.js` грузится сразу; `hrv.js` — по клику «Замерить HRV» (lazy-load).

## Подключение

- **I2C гипоксия:** SDA=GPIO**22**, SCL=GPIO**23**
- **I2C рабочий:** SDA=GPIO**18**, SCL=GPIO**19**
- **Клапан (тест):** GPIO **26**, активный HIGH
- **BLE-имя:** `ESP32-Tester`
- **WiFi AP:** `ESP32-Tester` / `12345678` / `192.168.4.1`

| Датчик | Шина | Адрес | Опрос |
|---|---|---|---|
| ADS1115 (O₂ гипоксия) | 22/23 | `0x48` | 50 ms |
| DPS310 (гипоксия) | 22/23 | `0x76` / `0x77` | 100 ms |
| ADS1115 (O₂ рабочий) | 18/19 | `0x48` | 50 ms |
| DPS310 (рабочий) | 18/19 | `0x76` / `0x77` | 100 ms |
| SFM3300-250-D | 18/19 | `0x40` | 10 ms |
| SCD41 | 18/19 | `0x62` | 1000 ms |

При старте — I2C-скан в Serial. Отсутствующий адрес → `sensors.*.ok = false` в JSON.

## WiFi / BLE

- AP+STA: credentials STA в NVS, настройка через веб-UI
- NimBLE peripheral (клиент) + central (Wellue, COOSPO)
- **Порядок init:** BLE до WiFi; modem sleep при WiFi+BLE

## Структура

| Файл | Назначение |
|---|---|
| `i2c_bus.*` | Шина + мьютекс + сканер |
| `ao02.*`, `sfm3300.*`, `dps310.*`, `scd41.*` | I2C-драйверы |
| `ble_sensors.*`, `wellue_oxyii.*` | BLE central |
| `gpio_controller.*`, `servo_controller.*` | Whitelist GPIO / Servo |
| `trend_buffer.*` | Тренды 1 Гц (~30 мин RAM) |
| `hrv_session.*` | Сессия HRV: все R-R с ремня (30 с / 2 / 3 / 5 мин) |
| `command_handler.*` | JSON cmd + `buildStatusJson` |
| `wifi_server.*`, `wifi_sta.*` | HTTP + AP/STA |
| `ble_server.*` | GATT peripheral |

## HTTP API

См. [`docs/API.md`](../docs/API.md).

## Калибровка

Константы в [`src/config.h`](src/config.h).

### O₂ (AO-02)

| Константа | Назначение |
|-----------|------------|
| `O2_HYPOXIA_OFFSET_MV` / `O2_HYPOXIA_AIR_MV` | Гипоксия: short → **0%**, воздух → **`O2_AIR_PERCENT`** |
| `O2_WORKING_OFFSET_MV` / `O2_WORKING_AIR_MV` | Рабочий контур: то же |
| `O2_AIR_PERCENT` | 20.9 — общий эталон воздуха |

Формула: `O2% = (мV − OFFSET) / (AIR_MV − OFFSET) × O2_AIR_PERCENT`

1. Serial: `AO-02 hypoxia` / `AO-02 working` → `O2=… %  mV=…`
2. Калибровать каждый датчик отдельно: short → `*_OFFSET_MV`; воздух → `*_AIR_MV`; перепрошить

Подключение: Vsensor+→A0, Vsensor−→A1, GAIN_SIXTEEN, ADDR→GND.

### Поток (SFM3300)

`flow_slm = (raw − SFM_FLOW_OFFSET) / SFM_FLOW_SCALE`

Fallback scale: **120** (воздух), **142.8** (O₂).  
Пороги: `FLOW_PHASE_THRESHOLD_SLM`, `FLOW_DEADBAND_SLM`.

### CO₂ (SCD41)

Прогрев `SCD41_WARMUP_MS`; обновление раз в 5 с.

## Питание сервы

Отдельный **5 V**, **общая GND** с ESP32.
