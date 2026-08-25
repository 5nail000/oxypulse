# HTTP / BLE API

Единый источник истины для [`firmware/`](../firmware/), [`web/`](../web/) и [`android/`](../android/).

Распиновка — [`HARDWARE.md`](HARDWARE.md).

## Транспорт

| Канал | Когда использовать |
|---|---|
| **HTTP** (WiFi AP `192.168.4.1` или STA IP) | **Основной:** веб-UI |
| **BLE GATT** (имя `ESP32-Tester`) | Android (низкий приоритет); тот же JSON |

Web Bluetooth в браузере **не используется** (HTTP AP без HTTPS). Wellue и COOSPO
подключает **ESP32** как BLE-central; клиент читает `/api/status`.

---

## BLE GATT (peripheral ESP32)

| Роль | UUID |
|---|---|
| Service | `a1b2c3d4-e5f6-7890-abcd-ef1234567890` |
| STATUS (Read + Notify) | `a1b2c3d4-e5f6-7890-abcd-ef1234567891` |
| CMD (Write) | `a1b2c3d4-e5f6-7890-abcd-ef1234567892` |

STATUS — JSON как `GET /api/status`. CMD — JSON как `POST /api/cmd`.

---

## HTTP API

| Метод | Путь | Назначение |
|---|---|---|
| GET | `/api/status` | Полное состояние (JSON) |
| POST | `/api/cmd` | GPIO/Servo/flow_reset |
| POST | `/api/wifi/scan` | Запуск async-скана WiFi |
| GET | `/api/wifi/scan` | `{scanning, networks:[{ssid,rssi,secure}]}` |
| POST | `/api/wifi/config` | STA credentials или `{"clear":true}` |
| GET | `/api/trends` | Страница истории (`?offset=0&limit=240`, max 300) |
| POST | `/api/trends/sources` | Включение каналов трендов |

Статика UI: `/`, `/app.js`, `/style.css` (LittleFS в `firmware/data/`).

---

## JSON status (`GET /api/status`)

| Поле | Тип | Описание |
|---|---|---|
| `uptime_ms` | number | Uptime ESP32 |
| `gpio_tap_active` | bool | Идёт импульс gpio_tap |
| `gpio` | array | `[{pin, state: 0\|1}]` |
| `servos` | array | `[{pin, angle, auto}]` |
| `whitelist` | array | Разрешённые GPIO |
| `sensors` | object | I2C + BLE central |
| `wifi` | object | AP + STA |
| `trends` | object | `sources` — маска каналов |

### `sensors.o2`

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
| `ppm` | int |
| `percent` | float |
| `temp_c`, `rh` | float |
| `warming_up` | bool |

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

### `trends.sources`

Булевы: `o2`, `flow`, `pressure`, `co2`, `wellue`, `hr`, `rr`.

---

## Команды (`POST /api/cmd`)

```json
{"cmd":"ping"}
{"cmd":"gpio_set","pin":26,"state":1}
{"cmd":"gpio_tap","pin":26}
{"cmd":"gpio_tap","pin":26,"pulse_ms":3000}
{"cmd":"servo_set","pin":18,"angle":90}
{"cmd":"servo_auto","pin":18,"enabled":true,"pause_ms":5000}
{"cmd":"flow_reset"}
```

`pin` ∈ `whitelist`. Пины **22/23** — I2C, не GPIO.

Включение трендов — **`POST /api/trends/sources`**:

```json
{"o2":true,"flow":true,"wellue":true,"hr":true,"rr":false}
```

---

## Тренды

- 1 Гц, ~30 мин RAM; reboot обнуляет точки
- `GET /api/trends?offset=&limit=` — постранично (max 300)

---

## BLE central

Константы в `firmware/src/config.h`:

| Устройство | Протокол |
|---|---|
| Wellue Ring O2 S | OxyII `e8fb0001-…` |
| COOSPO H6M | Heart Rate 0x180D / 0x2A37 |

**Init:** NimBLE до WiFi; WiFi+BLE — `WIFI_PS_MIN_MODEM`.

---

## Калибровка

Константы в `firmware/src/config.h` — порядок в [`firmware/README.md`](../firmware/README.md).
