#pragma once

#include <cstddef>
#include <cstdint>

constexpr const char *BLE_DEVICE_NAME = "OxyPulse-CU";
constexpr const char *CU_SERVICE_UUID = "c0a10000-4f78-11ef-b200-0800200c9a66";
constexpr const char *CU_LIVE_UUID = "c0a10001-4f78-11ef-b200-0800200c9a66";
constexpr const char *CU_CMD_UUID = "c0a10002-4f78-11ef-b200-0800200c9a66";

constexpr uint16_t CU_PREFERRED_MTU = 512;
constexpr uint32_t CU_LIVE_INTERVAL_MS = 1000;
constexpr uint32_t CU_LINK_TIMEOUT_MS = 5000;
constexpr uint32_t CU_PHYS_STALE_MS = 3000;

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

constexpr uint8_t SERVO1_PIN = 4;
constexpr uint8_t VALVE_IN_HYPO_PIN = 25;
constexpr uint8_t VALVE_IN_HYPER_PIN = 26;
constexpr uint8_t VALVE_IN_WORK_PIN = 27;
constexpr uint8_t VALVE_OUT_DEP_HYPO_PIN = 13;
constexpr uint8_t VALVE_OUT_DEP_HYPER_PIN = 14;
constexpr uint8_t VALVE_OUT_RICH_HYPER_PIN = 21;
constexpr uint8_t O2_MOTOR_PIN = 33;

#ifndef GPIO_VALVE_ACTIVE_LOW
#define GPIO_VALVE_ACTIVE_LOW 0
#endif
constexpr bool GPIO_VALVE_ACTIVE_LOW_B = GPIO_VALVE_ACTIVE_LOW != 0;

static constexpr uint8_t GPIO_WHITELIST[] = {
    4, 13, 14, 18, 19, 21, 22, 23, 25, 26, 27, 33,
};
constexpr size_t GPIO_WHITELIST_SIZE = sizeof(GPIO_WHITELIST) / sizeof(GPIO_WHITELIST[0]);

constexpr size_t LIVE_JSON_MAX = 512;
constexpr size_t CMD_JSON_MAX = 384;

constexpr int I2C_SDA_PIN = 22;
constexpr int I2C_SCL_PIN = 23;
constexpr int I2C_WORKING_SDA_PIN = 18;
constexpr int I2C_WORKING_SCL_PIN = 19;
constexpr uint32_t I2C_CLOCK_HZ = 100000;
constexpr uint32_t I2C_TIMEOUT_MS = 200;

constexpr uint8_t ADDR_ADS1115 = 0x48;
constexpr uint8_t ADDR_SFM3300 = 0x40;
constexpr uint8_t ADDR_DPS310 = 0x76;
constexpr uint8_t ADDR_DPS310_ALT = 0x77;
#ifndef WORKING_PRESSURE_BMP390
#define WORKING_PRESSURE_BMP390 0
#endif
#ifndef WORKING_PRESSURE_ENABLED
#define WORKING_PRESSURE_ENABLED 1
#endif
constexpr uint8_t ADDR_STC31 = 0x29;
constexpr uint8_t ADDR_SHT4X = 0x44;

constexpr float O2_HYPOXIA_OFFSET_MV = 0.19f;
constexpr float O2_HYPOXIA_AIR_MV = 10.85f;
constexpr float O2_AIR_PERCENT = 20.9f;

constexpr int OCS_UART_RX_PIN = 34;
constexpr int OCS_UART_TX_PIN = 32;
constexpr uint32_t OCS_UART_BAUD = 9600;
constexpr uint32_t OCS_POLL_INTERVAL_MS = 50;
constexpr uint32_t OCS_QUERY_INTERVAL_MS = 1000;
constexpr uint32_t OCS_STALE_MS = 3000;
#ifndef OCS_O2_CO2_PER_VOL
#define OCS_O2_CO2_PER_VOL 5.0f
#endif
#ifndef OCS_O2_CO2_AMBIENT_VOL
#define OCS_O2_CO2_AMBIENT_VOL 0.04f
#endif
#ifndef OCS_FRAME_LAYOUT
#define OCS_FRAME_LAYOUT 1
#endif
#ifndef OCS_O2_FIELD
#define OCS_O2_FIELD -1
#endif

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

constexpr uint32_t STC31_POLL_INTERVAL_MS = 1000;
constexpr uint32_t STC31_WARMUP_MS = 2000;
#ifndef STC31_ASC_ENABLED
#define STC31_ASC_ENABLED 0
#endif
#ifndef SCD41_CO2_DYNAMIC_COMP
#define SCD41_CO2_DYNAMIC_COMP 0
#endif

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
