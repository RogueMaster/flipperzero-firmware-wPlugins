package com.flipperzero.androidkeyboard.prefs

import android.content.Context
import com.flipperzero.androidkeyboard.OutputMode
import com.flipperzero.androidkeyboard.keyboard.KeyboardLayoutLoader
import com.flipperzero.androidkeyboard.keyboard.SystemLanguages
import com.flipperzero.androidkeyboard.keyboard.composedLayoutId

class AppPreferences(context: Context) {

    private val appContext = context.applicationContext
    private val prefs = appContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)

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

    var templateId: String
        get() {
            migrateLegacyLayoutsIfNeeded()
            val saved = prefs.getString(KEY_TEMPLATE_ID, null)?.takeIf { it.isNotBlank() }
            if (saved != null) return normalizeTemplateId(saved)
            return KeyboardLayoutLoader.loadTemplates(appContext).firstOrNull()?.id ?: "macos"
        }
        set(value) {
            prefs.edit().putString(KEY_TEMPLATE_ID, normalizeTemplateId(value)).apply()
        }

    var currentLayoutId: String?
        get() {
            migrateLegacyLayoutsIfNeeded()
            val raw = prefs.getString(KEY_CURRENT_LAYOUT, null)?.takeIf { it.isNotBlank() }
                ?: return null
            return normalizeComposedLayoutId(raw)
        }
        set(value) {
            prefs.edit().putString(
                KEY_CURRENT_LAYOUT,
                value?.let { normalizeComposedLayoutId(it) },
            ).apply()
        }

    fun enabledLanguageIds(): List<String> {
        migrateLegacyLayoutsIfNeeded()
        val all = KeyboardLayoutLoader.loadSelectableLanguages(appContext)
        val availableIds = all.map { it.id }.toSet()
        val raw = prefs.getString(KEY_ENABLED_LANGUAGES, null)
        if (raw.isNullOrBlank()) {
            return SystemLanguages.defaultEnabledIds(appContext, KeyboardLayoutLoader.loadLanguageCatalog(appContext))
        }
        val saved = raw.split(',').map { it.trim() }.filter { it.isNotEmpty() }
        val enabled = saved.filter { it in availableIds }
        return enabled.ifEmpty {
            SystemLanguages.defaultEnabledIds(appContext, KeyboardLayoutLoader.loadLanguageCatalog(appContext))
        }
    }

    fun setEnabledLanguageIds(ids: List<String>) {
        prefs.edit().putString(KEY_ENABLED_LANGUAGES, ids.joinToString(",")).apply()
    }

    /** Show current language large and the next enabled language small on letter keys. */
    var showDualLanguageLabels: Boolean
        get() = prefs.getBoolean(KEY_DUAL_LABELS, false)
        set(value) {
            prefs.edit().putBoolean(KEY_DUAL_LABELS, value).apply()
        }

    private fun migrateLegacyLayoutsIfNeeded() {
        if (prefs.contains(KEY_TEMPLATE_ID) || prefs.contains(KEY_ENABLED_LANGUAGES)) {
            return
        }
        val legacy = prefs.getString(KEY_ENABLED_LAYOUTS, null) ?: return
        val parts = legacy.split(',').map { it.trim() }.filter { it.isNotEmpty() }
        if (parts.isEmpty()) return

        var template = "macos"
        val langs = linkedSetOf<String>()
        for (id in parts) {
            when {
                id.startsWith("mx_mini") -> {
                    template = "macos"
                    langs += id.substringAfterLast('_')
                }
                id.startsWith("macos_") -> {
                    template = "macos"
                    langs += id.removePrefix("macos_")
                }
                id.startsWith("pc_") -> {
                    template = "pc"
                    langs += id.removePrefix("pc_")
                }
                id == "number" -> template = "number"
            }
        }
        val editor = prefs.edit().putString(KEY_TEMPLATE_ID, template)
        if (langs.isNotEmpty()) {
            editor.putString(KEY_ENABLED_LANGUAGES, langs.joinToString(","))
        }
        val current = prefs.getString(KEY_CURRENT_LAYOUT, null)
        if (current != null && ':' !in current && '_' in current) {
            val lang = when {
                current.endsWith("_ru") -> "ru"
                current.endsWith("_en") -> "en"
                else -> null
            }
            val tid = when {
                current.startsWith("mx_mini") -> "macos"
                current.startsWith("macos") -> "macos"
                current.startsWith("pc") -> "pc"
                current == "number" -> "number"
                else -> template
            }
            editor.putString(KEY_CURRENT_LAYOUT, composedLayoutId(tid, lang))
        }
        editor.apply()
    }

    companion object {
        const val DEFAULT_HID_DEVICE_NAME = "Flipper KB Bridge"
        const val MAX_HID_DEVICE_NAME_LEN = 32

        private const val PREFS_NAME = "akb_prefs"
        private const val KEY_OUTPUT_MODE = "output_mode"
        private const val KEY_FLIPPER_MAC = "flipper_mac"
        private const val KEY_HOST_MAC = "host_mac"
        private const val KEY_HID_DEVICE_NAME = "hid_device_name"
        private const val KEY_ENABLED_LAYOUTS = "enabled_layouts" // legacy
        private const val KEY_ENABLED_LANGUAGES = "enabled_languages"
        private const val KEY_TEMPLATE_ID = "template_id"
        private const val KEY_CURRENT_LAYOUT = "current_layout"
        private const val KEY_DUAL_LABELS = "dual_language_labels"

        /** Old template ids → current bundled ids. */
        private fun normalizeTemplateId(id: String): String = when (id) {
            "default", "mx_mini" -> "macos"
            else -> id
        }

        private fun normalizeComposedLayoutId(id: String): String {
            val colon = id.indexOf(':')
            if (colon < 0) return normalizeTemplateId(id)
            val template = normalizeTemplateId(id.substring(0, colon))
            val lang = id.substring(colon + 1)
            return composedLayoutId(template, lang)
        }
    }
}
