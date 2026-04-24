package xyz.i12bp8.tagtinker.android.model

import java.util.UUID

data class TagProfile(
    val type: Int,
    val width: Int,
    val height: Int,
    val kind: String,
    val color: String,
    val model: String,
    val known: Boolean = true,
)

data class TagRecord(
    val id: String = UUID.randomUUID().toString(),
    val barcode: String,
    val name: String,
    val width: Int,
    val height: Int,
    val color: String,
    val model: String,
    val type: Int,
)

enum class FitMode { Fit, Fill, Stretch }

enum class DitherMode { Floyd, Atkinson, Threshold }

data class EditorState(
    val fitMode: FitMode = FitMode.Fit,
    val ditherMode: DitherMode = DitherMode.Floyd,
    val detail: Int = 60,
    val scale: Float = 1f,
    val offsetX: Float = 0f,
    val offsetY: Float = 0f,
    val contrast: Int = 8,
    val brightness: Int = 0,
    val useColor: Boolean = false,
    val page: Int = 1,
)

data class PayloadHistoryItem(
    val id: String,
    val barcode: String,
    val name: String,
    val width: Int,
    val height: Int,
    val color: String,
    val page: Int,
    val bmpPath: String,
    val timestamp: Long,
)

data class BlePeripheral(
    val address: String,
    val name: String,
    val rssi: Int,
)

data class UploadJob(
    val jobId: String,
    val barcode: String,
    val width: Int,
    val height: Int,
    val page: Int,
    val bmpBytes: ByteArray,
)

data class UploadProgress(
    val bytesDone: Int,
    val bytesTotal: Int,
    val label: String,
)
