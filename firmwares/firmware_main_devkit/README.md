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

## Калибровка

Константы в [`src/config.h`](src/config.h). Порядок тот же, что в [`firmware_alone_devkit/README.md`](../firmware_alone_devkit/README.md).
