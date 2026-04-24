package xyz.i12bp8.tagtinker.android.data

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import xyz.i12bp8.tagtinker.android.model.PayloadHistoryItem

class HistoryStore(context: Context) {
    private val prefs = context.getSharedPreferences("tagtinker_history", Context.MODE_PRIVATE)

    fun load(): List<PayloadHistoryItem> {
        val raw = prefs.getString("items", "[]") ?: "[]"
        val json = JSONArray(raw)
        return buildList {
            for (index in 0 until json.length()) {
                val item = json.getJSONObject(index)
                add(
                    PayloadHistoryItem(
                        id = item.getString("id"),
                        barcode = item.getString("barcode"),
                        name = item.getString("name"),
                        width = item.getInt("width"),
                        height = item.getInt("height"),
                        color = item.getString("color"),
                        page = item.getInt("page"),
                        bmpPath = item.getString("bmpPath"),
                        timestamp = item.getLong("timestamp"),
                    ),
                )
            }
        }
    }

    fun save(items: List<PayloadHistoryItem>) {
        val array = JSONArray()
        items.take(24).forEach { item ->
            array.put(
                JSONObject()
                    .put("id", item.id)
                    .put("barcode", item.barcode)
                    .put("name", item.name)
                    .put("width", item.width)
                    .put("height", item.height)
                    .put("color", item.color)
                    .put("page", item.page)
                    .put("bmpPath", item.bmpPath)
                    .put("timestamp", item.timestamp),
            )
        }
        prefs.edit().putString("items", array.toString()).apply()
    }
}
