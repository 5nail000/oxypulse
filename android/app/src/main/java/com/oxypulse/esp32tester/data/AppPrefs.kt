package com.oxypulse.esp32tester.data

import android.content.Context
import com.oxypulse.esp32tester.domain.IgtLogic
import com.oxypulse.esp32tester.domain.IgtProtocol
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject
import javax.inject.Singleton
import org.json.JSONArray
import org.json.JSONObject

@Singleton
class AppPrefs @Inject constructor(
    @ApplicationContext context: Context,
) {
    private val prefs = context.getSharedPreferences("oxypulse", Context.MODE_PRIVATE)

    fun lastHost(): String = prefs.getString(KEY_HOST, "192.168.4.1") ?: "192.168.4.1"

    fun setLastHost(host: String) {
        prefs.edit().putString(KEY_HOST, host).apply()
    }

    fun hrViewBelt(): Boolean = prefs.getBoolean(KEY_HR_BELT, true)
    fun setHrViewBelt(value: Boolean) = prefs.edit().putBoolean(KEY_HR_BELT, value).apply()

    fun hrViewWellue(): Boolean = prefs.getBoolean(KEY_HR_WELLUE, true)
    fun setHrViewWellue(value: Boolean) = prefs.edit().putBoolean(KEY_HR_WELLUE, value).apply()

    fun flowDynamics(): Boolean = prefs.getBoolean(KEY_FLOW_DYN, false)
    fun setFlowDynamics(value: Boolean) = prefs.edit().putBoolean(KEY_FLOW_DYN, value).apply()

    fun loadCustomProtocols(): List<IgtProtocol> {
        val raw = prefs.getString(KEY_IGT, "[]") ?: "[]"
        return try {
            val arr = JSONArray(raw)
            (0 until arr.length()).mapNotNull { i ->
                val o = arr.optJSONObject(i) ?: return@mapNotNull null
                val id = o.optString("id")
                val name = o.optString("name")
                if (id.isBlank() || name.isBlank() || o.optBoolean("builtin")) return@mapNotNull null
                IgtProtocol(
                    id = id,
                    name = name,
                    o2Target = o.optDouble("o2_target", 15.0).toFloat(),
                    hypoxiaMin = o.optInt("hypoxia_min", 6),
                    normoxiaMin = o.optInt("normoxia_min", 5),
                    cycles = o.optInt("cycles", 6),
                    builtin = false,
                )
            }
        } catch (_: Exception) {
            emptyList()
        }
    }

    fun saveCustomProtocols(list: List<IgtProtocol>) {
        val arr = JSONArray()
        for (p in list.filter { !it.builtin }) {
            arr.put(
                JSONObject().apply {
                    put("id", p.id)
                    put("name", p.name)
                    put("o2_target", p.o2Target.toDouble())
                    put("hypoxia_min", p.hypoxiaMin)
                    put("normoxia_min", p.normoxiaMin)
                    put("cycles", p.cycles)
                },
            )
        }
        prefs.edit().putString(KEY_IGT, arr.toString()).apply()
    }

    fun lastWellueAddress(): String = prefs.getString(KEY_WELLUE, "") ?: ""
    fun setLastWellueAddress(value: String) = prefs.edit().putString(KEY_WELLUE, value).apply()

    fun lastCoospoAddress(): String = prefs.getString(KEY_COOSPO, "") ?: ""
    fun setLastCoospoAddress(value: String) = prefs.edit().putString(KEY_COOSPO, value).apply()

    fun allProtocols(): List<IgtProtocol> = listOf(IgtLogic.BUILTIN) + loadCustomProtocols()

    companion object {
        private const val KEY_HOST = "host"
        private const val KEY_HR_BELT = "hr_view_belt"
        private const val KEY_HR_WELLUE = "hr_view_wellue"
        private const val KEY_FLOW_DYN = "flow_dynamics"
        private const val KEY_IGT = "igt_protocols"
        private const val KEY_WELLUE = "wellue_addr"
        private const val KEY_COOSPO = "coospo_addr"
    }
}
