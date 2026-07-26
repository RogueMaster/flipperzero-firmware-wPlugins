package com.flipperzero.androidkeyboard.keyboard

import android.content.Context
import android.content.res.Resources
import android.os.Build
import android.os.LocaleList
import android.view.inputmethod.InputMethodManager
import java.util.Locale

/**
 * Best-effort match of language packs to phone locales / enabled IME subtypes.
 * On many OEMs the system only exposes the primary UI language — Settings therefore
 * lists all selectable languages for the user to pick manually.
 */
object SystemLanguages {

    fun systemLocaleTags(context: Context): Set<String> {
        val tags = linkedSetOf<String>()

        fun addTag(raw: String?) {
            var tag = raw?.trim()?.lowercase(Locale.ROOT) ?: return
            if (tag.isBlank() || tag == "und" || tag == "zz") return
            tag = tag.replace('_', '-')
            tags += tag
            val lang = tag.substringBefore('-')
            if (lang.isNotBlank()) tags += lang
        }

        fun addLocale(locale: Locale?) {
            if (locale == null || locale == Locale.ROOT) return
            val lang = locale.language.lowercase(Locale.ROOT)
            if (lang.isNotBlank()) tags += lang
            addTag(locale.toLanguageTag())
            @Suppress("DEPRECATION")
            addTag(locale.toString())
        }

        fun addLocaleList(list: LocaleList?) {
            if (list == null) return
            for (i in 0 until list.size()) {
                addLocale(list[i])
            }
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            runCatching {
                val lm = context.getSystemService(android.app.LocaleManager::class.java)
                addLocaleList(lm?.systemLocales)
            }
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            addLocaleList(Resources.getSystem().configuration.locales)
            addLocaleList(context.applicationContext.resources.configuration.locales)
            addLocaleList(LocaleList.getDefault())
            addLocaleList(LocaleList.getAdjustedDefault())
        } else {
            @Suppress("DEPRECATION")
            addLocale(Resources.getSystem().configuration.locale)
            @Suppress("DEPRECATION")
            addLocale(context.resources.configuration.locale)
            addLocale(Locale.getDefault())
        }

        val imm = context.getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager
        if (imm != null) {
            for (imi in imm.enabledInputMethodList) {
                for (subtype in imm.getEnabledInputMethodSubtypeList(imi, true).orEmpty()) {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                        addTag(subtype.languageTag)
                    }
                    @Suppress("DEPRECATION")
                    addTag(subtype.locale)
                }
            }
        }

        return tags
    }

    fun packMatches(pack: LanguageInfo, tags: Set<String>): Boolean {
        return pack.locales.any { loc ->
            val needle = loc.lowercase(Locale.ROOT)
            tags.any { tag ->
                tag == needle || tag.startsWith("$needle-")
            }
        }
    }

    fun matchedPacks(context: Context, all: List<LanguageInfo>): List<LanguageInfo> {
        val tags = systemLocaleTags(context)
        return all.filter { packMatches(it, tags) }
    }

    fun availablePacks(context: Context, all: List<LanguageInfo>): List<LanguageInfo> {
        if (all.isEmpty()) return emptyList()
        val matchedIds = matchedPacks(context, all).map { it.id }.toSet()
        val matched = all.filter { it.id in matchedIds }
        val rest = all.filter { it.id !in matchedIds }
        return matched + rest
    }

    fun defaultEnabledIds(context: Context, all: List<LanguageInfo>): List<String> {
        val matched = matchedPacks(context, all).map { it.id }
        if (matched.isNotEmpty()) return matched
        return listOfNotNull(all.firstOrNull { it.id == "en" }?.id ?: all.firstOrNull()?.id)
    }
}
