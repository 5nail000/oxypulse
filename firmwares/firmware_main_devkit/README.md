# OxyPulse Firmware — главный узел

ESP32-DEVKIT CH340: **WiFi AP/STA + HTTP + I2C + HRV**. BLE нет — Wellue/COOSPO приходят с BLE-узла ([`firmware_ble_s3/`](../firmware_ble_s3/) или [`firmware_ble_devkit/`](../firmware_ble_devkit/)) по UART.

Обзор — [`README.md`](../../README.md). API — [`docs/API.md`](../../docs/API.md).  
Распиновка — [`docs/HARDWARE.md`](../../docs/HARDWARE.md). Монолит на одной плате — [`firmware_alone_devkit/`](../firmware_alone_devkit/).

## Плата

- **ESP32-DEVKIT USB Type-C CH340** (`esp32dev`)
- UART-мост: **RX=GPIO16, TX=GPIO17** (`Serial2`, 921600). RX0/TX0 не трогать (CH340).
- I2C и клапан — как раньше (18/19, 22/23, GPIO26)

## Сборка

```bash
cd firmwares/firmware_main_devkit
pio run -t uploadfs   # web/ → LittleFS
pio run -t upload
pio device monitor
```

UI берётся из [`web/`](../../web/) (`data_dir = ../../web` в `platformio.ini`).

Открывайте веб по STA-IP или AP `192.168.4.1`. На главном узле нет BLE coexistence — HTTP должен быть заметно стабильнее, чем на монолите.

`app.js` грузится сразу; `hrv.js` — по клику «Замерить HRV».

## Подключение

- **I2C гипоксия:** SDA=GPIO**22**, SCL=GPIO**23**
- **I2C рабочий:** SDA=GPIO**18**, SCL=GPIO**19**
- **Клапан (тест):** GPIO **26**, активный HIGH
- **UART к S3:** GPIO**16** RX ← S3 TX, GPIO**17** TX → S3 RX, общая GND
- **WiFi AP:** `ESP32-Tester` / `12345678` / `192.168.4.1`

Не использовать GPIO **16/17** как реле/servo — мост к BLE-узлу.

| Датчик | Шина | Адрес | Опрос |
|---|---|---|---|
| ADS1115 (O₂ гипоксия) | 22/23 | `0x48` | 50 ms |
| DPS310 (гипоксия) | 22/23 | `0x76` / `0x77` | 100 ms |
| ADS1115 (O₂ рабочий) | 18/19 | `0x48` | 50 ms |
| DPS310 (рабочий) | 18/19 | `0x76` / `0x77` | 100 ms |
| SFM3300-250-D | 18/19 | `0x40` | 10 ms |
| SCD41 | 18/19 | `0x62` | 1000 ms |

При старте — I2C-скан в Serial. В `/api/status` поле `sensors.bridge.ok` — жив ли UART с S3.

## Флаги сборки (`platformio.ini`)

| Флаг | По умолчанию | Смысл |
|---|---|---|
| `WORKING_PRESSURE_BMP390` | `1` | Рабочий I2C: BMP390 вместо DPS310 |
| `SCD41_CO2_DYNAMIC_COMP` | `1` | Компенсация инерции CO₂ + T/RH (оценка + `co2_raw` в тренде) |
| `SCD41_CO2_TAU_SEC` | `60` | Постоянная времени τ CO₂, с |
| `SCD41_CO2_TRUST` | `70` | Доверие к lead-поправке, % (70 = 0.7 как в MVP) |
| `SCD41_CO2_COMP_MAX_DELTA_PPM` | `15000` | Макс. коррекция CO₂ за шаг, ppm |
| `SCD41_ASC_ENABLED` | `0` | ASC чипа: `1` = вкл (открытый воздух), `0` = выкл (маска/контур) |

## Калибровка

Константы в [`src/config.h`](src/config.h). Порядок O₂/поток — как в [`firmware_alone_devkit/README.md`](../firmware_alone_devkit/README.md).

**CO₂ SCD41:** FRC — кнопка в веб-UI или `{"cmd":"co2_frc","target_ppm":400}` (3+ мин на ~400 ppm). ASC — только `SCD41_ASC_ENABLED` в `platformio.ini`, применяется при старте и пишется в EEPROM датчика.
