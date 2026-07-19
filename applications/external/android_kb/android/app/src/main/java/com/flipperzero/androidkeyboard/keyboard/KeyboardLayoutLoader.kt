package com.flipperzero.androidkeyboard.keyboard

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.BufferedReader

object KeyboardLayoutLoader {

    private const val CATALOG_ASSET = "layouts/catalog.json"

    fun loadCatalog(context: Context): List<LayoutInfo> {
        val json = readAsset(context, CATALOG_ASSET)
        val root = JSONObject(json)
        val arr = root.getJSONArray("layouts")
        val result = mutableListOf<LayoutInfo>()
        for (i in 0 until arr.length()) {
            val obj = arr.getJSONObject(i)
            result += LayoutInfo(
                id = obj.getString("id"),
                title = obj.getString("title"),
                file = obj.getString("file"),
            )
        }
        return result
    }

    fun loadLayout(context: Context, info: LayoutInfo): KeyboardLayout {
        return parse(readAsset(context, info.file), fallbackId = info.id)
    }

    fun loadLayoutById(context: Context, id: String): KeyboardLayout? {
        val info = loadCatalog(context).firstOrNull { it.id == id } ?: return null
        return loadLayout(context, info)
    }

    fun parse(json: String, fallbackId: String = "unnamed"): KeyboardLayout {
        val root = JSONObject(json)
        val id = root.optString("id", fallbackId).ifBlank { fallbackId }
        val name = root.optString("name", id)
        val rowsJson = root.getJSONArray("rows")
        val rows = mutableListOf<List<KeyboardKey>>()
        for (r in 0 until rowsJson.length()) {
            rows += parseRow(rowsJson.getJSONArray(r))
        }
        return KeyboardLayout(id = id, name = name, rows = rows)
    }

    private fun parseRow(rowJson: JSONArray): List<KeyboardKey> {
        val keys = mutableListOf<KeyboardKey>()
        for (i in 0 until rowJson.length()) {
            val obj = rowJson.getJSONObject(i)
            val role = obj.optString("role", "").takeIf { it.isNotBlank() }
            keys += KeyboardKey(
                label = obj.getString("label"),
                hid = parseHexByte(obj.getString("hid")),
                mods = parseHexByte(obj.optString("mods", "0x00")),
                span = obj.optDouble("span", 1.0).toFloat().coerceAtLeast(0.5f),
                stickyMod = obj.optBoolean("sticky_mod", false),
                role = role,
            )
        }
        return keys
    }

    private fun parseHexByte(raw: String): Byte {
        val cleaned = raw.trim().removePrefix("0x").removePrefix("0X")
        return cleaned.toInt(16).toByte()
    }

    private fun readAsset(context: Context, assetName: String): String {
        return context.assets.open(assetName).bufferedReader().use(BufferedReader::readText)
    }
}
