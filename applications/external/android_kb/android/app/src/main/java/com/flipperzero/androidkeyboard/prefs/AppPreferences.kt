package com.flipperzero.androidkeyboard.prefs

import android.content.Context
import com.flipperzero.androidkeyboard.OutputMode
import com.flipperzero.androidkeyboard.keyboard.LayoutInfo

class AppPreferences(context: Context) {

    private val prefs = context.applicationContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

    var outputMode: OutputMode
        get() = when (prefs.getString(KEY_OUTPUT_MODE, OutputMode.FLIPPER.name)) {
            OutputMode.DIRECT_BT.name -> OutputMode.DIRECT_BT
            else -> OutputMode.FLIPPER
        }
        set(value) {
            prefs.edit().putString(KEY_OUTPUT_MODE, value.name).apply()
        }

    var flipperMac: String?
        get() = prefs.getString(KEY_FLIPPER_MAC, null)?.takeIf { it.isNotBlank() }
        set(value) {
            prefs.edit().putString(KEY_FLIPPER_MAC, value?.trim()?.uppercase()).apply()
        }

    var hostMac: String?
        get() = prefs.getString(KEY_HOST_MAC, null)?.takeIf { it.isNotBlank() }
        set(value) {
            prefs.edit().putString(KEY_HOST_MAC, value?.trim()?.uppercase()).apply()
        }

    /** Name shown to the PC when this phone advertises as a Bluetooth HID keyboard. */
    var hidDeviceName: String
        get() {
            val saved = prefs.getString(KEY_HID_DEVICE_NAME, null)?.trim().orEmpty()
            return saved.ifBlank { DEFAULT_HID_DEVICE_NAME }
        }
        set(value) {
            val cleaned = value.trim().take(MAX_HID_DEVICE_NAME_LEN)
            prefs.edit().putString(
                KEY_HID_DEVICE_NAME,
                cleaned.ifBlank { DEFAULT_HID_DEVICE_NAME },
            ).apply()
        }

    var currentLayoutId: String?
        get() = prefs.getString(KEY_CURRENT_LAYOUT, null)?.takeIf { it.isNotBlank() }
        set(value) {
            prefs.edit().putString(KEY_CURRENT_LAYOUT, value).apply()
        }

    fun enabledLayoutIds(catalog: List<LayoutInfo>): List<String> {
        val raw = prefs.getString(KEY_ENABLED_LAYOUTS, null)
        val catalogIds = catalog.map { it.id }
        if (raw.isNullOrBlank()) {
            return catalogIds
        }
        val saved = raw.split(',').map { it.trim() }.filter { it.isNotEmpty() }
        val enabled = saved.filter { it in catalogIds }
        return enabled.ifEmpty { catalogIds }
    }

    fun setEnabledLayoutIds(ids: List<String>) {
        prefs.edit().putString(KEY_ENABLED_LAYOUTS, ids.joinToString(",")).apply()
    }

    companion object {
        const val DEFAULT_HID_DEVICE_NAME = "Flipper KB Bridge"
        const val MAX_HID_DEVICE_NAME_LEN = 32

        private const val PREFS_NAME = "akb_prefs"
        private const val KEY_OUTPUT_MODE = "output_mode"
        private const val KEY_FLIPPER_MAC = "flipper_mac"
        private const val KEY_HOST_MAC = "host_mac"
        private const val KEY_HID_DEVICE_NAME = "hid_device_name"
        private const val KEY_ENABLED_LAYOUTS = "enabled_layouts"
        private const val KEY_CURRENT_LAYOUT = "current_layout"
    }
}
