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

// Разрешённые выходные GPIO (ESP32 DevKit). 16/17 заняты UART-мостом к BLE-узлу.
static constexpr uint8_t GPIO_WHITELIST[] = {
    13, 14, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33,
};
constexpr size_t GPIO_WHITELIST_SIZE = sizeof(GPIO_WHITELIST) / sizeof(GPIO_WHITELIST[0]);

constexpr size_t STATUS_JSON_MAX = 6144;
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
constexpr uint8_t ADDR_SCD41 = 0x62;

// AO-02 калибровка (каждый датчик — своя пара OFFSET/AIR_MV)
constexpr float O2_HYPOXIA_OFFSET_MV = 0.19f;
constexpr float O2_HYPOXIA_AIR_MV = 10.85f;
constexpr float O2_WORKING_OFFSET_MV = 0.3f;
constexpr float O2_WORKING_AIR_MV = 10.68f;
constexpr float O2_AIR_PERCENT = 20.9f;

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

// SCD41 (датчик обновляет значения раз в 5 с)
constexpr uint32_t SCD41_POLL_INTERVAL_MS = 1000;
constexpr uint32_t SCD41_WARMUP_MS = 10000;
// Компенсация инерции. Параметры как в OxyPulse MVP. Флаги — platformio.ini.
#ifndef SCD41_CO2_DYNAMIC_COMP
#define SCD41_CO2_DYNAMIC_COMP 0
#endif
#ifndef SCD41_CO2_TAU_SEC
#define SCD41_CO2_TAU_SEC 60
#endif
#ifndef SCD41_CO2_TRUST
#define SCD41_CO2_TRUST 70
#endif
#ifndef SCD41_CO2_COMP_MAX_DELTA_PPM
#define SCD41_CO2_COMP_MAX_DELTA_PPM 15000
#endif
#ifndef SCD41_ASC_ENABLED
#define SCD41_ASC_ENABLED 0
#endif
constexpr float SCD41_TAU_CO2_S = static_cast<float>(SCD41_CO2_TAU_SEC > 0 ? SCD41_CO2_TAU_SEC : 60);
constexpr float SCD41_TAU_TRUST_CO2 = static_cast<float>(SCD41_CO2_TRUST) / 100.0f;
constexpr float SCD41_MAX_CORR_PPM = static_cast<float>(SCD41_CO2_COMP_MAX_DELTA_PPM);
constexpr float SCD41_TAU_TEMP_S = 120.0f;
constexpr float SCD41_TAU_RH_S = 90.0f;
constexpr float SCD41_TAU_TRUST_TEMP = 0.35f;
constexpr float SCD41_TAU_TRUST_RH = 0.35f;
constexpr float SCD41_DECAY_DAMPING_TEMP = 0.25f;
constexpr float SCD41_DECAY_DAMPING_RH = 0.25f;
constexpr float SCD41_SAMPLE_INTERVAL_S = 5.0f;
constexpr size_t SCD41_SLOPE_WINDOW = 5;
constexpr float SCD41_MAX_CORR_TEMP_C = 2.0f;
constexpr float SCD41_MAX_CORR_RH = 12.0f;
constexpr float SCD41_BASELINE_ALPHA = 0.04f;
constexpr uint16_t SCD41_EVENT_CO2_PPM = 150;
constexpr float SCD41_EVENT_TEMP_C = 1.5f;
constexpr float SCD41_EVENT_RH = 8.0f;
constexpr float SCD41_CLAMP_TEMP_BELOW_C = 2.0f;
constexpr float SCD41_CLAMP_TEMP_ABOVE_C = 8.0f;
constexpr float SCD41_CLAMP_RH_BELOW = 10.0f;
constexpr float SCD41_CLAMP_RH_ABOVE = 25.0f;

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
