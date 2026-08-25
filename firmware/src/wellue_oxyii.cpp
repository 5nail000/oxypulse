#include "wellue_oxyii.h"

#include <cstring>

uint8_t wellueCrc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    if (data == nullptr) {
        return 0;
    }
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80U) ? static_cast<uint8_t>((crc << 1) ^ 0x07U) : static_cast<uint8_t>(crc << 1);
        }
    }
    return crc;
}

bool wellueOxyiiSelfTest() {
    const uint8_t fixture[] = {0xA5, 0xE1, 0x1E, 0x00, 0x02, 0x00, 0x00};
    return wellueCrc8(fixture, sizeof(fixture)) == 0xBF;
}

size_t wellueBuildRequest(uint8_t cmd, uint8_t seq, const uint8_t *payload, uint16_t payload_len,
                          uint8_t *out, size_t out_cap) {
    const size_t frame_len = 8U + payload_len;
    if (out == nullptr || out_cap < frame_len) {
        return 0;
    }
    if (payload_len > 0 && payload == nullptr) {
        return 0;
    }

    out[0] = OXYII_LEAD;
    out[1] = cmd;
    out[2] = static_cast<uint8_t>(~cmd);
    out[3] = OXYII_FLAG_REQ;
    out[4] = seq;
    out[5] = static_cast<uint8_t>(payload_len & 0xFFU);
    out[6] = static_cast<uint8_t>((payload_len >> 8) & 0xFFU);
    if (payload_len > 0) {
        memcpy(out + 7, payload, payload_len);
    }
    out[7 + payload_len] = wellueCrc8(out, 7U + payload_len);
    return frame_len;
}

bool wellueParseLiveBPayload(const uint8_t *payload, uint16_t payload_len, WellueLiveMetrics *out) {
    if (payload == nullptr || out == nullptr || payload_len < OXYII_LIVE_HEADER_SIZE) {
        return false;
    }

    out->ring_state = payload[5];
    out->spo2 = payload[6];
    out->motion = payload[7];
    out->hr = payload[8];
    out->battery = payload[13];
    out->contact = payload[5] != 0x00;
    out->valid = true;
    return true;
}

bool wellueFeedNotify(uint8_t *rx_buf, size_t *rx_len, size_t rx_cap, const uint8_t *data, size_t length,
                      WellueLiveMetrics *out) {
    if (rx_buf == nullptr || rx_len == nullptr || out == nullptr) {
        return false;
    }
    if (data == nullptr || length == 0) {
        return false;
    }

    if (*rx_len + length > rx_cap) {
        *rx_len = 0;
    }
    if (*rx_len + length > rx_cap) {
        return false;
    }
    memcpy(rx_buf + *rx_len, data, length);
    *rx_len += length;

    bool got_live = false;
    while (*rx_len >= 8) {
        size_t start = 0;
        while (start < *rx_len && rx_buf[start] != OXYII_LEAD) {
            ++start;
        }
        if (start > 0) {
            memmove(rx_buf, rx_buf + start, *rx_len - start);
            *rx_len -= start;
        }
        if (*rx_len < 8) {
            break;
        }

        const uint16_t payload_len =
            static_cast<uint16_t>(rx_buf[5] | (static_cast<uint16_t>(rx_buf[6]) << 8));
        const size_t frame_len = 8U + payload_len;
        if (frame_len > rx_cap) {
            memmove(rx_buf, rx_buf + 1, *rx_len - 1);
            *rx_len -= 1;
            continue;
        }
        if (*rx_len < frame_len) {
            break;
        }

        const uint8_t crc = wellueCrc8(rx_buf, frame_len - 1);
        if (crc != rx_buf[frame_len - 1] || rx_buf[2] != static_cast<uint8_t>(~rx_buf[1])) {
            memmove(rx_buf, rx_buf + 1, *rx_len - 1);
            *rx_len -= 1;
            continue;
        }

        if (rx_buf[1] == OXYII_CMD_LIVE_B && rx_buf[3] == OXYII_FLAG_RSP) {
            if (wellueParseLiveBPayload(rx_buf + 7, payload_len, out)) {
                got_live = true;
            }
        }

        const size_t remain = *rx_len - frame_len;
        if (remain > 0) {
            memmove(rx_buf, rx_buf + frame_len, remain);
        }
        *rx_len = remain;
    }

    return got_live;
}
