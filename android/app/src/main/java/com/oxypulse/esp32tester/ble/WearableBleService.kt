package com.oxypulse.esp32tester.ble

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import com.oxypulse.esp32tester.MainActivity
import com.oxypulse.esp32tester.R
import com.oxypulse.esp32tester.util.AppLogger
import dagger.hilt.android.AndroidEntryPoint
import javax.inject.Inject

@AndroidEntryPoint
class WearableBleService : Service() {

    @Inject lateinit var wearables: WearableBleManager

    override fun onCreate() {
        super.onCreate()
        createChannel()
        runCatching { startInForeground() }
            .onFailure { AppLogger.e("FGS foreground: ${it.message}") }
        wearables.start(allowConnect = true)
        AppLogger.i("Wearable BLE service created")
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        if (intent?.action == ACTION_STOP) {
            stopSelf()
            return START_NOT_STICKY
        }
        wearables.start(allowConnect = true)
        return START_STICKY
    }

    override fun onDestroy() {
        wearables.stop()
        AppLogger.i("Wearable BLE service destroyed")
        super.onDestroy()
    }

    override fun onBind(intent: Intent?): IBinder? = null

    private fun createChannel() {
        val manager = getSystemService(NotificationManager::class.java)
        val channel = NotificationChannel(
            CHANNEL_ID,
            getString(R.string.ble_fg_channel),
            NotificationManager.IMPORTANCE_LOW,
        )
        manager.createNotificationChannel(channel)
    }

    private fun startInForeground() {
        val launch = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE,
        )
        val notification = NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_launcher)
            .setContentTitle(getString(R.string.ble_fg_title))
            .setContentText(getString(R.string.ble_fg_text))
            .setContentIntent(launch)
            .setOngoing(true)
            .build()
        if (Build.VERSION.SDK_INT >= 34) {
            startForeground(
                NOTIFICATION_ID,
                notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE,
            )
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
    }

    companion object {
        private const val CHANNEL_ID = "oxypulse_ble"
        private const val NOTIFICATION_ID = 42
        const val ACTION_STOP = "com.oxypulse.esp32tester.STOP_BLE"

        fun start(context: Context) {
            val intent = Intent(context, WearableBleService::class.java)
            androidx.core.content.ContextCompat.startForegroundService(context, intent)
        }

        fun stop(context: Context) {
            context.stopService(Intent(context, WearableBleService::class.java))
        }
    }
}
