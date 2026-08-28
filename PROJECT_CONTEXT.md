### Общее описание проекта



> Карта документации: [`README.md`](README.md) · API [`docs/API.md`](docs/API.md) ·
> железо [`docs/HARDWARE.md`](docs/HARDWARE.md) ·
> E2E [`TESTING.md`](TESTING.md) · прошивки [`firmwares/README.md`](firmwares/README.md) ·
> BLE-узел [`firmwares/firmware_ble_s3/README.md`](firmwares/firmware_ble_s3/README.md) ·
> веб [`web/README.md`](web/README.md) · Android [`android/README.md`](android/README.md)



**Название:** OxyPulse — система мониторинга и управления замкнутым дыхательным контуром



**Цель:**  

Портативное устройство + клиенты для дыхательных и физических тренировок с реал-тайм

мониторингом газового состава, физиологических параметров и управлением газовой смесью.



### Приоритеты на текущем этапе

- **Прошивка ESP32** — [`firmwares/firmware_main_devkit/`](firmwares/firmware_main_devkit/) + [`firmwares/firmware_ble_s3/`](firmwares/firmware_ble_s3/) — основная работа
- **Веб-интерфейс** — [`web/`](web/) — основная работа
- **Android** — [`android/`](android/) — последний приоритет, WiFi HTTP (GATT только на монолите)



Связь с устройством: **HTTP JSON** (WiFi AP или STA главного узла). Внешние пульсоксиметры

(Wellue, COOSPO) подключает **ESP32-S3** как BLE-central — браузер их не видит напрямую.



### Типы тренировок (режимы работы)



**1. Бутейко (Rebreathing) — основной режим**

- Человек дышит в замкнутом контуре (вдыхает свой же выдыхаемый воздух).

- Мониторинг: CO₂, O₂, поток, давление, SpO₂ (Wellue), ЧСС (COOSPO / Wellue).

- При достижении заданных порогов — электромагнитный клапан кратковременно открывается.

- Цель — контролируемое повышение CO₂ и тренировка дыхательной системы.



**2. Интервальная Гипоксическая Тренировка (в будущем)**

- Дополнительные контуры O₂/N₂, управляемые клапаны, датчики в буферах.

- **Справочник по протоколам ИГТ** — внешний проект

  [`YouTube-Podcast-Reviews`](../../YouTube-Podcast-Reviews/PROJECT_CONTEXT.md).



**3. Мониторинг / Силовые тренировки**

- Свободный сбор метрик, уведомления при выходе за безопасные пределы.



### Аппаратная часть



**Главный узел:** ESP32-DEVKIT CH340 ([`firmwares/firmware_main_devkit/`](firmwares/firmware_main_devkit/))

**BLE-узел:** ESP32-S3-N16R8 ([`firmwares/firmware_ble_s3/`](firmwares/firmware_ble_s3/)) — Wellue, COOSPO; UART на главный узел

**Датчики I2C** — две шины на DevKit: гипоксия SDA=22/SCL=23; рабочий SDA=18/SCL=19.

- **ADS1115 + AO-02** — гипоксия 22/23 и рабочий 18/19, `0x48` — O₂ %, напряжение мВ
- **DPS310** — гипоксия 22/23 и рабочий 18/19, `0x76` / `0x77` — давление hPa, °C
- **SFM3300-250-D** — рабочий 18/19, `0x40` — поток, объёмы фаз, VE
- **SCD41** — рабочий 18/19, `0x62` — CO₂ ppm, T, RH

**Внешние BLE** (S3 central, данные на DevKit по UART):

- **Wellue Ring O2 S** — OxyII GATT — SpO₂, HR, батарея, контакт
- **COOSPO H6M** — Heart Rate 0x180D — ЧСС, R-R (если есть)



GPIO34/GPIO35 **не использовать** под I2C (input-only на `esp32dev`).



**Исполнитель:** электромагнитный клапан 12 V + реле GPIO26 (тест).



Подробная распиновка — [`docs/HARDWARE.md`](docs/HARDWARE.md).



### Архитектура системы



**Главный узел (`firmwares/firmware_main_devkit/`):** WiFi AP+STA, HTTP, LittleFS, I2C, GPIO/Servo, тренды, HRV; UART RX.

**BLE-узел (`firmwares/firmware_ble_s3/`):** NimBLE central (Wellue, COOSPO), WiFi выключен, UART TX. GATT нет.

**Монолит (`firmwares/firmware_alone_devkit/`):** одна плата WiFi+BLE — архив для отката.



**Узкое место одной ESP32 (зафиксировано на стенде):**



- WiFi (AP/STA + HTTP) и BLE (central ×2 + peripheral) делят **одно радио** — coexistence.

- Симптомы: `/api/status` ~1–2 с, медленная отдача статики, обрывы COOSPO mid-session,

  HRV на 2+ мин ненадёжен без grace/retry.

- Это **не** нехватка CPU (240 МГц хватает для I2C, JSON, HRV); узкое место — радио и

  нагрузка HTTP-поллинга поверх BLE.



### Эволюция контроллера



**Текущий этап — вариант A.** Монолит в [`firmwares/firmware_alone_devkit/`](firmwares/firmware_alone_devkit/). B/C — только как отдельный контроллер аппарата.

**Общий принцип:**



```

Wellue / COOSPO  --BLE-->  [BLE-узел]  --UART/SPI-->  [Главный узел]  --сеть-->  браузер

I2C, клапаны, поток, HRV-буфер, тренды          ↑

                                                  WiFi или Ethernet

```



BLE-узел не поднимает WiFi. Главный узел не делает BLE central. Coexistence исчезает —

стабильность ремня на 2–5 мин должна вырасти **примерно одинаково** во всех вариантах ниже.

#### A. Две ESP32 (**текущий этап**)

- **BLE-узел:** ESP32-S3-N16R8 — NimBLE central, без WiFi, без GATT
- **Главный узел:** ESP32-DEVKIT CH340 — WiFi + HTTP + I2C + HRV
- **Мост:** UART 921600, прямая коммутация **GPIO16↔16**, **GPIO17↔17**
- **Когда:** рабочая схема стенда

#### B. ESP + RP2350

- **BLE-узел:** ESP — только BLE
- **Главный узел:** RP2350 — I2C, HRV, HTTP
- **Выигрыш:** то же по BLE; CPU с запасом, PIO
- **Сложность:** средняя
- **Когда:** «мозг» контура без промышленного Ethernet

#### C. ESP + STM32H723

- **BLE-узел:** ESP — только BLE
- **Главный узел:** H723 — I2C, HRV, Ethernet
- **Выигрыш:** то же по BLE; максимум CPU + стабильная сеть
- **Сложность:** высокая
- **Когда:** промышленный контроллер, жёсткий realtime, RMII PHY

#### D. Одна ESP32 (архив `firmware_alone_devkit`)

- **Всё на одном чипе:** WiFi + BLE + I2C + HTTP
- **Когда:** откат, если сплит на столе не собран



**Выводы (зафиксировать):**



1. **Смена MCU (H723 / RP2350) сама по себе не ускорит веб и не починит BLE**, если WiFi

   остаётся на том же чипе, что и ремень. У H723/RP2350 нет встроенного WiFi+BLE как у ESP32.

2. **Две ESP32 ≈ тот же архитектурный сплит**, что ESP + H723/RP2350, но с **~20% работы**

   вместо переписывания всего стека (PlatformIO, NimBLE, WebServer, `config.h`).

3. **H723** — не замена «медленной ESP», а запас на **тяжёлый контур** (кГц-управление,

   много фильтров, Ethernet без WiFi-глюков). Дороже, Cube/HAL, дольше bring-up.

4. **RP2350** — компромисс между двумя ESP и H723: дешевле H7, два ядра M33, удобен для I2C/HRV;

   веб через **Pico 2 W** (WiFi без BLE на CYW43439) или **W5500** (Ethernet по SPI).

5. **Порядок решений:** **A (две ESP32) сделан** → B/C только если появится задача «контроллер аппарата».



**Мост ESP ↔ главный узел (для A/B/C):**



- Физика: UART 921600 или SPI; общая GND; при необходимости flow-control.

- Протокол: компактный поток (бинарный или NDJSON), не полный `/api/status`.

- Минимум с BLE-ESP: `wellue`/`coospo` snapshot 1 Гц + **все R-R** (не 1 Гц тренд).

- Главный узел собирает полный JSON для `/api/status`, считает HRV, хранит тренды.



**Сеть для веб-UI на главном узле:**

- **ESP-B (вариант A)** — WiFi AP+STA. Уже отлажено в текущем проекте.
- **RP2350 (вариант B)** — Pico 2 W (WiFi) или W5500 (Ethernet по SPI). WiFi на Pico слабее ESP-IDF, но BLE на CYW43439 не используется.
- **H723 (вариант C)** — Ethernet RMII + PHY. Самый стабильный канал для постоянного UI в LAN.



**Что не переносить на BLE-ESP:** I2C-датчики, LittleFS/веб, `hrv_session`, `trend_buffer`,

`wifi_server` — только `ble_sensors` (+ опционально GATT peripheral для Android).



**Веб (`web/`):**



- Vanilla JS, поллинг `/api/status` 1 Гц

- Плитки датчиков, действия (HRV, КП Бутейко в модалке), тренды (~75 мин RAM, окна 2/5/10/60/75 мин, PNG текущего окна), WiFi-настройка, GPIO/Servo

- После правок — `pio run -t uploadfs` из `firmwares/firmware_main_devkit/`



**Android (`android/`):**



- Compose, WiFi HTTP (GATT — только `firmware_alone_devkit`)

- GPIO/Servo диалоги, Wellue/COOSPO из JSON

- I2C-метрики в UI пока не вынесены — см. веб



### Текущий статус



**Сделано:**



- Прошивка: сплит `firmwares/firmware_main_devkit` + `firmwares/firmware_ble_s3` (UART NDJSON)

- BLE central на S3: Wellue Ring O2 S, COOSPO H6M

- Веб-UI: датчики, действия (HRV, КП Бутейко в модалке), тренды (~75 мин RAM, PNG окна), WiFi, GPIO/Servo

- Android: базовое подключение WiFi/BLE, GPIO/Servo, Wellue/COOSPO

- Документация: `docs/API.md`, README прошивки/веба



**Ближайшие задачи:**



- E2E на двух платах ([`TESTING.md`](TESTING.md)): UART `bridge.ok`, I2C, Wellue/COOSPO/HRV

- Калибровка O₂ (`O2_HYPOXIA_*` / `O2_WORKING_*` в `firmwares/firmware_main_devkit/src/config.h`)

- Логика автоматического клапана по порогам CO₂ / SpO₂ / ЧСС



**Дальше:**



- Запись сессий, Room (Android), облако

- HRV: персональная шкала stress/normal (градиент по своей базе rMSSD) — см. ниже

- Интервальная гипоксическая тренировка

- Интеграция Garmin / доп. BLE-датчиков



### HRV — шкала stress / normal (идея на будущее)



Сейчас модалка HRV показывает **сырые метрики** (SDNN, rMSSD, pNN50, SD1/SD2) и Poincaré — **без** вердикта «стресс / норма» и цветового градиента. В потребительских приложениях (Wellue, Elite HRV и др.) часто есть шкала **normal → stress** — это упрощённая интерпретация, обычно по **rMSSD** или **SDNN** (ниже порога ≈ «stress», выше ≈ «recovery»).



**Почему не делать сразу универсальный градиент:**

- Нормы сильно зависят от возраста, фитнеса, положения, дыхания, длительности замера.
- Короткий замер (30 с–5 мин) с нагрудного ремня — **скрининг**, не клинический HRV-тест.
- SD1/SD2 — геометрия Poincaré (краткосрочная / долгосрочная вариабельность), не отдельная «шкала стресса».

**Предлагаемый подход для OxyPulse:**

1. **Фаза 1 (сейчас):** только числа + график, без цветовых зон.
2. **Фаза 2:** накопление **личной базы** — median rMSSD (и опционально SDNN) по 10+ замерам в одинаковых условиях (покой, положение, время суток).
3. **Фаза 3:** градиент **относительно своей базы** (не «норма населения»):
   - зелёная зона: rMSSD в пределах median ±20%;
   - жёлтая: ниже базы, но не критично;
   - красная: заметно ниже базы.
4. Подпись в UI: «относительно **ваших** замеров», не медицинский диагноз.
5. Хранение: `localStorage` в браузере или история сессий на ESP/облаке (когда появится запись сессий).

**Основная метрика для шкалы:** rMSSD (SD1 ≈ rMSSD/√2). SD2 и SDNN — дополнительный контекст на экране результата, не для одного цветного «stress score».



### Калибровки (`firmwares/firmware_main_devkit/src/config.h`)



**O₂ (AO-02):** `O2% = (мВ − OFFSET) / (AIR_MV − OFFSET) × O2_AIR_PERCENT` — отдельные `O2_HYPOXIA_*` и `O2_WORKING_*`



**Поток (SFM3300):** `flow_slm = (raw − SFM_FLOW_OFFSET) / SFM_FLOW_SCALE`  

Scale из датчика или fallback 120 (воздух) / 142.8 (O₂).



**CO₂ (SCD41):** прогрев ~10 с; датчик обновляется раз в 5 с.



Подробный порядок — [`firmwares/firmware_main_devkit/README.md`](firmwares/firmware_main_devkit/README.md).



### Управление клапаном (текущее)



Через HTTP/BLE cmd: `gpio_set` / `gpio_tap` на GPIO **26** (whitelist).  

Автоматика по порогам — в планах.



### API (кратко)

- `GET /api/status` — JSON: sensors, gpio, wifi, trends
- `POST /api/cmd` — gpio_set, gpio_tap, servo_*, flow_reset
- `GET/POST /api/trends*` — история и маска каналов
- `GET/POST /api/hrv*` — замер HRV по R-R с ремня (2/3/5 мин + 30 с отладка)



Полная спецификация — [`docs/API.md`](docs/API.md).

