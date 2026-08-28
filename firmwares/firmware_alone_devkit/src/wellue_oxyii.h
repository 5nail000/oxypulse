#pragma once

#include <cstddef>
#include <cstdint>

constexpr uint8_t OXYII_LEAD = 0xA5;
constexpr uint8_t OXYII_CMD_LIVE_B = 0x04;
constexpr uint8_t OXYII_FLAG_REQ = 0x00;
constexpr uint8_t OXYII_FLAG_RSP = 0x01;
constexpr size_t OXYII_LIVE_HEADER_SIZE = 24;

struct WellueLiveMetrics {
    uint8_t spo2 = 0;
    uint8_t hr = 0;
    uint8_t battery = 0;
    uint8_t motion = 0;
    uint8_t ring_state = 0;
    bool contact = false;
    bool valid = false;
};

uint8_t wellueCrc8(const uint8_t *data, size_t len);
bool wellueOxyiiSelfTest();
size_t wellueBuildRequest(uint8_t cmd, uint8_t seq, const uint8_t *payload, uint16_t payload_len,
                          uint8_t *out, size_t out_cap);
bool wellueParseLiveBPayload(const uint8_t *payload, uint16_t payload_len, WellueLiveMetrics *out);
bool wellueFeedNotify(uint8_t *rx_buf, size_t *rx_len, size_t rx_cap, const uint8_t *data, size_t length,
                      WellueLiveMetrics *out);
