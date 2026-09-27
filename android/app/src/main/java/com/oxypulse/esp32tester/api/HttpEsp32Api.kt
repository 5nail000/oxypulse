package com.oxypulse.esp32tester.api

import com.oxypulse.esp32tester.domain.DeviceStatus
import com.oxypulse.esp32tester.domain.HrvSnapshot
import com.oxypulse.esp32tester.domain.TrendPage
import com.oxypulse.esp32tester.domain.TrendSources
import com.oxypulse.esp32tester.domain.WifiScanResult
import com.oxypulse.esp32tester.util.AppLogger
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import java.util.concurrent.TimeUnit

class HttpEsp32Api(
    private val baseUrl: String,
) : Esp32Api {
    private val normalizedBase = baseUrl.trimEnd('/')
    private val jsonType = "application/json".toMediaType()
    private val emptyBody = "{}".toRequestBody(jsonType)

    private val client = OkHttpClient.Builder()
        .connectTimeout(8, TimeUnit.SECONDS)
        .readTimeout(8, TimeUnit.SECONDS)
        .writeTimeout(8, TimeUnit.SECONDS)
        .build()

    private val longClient = client.newBuilder()
        .readTimeout(45, TimeUnit.SECONDS)
        .build()

    val host: String
        get() = normalizedBase.removePrefix("http://").removePrefix("https://")

    override suspend fun getStatus(): DeviceStatus = withContext(Dispatchers.IO) {
        StatusParser.parse(getText("/api/status"))
    }

    override suspend fun sendCommand(json: String) = withContext(Dispatchers.IO) {
        AppLogger.d("HTTP cmd: $json")
        postText("/api/cmd", json)
        Unit
    }

    override suspend fun getTrends(offset: Int, limit: Int): TrendPage = withContext(Dispatchers.IO) {
        StatusParser.parseTrends(getText("/api/trends?offset=$offset&limit=$limit", timeoutMs = 5000))
    }

    override suspend fun postTrendSources(sources: TrendSources) = withContext(Dispatchers.IO) {
        postText("/api/trends/sources", CommandBuilder.trendSourcesJson(sources))
        Unit
    }

    override suspend fun startHrv(durationSec: Int) = withContext(Dispatchers.IO) {
        postText("/api/hrv/start", CommandBuilder.hrvStartJson(durationSec), timeoutMs = 8000)
        Unit
    }

    override suspend fun stopHrv() = withContext(Dispatchers.IO) {
        try {
            postText("/api/hrv/stop", "{}", timeoutMs = 4000)
        } catch (_: Exception) {
            // ignore cancel races
        }
        Unit
    }

    override suspend fun getHrv(): HrvSnapshot = withContext(Dispatchers.IO) {
        StatusParser.parseHrv(getText("/api/hrv", timeoutMs = 45000, longRead = true))
    }

    override suspend fun igtPost(path: String, body: String?) = withContext(Dispatchers.IO) {
        postText(path, body ?: "{}", timeoutMs = 8000)
        Unit
    }

    override suspend fun startWifiScan() = withContext(Dispatchers.IO) {
        postText("/api/wifi/scan", "{}", timeoutMs = 6000)
        Unit
    }

    override suspend fun getWifiScan(): WifiScanResult = withContext(Dispatchers.IO) {
        StatusParser.parseWifiScan(getText("/api/wifi/scan", timeoutMs = 8000))
    }

    override suspend fun saveWifi(ssid: String, password: String) = withContext(Dispatchers.IO) {
        postText("/api/wifi/config", CommandBuilder.wifiConfigJson(ssid, password), timeoutMs = 8000)
        Unit
    }

    override suspend fun forgetWifi() = withContext(Dispatchers.IO) {
        postText("/api/wifi/config", CommandBuilder.wifiForgetJson(), timeoutMs = 8000)
        Unit
    }

    private fun getText(path: String, timeoutMs: Long = 6000, longRead: Boolean = false): String {
        val http = if (longRead) longClient else timed(timeoutMs)
        val request = Request.Builder().url("$normalizedBase$path").get().build()
        http.newCall(request).execute().use { response ->
            val body = response.body?.string()
            if (!response.isSuccessful) {
                val err = StatusParser.errorFromBody(body).ifBlank { "HTTP ${response.code}" }
                throw IllegalStateException(err)
            }
            return body ?: throw IllegalStateException("empty body")
        }
    }

    private fun postText(path: String, json: String, timeoutMs: Long = 6000): String {
        val request = Request.Builder()
            .url("$normalizedBase$path")
            .post(if (json.isBlank()) emptyBody else json.toRequestBody(jsonType))
            .build()
        timed(timeoutMs).newCall(request).execute().use { response ->
            val body = response.body?.string().orEmpty()
            if (!response.isSuccessful) {
                val err = StatusParser.errorFromBody(body).ifBlank { "HTTP ${response.code}" }
                throw IllegalStateException(err)
            }
            return body
        }
    }

    private fun timed(timeoutMs: Long): OkHttpClient =
        client.newBuilder()
            .connectTimeout(timeoutMs, TimeUnit.MILLISECONDS)
            .readTimeout(timeoutMs, TimeUnit.MILLISECONDS)
            .writeTimeout(timeoutMs, TimeUnit.MILLISECONDS)
            .build()
}
