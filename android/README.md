# OxyPulse — Android

Полевой клиент: BLE к ЦУ [`firmware_valve_devkit/`](../firmwares/firmware_valve_devkit/) плюс Wellue/COOSPO на телефоне.

Стендовый откат: WiFi HTTP к [`firmware_main_devkit/`](../firmwares/firmware_main_devkit/) (как веб [`web/`](../web/)). Веб — клиент сплита main, не полевой UI.

Протокол ЦУ — [`docs/API.md`](../docs/API.md) (имя `OxyPulse-CU`, UUID `c0a10000-…`).

## Возможности

- **ЦУ (основное):** GATT LIVE/CMD, GPIO/Servo/ИГТ, keep-alive; кольцо и ремень на телефоне
- **Стенд WiFi:** HTTP к AP `192.168.4.1` или STA IP; Wellue/COOSPO приходят из `/api/status` (S3)
- Датчики I²C с ЦУ, GPIO / Servo / мотор O₂, сброс потока, FRC CO₂
- Тренды: на ЦУ — локальное кольцо 1 Гц; на WiFi — ещё и `GET /api/trends`
- **HRV:** на ЦУ — R-R с ремня на телефоне; на WiFi — `/api/hrv` на ESP
- КП, практика Бутейко, **ИГТ** (prep/start/abort; обрыв GATT **не** abort на ESP, reconnect + запись в отчёт)

## Сборка

1. Android SDK (API 35), `local.properties`:

```properties
sdk.dir=C\:\\Users\\YourUser\\AppData\\Local\\Android\\Sdk
```

2. APK:

```bash
cd android
gradlew.bat assembleDebug
gradlew.bat installDebug

adb logcat -s OxyPulse #Для просмотра логов приложения
```

APK: `app/build/outputs/apk/debug/app-debug.apk`  
minSdk 28, targetSdk 35.

Нужны разрешения BLE (и уведомления для foreground service). На ЦУ общая WiFi-сеть с ESP не нужна.

## Экраны

| Экран | Назначение |
|---|---|
| **ConnectScreen** | Вкладка «ЦУ» (`OxyPulse-CU`); «Стенд WiFi» — откат на main |
| **Dashboard** | Статус, GPIO/Servo, датчики, действия, тренды |
| **HRV / КП / практика / ИГТ** | Как в веб-UI; WiFi STA — только на HTTP |

## Архитектура

```
TesterViewModel
    └── Esp32Repository
            ├── HttpEsp32Api          (стенд main)
            ├── CuBleClient           (OxyPulse-CU)
            └── WearableBleManager    (Wellue OxyII + COOSPO 0x180D)
                    └── WearableBleService (foreground)
```

Пакет Kotlin: `com.oxypulse.esp32tester` (историческое имя).

## Стек

Kotlin, Jetpack Compose, Hilt, blessed-kotlin, OkHttp.

## Документация

| Документ | Содержание |
|---|---|
| [`docs/API.md`](../docs/API.md) | HTTP и GATT ЦУ |
| [`firmware_valve_devkit/README.md`](../firmwares/firmware_valve_devkit/README.md) | ЦУ |
| [`firmware_main_devkit/README.md`](../firmwares/firmware_main_devkit/README.md) | Стенд |
| [`docs/HARDWARE.md`](../docs/HARDWARE.md) | Распиновка |
| [`TESTING.md`](../TESTING.md) | Чеклист мэтча ролей |
