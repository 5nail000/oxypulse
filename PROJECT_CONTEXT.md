### Общее описание проекта



> Карта документации: [`README.md`](README.md) · API [`docs/API.md`](docs/API.md) ·
> железо [`docs/HARDWARE.md`](docs/HARDWARE.md) ·
> E2E [`TESTING.md`](TESTING.md) · прошивка [`firmware/README.md`](firmware/README.md) ·
> веб [`web/README.md`](web/README.md) · Android [`android/README.md`](android/README.md)



**Название:** OxyPulse — система мониторинга и управления замкнутым дыхательным контуром



**Цель:**  

Портативное устройство + клиенты для дыхательных и физических тренировок с реал-тайм

мониторингом газового состава, физиологических параметров и управлением газовой смесью.



### Приоритеты на текущем этапе



| Компонент | Путь | Статус |

|---|---|---|

| **Прошивка ESP32** | `firmware/` | основная работа |

| **Веб-интерфейс** | `web/` (+ `firmware/data/`) | основная работа |

| **Android** | `android/` | последний приоритет, базовый WiFi/BLE-клиент |



Связь с устройством: **HTTP JSON** (WiFi AP или STA). BLE GATT дублирует тот же JSON

для мобильного клиента. Внешние пульсоксиметры (Wellue, COOSPO) подключает **сам ESP32**

как BLE-central — браузер их не видит напрямую.



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



**Центральный контроллер:** ESP32 DevKit V1 (Type-C)



**Датчики I2C** (SDA=GPIO22 / SCL=GPIO23, 100 kHz):



| Датчик | Адрес | Что даёт |

|---|---|---|

| ADS1115 + AO-02 | `0x48` | O₂ %, напряжение мВ |

| SFM3300-250-D | `0x40` | поток, объёмы фаз, VE |

| DPS310 | `0x76` / `0x77` | давление hPa, °C |

| SCD41 | `0x62` | CO₂ ppm, T, RH |



**Внешние BLE** (ESP32 central):



| Устройство | Протокол | Данные |

|---|---|---|

| Wellue Ring O2 S | OxyII GATT | SpO₂, HR, батарея, контакт |

| COOSPO H6M | Heart Rate 0x180D | ЧСС, R-R (если есть) |



GPIO34/GPIO35 **не использовать** под I2C (input-only на `esp32dev`).



**Исполнитель:** электромагнитный клапан 12 V + реле GPIO26 (тест).



Подробная распиновка — [`docs/HARDWARE.md`](docs/HARDWARE.md).



### Архитектура системы



**ESP32 (`firmware/`):**



- WiFi AP+STA, HTTP-сервер, LittleFS с веб-UI

- NimBLE: peripheral (телефон/клиент) + central (Wellue, COOSPO)

- I2C-драйверы, GPIO/Servo whitelist, кольцо трендов ~30 мин в RAM

- JSON status/cmd — см. [`docs/API.md`](docs/API.md)



**Веб (`web/`):**



- Vanilla JS, поллинг `/api/status` 1 Гц

- Плитки датчиков, тренды, WiFi-настройка, GPIO/Servo

- После правок — `pio run -t uploadfs` из `firmware/`



**Android (`android/`):**



- Compose, WiFi HTTP + BLE GATT

- GPIO/Servo диалоги, Wellue/COOSPO из JSON

- I2C-метрики в UI пока не вынесены — см. веб



### Текущий статус



**Сделано:**



- Прошивка: WiFi AP/STA, HTTP API, BLE dual-role, I2C (O₂, поток, давление, CO₂)

- BLE central: Wellue Ring O2 S, COOSPO H6M

- Веб-UI: датчики, тренды, WiFi, GPIO/Servo

- Android: базовое подключение WiFi/BLE, GPIO/Servo, Wellue/COOSPO

- Документация: `docs/API.md`, README прошивки/веба



**Ближайшие задачи:**



- E2E на стенде ([`TESTING.md`](TESTING.md)): I2C-скан, датчики, Wellue/COOSPO

- Калибровка O₂ (`O2_OFFSET_MV` / `O2_AIR_MV` в `config.h`)

- Логика автоматического клапана по порогам CO₂ / SpO₂ / ЧСС

- Стабильность WiFi+BLE (modem sleep, порядок init)



**Дальше:**



- Запись сессий, Room (Android), облако

- Интервальная гипоксическая тренировка

- Интеграция Garmin / доп. BLE-датчиков



### Калибровки (`firmware/src/config.h`)



**O₂ (AO-02):** `O2% = (мВ − O2_OFFSET_MV) / (O2_AIR_MV − O2_OFFSET_MV) × O2_AIR_PERCENT`



**Поток (SFM3300):** `flow_slm = (raw − SFM_FLOW_OFFSET) / SFM_FLOW_SCALE`  

Scale из датчика или fallback 120 (воздух) / 142.8 (O₂).



**CO₂ (SCD41):** прогрев ~10 с; датчик обновляется раз в 5 с.



Подробный порядок — [`firmware/README.md`](firmware/README.md).



### Управление клапаном (текущее)



Через HTTP/BLE cmd: `gpio_set` / `gpio_tap` на GPIO **26** (whitelist).  

Автоматика по порогам — в планах.



### API (кратко)



| Endpoint | Назначение |

|---|---|

| `GET /api/status` | JSON: sensors, gpio, wifi, trends |

| `POST /api/cmd` | gpio_set, gpio_tap, servo_*, flow_reset |

| `GET/POST /api/trends*` | История и маска каналов |



Полная спецификация — [`docs/API.md`](docs/API.md).

