#include "ble_sensors.h"

#include <NimBLEDevice.h>
#include <Arduino.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "logger.h"
#include "wellue_oxyii.h"

namespace {

SemaphoreHandle_t g_mutex = nullptr;
TaskHandle_t g_task = nullptr;

WellueSnapshot g_wellue;
CoospoSnapshot g_coospo;
uint32_t g_wellue_sample_ms = 0;
uint32_t g_coospo_sample_ms = 0;

NimBLEClient *g_wellue_client = nullptr;
NimBLEClient *g_coospo_client = nullptr;
NimBLERemoteCharacteristic *g_wellue_write = nullptr;

uint8_t g_wellue_rx[OXYII_RX_MAX];
size_t g_wellue_rx_len = 0;
uint8_t g_wellue_seq = 0;

volatile bool g_wellue_drop = false;
volatile bool g_coospo_drop = false;
volatile bool g_scan_running = false;
volatile bool g_wifi_scan_pause = false;

struct PendingConnect {
    bool valid = false;
    bool wellue = false;
    NimBLEAddress addr;
    char name[BLE_NAME_MAX] = "";
};

PendingConnect g_pending;
bool g_connecting = false;
uint32_t g_next_scan_ms = 0;

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

void copyName(char *dst, size_t dst_len, const std::string &src) {
    copyCstr(dst, dst_len, src.c_str());
    size_t n = strlen(dst);
    while (n > 0 && (dst[n - 1] == ' ' || dst[n - 1] == '\t')) {
        dst[--n] = '\0';
    }
}

void asciiLowerCopy(char *dst, size_t dst_len, const char *src) {
    copyCstr(dst, dst_len, src);
    for (char *p = dst; *p != '\0'; ++p) {
        if (*p >= 'A' && *p <= 'Z') {
            *p = static_cast<char>(*p - 'A' + 'a');
        }
    }
}

bool startsWith(const char *s, const char *prefix) {
    if (s == nullptr || prefix == nullptr) {
        return false;
    }
    while (*prefix != '\0') {
        if (*s != *prefix) {
            return false;
        }
        ++s;
        ++prefix;
    }
    return true;
}

void lock() {
    if (g_mutex != nullptr) {
        xSemaphoreTake(g_mutex, portMAX_DELAY);
    }
}

void unlock() {
    if (g_mutex != nullptr) {
        xSemaphoreGive(g_mutex);
    }
}

bool tryLock() {
    return g_mutex != nullptr && xSemaphoreTake(g_mutex, 0) == pdTRUE;
}

void ensureAdvertising() {
    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    if (adv != nullptr && !adv->isAdvertising()) {
        adv->start();
    }
}

void setWellueError(const char *msg) {
    copyCstr(g_wellue.error, sizeof(g_wellue.error), msg);
}

void setCoospoError(const char *msg) {
    copyCstr(g_coospo.error, sizeof(g_coospo.error), msg);
}

void markWellueDisconnected() {
    g_wellue.connected = false;
    g_wellue.ok = false;
    g_wellue.spo2 = 0;
    g_wellue.hr = 0;
    g_wellue.contact = false;
    g_wellue_sample_ms = 0;
}

void markCoospoDisconnected() {
    g_coospo.connected = false;
    g_coospo.ok = false;
    g_coospo.bpm = 0;
    g_coospo.rr_ms = 0;
    g_coospo.contact = false;
    g_coospo_sample_ms = 0;
}

bool parseHrMeasurement(const uint8_t *data, size_t len, uint16_t *bpm, bool *contact, uint16_t *rr_ms) {
    if (data == nullptr || bpm == nullptr || contact == nullptr || len < 2) {
        return false;
    }
    const uint8_t flags = data[0];
    const bool wide = (flags & 0x01U) != 0;
    const uint8_t contact_bits = (flags >> 1) & 0x03U;
    if (contact_bits == 0x02U) {
        *contact = false;
    } else if (contact_bits == 0x03U) {
        *contact = true;
    } else {
        *contact = true;
    }
    size_t idx = 1;
    if (wide) {
        if (len < 3) {
            return false;
        }
        *bpm = static_cast<uint16_t>(data[1] | (static_cast<uint16_t>(data[2]) << 8));
        idx = 3;
    } else {
        *bpm = data[1];
        idx = 2;
    }
    if ((flags & 0x08U) != 0) {
        if (len < idx + 2) {
            return false;
        }
        idx += 2;
    }
    uint16_t last_rr = 0;
    bool has_rr = false;
    if ((flags & 0x10U) != 0) {
        while (idx + 1 < len) {
            const uint16_t raw =
                static_cast<uint16_t>(data[idx] | (static_cast<uint16_t>(data[idx + 1]) << 8));
            idx += 2;
            last_rr = static_cast<uint16_t>((static_cast<uint32_t>(raw) * 1000U) / 1024U);
            has_rr = true;
        }
    }
    if (rr_ms != nullptr) {
        *rr_ms = has_rr ? last_rr : 0;
    }
    return true;
}

bool isSelfDevice(const std::string &name) {
    return name.find(BLE_DEVICE_NAME) != std::string::npos;
}

bool isWellueAdvert(NimBLEAdvertisedDevice *dev, const char *name_lc) {
    if (dev == nullptr) {
        return false;
    }
    if (startsWith(name_lc, "s8-aw") || startsWith(name_lc, "t8520_")) {
        return true;
    }
    if (strstr(name_lc, "o2ring") != nullptr || strstr(name_lc, "oxylink") != nullptr) {
        return true;
    }
    if (dev->isAdvertisingService(NimBLEUUID(OXYII_SERVICE_UUID))) {
        return true;
    }
    if (dev->getManufacturerDataCount() > 0) {
        const std::string md = dev->getManufacturerData();
        if (md.size() >= 2) {
            const uint16_t id = static_cast<uint8_t>(md[0]) |
                                (static_cast<uint16_t>(static_cast<uint8_t>(md[1])) << 8);
            if (id == WELLUE_MFG_OXYII || id == WELLUE_MFG_VIATOM) {
                return true;
            }
        }
    }
    return false;
}

bool isCoospoAdvert(NimBLEAdvertisedDevice *dev, const char *name_lc, bool wellue) {
    if (dev == nullptr || wellue) {
        return false;
    }
    if (strstr(name_lc, "coospo") != nullptr || strstr(name_lc, "h6m") != nullptr) {
        return true;
    }
    return dev->isAdvertisingService(NimBLEUUID(HR_SERVICE_UUID));
}

void wellueNotifyCb(NimBLERemoteCharacteristic *characteristic, uint8_t *data, size_t length, bool is_notify) {
    (void)characteristic;
    (void)is_notify;
    if (data == nullptr || length == 0) {
        return;
    }
    if (!tryLock()) {
        return;
    }
    WellueLiveMetrics metrics;
    if (wellueFeedNotify(g_wellue_rx, &g_wellue_rx_len, sizeof(g_wellue_rx), data, length, &metrics) &&
        metrics.valid) {
        g_wellue.contact = metrics.contact;
        g_wellue.battery = metrics.battery;
        g_wellue.motion = metrics.motion;
        if (metrics.contact && metrics.spo2 >= 50 && metrics.spo2 <= 100) {
            g_wellue.spo2 = metrics.spo2;
        } else {
            g_wellue.spo2 = 0;
        }
        if (metrics.contact && metrics.hr >= 25 && metrics.hr <= 240) {
            g_wellue.hr = metrics.hr;
        } else {
            g_wellue.hr = 0;
        }
        g_wellue_sample_ms = millis();
        g_wellue.error[0] = '\0';
    }
    unlock();
}

void coospoNotifyCb(NimBLERemoteCharacteristic *characteristic, uint8_t *data, size_t length, bool is_notify) {
    (void)characteristic;
    (void)is_notify;
    uint16_t bpm = 0;
    uint16_t rr_ms = 0;
    bool contact = false;
    if (!parseHrMeasurement(data, length, &bpm, &contact, &rr_ms)) {
        return;
    }
    if (!tryLock()) {
        return;
    }
    g_coospo.contact = contact;
    if (contact && bpm >= 25 && bpm <= 240) {
        g_coospo.bpm = bpm;
    } else {
        g_coospo.bpm = 0;
    }
    if (contact && rr_ms >= 250 && rr_ms <= 2000) {
        g_coospo.rr_ms = rr_ms;
    } else {
        g_coospo.rr_ms = 0;
    }
    g_coospo_sample_ms = millis();
    g_coospo.error[0] = '\0';
    unlock();
}

class WellueClientCb : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient *client) override {
        (void)client;
        logPrintf("Wellue: GATT connected");
    }

    void onDisconnect(NimBLEClient *client) override {
        (void)client;
        logPrintf("Wellue: disconnected");
        g_wellue_drop = true;
    }
};

class CoospoClientCb : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient *client) override {
        (void)client;
        logPrintf("COOSPO: GATT connected");
    }

    void onDisconnect(NimBLEClient *client) override {
        (void)client;
        logPrintf("COOSPO: disconnected");
        g_coospo_drop = true;
    }
};

WellueClientCb g_wellue_cb;
CoospoClientCb g_coospo_cb;

class ScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice *dev) override {
        if (dev == nullptr) {
            return;
        }
        const std::string name = dev->getName();
        if (isSelfDevice(name)) {
            return;
        }

        char name_lc[BLE_NAME_MAX];
        asciiLowerCopy(name_lc, sizeof(name_lc), name.c_str());

        const bool wellue_match = isWellueAdvert(dev, name_lc);
        const bool coospo_match = isCoospoAdvert(dev, name_lc, wellue_match);

        if (!tryLock()) {
            return;
        }
        const bool need_wellue = !g_wellue.connected && g_wellue_client == nullptr;
        const bool need_coospo = !g_coospo.connected && g_coospo_client == nullptr;
        if (g_pending.valid || g_connecting) {
            unlock();
            return;
        }

        PendingConnect next;
        next.valid = false;
        if (wellue_match && need_wellue) {
            next.valid = true;
            next.wellue = true;
            next.addr = dev->getAddress();
            copyName(next.name, sizeof(next.name), name);
        } else if (coospo_match && need_coospo) {
            next.valid = true;
            next.wellue = false;
            next.addr = dev->getAddress();
            copyName(next.name, sizeof(next.name), name);
        }
        if (next.valid) {
            g_pending = next;
            if (g_pending.name[0] == '\0') {
                copyName(g_pending.name, sizeof(g_pending.name), next.addr.toString());
            }
        }
        unlock();

        if (next.valid) {
            NimBLEScan *scan = NimBLEDevice::getScan();
            if (scan != nullptr && scan->isScanning()) {
                scan->stop();
            }
        }
    }
};

ScanCallbacks g_scan_cb;

void onScanDone(NimBLEScanResults results) {
    (void)results;
    g_scan_running = false;
    if (!g_pending.valid && !g_connecting) {
        g_next_scan_ms = millis() + BLE_SCAN_PAUSE_MS;
    }
}

void startScanIfNeeded() {
    if (g_wifi_scan_pause) {
        return;
    }
    const uint32_t now = millis();
    const bool need_wellue = g_wellue_client == nullptr;
    const bool need_coospo = g_coospo_client == nullptr;
    if (!need_wellue && !need_coospo) {
        return;
    }
    if (g_scan_running || g_connecting || g_pending.valid) {
        return;
    }
    if (now < g_next_scan_ms) {
        return;
    }

    NimBLEScan *scan = NimBLEDevice::getScan();
    if (scan == nullptr) {
        return;
    }
    scan->setAdvertisedDeviceCallbacks(&g_scan_cb, false);
    scan->setActiveScan(true);
    scan->setInterval(100);
    scan->setWindow(80);
    scan->setMaxResults(0);
    g_scan_running = true;
    if (!scan->start(BLE_SCAN_DURATION_S, onScanDone, false)) {
        g_scan_running = false;
        logPrintf("BLE sensors: scan start failed, heap=%u", ESP.getFreeHeap());
        g_next_scan_ms = now + BLE_SCAN_PAUSE_MS;
        return;
    }
    logPrintf("BLE sensors: scan %lus (wellue=%s coospo=%s)",
              static_cast<unsigned long>(BLE_SCAN_DURATION_S),
              need_wellue ? "need" : "ok",
              need_coospo ? "need" : "ok");
}

void stopScan() {
    NimBLEScan *scan = NimBLEDevice::getScan();
    if (scan != nullptr && scan->isScanning()) {
        scan->stop();
    }
    g_scan_running = false;
}

void cleanupWellueClient() {
    g_wellue_write = nullptr;
    g_wellue_rx_len = 0;
    if (g_wellue_client != nullptr) {
        if (g_wellue_client->isConnected()) {
            g_wellue_client->disconnect();
        }
        NimBLEDevice::deleteClient(g_wellue_client);
        g_wellue_client = nullptr;
    }
    lock();
    markWellueDisconnected();
    unlock();
    g_wellue_drop = false;
    ensureAdvertising();
}

void cleanupCoospoClient() {
    if (g_coospo_client != nullptr) {
        if (g_coospo_client->isConnected()) {
            g_coospo_client->disconnect();
        }
        NimBLEDevice::deleteClient(g_coospo_client);
        g_coospo_client = nullptr;
    }
    lock();
    markCoospoDisconnected();
    unlock();
    g_coospo_drop = false;
    ensureAdvertising();
}

bool connectWellue(const NimBLEAddress &addr, const char *name) {
    logPrintf("Wellue: connecting %s %s heap=%u", name, addr.toString().c_str(), ESP.getFreeHeap());
    NimBLEClient *client = NimBLEDevice::createClient();
    if (client == nullptr) {
        lock();
        setWellueError("нет BLE-слота");
        unlock();
        logPrintf("Wellue: createClient failed, heap=%u", ESP.getFreeHeap());
        return false;
    }
    client->setClientCallbacks(&g_wellue_cb, false);
    client->setConnectTimeout(BLE_CONNECT_TIMEOUT_S);
    if (!client->connect(addr)) {
        NimBLEDevice::deleteClient(client);
        lock();
        setWellueError("connect failed");
        unlock();
        logPrintf("Wellue: connect failed");
        return false;
    }

    NimBLERemoteService *svc = client->getService(NimBLEUUID(OXYII_SERVICE_UUID));
    if (svc == nullptr) {
        logPrintf("Wellue: OxyII не найден (режим записи?). name=%s", name);
        client->disconnect();
        NimBLEDevice::deleteClient(client);
        lock();
        setWellueError("OxyII не найден (режим записи?)");
        unlock();
        return false;
    }

    NimBLERemoteCharacteristic *notify_char = svc->getCharacteristic(NimBLEUUID(OXYII_NOTIFY_UUID));
    NimBLERemoteCharacteristic *write_char = svc->getCharacteristic(NimBLEUUID(OXYII_WRITE_UUID));
    if (notify_char == nullptr || write_char == nullptr || !notify_char->canNotify()) {
        logPrintf("Wellue: нет notify/write характеристик");
        client->disconnect();
        NimBLEDevice::deleteClient(client);
        lock();
        setWellueError("нет OxyII характеристик");
        unlock();
        return false;
    }
    if (!notify_char->subscribe(true, wellueNotifyCb, true)) {
        logPrintf("Wellue: subscribe failed");
        client->disconnect();
        NimBLEDevice::deleteClient(client);
        lock();
        setWellueError("subscribe failed");
        unlock();
        return false;
    }

    g_wellue_client = client;
    g_wellue_write = write_char;
    g_wellue_rx_len = 0;
    g_wellue_seq = 0;
    lock();
    g_wellue.connected = true;
    copyCstr(g_wellue.name, sizeof(g_wellue.name), name);
    g_wellue.error[0] = '\0';
    unlock();
    logPrintf("Wellue: connected %s heap=%u", name, ESP.getFreeHeap());
    return true;
}

bool connectCoospo(const NimBLEAddress &addr, const char *name) {
    logPrintf("COOSPO: connecting %s %s heap=%u", name, addr.toString().c_str(), ESP.getFreeHeap());
    NimBLEClient *client = NimBLEDevice::createClient();
    if (client == nullptr) {
        lock();
        setCoospoError("нет BLE-слота");
        unlock();
        logPrintf("COOSPO: createClient failed, heap=%u", ESP.getFreeHeap());
        return false;
    }
    client->setClientCallbacks(&g_coospo_cb, false);
    client->setConnectTimeout(BLE_CONNECT_TIMEOUT_S);
    if (!client->connect(addr)) {
        NimBLEDevice::deleteClient(client);
        lock();
        setCoospoError("connect failed");
        unlock();
        logPrintf("COOSPO: connect failed");
        return false;
    }

    NimBLERemoteService *svc = client->getService(NimBLEUUID(HR_SERVICE_UUID));
    if (svc == nullptr) {
        logPrintf("COOSPO: HR service 0x180D не найден");
        client->disconnect();
        NimBLEDevice::deleteClient(client);
        lock();
        setCoospoError("нет Heart Rate service");
        unlock();
        return false;
    }
    NimBLERemoteCharacteristic *meas = svc->getCharacteristic(NimBLEUUID(HR_MEAS_CHAR_UUID));
    if (meas == nullptr || !meas->canNotify()) {
        logPrintf("COOSPO: нет 0x2A37 notify");
        client->disconnect();
        NimBLEDevice::deleteClient(client);
        lock();
        setCoospoError("нет HR Measurement");
        unlock();
        return false;
    }
    if (!meas->subscribe(true, coospoNotifyCb, true)) {
        logPrintf("COOSPO: subscribe failed");
        client->disconnect();
        NimBLEDevice::deleteClient(client);
        lock();
        setCoospoError("subscribe failed");
        unlock();
        return false;
    }

    g_coospo_client = client;
    lock();
    g_coospo.connected = true;
    copyCstr(g_coospo.name, sizeof(g_coospo.name), name);
    g_coospo.error[0] = '\0';
    unlock();
    logPrintf("COOSPO: connected %s heap=%u", name, ESP.getFreeHeap());
    return true;
}

void pollWellue() {
    if (g_wellue_client == nullptr || g_wellue_write == nullptr || !g_wellue_client->isConnected()) {
        return;
    }
    uint8_t frame[8];
    const size_t n = wellueBuildRequest(OXYII_CMD_LIVE_B, g_wellue_seq++, nullptr, 0, frame, sizeof(frame));
    if (n == 0) {
        return;
    }
    if (!g_wellue_write->writeValue(frame, n, false)) {
        logPrintf("Wellue: write 0x04 failed");
    }
}

void bleSensorsTask(void *param) {
    (void)param;
    uint32_t last_poll_ms = 0;
    g_next_scan_ms = millis() + 3000;

    for (;;) {
        if (g_wellue_drop) {
            cleanupWellueClient();
            g_next_scan_ms = millis() + BLE_SCAN_PAUSE_MS;
        }
        if (g_coospo_drop) {
            cleanupCoospoClient();
            g_next_scan_ms = millis() + BLE_SCAN_PAUSE_MS;
        }

        PendingConnect pending;
        pending.valid = false;
        lock();
        if (g_pending.valid && !g_connecting) {
            pending = g_pending;
            g_pending.valid = false;
            g_connecting = true;
        }
        unlock();

        if (pending.valid) {
            stopScan();
            bool ok = false;
            if (pending.wellue) {
                ok = connectWellue(pending.addr, pending.name);
            } else {
                ok = connectCoospo(pending.addr, pending.name);
            }
            if (!ok) {
                g_next_scan_ms = millis() + BLE_SCAN_PAUSE_MS;
            }
            g_connecting = false;
            ensureAdvertising();
        }

        const uint32_t now = millis();
        if (g_wellue_client != nullptr && (now - last_poll_ms) >= WELLUE_POLL_INTERVAL_MS) {
            last_poll_ms = now;
            pollWellue();
        }

        startScanIfNeeded();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

}  // namespace

void bleSensorsInit() {
    if (!wellueOxyiiSelfTest()) {
        logPrintf("Wellue: CRC-8 self-test FAILED");
    } else {
        logPrintf("Wellue: CRC-8 self-test OK");
    }

    g_mutex = xSemaphoreCreateMutex();
    NimBLEDevice::setMTU(BLE_CLIENT_MTU);

    xTaskCreatePinnedToCore(bleSensorsTask, "bleSens", 8192, nullptr, 1, &g_task, 0);
    logPrintf("BLE sensors: Wellue + COOSPO central started, heap=%u", ESP.getFreeHeap());
}

WellueSnapshot bleSensorsGetWellue() {
    WellueSnapshot snap;
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return snap;
    }
    snap = g_wellue;
    const uint32_t age = millis() - g_wellue_sample_ms;
    snap.ok = snap.connected && g_wellue_sample_ms != 0 && age < BLE_SAMPLE_STALE_MS;
    xSemaphoreGive(g_mutex);
    return snap;
}

CoospoSnapshot bleSensorsGetCoospo() {
    CoospoSnapshot snap;
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return snap;
    }
    snap = g_coospo;
    const uint32_t age = millis() - g_coospo_sample_ms;
    snap.ok = snap.connected && g_coospo_sample_ms != 0 && age < BLE_SAMPLE_STALE_MS;
    xSemaphoreGive(g_mutex);
    return snap;
}

void bleSensorsSetWifiScanActive(bool active) {
    g_wifi_scan_pause = active;
    if (active) {
        stopScan();
        logPrintf("BLE sensors: пауза на время WiFi scan");
    }
}
