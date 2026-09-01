# HTTP API

Единый источник истины для [`firmwares/firmware_main_devkit/`](../firmwares/firmware_main_devkit/), [`web/`](../web/) и [`android/`](../android/).

Распиновка — [`HARDWARE.md`](HARDWARE.md).

## Транспорт

| Канал | Когда использовать |
|---|---|
| **HTTP** (WiFi AP `192.168.4.1` или STA IP главного узла) | **Основной:** веб-UI и Android |
| **BLE GATT** | Только монолит [`firmware_alone_devkit/`](../firmwares/firmware_alone_devkit/) (имя `ESP32-Tester`). На сплите GATT нет |

Web Bluetooth в браузере **не используется**. Wellue и COOSPO подключает **BLE-узел** (`firmwares/firmware_ble_s3` или `firmware_ble_devkit`); клиент читает `/api/status` с DevKit.

UART между платами — внутренний NDJSON, не HTTP.

---

## BLE GATT (peripheral ESP32)

| Роль | UUID |
|---|---|
| Service | `a1b2c3d4-e5f6-7890-abcd-ef1234567890` |
| STATUS (Read + Notify) | `a1b2c3d4-e5f6-7890-abcd-ef1234567891` |
| CMD (Write) | `a1b2c3d4-e5f6-7890-abcd-ef1234567892` |

STATUS — JSON как `GET /api/status`. CMD — JSON как `POST /api/cmd`. На сплите этот GATT **не поднимается**.

---

## HTTP API

| Метод | Путь | Назначение |
|---|---|---|
| GET | `/api/status` | Полное состояние (JSON) |
| POST | `/api/cmd` | GPIO/Servo/flow_reset |
| POST | `/api/wifi/scan` | Запуск скана WiFi (фоновая задача на ESP32, ~до 12 с) |
| GET | `/api/wifi/scan` | `{scanning, networks:[{ssid,rssi,secure,channel}], error?}` |
| POST | `/api/wifi/config` | STA credentials или `{"clear":true}` |
| GET | `/api/trends` | Страница истории (`?offset=0&limit=240`, max 300) |
| POST | `/api/trends/sources` | Включение каналов трендов |
| POST | `/api/hrv/start` | Старт записи R-R (`{"duration_sec":30\|120\|180\|300}`) |
| POST | `/api/hrv/stop` | Отмена HRV-сессии |
| GET | `/api/hrv` | Статус сессии; массив `rr` только в `done` |

Статика UI: `/`, `/app.js`, `/style.css` (LittleFS из [`web/`](../web/), прошивка [`firmwares/firmware_main_devkit/`](../firmwares/firmware_main_devkit/)).

---

## JSON status (`GET /api/status`)

| Поле | Тип | Описание |
|---|---|---|
| `uptime_ms` | number | Uptime ESP32 |
| `gpio_tap_active` | bool | Идёт импульс gpio_tap |
| `gpio` | array | `[{pin, state: 0\|1}]` |
| `servos` | array | `[{pin, angle, auto}]` |
| `whitelist` | array | Разрешённые GPIO |
| `sensors` | object | I2C + BLE. Группы `hypoxia` / `working` + плоские алиасы |

Корневые `sensors.o2` / `sensors.pressure` — **буфер гипоксии**.  
`sensors.flow` / `sensors.co2` — **рабочий контур** (дублируются в `sensors.working`).

### `sensors.hypoxia`

| Поле | Содержимое |
|---|---|
| `o2` | как `sensors.o2`: `ok`, `percent`, `mv` |
| `pressure` | как `sensors.pressure`: `ok`, `hpa`, `temp_c` |

### `sensors.working`

| Поле | Содержимое |
|---|---|
| `o2` | O₂ рабочего контура (`ok`, `percent`, `mv`) |
| `pressure` | DPS310 рабочего контура |
| `flow` | SFM3300 |
| `co2` | SCD41 |

### `sensors.o2` (алиас гипоксии)

| Поле | Тип |
|---|---|
| `ok` | bool |
| `percent` | float, % O₂ |
| `mv` | float, мВ |

### `sensors.flow`

| Поле | Тип |
|---|---|
| `ok` | bool |
| `slm` | float, л/мин |
| `inhale_l`, `exhale_l` | float |
| `ve_lpm` | float |
| `phase` | `"idle"` \| `"inhale"` \| `"exhale"` |

### `sensors.pressure`

| Поле | Тип |
|---|---|
| `ok` | bool |
| `hpa` | float |
| `temp_c` | float |

### `sensors.co2`

| Поле | Тип |
|---|---|
| `ok` | bool |
| `ppm` | int — отображаемое (`ppm_est` при `dynamic_comp`, иначе `ppm_raw`) |
| `ppm_raw` | int — сырое с чипа |
| `ppm_est` | int — оценка с компенсацией инерции |
| `dynamic_comp` | bool — сборка с `SCD41_CO2_DYNAMIC_COMP=1` |
| `percent` | float — `ppm / 10000` |
| `temp_c`, `rh` | float — сырые T/RH с чипа |
| `temp_c_est`, `rh_est` | float — оценка T/RH (при `dynamic_comp`) |
| `warming_up` | bool |
| `asc_enabled` | bool — ASC чипа (задаётся `SCD41_ASC_ENABLED` в `platformio.ini` при старте) |

### `sensors.wellue`

| Поле | Тип |
|---|---|
| `ok`, `connected`, `name`, `contact` | |
| `spo2`, `hr`, `battery`, `motion` | int |
| `error` | string |

Кольцо — режим **S8-AW**.

### `sensors.coospo`

| Поле | Тип |
|---|---|
| `ok`, `connected`, `name`, `contact` | |
| `bpm` | int |
| `rr_ms` | int (если ремень отдаёт) |
| `error` | string |

### `sensors.bridge`

UART к BLE-узлу (только сплит `firmware_main_devkit`). Веб может игнорировать поле.

| Поле | Тип |
|---|---|
| `ok` | bool — недавно была строка с S3 |
| `error` | string, `"uart"` если мост молчит |

### `trends.sources`

Булевы: `o2` (гипоксия), `pressure` (гипоксия), `flow`, `co2`, `work_o2`, `work_pressure`, `wellue`, `hr`, `rr`.

---

## Команды (`POST /api/cmd`)

```json
{"cmd":"ping"}
{"cmd":"gpio_set","pin":26,"state":1}
{"cmd":"gpio_tap","pin":26}
{"cmd":"gpio_tap","pin":26,"pulse_ms":3000}
{"cmd":"servo_set","pin":25,"angle":90}
{"cmd":"servo_auto","pin":25,"enabled":true,"pause_ms":5000}
{"cmd":"flow_reset"}
{"cmd":"co2_frc","target_ppm":400}
```

`co2_frc` — Forced Recalibration: датчик должен 3+ минуты работать в известной среде (обычно 400 ppm, свежий воздух). Команда останавливает periodic mode, пишет в EEPROM датчика и запускает измерения снова.

ASC (Automatic Self-Calibration) — только через флаг сборки `SCD41_ASC_ENABLED` в `platformio.ini` (по умолчанию `0` для маски/контура). При старте прошивка задаёт ASC и сохраняет в EEPROM датчика. В UI и HTTP-команд нет.

`pin` ∈ `whitelist`. Не использовать как GPIO: **22/23** (гипоксия), **18/19** (рабочий), **16/17** (UART-мост).

Включение трендов — **`POST /api/trends/sources`**:

```json
{"o2":true,"flow":true,"wellue":true,"hr":true,"rr":false}
```

---

## Тренды

- 1 Гц, ~75 мин RAM (кольцо 4500 точек в heap на DevKit); reboot обнуляет точки
- `GET /api/trends?offset=&limit=` — постранично (max 300)
- Массив `flow` в JSON трендов — **среднее VE за 30 с** (`ve_lpm`, л/мин), не мгновенный поток
- При `SCD41_CO2_DYNAMIC_COMP=1`: `co2` — ppm оценки (est), `co2_raw` — ppm с чипа (hold между sample 5 с)

---

## HRV (`/api/hrv`)

Краткосрочный замер по **всем** R-R с нагрудного ремня (не 1 Гц тренд). Длительность: **30 / 120 / 180 / 300** с (30 с — для отладки связи).

Старт:

```json
{"duration_sec":30}
```

Ошибки старта: `belt_not_connected`, `busy`, `invalid_duration`. Повторный `start` сбрасывает предыдущую сессию.

`GET /api/hrv`:

| Поле | Когда |
|---|---|
| `state` | `idle` \| `recording` \| `done` \| `error` |
| `duration_sec`, `elapsed_ms`, `rr_count` | всегда |
| `error` | `state=error` (например «ремень отключён») |
| `rr` | только `state=done` — массив интервалов в мс |

Клиент считает SDNN, rMSSD, pNN50, SD1/SD2 и рисует Poincaré. Минимум **15** интервалов после фильтрации артефактов. Краткий обрыв BLE до **10 с** не прерывает запись.

---

## BLE central

Константы в `firmwares/firmware_ble_s3/src/config.h` (или `firmware_ble_devkit`). Данные приходят на main по UART.

| Устройство | Протокол |
|---|---|
| Wellue Ring O2 S | OxyII `e8fb0001-…` |
| COOSPO H6M | Heart Rate 0x180D / 0x2A37 |

WiFi только на главном узле; на S3 WiFi выключен.

---

## Калибровка

Константы в `firmwares/firmware_main_devkit/src/config.h` — порядок в [`firmwares/firmware_main_devkit/README.md`](../firmwares/firmware_main_devkit/README.md).
