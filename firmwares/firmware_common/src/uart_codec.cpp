#include "uart_codec.h"

#include <ArduinoJson.h>
#include <cstdio>
#include <cstring>

#include "uart_protocol.h"

void uartCopyCstr(char *dst, size_t dst_len, const char *src) {
    if (dst == nullptr || dst_len == 0) {
        return;
    }
    if (src == nullptr) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_len - 1);
    dst[dst_len - 1] = '\0';
}

size_t uartEncodeHello(char *buf, size_t cap) {
    if (buf == nullptr || cap == 0) {
        return 0;
    }
    JsonDocument doc;
    doc["t"] = UART_T_HELLO;
    doc["ver"] = UART_BRIDGE_VER;
    return serializeJson(doc, buf, cap);
}

size_t uartEncodeWellue(char *buf, size_t cap, const WellueSnapshot &snap) {
    if (buf == nullptr || cap == 0) {
        return 0;
    }
    JsonDocument doc;
    doc["t"] = UART_T_WELLUE;
    doc["connected"] = snap.connected;
    doc["ok"] = snap.ok;
    doc["name"] = snap.name;
    doc["spo2"] = snap.spo2;
    doc["hr"] = snap.hr;
    doc["battery"] = snap.battery;
    doc["motion"] = snap.motion;
    doc["contact"] = snap.contact;
    doc["error"] = snap.error;
    return serializeJson(doc, buf, cap);
}

size_t uartEncodeCoospo(char *buf, size_t cap, const CoospoSnapshot &snap) {
    if (buf == nullptr || cap == 0) {
        return 0;
    }
    JsonDocument doc;
    doc["t"] = UART_T_COOSPO;
    doc["connected"] = snap.connected;
    doc["ok"] = snap.ok;
    doc["name"] = snap.name;
    doc["bpm"] = snap.bpm;
    doc["rr_ms"] = snap.rr_ms;
    doc["contact"] = snap.contact;
    doc["error"] = snap.error;
    return serializeJson(doc, buf, cap);
}

size_t uartEncodeRr(char *buf, size_t cap, uint16_t rr_ms) {
    if (buf == nullptr || cap < 24) {
        return 0;
    }
    return static_cast<size_t>(
        snprintf(buf, cap, "{\"t\":\"rr\",\"ms\":%u}", static_cast<unsigned>(rr_ms)));
}

bool uartParseLine(const char *line, UartIncoming *out) {
    if (line == nullptr || out == nullptr || line[0] == '\0') {
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, line)) {
        return false;
    }

    const char *t = doc["t"] | "";
    UartIncoming msg;
    if (strcmp(t, UART_T_HELLO) == 0) {
        msg.type = UartMsgType::Hello;
    } else if (strcmp(t, UART_T_WELLUE) == 0) {
        msg.type = UartMsgType::Wellue;
        msg.wellue.connected = doc["connected"] | false;
        msg.wellue.ok = doc["ok"] | false;
        uartCopyCstr(msg.wellue.name, sizeof(msg.wellue.name), doc["name"] | "");
        msg.wellue.spo2 = doc["spo2"] | 0;
        msg.wellue.hr = doc["hr"] | 0;
        msg.wellue.battery = doc["battery"] | 0;
        msg.wellue.motion = doc["motion"] | 0;
        msg.wellue.contact = doc["contact"] | false;
        uartCopyCstr(msg.wellue.error, sizeof(msg.wellue.error), doc["error"] | "");
    } else if (strcmp(t, UART_T_COOSPO) == 0) {
        msg.type = UartMsgType::Coospo;
        msg.coospo.connected = doc["connected"] | false;
        msg.coospo.ok = doc["ok"] | false;
        uartCopyCstr(msg.coospo.name, sizeof(msg.coospo.name), doc["name"] | "");
        msg.coospo.bpm = doc["bpm"] | 0;
        msg.coospo.rr_ms = doc["rr_ms"] | 0;
        msg.coospo.contact = doc["contact"] | false;
        uartCopyCstr(msg.coospo.error, sizeof(msg.coospo.error), doc["error"] | "");
    } else if (strcmp(t, UART_T_RR) == 0) {
        msg.type = UartMsgType::Rr;
        msg.rr_ms = static_cast<uint16_t>(doc["ms"] | 0);
    } else {
        return false;
    }

    *out = msg;
    return true;
}
