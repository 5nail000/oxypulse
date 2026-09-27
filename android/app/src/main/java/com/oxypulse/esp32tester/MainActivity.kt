package com.oxypulse.esp32tester

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Surface
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.core.content.ContextCompat
import androidx.hilt.navigation.compose.hiltViewModel
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.oxypulse.esp32tester.domain.TransportType
import com.oxypulse.esp32tester.ui.ConnectScreen
import com.oxypulse.esp32tester.ui.DashboardScreen
import com.oxypulse.esp32tester.ui.TesterViewModel
import com.oxypulse.esp32tester.ui.theme.Bg
import com.oxypulse.esp32tester.ui.theme.OxyTheme
import dagger.hilt.android.AndroidEntryPoint

@AndroidEntryPoint
class MainActivity : ComponentActivity() {

    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions(),
    ) { }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requestBlePermissions()

        setContent {
            OxyTheme {
                Surface(modifier = Modifier.fillMaxSize(), color = Bg) {
                    val viewModel: TesterViewModel = hiltViewModel()
                    val transport by viewModel.transport.collectAsStateWithLifecycle()
                    if (transport == TransportType.None) {
                        ConnectScreen(viewModel)
                    } else {
                        DashboardScreen(viewModel)
                    }
                }
            }
        }
    }

    private fun requestBlePermissions() {
        val permissions = mutableListOf<String>()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissions += Manifest.permission.BLUETOOTH_SCAN
            permissions += Manifest.permission.BLUETOOTH_CONNECT
        }
        if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.R) {
            permissions += Manifest.permission.ACCESS_FINE_LOCATION
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            permissions += Manifest.permission.POST_NOTIFICATIONS
        }
        val missing = permissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        if (missing.isNotEmpty()) {
            permissionLauncher.launch(missing.toTypedArray())
        }
    }
}
