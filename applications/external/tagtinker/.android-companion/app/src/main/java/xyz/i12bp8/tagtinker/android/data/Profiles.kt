package xyz.i12bp8.tagtinker.android.data

import xyz.i12bp8.tagtinker.android.model.TagProfile
import xyz.i12bp8.tagtinker.android.model.TagRecord

private val tagProfiles = listOf(
    TagProfile(1275, 320, 192, "graphic", "mono", "DM110"),
    TagProfile(1276, 320, 140, "graphic", "mono", "DM90"),
    TagProfile(1300, 172, 72, "graphic", "mono", "DM3370"),
    TagProfile(1314, 400, 300, "graphic", "mono", "SmartTag HD110"),
    TagProfile(1315, 296, 128, "graphic", "mono", "SmartTag HD L"),
    TagProfile(1317, 152, 152, "graphic", "mono", "SmartTag HD S"),
    TagProfile(1318, 208, 112, "graphic", "mono", "SmartTag HD M"),
    TagProfile(1319, 800, 480, "graphic", "mono", "SmartTag HD200"),
    TagProfile(1322, 152, 152, "graphic", "mono", "SmartTag HD S"),
    TagProfile(1324, 208, 112, "graphic", "mono", "SmartTag HD M FZ"),
    TagProfile(1327, 208, 112, "graphic", "red", "SmartTag HD M Red"),
    TagProfile(1328, 296, 128, "graphic", "red", "SmartTag HD L Red"),
    TagProfile(1336, 400, 300, "graphic", "red", "SmartTag HD110 Red"),
    TagProfile(1339, 152, 152, "graphic", "red", "SmartTag HD S Red"),
    TagProfile(1340, 800, 480, "graphic", "red", "SmartTag HD200 Red"),
    TagProfile(1344, 296, 128, "graphic", "yellow", "SmartTag HD L Yellow"),
    TagProfile(1346, 800, 480, "graphic", "yellow", "SmartTag HD200 Yellow"),
    TagProfile(1348, 264, 176, "graphic", "red", "SmartTag HD T Red"),
    TagProfile(1349, 264, 176, "graphic", "yellow", "SmartTag HD T Yellow"),
    TagProfile(1351, 648, 480, "graphic", "mono", "SmartTag HD150"),
    TagProfile(1353, 648, 480, "graphic", "red", "SmartTag HD150 Red"),
    TagProfile(1370, 296, 128, "graphic", "red", "SmartTag HD L Red 2021"),
    TagProfile(1371, 648, 480, "graphic", "red", "SmartTag HD150 Red 2021"),
    TagProfile(1627, 296, 128, "graphic", "red", "SmartTag HD L Red"),
    TagProfile(1628, 296, 128, "graphic", "red", "SmartTag HD L Red"),
    TagProfile(1639, 152, 152, "graphic", "red", "SmartTag HD S Red"),
)

fun normalizeBarcode(raw: String): String {
    val compact = raw.filter { it.isLetterOrDigit() }.uppercase()
    return when {
        compact.length >= 17 && compact.firstOrNull()?.isLetter() == true -> compact.take(17)
        compact.length == 16 && compact.all { it.isDigit() } -> "N$compact"
        else -> compact.take(17)
    }
}

fun isValidBarcode(barcode: String): Boolean {
    return barcode.length == 17 &&
        barcode.first().isLetter() &&
        barcode.drop(1).all { it.isDigit() }
}

fun fallbackProfile(type: Int = 0): TagProfile {
    return TagProfile(
        type = type,
        width = 296,
        height = 128,
        kind = "graphic",
        color = "mono",
        model = if (type == 0) "Unknown graphic tag" else "Unknown $type",
        known = false,
    )
}

fun profileFromBarcode(barcode: String): TagProfile {
    if (!isValidBarcode(barcode)) return fallbackProfile()
    val type = barcode.substring(12, 16).toIntOrNull() ?: 0
    return tagProfiles.firstOrNull { it.type == type } ?: fallbackProfile(type)
}

fun tagFromBarcode(raw: String): TagRecord {
    val barcode = normalizeBarcode(raw)
    val profile = profileFromBarcode(barcode)
    return TagRecord(
        barcode = barcode,
        name = profile.model.ifBlank { barcode },
        width = profile.width,
        height = profile.height,
        color = profile.color,
        model = profile.model,
        type = profile.type,
    )
}
