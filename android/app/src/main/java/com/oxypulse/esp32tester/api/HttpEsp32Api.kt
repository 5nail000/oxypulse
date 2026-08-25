package com.oxypulse.esp32tester.api

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
    private val client: OkHttpClient = OkHttpClient.Builder()
        .connectTimeout(5, TimeUnit.SECONDS)
        .readTimeout(5, TimeUnit.SECONDS)
        .writeTimeout(5, TimeUnit.SECONDS)
        .build(),
) : Esp32Api {

    private val normalizedBase = baseUrl.trimEnd('/')

    override suspend fun getStatus() = withContext(Dispatchers.IO) {
        val request = Request.Builder()
            .url("$normalizedBase/api/status")
            .get()
            .build()
        client.newCall(request).execute().use { response ->
            if (!response.isSuccessful) {
                throw IllegalStateException("HTTP ${response.code}")
            }
            val body = response.body?.string() ?: throw IllegalStateException("empty body")
            StatusParser.parse(body)
        }
    }

    override suspend fun sendCommand(json: String) = withContext(Dispatchers.IO) {
        AppLogger.d("HTTP cmd: $json")
        val body = json.toRequestBody("application/json".toMediaType())
        val request = Request.Builder()
            .url("$normalizedBase/api/cmd")
            .post(body)
            .build()
        client.newCall(request).execute().use { response ->
            if (!response.isSuccessful) {
                throw IllegalStateException("HTTP ${response.code}")
            }
        }
    }
}
