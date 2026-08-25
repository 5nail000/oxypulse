# OxyPulse — Web UI

Vanilla HTML/JS — **основной клиент** прошивки [`firmware/`](../firmware/).

Копия для LittleFS: [`firmware/data/`](../firmware/data/).

> **Карта документации:** API [`docs/API.md`](../docs/API.md) ·
> **железо [`docs/HARDWARE.md`](../docs/HARDWARE.md)** ·
> прошивка [`firmware/README.md`](../firmware/README.md) ·
> Android [`android/README.md`](../android/README.md) (низкий приоритет) ·
> обзор [`README.md`](../README.md)

## Использование

1. Подключитесь к WiFi AP `ESP32-Tester` (пароль `12345678`) **или** к домашней сети по IP ESP32 (если STA уже настроена)
2. Откройте `http://192.168.4.1/` или IP устройства в LAN

Кнопка **WiFi** в блоке «Подключение»: SSID можно **ввести вручную** или выбрать после скана. Скан асинхронный (`POST /api/wifi/scan` → опрос `GET /api/wifi/scan`); на 5–10 с AP может кратко пропасть — страницу не закрывайте. Сохранение в NVS и перезагрузка. После успешного STA ESP32 доступен по IP в общей сети; AP остаётся для повторной настройки.

Секция **Датчики** показывает I2C (O₂, поток, DPS310, SCD41) и BLE:

- **Wellue Ring O2 S** — SpO₂, ЧСС, батарея, контакт, motion (ESP32 подключается сам)
- **Нагрудный HR** — любой ремень с Heart Rate Profile; имя с BLE, ЧСС и R-R (если ремень отдаёт RR-Interval)

Кольцо лучше будить в режим `S8-AW`; в записи `T8520_xxxx` OxyII-сервис часто недоступен.

Секция **Тренды**: на плитке датчика кнопка **Тренд**; у нагрудного HR отдельно **ЧСС** и **R-R**. Набор сохраняется на ESP32 (`POST /api/trends/sources`). Если ничего не включено — графики пустые. История — RAM ~30 мин, UI забирает её страницами (`GET /api/trends?offset=&limit=`), чтобы вторая вкладка/телефон не роняла статус. Если включены и Wellue, и ЧСС ремня, на графике ЧСС появляются чекбоксы отображения (только в браузере, `localStorage`). Окна 2 / 5 / 10 / 60 мин.

## Прошивка UI на ESP32

```bash
cd firmware
copy ..\web\index.html, ..\web\app.js, ..\web\style.css data\
pio run -t uploadfs
pio run -t upload
```

## Распиновка и проводка

Полный справочник — **[`docs/HARDWARE.md`](../docs/HARDWARE.md)** (I²C, AO-02, SFM3300, DPS310, STEMMA QT, GPIO, BLE, питание).

Кратко:

- I²C: **SDA=GPIO22**, **SCL=GPIO23**, 100 kHz, pull-up 4.7 kΩ к 3.3 V
- Клапан (тест): реле на **GPIO26**, HIGH = открыт
- Servo (тест): PWM на GPIO18, питание 5 V отдельно
- GPIO **34/35** — не для I²C; **22/23** — не трогать как GPIO

## Занятые / не использовать для I2C

- **GPIO 34 / 35** — input-only на `esp32dev`, SDA/SCL на них нельзя.
- **GPIO 2 / 15** — strapping pins, для I2C не рекомендуются.
