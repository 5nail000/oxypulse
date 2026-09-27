package com.oxypulse.esp32tester.api

import com.oxypulse.esp32tester.domain.DeviceStatus
import com.oxypulse.esp32tester.domain.HrvSnapshot
import com.oxypulse.esp32tester.domain.TrendPage
import com.oxypulse.esp32tester.domain.TrendSources
import com.oxypulse.esp32tester.domain.WifiScanResult

interface Esp32Api {
    suspend fun getStatus(): DeviceStatus
    suspend fun sendCommand(json: String)

    suspend fun getTrends(offset: Int, limit: Int): TrendPage {
        notWifi()
    }

    suspend fun postTrendSources(sources: TrendSources) {
        notWifi()
    }

    suspend fun startHrv(durationSec: Int) {
        notWifi()
    }

    suspend fun stopHrv() {
        notWifi()
    }

    suspend fun getHrv(): HrvSnapshot {
        notWifi()
    }

    suspend fun igtPost(path: String, body: String?) {
        notWifi()
    }

    suspend fun startWifiScan() {
        notWifi()
    }

    suspend fun getWifiScan(): WifiScanResult {
        notWifi()
    }

    suspend fun saveWifi(ssid: String, password: String) {
        notWifi()
    }

    suspend fun forgetWifi() {
        notWifi()
    }
}

private fun notWifi(): Nothing =
    throw IllegalStateException("Только WiFi HTTP")
