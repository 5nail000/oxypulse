package com.oxypulse.esp32tester.util

import android.util.Log
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

object AppLogger {
    private const val TAG = "Esp32Tester"
    private val formatter = SimpleDateFormat("HH:mm:ss.SSS", Locale.US)

    private fun prefix(): String = "[${formatter.format(Date())}]"

    fun i(message: String) = Log.i(TAG, "${prefix()} $message")
    fun d(message: String) = Log.d(TAG, "${prefix()} $message")
    fun w(message: String) = Log.w(TAG, "${prefix()} $message")
    fun e(message: String) = Log.e(TAG, "${prefix()} $message")
}
