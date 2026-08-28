#include "wifi_server.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <WiFi.h>

#include <ArduinoJson.h>

#include "ble_sensors.h"
#include "command_handler.h"
#include "config.h"
#include "hrv_session.h"
#include "logger.h"
#include "trend_buffer.h"
#include "wifi_sta.h"

namespace {

WebServer g_server(80);
char g_status_buffer[STATUS_JSON_MAX];
char g_cmd_buffer[CMD_JSON_MAX];
char g_wifi_scan_buffer[WIFI_SCAN_JSON_MAX];
char g_wifi_cmd_buffer[WIFI_CMD_JSON_MAX];
char g_trend_src_buffer[TREND_SRC_JSON_MAX];
char g_hrv_cmd_buffer[HRV_CMD_JSON_MAX];

bool httpChunkSink(const char *data, size_t len) {
    if (len == 0 || data == nullptr) {
        return true;
    }
    WiFiClient client = g_server.client();
    if (!client.connected()) {
        return false;
    }
    const uint32_t start = millis();
    while (client.connected() && client.availableForWrite() < static_cast<int>(len) &&
           (millis() - start) < 500) {
        yield();
        delay(1);
        client = g_server.client();
    }
    if (!client.connected()) {
        return false;
    }
    g_server.sendContent(data, len);
    yield();
    return g_server.client().connected();
}

bool streamLittleFsFile(File &file, const char *content_type) {
    if (!file) {
        return false;
    }
    const size_t total = file.size();
    g_server.setContentLength(total);
    g_server.send(200, content_type, "");
    uint8_t chunk[STATIC_FILE_CHUNK];
    size_t sent = 0;
    while (file.available()) {
        const size_t n = file.read(chunk, sizeof(chunk));
        if (n == 0) {
            break;
        }
        WiFiClient client = g_server.client();
        if (!client.connected()) {
            logPrintf("Static: client gone at %u/%u", static_cast<unsigned>(sent),
                      static_cast<unsigned>(total));
            return false;
        }
        const uint32_t start = millis();
        while (client.connected() && client.availableForWrite() < static_cast<int>(n) &&
               (millis() - start) < 500) {
            yield();
            delay(1);
            client = g_server.client();
        }
        if (!client.connected()) {
            return false;
        }
        g_server.sendContent(reinterpret_cast<const char *>(chunk), n);
        sent += n;
        yield();
    }
    return true;
}

void handleStatus() {
    const size_t len = buildStatusJson(g_status_buffer, sizeof(g_status_buffer));
    if (len == 0) {
        g_server.send(500, "application/json", "{\"error\":\"status_failed\"}");
        return;
    }
    g_server.send(200, "application/json", g_status_buffer);
}

void handleCmd() {
    if (g_server.method() != HTTP_POST) {
        g_server.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
        return;
    }

    const String body = g_server.arg("plain");
    if (body.isEmpty()) {
        g_server.send(400, "application/json", "{\"error\":\"empty_body\"}");
        return;
    }

    const size_t copy_len = body.length() < (CMD_JSON_MAX - 1) ? body.length() : (CMD_JSON_MAX - 1);
    body.substring(0, copy_len).toCharArray(g_cmd_buffer, CMD_JSON_MAX);

    const bool ok = parseCommandJson(g_cmd_buffer, copy_len);
    if (!ok) {
        g_server.send(400, "application/json", "{\"error\":\"command_failed\"}");
        return;
    }

    g_server.send(200, "application/json", "{\"ok\":true}");
}

void handleWifiScan() {
    if (g_server.method() != HTTP_GET) {
        g_server.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
        return;
    }
    wifiStaPoll();
    const size_t len = wifiStaBuildScanJson(g_wifi_scan_buffer, sizeof(g_wifi_scan_buffer));
    if (len == 0) {
        g_server.send(500, "application/json", "{\"error\":\"scan_failed\"}");
        return;
    }
    g_server.send(200, "application/json", g_wifi_scan_buffer);
}

void handleWifiScanStart() {
    if (g_server.method() != HTTP_POST) {
        g_server.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
        return;
    }
    wifiStaPoll();
    const bool started = wifiStaStartScan();
    if (!started) {
        g_server.send(500, "application/json", "{\"error\":\"scan_start_failed\"}");
        return;
    }
    g_server.send(200, "application/json", "{\"ok\":true,\"scanning\":true}");
}

bool trendsJsonSink(const char *data, size_t len) {
    if (len == 0 || data == nullptr) {
        return true;
    }
    WiFiClient client = g_server.client();
    if (!client.connected()) {
        logPrintf("Trends: client gone before chunk");
        return false;
    }
    const uint32_t start = millis();
    while (client.connected() && client.availableForWrite() < static_cast<int>(len) &&
           (millis() - start) < 500) {
        yield();
        delay(1);
        client = g_server.client();
    }
    if (!client.connected()) {
        logPrintf("Trends: client gone while waiting to write, heap=%u", ESP.getFreeHeap());
        return false;
    }
    g_server.sendContent(data, len);
    yield();
    if (!g_server.client().connected()) {
        logPrintf("Trends: client dropped during JSON, heap=%u", ESP.getFreeHeap());
        return false;
    }
    return true;
}

int parseTrendsQueryInt(const char *name, int fallback, int min_v, int max_v) {
    int v = fallback;
    if (g_server.hasArg(name)) {
        v = g_server.arg(name).toInt();
    }
    if (v < min_v) {
        v = min_v;
    }
    if (v > max_v) {
        v = max_v;
    }
    return v;
}

void handleTrends() {
    if (g_server.method() != HTTP_GET) {
        g_server.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
        return;
    }
    const int offset = parseTrendsQueryInt("offset", 0, 0, static_cast<int>(TREND_CAPACITY));
    const int limit = parseTrendsQueryInt(
        "limit",
        static_cast<int>(TREND_JSON_PAGE_DEFAULT),
        1,
        static_cast<int>(TREND_JSON_PAGE_MAX));
    g_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    g_server.send(200, "application/json", "");
    trendBufferWriteJson(trendsJsonSink, static_cast<size_t>(offset), static_cast<size_t>(limit));
    if (g_server.client().connected()) {
        g_server.sendContent("", 0);
    }
}

void handleTrendSources() {
    if (g_server.method() != HTTP_POST) {
        g_server.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
        return;
    }
    const String body = g_server.arg("plain");
    if (body.isEmpty()) {
        g_server.send(400, "application/json", "{\"error\":\"empty_body\"}");
        return;
    }
    const size_t copy_len =
        body.length() < (TREND_SRC_JSON_MAX - 1) ? body.length() : (TREND_SRC_JSON_MAX - 1);
    body.substring(0, copy_len).toCharArray(g_trend_src_buffer, TREND_SRC_JSON_MAX);
    if (!trendBufferApplySourcesJson(g_trend_src_buffer, copy_len)) {
        g_server.send(400, "application/json", "{\"error\":\"sources_failed\"}");
        return;
    }
    g_server.send(200, "application/json", "{\"ok\":true}");
}

void handleHrvStart() {
    if (g_server.method() != HTTP_POST) {
        g_server.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
        return;
    }
    const String body = g_server.arg("plain");
    if (body.isEmpty()) {
        g_server.send(400, "application/json", "{\"error\":\"empty_body\"}");
        return;
    }
    const size_t copy_len =
        body.length() < (HRV_CMD_JSON_MAX - 1) ? body.length() : (HRV_CMD_JSON_MAX - 1);
    body.substring(0, copy_len).toCharArray(g_hrv_cmd_buffer, HRV_CMD_JSON_MAX);

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, g_hrv_cmd_buffer, copy_len);
    if (err) {
        logPrintf("HRV: start JSON error: %s", err.c_str());
        g_server.send(400, "application/json", "{\"error\":\"bad_json\"}");
        return;
    }
    if (!doc["duration_sec"].is<uint32_t>() && !doc["duration_sec"].is<int>()) {
        g_server.send(400, "application/json", "{\"error\":\"invalid_duration\"}");
        return;
    }
    const uint32_t duration_sec = doc["duration_sec"].as<uint32_t>();
    if (!hrvDurationAllowed(duration_sec)) {
        g_server.send(400, "application/json", "{\"error\":\"invalid_duration\"}");
        return;
    }
    const CoospoSnapshot coospo = bleSensorsGetCoospo();
    if (!coospo.connected) {
        logPrintf("HRV: start rejected, belt not connected");
        g_server.send(400, "application/json", "{\"error\":\"belt_not_connected\"}");
        return;
    }
    if (!hrvSessionStart(duration_sec)) {
        g_server.send(503, "application/json", "{\"error\":\"busy\"}");
        return;
    }
    g_server.send(200, "application/json", "{\"ok\":true}");
}

void handleHrvStop() {
    if (g_server.method() != HTTP_POST) {
        g_server.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
        return;
    }
    hrvSessionStop();
    g_server.send(200, "application/json", "{\"ok\":true}");
}

void handleHrvGet() {
    if (g_server.method() != HTTP_GET) {
        g_server.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
        return;
    }
    g_server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    g_server.send(200, "application/json", "");
    if (!hrvSessionStreamJson(httpChunkSink)) {
        logPrintf("HRV: stream failed");
    }
    if (g_server.client().connected()) {
        g_server.sendContent("", 0);
    }
}

void handleWifiConfig() {
    if (g_server.method() != HTTP_POST) {
        g_server.send(405, "application/json", "{\"error\":\"method_not_allowed\"}");
        return;
    }

    const String body = g_server.arg("plain");
    if (body.isEmpty()) {
        g_server.send(400, "application/json", "{\"error\":\"empty_body\"}");
        return;
    }

    const size_t copy_len =
        body.length() < (WIFI_CMD_JSON_MAX - 1) ? body.length() : (WIFI_CMD_JSON_MAX - 1);
    body.substring(0, copy_len).toCharArray(g_wifi_cmd_buffer, WIFI_CMD_JSON_MAX);

    bool reboot = false;
    const bool ok = wifiStaApplyConfigJson(g_wifi_cmd_buffer, copy_len, &reboot);
    if (!ok) {
        g_server.send(400, "application/json", "{\"error\":\"config_failed\"}");
        return;
    }

    g_server.send(200, "application/json", "{\"ok\":true,\"reboot\":true}");
    if (reboot) {
        logPrintf("WiFi: перезагрузка после смены конфигурации");
        delay(500);
        ESP.restart();
    }
}

void handleRoot() {
    if (LittleFS.exists("/index.html")) {
        File file = LittleFS.open("/index.html", "r");
        streamLittleFsFile(file, "text/html");
        file.close();
        return;
    }
    g_server.send(200, "text/plain", "ESP32-Tester: index.html not found. Run uploadfs.");
}

void handleStatic() {
    const String path = g_server.uri();
    if (!LittleFS.exists(path)) {
        g_server.send(404, "text/plain", "Not found");
        return;
    }
    File file = LittleFS.open(path, "r");
    const String content_type = path.endsWith(".js")   ? "application/javascript"
                                : path.endsWith(".css") ? "text/css"
                                                        : "text/plain";
    streamLittleFsFile(file, content_type.c_str());
    file.close();
}

void startAccessPoint() {
    const uint8_t ch = (WiFi.status() == WL_CONNECTED)
                           ? static_cast<uint8_t>(WiFi.channel())
                           : WIFI_AP_CHANNEL;
    const bool ap_ok = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, ch);
    if (!ap_ok) {
        logPrintf("WiFi AP: failed to start");
    }
    logPrintf("WiFi AP: SSID=%s ch=%u IP=%s",
              WIFI_AP_SSID,
              static_cast<unsigned>(ch),
              WiFi.softAPIP().toString().c_str());
}

}  // namespace

void wifiServerInit() {
    if (!LittleFS.begin(true)) {
        logPrintf("LittleFS: mount failed");
    } else {
        logPrintf("LittleFS: mounted");
    }

    wifiStaLoadCredentials();

    if (wifiStaIsConfigured()) {
        WiFi.mode(WIFI_AP_STA);
    } else {
        WiFi.mode(WIFI_AP);
    }
    // Required when BLE is active — WiFi.setSleep(false) triggers ESP-IDF abort().
    WiFi.setSleep(WIFI_PS_MIN_MODEM);

    startAccessPoint();
    if (wifiStaIsConfigured()) {
        wifiStaTryConnect();
    }

    logPrintf("Web UI AP: http://%s/", WiFi.softAPIP().toString().c_str());
    const WifiStaStatus sta = wifiStaGetStatus();
    if (sta.connected && sta.ip[0] != '\0') {
        logPrintf("Web UI STA: http://%s/", sta.ip);
    }

    g_server.on("/", HTTP_GET, handleRoot);
    g_server.on("/api/status", HTTP_GET, handleStatus);
    g_server.on("/api/cmd", HTTP_POST, handleCmd);
    g_server.on("/api/wifi/scan", HTTP_GET, handleWifiScan);
    g_server.on("/api/wifi/scan", HTTP_POST, handleWifiScanStart);
    g_server.on("/api/wifi/config", HTTP_POST, handleWifiConfig);
    g_server.on("/api/trends", HTTP_GET, handleTrends);
    g_server.on("/api/trends/sources", HTTP_POST, handleTrendSources);
    g_server.on("/api/hrv/start", HTTP_POST, handleHrvStart);
    g_server.on("/api/hrv/stop", HTTP_POST, handleHrvStop);
    g_server.on("/api/hrv", HTTP_GET, handleHrvGet);
    g_server.onNotFound([]() {
        if (g_server.uri().startsWith("/api/")) {
            g_server.send(404, "application/json", "{\"error\":\"not_found\"}");
            return;
        }
        handleStatic();
    });

    trendBufferInit();
    hrvSessionInit();

    g_server.begin();
    logPrintf("HTTP server started on port 80");
}

void wifiServerLoop() {
    wifiStaPoll();
    trendBufferPoll();
    const CoospoSnapshot coospo = bleSensorsGetCoospo();
    hrvSessionPoll(coospo.connected);
    for (uint8_t i = 0; i < 4; i += 1) {
        g_server.handleClient();
        yield();
    }
}
