#pragma once

#include <cstddef>
#include <cstdint>

#include "ble_snapshots.h"

enum class UartMsgType : uint8_t { Unknown, Hello, Wellue, Coospo, Rr };

struct UartIncoming {
    UartMsgType type = UartMsgType::Unknown;
    WellueSnapshot wellue;
    CoospoSnapshot coospo;
    uint16_t rr_ms = 0;
};

size_t uartEncodeHello(char *buf, size_t cap);
size_t uartEncodeWellue(char *buf, size_t cap, const WellueSnapshot &snap);
size_t uartEncodeCoospo(char *buf, size_t cap, const CoospoSnapshot &snap);
size_t uartEncodeRr(char *buf, size_t cap, uint16_t rr_ms);
bool uartParseLine(const char *line, UartIncoming *out);
void uartCopyCstr(char *dst, size_t dst_len, const char *src);
