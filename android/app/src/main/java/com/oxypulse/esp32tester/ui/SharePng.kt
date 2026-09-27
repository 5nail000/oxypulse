package com.oxypulse.esp32tester.ui

import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import androidx.core.content.FileProvider
import com.oxypulse.esp32tester.util.AppLogger
import java.io.File
import java.io.FileOutputStream

object SharePng {
    fun share(context: Context, bitmap: Bitmap, filename: String) {
        val file = File(context.cacheDir, filename)
        FileOutputStream(file).use { out ->
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, out)
        }
        val uri = FileProvider.getUriForFile(context, "${context.packageName}.files", file)
        val intent = Intent(Intent.ACTION_SEND).apply {
            type = "image/png"
            putExtra(Intent.EXTRA_STREAM, uri)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        AppLogger.i("Share PNG $filename")
        context.startActivity(Intent.createChooser(intent, "Сохранить PNG"))
    }
}
