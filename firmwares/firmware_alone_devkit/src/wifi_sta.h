#pragma once

#include <cstddef>
#include <cstdint>

struct WifiStaStatus {
    bool configured = false;
    bool connected = false;
    char ssid[33] = "";
    char ip[16] = "";
    int32_t rssi = 0;
    char error[48] = "";
};

void wifiStaLoadCredentials();
bool wifiStaIsConfigured();
bool wifiStaTryConnect();
WifiStaStatus wifiStaGetStatus();

bool wifiStaSaveCredentials(const char *ssid, const char *password);
bool wifiStaClearCredentials();

void wifiStaPoll();
bool wifiStaStartScan();
size_t wifiStaBuildScanJson(char *buffer, size_t capacity);
bool wifiStaApplyConfigJson(const char *json, size_t len, bool *reboot_requested);
