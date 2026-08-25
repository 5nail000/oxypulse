package com.oxypulse.esp32tester.api

import com.oxypulse.esp32tester.domain.DeviceStatus

interface Esp32Api {
    suspend fun getStatus(): DeviceStatus
    suspend fun sendCommand(json: String)
}
