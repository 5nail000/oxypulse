#pragma once

#include <cstddef>
#include <cstdint>

// WiFi AP (всегда для первичной настройки)
constexpr const char *WIFI_AP_SSID = "ESP32-Tester";
constexpr const char *WIFI_AP_PASS = "12345678";
constexpr uint8_t WIFI_AP_CHANNEL = 1;

// WiFi STA (домашняя сеть, credentials в NVS)
constexpr const char *WIFI_NVS_NAMESPACE = "wifi_cfg";
constexpr const char *WIFI_NVS_KEY_SSID = "ssid";
constexpr const char *WIFI_NVS_KEY_PASS = "pass";
constexpr size_t WIFI_STA_SSID_MAX = 32;
constexpr size_t WIFI_STA_PASS_MAX = 64;
constexpr size_t WIFI_STA_ERROR_MAX = 48;
constexpr uint32_t WIFI_STA_CONNECT_TIMEOUT_MS = 15000;
constexpr size_t WIFI_SCAN_JSON_MAX = 2048;
constexpr size_t WIFI_CMD_JSON_MAX = 384;
constexpr size_t WIFI_SCAN_MAX_NETWORKS = 24;
constexpr uint32_t WIFI_SCAN_TIMEOUT_MS = 12000;

// Servo PWM
constexpr uint32_t SERVO_FREQ_HZ = 50;
constexpr uint8_t SERVO_RESOLUTION_BITS = 16;
constexpr uint32_t SERVO_PERIOD_US = 20000;
constexpr uint16_t PULSE_US_MIN = 500;
constexpr uint16_t PULSE_US_MAX = 2500;
constexpr uint8_t SERVO_ANGLE_MAX = 180;

constexpr uint32_t SERVO_AUTO_PAUSE_MS = 5000;
constexpr size_t MAX_SERVOS = 4;
constexpr size_t MAX_GPIO_TRACKED = 16;

constexpr uint32_t GPIO_TAP_MS = 3000;
constexpr uint32_t GPIO_TAP_MAX_MS = 10000;
constexpr uint32_t O2_MOTOR_TAP_MS = 250;

// Исполнители — docs/HARDWARE.md. GPIO 4 только PWM (Servo1), не gpio_set.
constexpr uint8_t SERVO1_PIN = 4;
constexpr uint8_t VALVE_IN_HYPO_PIN = 25;
constexpr uint8_t VALVE_IN_HYPER_PIN = 26;
constexpr uint8_t VALVE_IN_WORK_PIN = 27;
constexpr uint8_t VALVE_OUT_DEP_HYPO_PIN = 13;
constexpr uint8_t VALVE_OUT_DEP_HYPER_PIN = 14;
constexpr uint8_t VALVE_OUT_RICH_HYPER_PIN = 21;
constexpr uint8_t O2_MOTOR_PIN = 33;

// Реле клапанов V1–V6: platformio.ini → GPIO_VALVE_ACTIVE_LOW (1 = LOW→ON).
#ifndef GPIO_VALVE_ACTIVE_LOW
#define GPIO_VALVE_ACTIVE_LOW 0
#endif
constexpr bool GPIO_VALVE_ACTIVE_LOW_B = GPIO_VALVE_ACTIVE_LOW != 0;

// Разрешённые выходные GPIO (ESP32 DevKit). 16/17 — UART BLE; 32 — OCS TX.
static constexpr uint8_t GPIO_WHITELIST[] = {
    4, 13, 14, 18, 19, 21, 22, 23, 25, 26, 27, 33,
};
constexpr size_t GPIO_WHITELIST_SIZE = sizeof(GPIO_WHITELIST) / sizeof(GPIO_WHITELIST[0]);

constexpr size_t STATUS_JSON_MAX = 7680;
constexpr size_t CMD_JSON_MAX = 256;
constexpr size_t TREND_SRC_JSON_MAX = 384;

// Тренды в RAM (1 Гц, кольцо ~75 мин). Буфер malloc, не BSS: 90 КБ не влезают в dram0 ESP32.
constexpr size_t TREND_CAPACITY = 4500;
constexpr uint32_t TREND_INTERVAL_MS = 1000;
constexpr int16_t TREND_MISSING = -32768;
constexpr size_t TREND_JSON_CHUNK = 512;
constexpr size_t TREND_JSON_PAGE_DEFAULT = 240;
constexpr size_t TREND_JSON_PAGE_MAX = 300;
constexpr uint32_t TREND_LOG_EVERY_SAMPLES = 60;

constexpr const char *TREND_NVS_NAMESPACE = "trend_src";
constexpr const char *TREND_NVS_KEY_MASK = "mask";
constexpr uint32_t TREND_SRC_O2 = 1U << 0;
constexpr uint32_t TREND_SRC_FLOW = 1U << 1;
constexpr uint32_t TREND_SRC_PRESSURE = 1U << 2;
constexpr uint32_t TREND_SRC_CO2 = 1U << 3;
constexpr uint32_t TREND_SRC_WELLUE = 1U << 4;
constexpr uint32_t TREND_SRC_HR = 1U << 5;
constexpr uint32_t TREND_SRC_RR = 1U << 6;
constexpr uint32_t TREND_SRC_WORK_O2 = 1U << 7;
constexpr uint32_t TREND_SRC_WORK_PRESSURE = 1U << 8;

// I2C: буфер гипоксии (уже разведён на стенде)
constexpr int I2C_SDA_PIN = 22;
constexpr int I2C_SCL_PIN = 23;
// Рабочий контур: SDA=18 / SCL=19.
constexpr int I2C_WORKING_SDA_PIN = 18;
constexpr int I2C_WORKING_SCL_PIN = 19;
constexpr uint32_t I2C_CLOCK_HZ = 100000;
constexpr uint32_t I2C_TIMEOUT_MS = 200;

constexpr uint8_t ADDR_ADS1115 = 0x48;
constexpr uint8_t ADDR_SFM3300 = 0x40;
constexpr uint8_t ADDR_DPS310 = 0x76;  // SDO→GND; 0x77 если SDO→VCC
constexpr uint8_t ADDR_DPS310_ALT = 0x77;
// Рабочий I2C: BMP390 (CJMCU-390) те же 0x76/0x77. Флаг — в platformio.ini.
#ifndef WORKING_PRESSURE_BMP390
#define WORKING_PRESSURE_BMP390 0
#endif
#ifndef WORKING_PRESSURE_ENABLED
#define WORKING_PRESSURE_ENABLED 1
#endif
constexpr uint8_t ADDR_STC31 = 0x29;  // STC31-C ADDR→GND/NC
constexpr uint8_t ADDR_SHT4X = 0x44;  // SHT4x, влажность рядом с STC31

// AO-02 калибровка — только буфер гипоксии (рабочий O₂ = OCS-3RL UART)
constexpr float O2_HYPOXIA_OFFSET_MV = 0.19f;
constexpr float O2_HYPOXIA_AIR_MV = 10.85f;
constexpr float O2_AIR_PERCENT = 20.9f;

// OCS-3RL2.0 (рабочий O₂, UART 9600 TTL 3.3 V)
constexpr int OCS_UART_RX_PIN = 34;  // input-only ← TX датчика
constexpr int OCS_UART_TX_PIN = 32;  // → RX датчика
constexpr uint32_t OCS_UART_BAUD = 9600;
constexpr uint32_t OCS_POLL_INTERVAL_MS = 50;
constexpr uint32_t OCS_QUERY_INTERVAL_MS = 1000;
constexpr uint32_t OCS_STALE_MS = 3000;
// Поправка O₂ OCS по CO₂ STC31: лишний vol% CO₂ над фоном → ~5 % «лишнего» O₂ на 1 % CO₂.
#ifndef OCS_O2_CO2_PER_VOL
#define OCS_O2_CO2_PER_VOL 5.0f
#endif
#ifndef OCS_O2_CO2_AMBIENT_VOL
#define OCS_O2_CO2_AMBIENT_VOL 0.04f
#endif
// Раскладка короткого кадра 16 09 01 | u16_a | u16_b | u16_c | ST | CS
// 0 = классика OCS-3F: a=O2, b=flow/aux, c=TEMP
// 1 = OCS-3RL2.0: выдох греет поле a → a=TEMP; O₂ ищем в b/c (полоса ~воздух)
#ifndef OCS_FRAME_LAYOUT
#define OCS_FRAME_LAYOUT 1
#endif
// Явный индекс O₂ в short-кадре (0=a,1=b,2=c). -1 = авто (layout).
#ifndef OCS_O2_FIELD
#define OCS_O2_FIELD -1
#endif

// SFM3300
constexpr float SFM_FLOW_OFFSET = 32768.0f;
constexpr float SFM_FLOW_SCALE = 120.0f;
constexpr uint32_t SFM_POLL_INTERVAL_MS = 10;
constexpr float FLOW_DEADBAND_SLM = 0.5f;
constexpr float FLOW_PHASE_THRESHOLD_SLM = 2.0f;
constexpr uint32_t FLOW_VE_WINDOW_MS = 30000;
constexpr uint32_t FLOW_VE_BUCKET_MS = 100;
constexpr size_t FLOW_VE_BUCKETS = FLOW_VE_WINDOW_MS / FLOW_VE_BUCKET_MS;

constexpr uint32_t O2_POLL_INTERVAL_MS = 50;
constexpr uint32_t DPS_POLL_INTERVAL_MS = 100;

// STC31-C (CO₂ рабочий контур, vol%, ~1 Гц)
constexpr uint32_t STC31_POLL_INTERVAL_MS = 1000;
constexpr uint32_t STC31_WARMUP_MS = 2000;
#ifndef STC31_ASC_ENABLED
#define STC31_ASC_ENABLED 0
#endif
// Совместимость трендов: ppm_raw дублирует ppm (динамической компенсации нет).
#ifndef SCD41_CO2_DYNAMIC_COMP
#define SCD41_CO2_DYNAMIC_COMP 0
#endif

// HRV-сессия: все R-R с ремня (не 1 Гц тренд)
constexpr size_t HRV_RR_MAX = 600;
constexpr uint16_t HRV_RR_MIN_MS = 250;
constexpr uint16_t HRV_RR_MAX_MS = 2000;
constexpr uint32_t HRV_DURATION_MIN_SEC = 30;
constexpr uint32_t HRV_DURATION_MAX_SEC = 300;
constexpr uint32_t HRV_BLE_GRACE_MS = 10000;
constexpr size_t HRV_CMD_JSON_MAX = 128;
constexpr size_t HRV_ERROR_MAX = 48;
constexpr size_t HRV_RR_PER_NOTIFY_MAX = 16;
constexpr size_t HRV_JSON_CHUNK = 512;
constexpr size_t STATIC_FILE_CHUNK = 1024;

#ifndef IGT_PRESSURE_DELTA_HPA
#ifdef IGT_PRESSURE_DROP_HPA
#define IGT_PRESSURE_DELTA_HPA IGT_PRESSURE_DROP_HPA
#else
#define IGT_PRESSURE_DELTA_HPA 10
#endif
#endif
#ifndef IGT_PRESSURE_DROP_HPA
#define IGT_PRESSURE_DROP_HPA IGT_PRESSURE_DELTA_HPA
#endif
#ifndef IGT_WORK_PRESSURE_DELTA_HPA
#define IGT_WORK_PRESSURE_DELTA_HPA 3
#endif
#ifndef IGT_PRESSURE_BASELINE_LIMIT_HPA
#define IGT_PRESSURE_BASELINE_LIMIT_HPA 25
#endif
#ifndef IGT_PRIMARY_FILL_SEC
#define IGT_PRIMARY_FILL_SEC 120
#endif
#ifndef IGT_PRESSURE_WAIT_SEC
#define IGT_PRESSURE_WAIT_SEC 60
#endif
#ifndef IGT_RECIRC_TIMEOUT_SEC
#define IGT_RECIRC_TIMEOUT_SEC 600
#endif
#ifndef IGT_SERVO_HYPOXIA_DEG
#define IGT_SERVO_HYPOXIA_DEG 180
#endif
#ifndef IGT_SERVO_NORMOXIA_DEG
#define IGT_SERVO_NORMOXIA_DEG 0
#endif

constexpr float IGT_PRESSURE_DELTA_HPA_F = static_cast<float>(IGT_PRESSURE_DELTA_HPA);
constexpr float IGT_PRESSURE_DROP_HPA_F = IGT_PRESSURE_DELTA_HPA_F;
constexpr float IGT_WORK_PRESSURE_DELTA_HPA_F = static_cast<float>(IGT_WORK_PRESSURE_DELTA_HPA);
constexpr float IGT_PRESSURE_BASELINE_LIMIT_HPA_F =
    static_cast<float>(IGT_PRESSURE_BASELINE_LIMIT_HPA);
constexpr uint32_t IGT_PRIMARY_FILL_MS = static_cast<uint32_t>(IGT_PRIMARY_FILL_SEC) * 1000U;
constexpr uint32_t IGT_PRESSURE_WAIT_MS = static_cast<uint32_t>(IGT_PRESSURE_WAIT_SEC) * 1000U;
constexpr uint32_t IGT_RECIRC_TIMEOUT_MS = static_cast<uint32_t>(IGT_RECIRC_TIMEOUT_SEC) * 1000U;
constexpr uint8_t IGT_SERVO_HYPOXIA_ANGLE = static_cast<uint8_t>(IGT_SERVO_HYPOXIA_DEG);
constexpr uint8_t IGT_SERVO_NORMOXIA_ANGLE = static_cast<uint8_t>(IGT_SERVO_NORMOXIA_DEG);

constexpr float IGT_O2_TARGET_MIN = 9.0f;
constexpr float IGT_O2_TARGET_MAX = 19.0f;
constexpr uint32_t IGT_PHASE_SEC_MIN = 60;
constexpr uint32_t IGT_PHASE_SEC_MAX = 900;
constexpr uint8_t IGT_CYCLES_MIN = 1;
constexpr uint8_t IGT_CYCLES_MAX = 12;
constexpr size_t IGT_NAME_MAX = 80;
constexpr size_t IGT_ERROR_MAX = 48;
constexpr size_t IGT_PHASE_MARK_MAX = 24;
constexpr size_t IGT_CMD_JSON_MAX = 384;
constexpr size_t IGT_JSON_MAX = 3072;
