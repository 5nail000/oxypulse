#include "wifi_sta.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

#include "config.h"
#include "logger.h"

namespace {

Preferences g_prefs;
char g_saved_ssid[WIFI_STA_SSID_MAX + 1] = "";
char g_saved_pass[WIFI_STA_PASS_MAX + 1] = "";
bool g_configured = false;
bool g_connected = false;
char g_sta_error[WIFI_STA_ERROR_MAX] = "";

enum class ScanState { Idle, Running, Ready, Failed };

ScanState g_scan_state = ScanState::Idle;
char g_scan_error[48] = "";
uint32_t g_scan_ready_ms = 0;
uint32_t g_scan_started_ms = 0;
TaskHandle_t g_scan_task = nullptr;

struct ScanEntry {
    char ssid[WIFI_STA_SSID_MAX + 1];
    int32_t rssi;
    uint8_t channel;
    bool secure;
};

ScanEntry g_scan_entries[WIFI_SCAN_MAX_NETWORKS];
size_t g_scan_count = 0;

void copyCstr(char *dst, size_t dst_len, const char *src) {
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

void setStaError(const char *msg) {
    copyCstr(g_sta_error, sizeof(g_sta_error), msg);
}

void loadFromNvs() {
    g_prefs.begin(WIFI_NVS_NAMESPACE, true);
    copyCstr(g_saved_ssid, sizeof(g_saved_ssid), g_prefs.getString(WIFI_NVS_KEY_SSID, "").c_str());
    copyCstr(g_saved_pass, sizeof(g_saved_pass), g_prefs.getString(WIFI_NVS_KEY_PASS, "").c_str());
    g_prefs.end();
    g_configured = g_saved_ssid[0] != '\0';
}

uint8_t apChannel() {
    if (WiFi.status() == WL_CONNECTED) {
        const int sta_ch = WiFi.channel();
        if (sta_ch >= 1 && sta_ch <= 13) {
            return static_cast<uint8_t>(sta_ch);
        }
    }
    return WIFI_AP_CHANNEL;
}

void restoreAccessPoint() {
    if (WiFi.getMode() == WIFI_OFF) {
        return;
    }
    const uint8_t ch = apChannel();
    if (!WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, ch)) {
        logPrintf("WiFi AP: restore failed");
        return;
    }
    logPrintf("WiFi AP: restored SSID=%s ch=%u IP=%s",
              WIFI_AP_SSID,
              static_cast<unsigned>(ch),
              WiFi.softAPIP().toString().c_str());
}

bool connectSta() {
    if (!g_configured) {
        g_connected = false;
        setStaError("");
        return false;
    }

    logPrintf("WiFi STA: подключение к %s", g_saved_ssid);
    WiFi.begin(g_saved_ssid, g_saved_pass);

    const uint32_t deadline = millis() + WIFI_STA_CONNECT_TIMEOUT_MS;
    while (millis() < deadline) {
        const wl_status_t st = WiFi.status();
        if (st == WL_CONNECTED) {
            g_connected = true;
            setStaError("");
            logPrintf("WiFi STA: подключено IP=%s RSSI=%d ch=%d",
                      WiFi.localIP().toString().c_str(),
                      WiFi.RSSI(),
                      WiFi.channel());
            restoreAccessPoint();
            return true;
        }
        if (st == WL_CONNECT_FAILED) {
            break;
        }
        delay(200);
    }

    g_connected = false;
    const wl_status_t st = WiFi.status();
    if (st == WL_CONNECT_FAILED) {
        setStaError("неверный пароль или сеть");
    } else if (st == WL_NO_SSID_AVAIL) {
        setStaError("сеть не найдена");
    } else {
        setStaError("таймаут подключения");
    }
    logPrintf("WiFi STA: ошибка (%s)", g_sta_error);
    return false;
}

void clearScanResults() {
    g_scan_count = 0;
    g_scan_error[0] = '\0';
}

void collectScanResults(int count) {
    clearScanResults();
    for (int i = 0; i < count && g_scan_count < WIFI_SCAN_MAX_NETWORKS; ++i) {
        const String ssid = WiFi.SSID(i);
        if (ssid.isEmpty()) {
            continue;
        }
        ScanEntry &entry = g_scan_entries[g_scan_count++];
        copyCstr(entry.ssid, sizeof(entry.ssid), ssid.c_str());
        entry.rssi = WiFi.RSSI(i);
        entry.channel = static_cast<uint8_t>(WiFi.channel(i));
        entry.secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
}

void finishScan(int count) {
    if (count < 0) {
        WiFi.scanDelete();
        restoreAccessPoint();
        g_scan_state = ScanState::Failed;
        copyCstr(g_scan_error, sizeof(g_scan_error), "scan failed");
        clearScanResults();
        logPrintf("WiFi: scan failed (%d)", count);
        return;
    }

    collectScanResults(count);
    WiFi.scanDelete();
    restoreAccessPoint();

    g_scan_state = ScanState::Ready;
    g_scan_ready_ms = millis();
    g_scan_error[0] = '\0';
    logPrintf("WiFi: scan OK, сетей=%u", static_cast<unsigned>(g_scan_count));
}

void abortScanRunning(const char *reason) {
    WiFi.scanDelete();
    restoreAccessPoint();
    g_scan_state = ScanState::Failed;
    copyCstr(g_scan_error, sizeof(g_scan_error), reason != nullptr ? reason : "scan timeout");
    clearScanResults();
    logPrintf("WiFi: scan aborted (%s)", g_scan_error);
}

void scanTask(void *param) {
    (void)param;
    logPrintf("WiFi: sync scan task start");
    WiFi.scanDelete();
    const int count = WiFi.scanNetworks(false, false);
    finishScan(count);
    g_scan_task = nullptr;
    vTaskDelete(nullptr);
}

bool scanTimedOut() {
    return g_scan_started_ms != 0 &&
           (millis() - g_scan_started_ms) > WIFI_SCAN_TIMEOUT_MS;
}

}  // namespace

void wifiStaLoadCredentials() {
    loadFromNvs();
}

bool wifiStaTryConnect() {
    return connectSta();
}

bool wifiStaIsConfigured() {
    return g_configured;
}

WifiStaStatus wifiStaGetStatus() {
    WifiStaStatus status;
    status.configured = g_configured;
    status.connected = g_connected && WiFi.status() == WL_CONNECTED;
    copyCstr(status.ssid, sizeof(status.ssid), g_saved_ssid);
    copyCstr(status.error, sizeof(status.error), g_sta_error);
    if (status.connected) {
        copyCstr(status.ip, sizeof(status.ip), WiFi.localIP().toString().c_str());
        status.rssi = WiFi.RSSI();
    }
    return status;
}

bool wifiStaSaveCredentials(const char *ssid, const char *password) {
    if (ssid == nullptr || ssid[0] == '\0') {
        return false;
    }
    if (strlen(ssid) > WIFI_STA_SSID_MAX) {
        return false;
    }
    if (password != nullptr && strlen(password) > WIFI_STA_PASS_MAX) {
        return false;
    }

    g_prefs.begin(WIFI_NVS_NAMESPACE, false);
    g_prefs.putString(WIFI_NVS_KEY_SSID, ssid);
    g_prefs.putString(WIFI_NVS_KEY_PASS, password != nullptr ? password : "");
    g_prefs.end();

    copyCstr(g_saved_ssid, sizeof(g_saved_ssid), ssid);
    copyCstr(g_saved_pass, sizeof(g_saved_pass), password != nullptr ? password : "");
    g_configured = true;
    logPrintf("WiFi STA: сохранено SSID=%s", g_saved_ssid);
    return true;
}

bool wifiStaClearCredentials() {
    g_prefs.begin(WIFI_NVS_NAMESPACE, false);
    g_prefs.remove(WIFI_NVS_KEY_SSID);
    g_prefs.remove(WIFI_NVS_KEY_PASS);
    g_prefs.end();

    g_saved_ssid[0] = '\0';
    g_saved_pass[0] = '\0';
    g_configured = false;
    g_connected = false;
    setStaError("");
    WiFi.disconnect(true);
    logPrintf("WiFi STA: credentials удалены");
    return true;
}

bool wifiStaStartScan() {
    if (g_scan_state == ScanState::Running) {
        if (scanTimedOut()) {
            abortScanRunning("scan timeout");
            g_scan_task = nullptr;
        } else {
            return true;
        }
    }

    if (g_scan_task != nullptr) {
        logPrintf("WiFi: scan task still active");
        return true;
    }

    logPrintf("WiFi: scan start (background sync)");
    WiFi.scanDelete();
    g_scan_state = ScanState::Running;
    g_scan_error[0] = '\0';
    g_scan_started_ms = millis();

    const BaseType_t created =
        xTaskCreatePinnedToCore(scanTask, "wifiScan", 4096, nullptr, 1, &g_scan_task, 0);
    if (created != pdPASS) {
        g_scan_task = nullptr;
        abortScanRunning("scan task failed");
        return false;
    }
    return true;
}

void wifiStaPoll() {
    if (g_scan_state != ScanState::Running) {
        return;
    }
    if (!scanTimedOut()) {
        return;
    }
    abortScanRunning("scan timeout");
    g_scan_task = nullptr;
}

size_t wifiStaBuildScanJson(char *buffer, size_t capacity) {
    if (buffer == nullptr || capacity == 0) {
        return 0;
    }

    JsonDocument doc;
    doc["scanning"] = g_scan_state == ScanState::Running;
    if (g_scan_state == ScanState::Failed) {
        doc["error"] = g_scan_error;
    }
    if (g_scan_state == ScanState::Ready && g_scan_ready_ms != 0) {
        doc["age_ms"] = millis() - g_scan_ready_ms;
    }

    JsonArray arr = doc["networks"].to<JsonArray>();
    for (size_t i = 0; i < g_scan_count; ++i) {
        JsonObject item = arr.add<JsonObject>();
        item["ssid"] = g_scan_entries[i].ssid;
        item["rssi"] = g_scan_entries[i].rssi;
        item["secure"] = g_scan_entries[i].secure;
        item["channel"] = g_scan_entries[i].channel;
    }
    doc["count"] = g_scan_count;

    return serializeJson(doc, buffer, capacity);
}

bool wifiStaApplyConfigJson(const char *json, size_t len, bool *reboot_requested) {
    if (reboot_requested != nullptr) {
        *reboot_requested = false;
    }
    if (json == nullptr || len == 0 || len >= WIFI_CMD_JSON_MAX) {
        return false;
    }

    JsonDocument doc;
    if (deserializeJson(doc, json, len)) {
        return false;
    }

    if (doc["clear"].is<bool>() && doc["clear"].as<bool>()) {
        if (!wifiStaClearCredentials()) {
            return false;
        }
        if (reboot_requested != nullptr) {
            *reboot_requested = true;
        }
        return true;
    }

    const char *ssid = doc["ssid"] | "";
    if (ssid[0] == '\0') {
        return false;
    }
    const char *password = doc["password"] | "";
    if (!wifiStaSaveCredentials(ssid, password)) {
        return false;
    }
    if (reboot_requested != nullptr) {
        *reboot_requested = true;
    }
    return true;
}
