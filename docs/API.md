# API и транспорты

Единый источник истины для прошивок, [`web/`](../web/) и [`android/`](../android/).

Распиновка — [`HARDWARE.md`](HARDWARE.md). Роли плат — [`PROJECT_CONTEXT.md`](../PROJECT_CONTEXT.md).

## Транспорт

| Канал | Когда использовать |
|---|---|
| **OxyPulse-CU GATT** | Полевая схема: [`firmware_valve_devkit/`](../firmwares/firmware_valve_devkit/) + Android. Компактный LIVE/CMD, без WiFi |
| **HTTP** (WiFi AP `192.168.4.1` или STA IP) | Стендовый откат: [`firmware_main_devkit/`](../firmwares/firmware_main_devkit/) + веб / Android WiFi |
| **BLE GATT `ESP32-Tester`** | Только архивный монолит [`firmware_alone_devkit/`](../firmwares/firmware_alone_devkit/). UUID другие, не путать с ЦУ |

Web Bluetooth в браузере **не используется**.

На полевой схеме Wellue и COOSPO подключает **телефон** (BLE central). На стендовом сплите их подключает BLE-узел (`firmware_ble_s3` / `firmware_ble_devkit`), клиент читает `/api/status`.

UART между платами — внутренний NDJSON стендового сплита, не HTTP и не GATT ЦУ.

---

## Полевая схема: ЦУ `OxyPulse-CU`

Прошивка [`firmware_valve_devkit/`](../firmwares/firmware_valve_devkit/) на той же CH340: I²C, клапаны, Servo1, FSM ИГТ. BLE **только peripheral** к телефону. Нет WiFi, LittleFS, `trend_buffer`, `hrv_session`, UART к S3.

Имя рекламы: **`OxyPulse-CU`**. UUID **новые** (не `a1b2c3d4-…` монолита):

| Роль | UUID |
|---|---|
| Service | `c0a10000-4f78-11ef-b200-0800200c9a66` |
| LIVE (Read + Notify), ~1 Гц, JSON **< 500 байт** | `c0a10001-4f78-11ef-b200-0800200c9a66` |
| CMD (Write / Write NR) | `c0a10002-4f78-11ef-b200-0800200c9a66` |

После connect клиент запрашивает MTU **512**. Запись в CMD (`ping`, `phys`, команды) отмечает связь. Обрыв GATT или пауза CMD **не** прерывают ИГТ: FSM на ЦУ идёт дальше, телефон восстанавливает соединение, обрывы попадают в отчёт Android. Abort — только `igt_abort` / ошибка железа.

Телефон ~1 Гц шлёт `{cmd:"phys", spo2, hr, belt, contact}`. Если uplink нет — FSM **не** стопается; в LIVE `igt.sw` может подняться как предупреждение.

Тренды и HRV на ЦУ **нет**: их считает Android из merge LIVE + кольцо/ремень.

### LIVE (компактный JSON)

Смысл полей как у `GET /api/status`, короткие ключи:

```json
{"u":123456,"gt":0,"m":0,"h":{"o2":15.2,"p":1001.3},"w":{"o2":20.6,"p":1002.1,"ve":8.1,"slm":0.4,"co2":4.2},"gp":[[25,1],[26,0]],"sv":[[4,180,0]],"igt":{"st":"hypoxia","c":2,"n":6,"r":180}}
```

| Ключ | Смысл |
|---|---|
| `u` | `uptime_ms` |
| `gt` | `gpio_tap_active` 0/1 |
| `m` | мотор O₂ логически ON 0/1 |
| `h.o2` / `h.p` | O₂ % и давление hPa буфера гипоксии (`null` если `ok=false`) |
| `w.o2` / `w.p` / `w.ve` / `w.slm` / `w.il` / `w.el` / `w.co2` | рабочий контур: O₂ %, hPa, VE л/мин, поток slm, вдох/выдох л (кумулятив SFM за текущую фазу), CO₂ % |
| `gp` | `[[pin, state], …]` |
| `sv` | `[[pin, angle, auto], …]` |
| `igt.st` / `igt.ph` / `igt.nm` | state, phase, name |
| `igt.c` / `igt.n` / `igt.r` | cycle, cycles, phase_remain_sec |
| `igt.o2` / `igt.rd` / `igt.sw` | o2_target, ready_to_start, sensor_warn |
| `igt.bp` / `igt.br` / `igt.wb` | buffer_prep, buffer_prep_remain_sec, waiting_buffer |
| `igt.hs` / `igt.ns` / `igt.fr` | hypoxia_sec, normoxia_sec, fill_remain_sec |
| `igt.ta` / `igt.err` / `igt.p0` | test_action, error, practice_started_esp_ms |
| `igt.lv` / `igt.lk` / `igt.lc` / `igt.li` / `igt.le` | last_phase: ok 0/1, kind `h`\|`n`, cycle, inhale_l, exhale_l — только если фаза уже закрыта |

Whitelist GPIO на ЦУ тот же, что у main (клиент может подставить дефолт). Wellue/COOSPO в LIVE **нет**.

### CMD (JSON)

Те же `gpio_*` / `servo_*` / `flow_reset` / `co2_frc` / `ping`, плюс ИГТ и phys:

```json
{"cmd":"ping"}
{"cmd":"phys","spo2":96,"hr":72,"belt":1,"contact":1}
{"cmd":"igt_prep","name":"IGT","o2_target":15,"hypoxia_sec":360,"normoxia_sec":300,"cycles":6}
{"cmd":"igt_start"}
{"cmd":"igt_start","name":"IGT","o2_target":15,"hypoxia_sec":360,"normoxia_sec":300,"cycles":6}
{"cmd":"igt_abort"}
{"cmd":"igt_ack"}
{"cmd":"igt_skip"}
{"cmd":"igt_fill_continue"}
{"cmd":"igt_fill_abort"}
{"cmd":"igt_test","action":"hypo_evac","o2_target":15}
{"cmd":"igt_test_stop"}
```

`igt_start` без полей протокола — старт из `ready` после prep. С полями — `igtSessionStartDirect`. `igt_skip` = HTTP `POST /api/igt/buffer/skip-recirculate`. `igt_fill_continue` / `igt_fill_abort` — после таймаута наполнения (`igt.ftp=1` в LIVE).

---

## BLE GATT монолита `ESP32-Tester`

| Роль | UUID |
|---|---|
| Service | `a1b2c3d4-e5f6-7890-abcd-ef1234567890` |
| STATUS (Read + Notify) | `a1b2c3d4-e5f6-7890-abcd-ef1234567891` |
| CMD (Write) | `a1b2c3d4-e5f6-7890-abcd-ef1234567892` |

STATUS — полный JSON как `GET /api/status`. CMD — как `POST /api/cmd`. На сплите main и на ЦУ этот GATT **не поднимается**.

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
| POST | `/api/igt/prep` | Старт подготовки ИГТ |
| POST | `/api/igt/start` | Старт циклов из `ready` |
| POST | `/api/igt/abort` | Прервать + безопасное OFF |
| POST | `/api/igt/ack` | Сброс `done`/`aborted`/`error` в `idle` |
| GET | `/api/igt` | Полный снимок ИГТ (фазы, таймеры) |

Статика UI: `/`, `/app.js`, `/style.css`, `/hrv.js`, `/igt.js` (LittleFS из [`web/`](../web/), прошивка [`firmwares/firmware_main_devkit/`](../firmwares/firmware_main_devkit/)).

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
| `igt` | object | Компактный статус ИГТ (см. ниже) |

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

### `igt` (компактный статус)

| Поле | Тип | Описание |
|---|---|---|
| `state` | string | `idle` \| `check_sensors` \| `prime` \| `primary_fill` \| `recirculate` \| `ready` \| `hypoxia` \| `normoxia` \| `done` \| `aborted` \| `error` |
| `phase` | string | `prep` \| `hypoxia` \| `normoxia` \| `""` |
| `name` | string | имя протокола |
| `cycle`, `cycles` | int | текущий цикл (1-based в практике) и всего |
| `hypoxia_sec`, `normoxia_sec` | int | длительности фаз |
| `phase_remain_sec` | int | обратный отсчёт текущей фазы/шага |
| `o2_target` | float | целевой % O₂ буфера гипоксии |
| `ready_to_start` | bool | можно слать `/api/igt/start` |
| `sensor_warn` | bool | нет обязательных датчиков во время практики (фазы не стопаются) |
| `buffer_prep` | string | `idle` \| `prime` \| `primary_fill` \| `recirculate` \| `ready` — параллельная подготовка буфера гипоксии во время нормоксии (кроме последнего цикла; **без** выкачки, сразу наполнение) |
| `buffer_prep_remain_sec` | int | обратный отсчёт шага `buffer_prep` (наполнение / рециркуляция) |
| `waiting_buffer` | bool | таймер нормоксии истёк, но буфер ещё не готов — переход в гипоксию отложен |
| `practice_started_esp_ms` | int | `millis()` старта циклов |
| `error` | string | только при `error` |
| `last_phase_volume_ok` | bool | есть объём закрытой фазы hypoxia/normoxia |
| `last_phase_kind` | string | `hypoxia` \| `normoxia` |
| `last_phase_cycle` | int | цикл закрытой фазы (не путать с текущим `cycle`) |
| `last_phase_inhale_l`, `last_phase_exhale_l` | float | литры за закрытую фазу |

---

## Команды (`POST /api/cmd`)

```json
{"cmd":"ping"}
{"cmd":"gpio_set","pin":26,"state":1}
{"cmd":"gpio_tap","pin":26,"pulse_ms":500}
{"cmd":"servo_set","pin":4,"angle":90}
{"cmd":"servo_auto","pin":4,"enabled":true,"pause_ms":5000}
{"cmd":"gpio_set","pin":33,"state":1}
{"cmd":"flow_reset"}
{"cmd":"co2_frc","target_ppm":400}
```

Клапаны (NC, логический ON/OFF в API): **V1=25**, **V2=26**, **V3=27**, **V4=13**, **V5=14**, **V6=21**. Полярность линии — `GPIO_VALVE_ACTIVE_LOW` в `platformio.ini`. **O2_MOTOR=33** — только `gpio_tap` (toggle ON/OFF импульсом ~250 ms); `gpio_set` на 33 отклоняется. **Servo1=4** (D4) — только `servo_*`, не `gpio_set`. Логика — [`HARDWARE.md`](HARDWARE.md).

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

## ИГТ (`/api/igt`)

Автомат подготовки буфера гипоксии и интервалов гипоксия/нормоксия живёт на ESP32. Обрыв WiFi **не** останавливает процесс.

`POST /api/igt/prep`:

```json
{"name":"Гипоксия/Нормоксия на каждый день","o2_target":15,"hypoxia_sec":360,"normoxia_sec":300,"cycles":6}
```

Лимиты: O₂ 9–19%, фазы 60–900 с, циклы 1–12. Повторный `prep` разрешён из `idle` / `done` / `aborted` / `error`. Во время активного процесса — `409 busy`.

Порядок подготовки: проверка датчиков (O₂ и давление **буфера гипоксии**; поток рабочего контура **не обязателен**) → V1 + мотор (**выкачка**): переход к наполнению при **любом** из условий — падение P_гипо на `IGT_PRESSURE_DELTA_HPA` **от baseline** датчика гипоксии **или** разность P_раб − P_гипо ≥ `IGT_WORK_PRESSURE_DELTA_HPA` → V4 + мотор (**наполнение**): досрочный переход к рециркуляции при **росте** P_гипо от точки старта fill на `IGT_PRESSURE_DELTA_HPA` **или** при восстановлении к baseline (предохранитель `IGT_PRESSURE_BASELINE_LIMIT_HPA`); иначе таймаут `IGT_PRIMARY_FILL_SEC` → `fill_timeout_pending=true`, клапаны OFF, UI спрашивает «продолжить к рециркуляции?» (`POST /api/igt/buffer/fill-continue` / `fill-abort`) → рециркуляция V1+V4 до `o2_target` → мотор/клапаны OFF, `ready` (**Servo1 ещё не переключается**). **Аварийный стоп** подготовки (prime/fill/recirculate): P_гипо или P_раб ниже **своего** baseline на `IGT_PRESSURE_BASELINE_LIMIT_HPA` (по умолчанию 25 hPa). То же для параллельной подготовки в нормоксии и тест-драйва `hypo_fill`.

`POST /api/igt/start` — два режима:

1. **Без тела** — только из `ready` после подготовки (иначе `not_ready`).
2. **С телом протокола** (как у `prep`) — прямой старт из `idle` / `done` / `aborted` / `error`, **без** подготовки буфера; сразу цикл 1, фаза гипоксия. Занято — `409 busy`.

Циклы: гипоксия (Servo1 180° **в момент входа в фазу**, в том числе `igt_start` из `ready`) → нормоксия (0°). После **последней** нормоксии серва остаётся на 0°, `done`. Подготовка буфера в нормоксии (кроме последнего цикла) идёт параллельно фазе.

В каждой **нормоксии, кроме последнего цикла**, параллельно с отсчётом фазы идёт подготовка буфера для следующей гипоксии: сразу **наполнение** (V4 + мотор, без выкачки) → рециркуляция (V1+V4) до `o2_target`. Если таймер нормоксии истёк раньше готовности буфера — `waiting_buffer=true`, `state` остаётся `normoxia`, гипоксия начинается только когда `buffer_prep=ready`.

`POST /api/igt/buffer/skip-recirculate` — во время **нормоксии** (не последний цикл), если параллельная подготовка буфера на этапе `prime` или `primary_fill`: пропустить выкачку/наполнение и сразу перейти к **рециркуляции** до `o2_target`. Иначе `400 invalid_state`.

`POST /api/igt/buffer/fill-continue` — после таймаута наполнения (`fill_timeout_pending`): перейти к рециркуляции (или завершить `hypo_fill` в тест-драйве). BLE: `{"cmd":"igt_fill_continue"}`.

`POST /api/igt/buffer/fill-abort` — отклонить продолжение после таймаута наполнения → `state=error`. BLE: `{"cmd":"igt_fill_abort"}`.

`POST /api/igt/abort` — все клапаны OFF, мотор OFF (tap при необходимости), Servo1 = 0°. Также останавливает тест-драйв.

`POST /api/igt/test/start` — одиночное действие подготовки (только из `idle`). Тело:

```json
{"action":"hypo_evac"}
```

`action`: `hypo_evac` | `hyper_evac` | `work_flush` | `hypo_fill` | `hypo_recirculate`. Для `hypo_recirculate` опционально `o2_target` (по умолчанию 15). Ответ `409 busy`, если ИГТ не idle.

`POST /api/igt/test/stop` — остановить тест-драйв (эквивалент abort для `state=test`).

В статусе: `state=test`, `test_action`, `phase_remain_sec`.

`POST /api/igt/ack` — сброс терминального состояния в `idle` (иначе `not_terminal`).

`GET /api/igt` — как `status.igt` плюс `fill_remain_sec`, `pressure_start_hpa`, `o2_now`, `pressure_now`, `work_pressure_now`, `phases:[{kind,cycle,start_esp_ms}]`.

При наличии SFM3300 после каждой **завершённой** фазы гипоксии/нормоксии: `last_phase_volume_ok`, `last_phase_kind` (`hypoxia`|`normoxia`), `last_phase_cycle`, `last_phase_inhale_l`, `last_phase_exhale_l` (литры за фазу, сброс объёма в начале фазы; `last_phase_cycle` — цикл закрытой фазы, не текущий).

Пока `state ≠ idle`, ручные `gpio_set` / `gpio_tap` / `servo_*` на V1 (25), V4 (13), O2_MOTOR (33), Servo1 (4) отклоняются.

Конфиг: `IGT_*` в [`firmware_main_devkit/platformio.ini`](../firmwares/firmware_main_devkit/platformio.ini).

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
