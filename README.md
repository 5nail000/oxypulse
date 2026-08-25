# OxyPulse

Портативная система мониторинга и управления замкнутым дыхательным контуром:
ESP32 + датчики на I2C + веб-интерфейс (основной клиент).

**На текущем этапе:** основная работа в **`firmware/`** + **`web/`**.  
Android-клиент (`android/`) — **в последнем приоритете**, дублирует часть возможностей веб-UI.

## Структура репозитория

| Путь | Назначение | Приоритет |
|---|---|---|
| [`firmware/`](firmware/) | Прошивка ESP32 (PlatformIO): WiFi AP/STA, HTTP API, BLE peripheral + central | **основной** |
| [`web/`](web/) | Веб-UI (Vanilla JS); копия для LittleFS — `firmware/data/` | **основной** |
| [`docs/API.md`](docs/API.md) | HTTP/BLE API, JSON status, команды | |
| [`docs/HARDWARE.md`](docs/HARDWARE.md) | Распиновка, I²C, датчики, BLE, питание | |
| [`web/README.md`](web/README.md) | Веб-UI, тренды, WiFi-настройка | |
| [`android/`](android/) | Android-клиент (WiFi HTTP / BLE) | низкий |
| [`PROJECT_CONTEXT.md`](PROJECT_CONTEXT.md) | Цели, архитектура, дорожная карта | |
| [`TESTING.md`](TESTING.md) | Проверка на железе (firmware + web) | |

## Быстрый старт

### Прошивка + веб-UI

```bash
cd firmware
pio run -t uploadfs   # web/ → LittleFS
pio run -t upload
pio device monitor
```

Подключиться к AP **`ESP32-Tester`** (пароль `12345678`) → **`http://192.168.4.1/`**  
или по IP в LAN, если настроен WiFi STA (кнопка **WiFi** в интерфейсе).

Подробнее — [`firmware/README.md`](firmware/README.md), [`web/README.md`](web/README.md).

### Android (опционально)

```bash
cd android
gradlew.bat assembleDebug
```

APK: `android/app/build/outputs/apk/debug/app-debug.apk`. См. [`android/README.md`](android/README.md).

## Железо (кратко)

- **Контроллер:** ESP32 DevKit V1
- **I2C:** SDA=GPIO22, SCL=GPIO23, 100 kHz, подтяжки 4.7 kΩ к 3.3 V
- **Клапан (тест):** реле GPIO26, активный HIGH
- **I2C-датчики:** ADS1115+AO-02 (`0x48`), SFM3300 (`0x40`), DPS310 (`0x76`/`0x77`), SCD41 (`0x62`)
- **Внешние BLE (ESP32 central):** Wellue Ring O2 S, COOSPO H6M

GPIO34/35 на `esp32dev` — input-only, под I2C не использовать.

## API

JSON over HTTP (`/api/status`, `/api/cmd`) и дублирующий BLE GATT.  
Полная спецификация — [`docs/API.md`](docs/API.md).

## Ближайшие шаги

1. Стабильность WiFi + BLE dual-role, тренды, датчики на стенде
2. Калибровка O₂ (`O2_OFFSET_MV` / `O2_AIR_MV` в `firmware/src/config.h`)
3. Логика клапана по порогам CO₂ / SpO₂ / ЧСС
4. Android — по мере необходимости, после firmware + web
