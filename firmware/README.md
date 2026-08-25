# OxyPulse Firmware

Прошивка ESP32: **WiFi AP/STA + HTTP API + BLE peripheral + BLE central**
(I2C-датчики, GPIO/Servo, Wellue Ring O2 S, COOSPO H6M).

Обзор — [`README.md`](../README.md). API — [`docs/API.md`](../docs/API.md).  
Распиновка — [`docs/HARDWARE.md`](../docs/HARDWARE.md).

## Сборка

```bash
cd firmware
pio run -t uploadfs   # web/ → LittleFS (firmware/data/)
pio run -t upload
pio device monitor
```

После правок в [`web/`](../web/) синхронизируйте UI:

```bash
copy ..\web\index.html, ..\web\app.js, ..\web\style.css data\
pio run -t uploadfs
```

(На Linux/macOS — `cp ../web/{index.html,app.js,style.css} data/`.)

## Подключение

- **I2C:** SDA=GPIO**22**, SCL=GPIO**23**, 100 kHz, подтяжки 4.7 kΩ к 3.3 V
- **Клапан (тест):** GPIO **26**, активный HIGH
- **BLE-имя:** `ESP32-Tester`
- **WiFi AP:** `ESP32-Tester` / `12345678` / `192.168.4.1`

| Датчик | Адрес | Опрос |
|---|---|---|
| ADS1115 (AO-02) | `0x48` | 50 ms |
| SFM3300-250-D | `0x40` | 10 ms |
| DPS310 | `0x76` / `0x77` | 100 ms |
| SCD41 | `0x62` | 1000 ms |

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
| `O2_OFFSET_MV` | Short Pin1↔Pin2+3 → **0%** |
| `O2_AIR_MV` | Воздух → **`O2_AIR_PERCENT`** (20.9) |

Формула: `O2% = (мV − OFFSET) / (AIR_MV − OFFSET) × O2_AIR_PERCENT`

1. Serial: `AO-02: O2=… %  mV=…`
2. Short → `O2_OFFSET_MV`; воздух → `O2_AIR_MV`; перепрошить

Подключение: Vsensor+→A0, Vsensor−→A1, GAIN_SIXTEEN, ADDR→GND.

### Поток (SFM3300)

`flow_slm = (raw − SFM_FLOW_OFFSET) / SFM_FLOW_SCALE`

Fallback scale: **120** (воздух), **142.8** (O₂).  
Пороги: `FLOW_PHASE_THRESHOLD_SLM`, `FLOW_DEADBAND_SLM`.

### CO₂ (SCD41)

Прогрев `SCD41_WARMUP_MS`; обновление раз в 5 с.

## Питание сервы

Отдельный **5 V**, **общая GND** с ESP32.
