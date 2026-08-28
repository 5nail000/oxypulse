# OxyPulse — Android

Compose-приложение для HTTP API главного узла [`firmware_main_devkit/`](../firmwares/firmware_main_devkit/).

> **Приоритет на текущем этапе — последний.** Основная работа в
> [`firmware_main_devkit/`](../firmwares/firmware_main_devkit/) + [`firmware_ble_s3/`](../firmwares/firmware_ble_s3/) (или [`firmware_ble_devkit/`](../firmwares/firmware_ble_devkit/)) + [`web/`](../web/).
> На сплите используйте **WiFi HTTP**. BLE GATT есть только у монолита [`firmware_alone_devkit/`](../firmwares/firmware_alone_devkit/).

API — [`docs/API.md`](../docs/API.md).

## Возможности

- **WiFi**: HTTP к AP `192.168.4.1` или STA IP ESP32
- **BLE**: GATT `ESP32-Tester` — **только** [`firmware_alone_devkit/`](../firmwares/firmware_alone_devkit/); на сплите — WiFi
- Диалоги **GPIO** (ON/OFF, tap 3 с) и **Servo** (угол, авто 0↔180)
- **Wellue Ring O2 S** и **COOSPO H6M** из JSON (ESP32 подключается сам)

## Сборка

1. Android SDK (API 35), `local.properties`:

```properties
sdk.dir=C\:\\Users\\YourUser\\AppData\\Local\\Android\\Sdk
```

2. APK:

```bash
cd android
gradlew.bat assembleDebug
```

APK: `app/build/outputs/apk/debug/app-debug.apk`  
minSdk 28, BLE нужен для BLE-режима.

## Экраны

| Экран | Назначение |
|---|---|
| **ConnectScreen** | WiFi (IP) / BLE (скан `ESP32-Tester`) |
| **TesterScreen** | Uptime, GPIO/Servo, Wellue/COOSPO, отключение |

## Архитектура

```
TesterViewModel
    ├── Esp32Repository (HTTP)
    └── BleEsp32Repository (GATT)
            └── StatusParser → DeviceStatus
```

| Компонент | Роль |
|---|---|
| `api/HttpEsp32Api.kt` | GET `/api/status`, POST `/api/cmd` |
| `ble/BleEsp32Repository.kt` | scan, connect, notify |
| `api/Protocol.kt` | UUID, `CommandBuilder`, whitelist GPIO |
| `ui/Screens.kt`, `ui/Dialogs.kt` | Compose UI |

Пакет Kotlin: `com.oxypulse.esp32tester` (историческое имя, переименование — позже).

## BLE UUID

| Роль | UUID |
|---|---|
| Service | `a1b2c3d4-e5f6-7890-abcd-ef1234567890` |
| STATUS | `a1b2c3d4-e5f6-7890-abcd-ef1234567891` |
| CMD | `a1b2c3d4-e5f6-7890-abcd-ef1234567892` |

Тест клапана: GPIO **26** (не 22/23 — I2C).

## Стек

Kotlin, Jetpack Compose, Hilt, blessed-kotlin, OkHttp.

## Документация

| Документ | Содержание |
|---|---|
| [`docs/API.md`](../docs/API.md) | JSON, endpoints |
| [`firmware_main_devkit/README.md`](../firmwares/firmware_main_devkit/README.md) | Главный узел, калибровки |
| [`docs/HARDWARE.md`](../docs/HARDWARE.md) | Распиновка, датчики, питание |
| [`web/README.md`](../web/README.md) | Веб-UI, тренды |
