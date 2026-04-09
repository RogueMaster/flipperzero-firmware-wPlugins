package com.i12bp8.tagtinker.companion

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattService
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.pm.PackageManager
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.RectF
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Base64
import android.util.Log
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.unit.dp
import com.google.mlkit.vision.codescanner.GmsBarcodeScannerOptions
import com.google.mlkit.vision.codescanner.GmsBarcodeScanning
import java.util.UUID
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean

data class EslSize(val width: Int, val height: Int)

@Composable
private fun CompactSliderRow(
    label: String,
    valueText: String,
    value: Float,
    onValueChange: (Float) -> Unit,
    valueRange: ClosedFloatingPointRange<Float>,
) {
    Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically,
        ) {
            Text(label, style = MaterialTheme.typography.bodySmall)
            Text(valueText, style = MaterialTheme.typography.bodySmall)
        }
        Slider(
            value = value,
            onValueChange = onValueChange,
            valueRange = valueRange,
            modifier = Modifier.fillMaxWidth().heightIn(min = 20.dp),
        )
    }
}

class MainActivity : ComponentActivity() {
    private data class ScanCandidate(
        val device: BluetoothDevice,
        var bestRssi: Int,
        var hasService: Boolean,
        var looksLikeFlipper: Boolean,
        val bonded: Boolean,
    )

    private data class PendingLine(val line: String, val ackToken: String?, var attempts: Int = 0)

    private data class ImageUploadJob(
        val jobId: String,
        val barcode: String,
        val width: Int,
        val height: Int,
        val page: Int,
        val bmpBytes: ByteArray,
    )

    private data class SavedTarget(
        val barcode: String,
        val name: String,
        val width: Int,
        val height: Int,
    )

    private val serialServiceUuid = UUID.fromString("8FE5B3D5-2E7F-4A98-2A48-7ACC60FE0000")
    private val serialWriteUuid = UUID.fromString("19ED82AE-ED21-4C9D-4145-228E62FE0000")
    private val serialNotifyUuid = UUID.fromString("19ED82AE-ED21-4C9D-4145-228E61FE0000")
    private val serialFlowUuid = UUID.fromString("19ED82AE-ED21-4C9D-4145-228E63FE0000")
    private val serialStatusUuid = UUID.fromString("19ED82AE-ED21-4C9D-4145-228E64FE0000")
    private val serialFlowWindow = 8192
    private val maxCompactChunkBytes = 381
    private val cccdUuid = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    private val logTag = "TagTinkerBle"

    private val bleHandler = Handler(Looper.getMainLooper())
    private val pickedImageUri = mutableStateOf<Uri?>(null)
    private val txQueue = ArrayDeque<PendingLine>()

    private var gatt: BluetoothGatt? = null
    private var connectedAddress: String = ""
    private var writeChars: List<BluetoothGattCharacteristic> = emptyList()
    private var notifyChars: List<BluetoothGattCharacteristic> = emptyList()
    private var writeChar: BluetoothGattCharacteristic? = null
    private var flowChar: BluetoothGattCharacteristic? = null
    private var lastBleStatus = "Connecting..."
    private val bleRxAsciiBuffer = StringBuilder()

    private var bleStatusSink: (String) -> Unit = {}
    private var bleReadySink: (Boolean) -> Unit = {}
    private var targetListSink: (List<SavedTarget>) -> Unit = {}
    private var selectedBarcodeSink: (String) -> Unit = {}

    private var bleReadyState = false
    private var candidateSearchInProgress = false
    private var pendingCandidates = mutableListOf<BluetoothDevice>()
    private var notifySubscribeIndex = 0
    private var txInFlight: PendingLine? = null
    private var txDoneCallback: ((Boolean, String) -> Unit)? = null
    private var negotiatedMtu = 23
    @Volatile private var flowCredits = 0

    @Volatile private var writeLatch: CountDownLatch? = null
    @Volatile private var writeStatusOk: AtomicBoolean? = null
    @Volatile private var mtuLatch: CountDownLatch? = null
    @Volatile private var helloSeen = AtomicBoolean(false)
    @Volatile private var pongSeen = AtomicBoolean(false)
    private val remoteTargetsCache = mutableListOf<SavedTarget>()
    private val remoteTargetsBuffer = mutableListOf<SavedTarget>()
    private var targetSyncRequested = false
    private var lastSelectedBarcode = ""

    private val permissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) {}
    private val imagePicker =
        registerForActivityResult(ActivityResultContracts.GetContent()) { uri: Uri? ->
            pickedImageUri.value = uri
        }

    private fun ensurePermissions() {
        permissionLauncher.launch(
            arrayOf(
                Manifest.permission.CAMERA,
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT,
                Manifest.permission.ACCESS_FINE_LOCATION,
            ),
        )
    }

    private fun inferSizeFromBarcode(barcode: String): EslSize {
        if(barcode.length != 17) return EslSize(296, 128)
        val typeCode = barcode.substring(12, 16).toIntOrNull() ?: return EslSize(296, 128)
        return when(typeCode) {
            1300 -> EslSize(172, 72)
            1276 -> EslSize(320, 140)
            1275 -> EslSize(320, 192)
            1317, 1322, 1339, 1639 -> EslSize(152, 152)
            1318, 1327, 1324 -> EslSize(208, 112)
            1315, 1328, 1370, 1627, 1628, 1344 -> EslSize(296, 128)
            1348, 1349 -> EslSize(264, 176)
            1314, 1336 -> EslSize(400, 300)
            1351, 1353, 1354, 1371 -> EslSize(648, 480)
            1319, 1340, 1346 -> EslSize(800, 480)
            else -> EslSize(296, 128)
        }
    }

    private fun isValidBarcode(barcode: String): Boolean =
        barcode.length == 17 &&
            barcode.firstOrNull()?.isLetter() == true &&
            barcode.drop(1).all { it.isDigit() }

    private fun normalizeBarcode(raw: String): String {
        val compact = raw.filter { it.isLetterOrDigit() }.uppercase()
        return when {
            compact.length >= 17 &&
                compact.first().isLetter() &&
                compact.substring(1).all { it.isDigit() } -> compact.take(17)
            compact.length == 16 && compact.all { it.isDigit() } -> "N$compact"
            else -> compact.take(17)
        }
    }

    private fun eslSizeForBarcode(barcode: String): EslSize {
        val saved = remoteTargetsCache.firstOrNull { it.barcode == barcode }
        if(saved != null && saved.width > 0 && saved.height > 0) {
            return EslSize(saved.width, saved.height)
        }
        return inferSizeFromBarcode(barcode)
    }

    private fun applyRemoteTargets(targets: List<SavedTarget>) {
        remoteTargetsCache.clear()
        remoteTargetsCache.addAll(targets)
        runOnUiThread { targetListSink(targets) }

        val preferred =
            when {
                lastSelectedBarcode.isNotBlank() &&
                    targets.any { it.barcode == lastSelectedBarcode } -> lastSelectedBarcode
                targets.isNotEmpty() -> targets.first().barcode
                else -> ""
            }
        if(preferred.isNotEmpty()) {
            lastSelectedBarcode = preferred
            runOnUiThread { selectedBarcodeSink(preferred) }
        }
    }

    private fun requestTargetsSync(force: Boolean = false) {
        if(!bleReadyState || gatt == null) return
        if(txInFlight != null || txQueue.isNotEmpty()) return
        if(!force && targetSyncRequested && remoteTargetsCache.isNotEmpty()) return

        remoteTargetsBuffer.clear()
        if(writeLine("TT_LIST_TARGETS")) {
            targetSyncRequested = true
        }
    }

    private fun parseTargetLine(msg: String) {
        val parts = msg.split('|', limit = 5)
        if(parts.size != 5) return

        val barcode = normalizeBarcode(parts[1])
        if(!isValidBarcode(barcode)) return

        val fallbackSize = inferSizeFromBarcode(barcode)
        val target =
            SavedTarget(
                barcode = barcode,
                name = parts[2].trim().ifBlank { barcode },
                width = parts[3].toIntOrNull() ?: fallbackSize.width,
                height = parts[4].toIntOrNull() ?: fallbackSize.height,
            )

        remoteTargetsBuffer.removeAll { it.barcode == barcode }
        remoteTargetsBuffer.add(target)
    }

    private fun loadBitmap(uri: Uri): Bitmap? =
        contentResolver.openInputStream(uri)?.use { BitmapFactory.decodeStream(it) }

    private fun pricehaxThreshold(detail: Float): Int =
        (128 + ((detail - 50f) * 1.2f).toInt()).coerceIn(0, 255)

    private fun transformBitmapForFrame(
        bitmap: Bitmap,
        target: EslSize,
        detail: Float,
        scale: Float,
        offsetX: Float,
        offsetY: Float,
    ): Bitmap {
        val frame = Bitmap.createBitmap(target.width, target.height, Bitmap.Config.ARGB_8888)
        val canvas = Canvas(frame)
        val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply { isFilterBitmap = true }
        canvas.drawColor(0xFFFFFFFF.toInt())

        val fitScale =
            minOf(
                target.width.toFloat() / bitmap.width.toFloat(),
                target.height.toFloat() / bitmap.height.toFloat(),
            )
        val appliedScale = (fitScale * scale).coerceAtLeast(0.05f)
        val drawWidth = (bitmap.width * appliedScale).toInt().coerceAtLeast(1)
        val drawHeight = (bitmap.height * appliedScale).toInt().coerceAtLeast(1)
        val left = ((target.width - drawWidth) / 2f) + (offsetX * target.width * 0.5f)
        val top = ((target.height - drawHeight) / 2f) + (offsetY * target.height * 0.5f)

        canvas.drawBitmap(
            bitmap,
            Rect(0, 0, bitmap.width, bitmap.height),
            RectF(left, top, left + drawWidth, top + drawHeight),
            paint,
        )

        val threshold = pricehaxThreshold(detail)
        val output = Bitmap.createBitmap(target.width, target.height, Bitmap.Config.ARGB_8888)
        for(y in 0 until target.height) {
            for(x in 0 until target.width) {
                val color = frame.getPixel(x, y)
                val gray =
                    ((0.299f * ((color shr 16) and 0xFF)) +
                        (0.587f * ((color shr 8) and 0xFF)) +
                        (0.114f * (color and 0xFF))).toInt()
                output.setPixel(
                    x,
                    y,
                    if(gray > threshold) 0xFFFFFFFF.toInt() else 0xFF000000.toInt(),
                )
            }
        }
        return output
    }

    private fun toBwPricehax(
        bitmap: Bitmap,
        target: EslSize,
        detail: Float,
        scale: Float,
        offsetX: Float,
        offsetY: Float,
    ): Bitmap = transformBitmapForFrame(bitmap, target, detail, scale, offsetX, offsetY)

    private fun writeIntLe(buffer: ByteArray, offset: Int, value: Int) {
        buffer[offset + 0] = (value and 0xFF).toByte()
        buffer[offset + 1] = ((value ushr 8) and 0xFF).toByte()
        buffer[offset + 2] = ((value ushr 16) and 0xFF).toByte()
        buffer[offset + 3] = ((value ushr 24) and 0xFF).toByte()
    }

    private fun writeShortLe(buffer: ByteArray, offset: Int, value: Int) {
        buffer[offset + 0] = (value and 0xFF).toByte()
        buffer[offset + 1] = ((value ushr 8) and 0xFF).toByte()
    }

    private fun buildMonochromeBmp(bitmap: Bitmap): ByteArray {
        val width = bitmap.width
        val height = bitmap.height
        val rowStride = ((width + 31) / 32) * 4
        val pixelBytes = rowStride * height
        val dataOffset = 14 + 40 + 8
        val fileSize = dataOffset + pixelBytes
        val output = ByteArray(fileSize)

        output[0] = 'B'.code.toByte()
        output[1] = 'M'.code.toByte()
        writeIntLe(output, 2, fileSize)
        writeIntLe(output, 10, dataOffset)
        writeIntLe(output, 14, 40)
        writeIntLe(output, 18, width)
        writeIntLe(output, 22, height)
        writeShortLe(output, 26, 1)
        writeShortLe(output, 28, 1)
        writeIntLe(output, 34, pixelBytes)

        output[54] = 0x00
        output[55] = 0x00
        output[56] = 0x00
        output[57] = 0x00
        output[58] = 0xFF.toByte()
        output[59] = 0xFF.toByte()
        output[60] = 0xFF.toByte()
        output[61] = 0x00

        var dst = dataOffset
        for(y in height - 1 downTo 0) {
            val rowStart = dst
            var packedByte = 0
            var bitIndex = 0

            for(x in 0 until width) {
                val isWhite = (bitmap.getPixel(x, y) and 0x00FFFFFF) != 0
                if(isWhite) {
                    packedByte = packedByte or (1 shl (7 - bitIndex))
                }
                bitIndex++
                if(bitIndex == 8) {
                    output[dst++] = packedByte.toByte()
                    packedByte = 0
                    bitIndex = 0
                }
            }

            if(bitIndex != 0) {
                output[dst++] = packedByte.toByte()
            }

            while(dst - rowStart < rowStride) {
                output[dst++] = 0
            }
        }

        return output
    }

    private fun gattStatusString(status: Int): String =
        when(status) {
            BluetoothGatt.GATT_SUCCESS -> "GATT_SUCCESS"
            BluetoothGatt.GATT_FAILURE -> "GATT_FAILURE"
            else -> "status=$status"
        }

    private fun canWrite(ch: BluetoothGattCharacteristic): Boolean =
        (ch.properties and BluetoothGattCharacteristic.PROPERTY_WRITE) != 0 ||
            (ch.properties and BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE) != 0

    private fun canNotify(ch: BluetoothGattCharacteristic): Boolean =
        (ch.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY) != 0 ||
            (ch.properties and BluetoothGattCharacteristic.PROPERTY_INDICATE) != 0

    private fun characteristicProps(ch: BluetoothGattCharacteristic): String {
        val props = mutableListOf<String>()
        val value = ch.properties
        if((value and BluetoothGattCharacteristic.PROPERTY_READ) != 0) props.add("READ")
        if((value and BluetoothGattCharacteristic.PROPERTY_WRITE) != 0) props.add("WRITE")
        if((value and BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE) != 0) {
            props.add("WRITE_NR")
        }
        if((value and BluetoothGattCharacteristic.PROPERTY_NOTIFY) != 0) props.add("NOTIFY")
        if((value and BluetoothGattCharacteristic.PROPERTY_INDICATE) != 0) props.add("INDICATE")
        return props.joinToString("|").ifEmpty { "none" }
    }

    private fun setBleStatus(status: String) {
        lastBleStatus = status
        Log.d(logTag, status)
        runOnUiThread { bleStatusSink(status) }
    }

    private fun selectPreferredWriteChar(force: Boolean = false): BluetoothGattCharacteristic? {
        if(!force && writeChar != null) return writeChar
        val selected = writeChars.firstOrNull()
        if(selected != null && selected != writeChar) {
            Log.d(
                logTag,
                "Selected write ${selected.uuid} props=${characteristicProps(selected)}",
            )
        }
        writeChar = selected
        return selected
    }

    private fun findSerialWriteChars(service: BluetoothGattService): List<BluetoothGattCharacteristic> {
        service.getCharacteristic(serialWriteUuid)?.takeIf(::canWrite)?.let { exact ->
            return listOf(exact)
        }
        return emptyList()
    }

    private fun findSerialNotifyChars(
        service: BluetoothGattService,
    ): List<BluetoothGattCharacteristic> {
        val chars = mutableListOf<BluetoothGattCharacteristic>()
        service.getCharacteristic(serialNotifyUuid)?.takeIf(::canNotify)?.let { chars.add(it) }
        service.getCharacteristic(serialFlowUuid)?.takeIf(::canNotify)?.let {
            if(!chars.contains(it)) chars.add(it)
        }
        return chars
    }

    private fun decodeFlowCredits(bytes: ByteArray?): Int? {
        if(bytes == null || bytes.size < 4) return null
        return ((bytes[0].toInt() and 0xFF) shl 24) or
            ((bytes[1].toInt() and 0xFF) shl 16) or
            ((bytes[2].toInt() and 0xFF) shl 8) or
            (bytes[3].toInt() and 0xFF)
    }

    private fun setReady(ready: Boolean) {
        bleReadyState = ready
        runOnUiThread { bleReadySink(ready) }
    }

    private fun failSend(message: String) {
        txQueue.clear()
        txInFlight = null
        val callback = txDoneCallback
        txDoneCallback = null
        callback?.invoke(false, message)
    }

    @SuppressLint("MissingPermission")
    private fun disconnectGatt() {
        try {
            gatt?.disconnect()
        } catch(_: Throwable) {
        }
        try {
            gatt?.close()
        } catch(_: Throwable) {
        }
        gatt = null
        negotiatedMtu = 23
        writeChars = emptyList()
        notifyChars = emptyList()
        notifySubscribeIndex = 0
        writeChar = null
        flowChar = null
        flowCredits = 0
        targetSyncRequested = false
        remoteTargetsBuffer.clear()
        bleRxAsciiBuffer.setLength(0)
        txQueue.clear()
        txInFlight = null
        txDoneCallback = null
    }

    private fun onBleLine(msg: String) {
        if(msg == "TT_HELLO") {
            helloSeen.set(true)
            setBleStatus("BLE ready")
            setReady(true)
            return
        }

        if(msg == "TT_PONG") {
            pongSeen.set(true)
            setBleStatus("BLE ready")
            setReady(true)
            return
        }

        if(msg.startsWith("TT_TARGETS_BEGIN|")) {
            remoteTargetsBuffer.clear()
            return
        }

        if(msg.startsWith("TT_TARGET|")) {
            parseTargetLine(msg)
            return
        }

        if(msg == "TT_TARGETS_END") {
            targetSyncRequested = false
            applyRemoteTargets(remoteTargetsBuffer.toList())
            return
        }

        if(msg == "AB" || msg == "AT" || msg == "AE" ||
            (msg.length == 5 && msg.startsWith("A"))) {
            val current = txInFlight
            if(current != null && current.ackToken == msg) {
                txInFlight = null
                pumpTxQueue()
                setBleStatus(
                    when(msg) {
                        "AB" -> "Upload started"
                        "AT" -> "Target linked"
                        "AE" -> "Saved on Flipper"
                        else -> "Uploading..."
                    },
                )
                return
            }
        }

        if(msg.startsWith("TT_ACK|")) {
            val token = msg.removePrefix("TT_ACK|").split('|').firstOrNull().orEmpty()
            val current = txInFlight
            if(current != null && current.ackToken == token) {
                txInFlight = null
                pumpTxQueue()
                setBleStatus(
                    when(token) {
                        "BEGIN" -> "Upload started"
                        "END" -> "Saved on Flipper"
                        else -> "Uploading..."
                    },
                )
                return
            }
        }

        setBleStatus(msg)
    }

    private fun onBlePayload(bytes: ByteArray?) {
        val payload = bytes ?: return
        var ignoredControl = false

        for(byte in payload) {
            val value = byte.toInt() and 0xFF
            when(value) {
                '\n'.code, '\r'.code -> {
                    if(bleRxAsciiBuffer.isNotEmpty()) {
                        val line = bleRxAsciiBuffer.toString().trim()
                        bleRxAsciiBuffer.setLength(0)
                        if(line.isNotEmpty()) onBleLine(line)
                    }
                }
                in 0x20..0x7E -> bleRxAsciiBuffer.append(value.toChar())
                else -> ignoredControl = true
            }
        }

        if(ignoredControl) {
            Log.d(
                logTag,
                "Ignored control bytes ${payload.joinToString(" ") { "%02X".format(it.toInt() and 0xFF) }}",
            )
        }
    }

    private fun subscribeOne(g: BluetoothGatt, notify: BluetoothGattCharacteristic): Boolean {
        if(!g.setCharacteristicNotification(notify, true)) return false
        val cccd = notify.getDescriptor(cccdUuid) ?: return false
        cccd.value =
            if((notify.properties and BluetoothGattCharacteristic.PROPERTY_INDICATE) != 0 &&
                (notify.properties and BluetoothGattCharacteristic.PROPERTY_NOTIFY) == 0) {
                BluetoothGattDescriptor.ENABLE_INDICATION_VALUE
            } else {
                BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
            }
        return g.writeDescriptor(cccd)
    }

    @SuppressLint("MissingPermission")
    private fun tryNextCandidate() {
        if(pendingCandidates.isEmpty()) {
            candidateSearchInProgress = false
            setBleStatus("No Flipper serial device found")
            return
        }

        val next = pendingCandidates.removeAt(0)
        setBleStatus("Trying ${next.name ?: next.address}")
        connectToDevice(next)
    }

    @SuppressLint("MissingPermission")
    private fun enableNotifications(g: BluetoothGatt, notifies: List<BluetoothGattCharacteristic>) {
        if(notifies.isEmpty()) {
            setBleStatus("No notify characteristic")
            setReady(false)
            return
        }

        notifySubscribeIndex = 0
        if(!subscribeOne(g, notifies.first())) {
            setBleStatus("Notification subscribe failed")
            setReady(false)
        }
    }

    private fun maxWritePayload(): Int = (negotiatedMtu - 3).coerceAtLeast(20)

    private fun consumeFlowCredits(count: Int) {
        if(count <= 0) return
        flowCredits = (flowCredits - count).coerceAtLeast(0)
        Log.d(logTag, "Flow credits -> $flowCredits")
    }

    private fun writeLineToCharacteristic(ch: BluetoothGattCharacteristic, line: String): Boolean {
        val g = gatt ?: return false
        val payload = (line + "\n").toByteArray(Charsets.UTF_8)
        if(payload.size > maxWritePayload()) {
            setBleStatus("BLE packet too large")
            return false
        }
        if(flowCredits in 1 until payload.size) {
            setBleStatus("Waiting for flow credit")
            return false
        }
        if(flowCredits == 0) {
            setBleStatus("Waiting for flow credit")
            return false
        }

        val supportsWrite = (ch.properties and BluetoothGattCharacteristic.PROPERTY_WRITE) != 0
        val supportsWriteNr =
            (ch.properties and BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE) != 0
        if(!supportsWrite && !supportsWriteNr) return false

        ch.value = payload
        ch.writeType =
            if(supportsWrite) {
                BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            } else {
                BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
            }

        if(ch.writeType == BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE) {
            val started = g.writeCharacteristic(ch)
            if(started) consumeFlowCredits(payload.size)
            return started
        }

        val latch = CountDownLatch(1)
        val ok = AtomicBoolean(false)
        writeLatch = latch
        writeStatusOk = ok
        val started = g.writeCharacteristic(ch)
        if(!started) {
            writeLatch = null
            writeStatusOk = null
            setBleStatus("Write start failed")
            return false
        }

        val completed = latch.await(1500, TimeUnit.MILLISECONDS)
        writeLatch = null
        writeStatusOk = null
        val success = completed && ok.get()
        if(success) consumeFlowCredits(payload.size)
        return success
    }

    private fun orderedWriteChars(): List<BluetoothGattCharacteristic> {
        val ordered = mutableListOf<BluetoothGattCharacteristic>()
        writeChar?.let { ordered.add(it) }
        writeChars.forEach { ch ->
            if(!ordered.contains(ch)) ordered.add(ch)
        }
        return ordered
    }

    private fun waitForPong(timeoutMs: Long): Boolean {
        val deadline = System.currentTimeMillis() + timeoutMs
        while(System.currentTimeMillis() < deadline) {
            if(pongSeen.get()) return true
            Thread.sleep(80)
        }
        return pongSeen.get()
    }

    private fun probeWriteCharacteristic(force: Boolean): Boolean {
        val chars = orderedWriteChars()
        if(chars.isEmpty()) return false
        if(writeChar != null && !force) return true

        for(ch in chars) {
            pongSeen.set(false)
            if(!writeLineToCharacteristic(ch, "TT_PING")) continue
            if(waitForPong(1800)) {
                writeChar = ch
                return true
            }
        }

        return false
    }

    private fun writeLine(line: String): Boolean {
        val selected = selectPreferredWriteChar() ?: return false
        return writeLineToCharacteristic(selected, line)
    }

    private fun verifyTransport() {
        val candidates = writeChars.toList()
        if(candidates.isEmpty()) {
            setBleStatus("No write characteristic")
            setReady(false)
            return
        }

        Thread {
            setBleStatus("Finalizing connection...")

            repeat(12) {
                if(helloSeen.get() || pongSeen.get()) {
                    setBleStatus("BLE ready")
                    setReady(true)
                    return@Thread
                }
                Thread.sleep(80)
            }

            for(candidate in candidates) {
                pongSeen.set(false)
                val sent = writeLineToCharacteristic(candidate, "TT_PING")
                if(!sent) continue

                repeat(16) {
                    if(pongSeen.get()) {
                        writeChar = candidate
                        setBleStatus("BLE ready")
                        setReady(true)
                        return@Thread
                    }
                    Thread.sleep(120)
                }
            }

            if(helloSeen.get()) {
                setBleStatus("BLE ready")
                setReady(true)
            } else {
                setBleStatus("Waiting for Phone Sync...")
                try {
                    gatt?.disconnect()
                } catch(_: Throwable) {
                }
                setReady(false)
            }
        }.start()
    }

    private fun pumpTxQueue() {
        if(txInFlight != null) return
        if(txQueue.isEmpty()) {
            val callback = txDoneCallback
            txDoneCallback = null
            callback?.invoke(true, "Saved on Flipper")
            return
        }

        val pending = txQueue.removeFirst()
        txInFlight = pending

        fun attemptSend() {
            val current = txInFlight ?: return
            val ok = writeLine(current.line)
            if(!ok) {
                if(lastBleStatus == "Waiting for flow credit") {
                    bleHandler.postDelayed({ if(txInFlight == current) attemptSend() }, 250)
                    return
                }

                current.attempts++
                if(current.attempts >= 6) {
                    failSend("Send timeout")
                    return
                }
                bleHandler.postDelayed({ if(txInFlight == current) attemptSend() }, 250)
                return
            }

            current.attempts++
            if(current.ackToken == null) {
                txInFlight = null
                pumpTxQueue()
                return
            }

            bleHandler.postDelayed({
                if(txInFlight == current) {
                    attemptSend()
                }
            }, 1200)
        }

        attemptSend()
    }

    private fun buildJobId(): String {
        return ((System.currentTimeMillis() / 1000L) and 0xFFFFFFL)
            .toString(16)
            .uppercase()
            .padStart(6, '0')
    }

    private fun maxChunkBytesForMtu(mtu: Int): Int {
        val lineBudget = (mtu - 4).coerceAtLeast(9)
        val base64Chars = (lineBudget - 5).coerceAtLeast(4)
        val alignedChars = base64Chars - (base64Chars % 4)
        return ((alignedChars / 4) * 3).coerceAtLeast(3).coerceAtMost(maxCompactChunkBytes)
    }

    private fun buildUploadQueue(job: ImageUploadJob, mtu: Int): List<PendingLine> {
        val lines = mutableListOf<PendingLine>()
        val begin =
            "B%06X%03X%03X%01X%04X".format(
                job.jobId.toInt(16),
                job.width,
                job.height,
                job.page,
                job.bmpBytes.size,
            )
        lines.add(PendingLine(begin, "AB"))
        lines.add(PendingLine("C${job.barcode}", "AT"))

        val chunkSize = maxChunkBytesForMtu(mtu)
        val flags = Base64.NO_WRAP or Base64.URL_SAFE
        var offset = 0
        var seq = 1
        while(offset < job.bmpBytes.size) {
            val length = minOf(chunkSize, job.bmpBytes.size - offset)
            val encoded = Base64.encodeToString(job.bmpBytes, offset, length, flags)
            lines.add(PendingLine("D%04X%s".format(seq, encoded), "A%04X".format(seq)))
            offset += length
            seq++
        }

        lines.add(PendingLine("E${job.jobId}", "AE"))
        return lines
    }

    private fun startProtocolSend(job: ImageUploadJob, mtu: Int, onDone: (Boolean, String) -> Unit) {
        txQueue.clear()
        txInFlight = null
        txDoneCallback = onDone
        buildUploadQueue(job, mtu).forEach(txQueue::add)
        pumpTxQueue()
    }

    @SuppressLint("MissingPermission")
    private fun requestPreferredMtu(preferred: Int): Int {
        val currentGatt = gatt ?: return negotiatedMtu
        if(negotiatedMtu >= preferred) return negotiatedMtu

        val latch = CountDownLatch(1)
        mtuLatch = latch
        val started = currentGatt.requestMtu(preferred)
        if(started) {
            latch.await(2500, TimeUnit.MILLISECONDS)
        }
        mtuLatch = null
        return negotiatedMtu
    }

    private fun sendSyncOverBle(job: ImageUploadJob): Boolean {
        if(selectPreferredWriteChar() == null) {
            setBleStatus("No write characteristic")
            return false
        }

        val mtu = requestPreferredMtu(247).coerceAtLeast(23)
        repeat(2) { attempt ->
            val okRef = AtomicBoolean(false)
            val done = CountDownLatch(1)
            runOnUiThread {
                startProtocolSend(job, mtu) { ok, status ->
                    okRef.set(ok)
                    setBleStatus(status)
                    done.countDown()
                }
            }

            val timeoutSeconds = maxOf(45L, (job.bmpBytes.size / 300L) + 30L)
            val waited = done.await(timeoutSeconds, TimeUnit.SECONDS)
            if(waited && okRef.get()) return true
            if(waited && !okRef.get()) return false
            if(gatt == null) return false

            if(attempt == 0) {
                probeWriteCharacteristic(true)
                selectPreferredWriteChar()
                setBleStatus("Retrying upload...")
            } else {
                setBleStatus("Upload timed out")
                return false
            }
        }

        return false
    }

    @SuppressLint("MissingPermission")
    private fun connectToDevice(dev: BluetoothDevice) {
        disconnectGatt()
        helloSeen.set(false)
        pongSeen.set(false)
        connectedAddress = dev.address
        setBleStatus("Connecting to ${dev.name ?: connectedAddress}")
        setReady(false)

        gatt =
            dev.connectGatt(
                this,
                false,
                object : BluetoothGattCallback() {
                    override fun onConnectionStateChange(
                        g: BluetoothGatt,
                        status: Int,
                        newState: Int,
                    ) {
                        if(newState == BluetoothProfile.STATE_CONNECTED) {
                            connectedAddress = g.device?.address ?: connectedAddress
                            negotiatedMtu = 23
                            try {
                                g.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH)
                            } catch(_: Throwable) {
                            }
                            setBleStatus("Connected, discovering...")
                            g.discoverServices()
                        } else if(newState == BluetoothProfile.STATE_DISCONNECTED) {
                            val reason = "Disconnected (${gattStatusString(status)})"
                            val reconnecting = txDoneCallback == null && txInFlight == null && txQueue.isEmpty()
                            setBleStatus(if(reconnecting) "Waiting for Phone Sync..." else reason)
                            setReady(false)
                            if(txDoneCallback != null || txInFlight != null || txQueue.isNotEmpty()) {
                                failSend(reason)
                            }
                            if(gatt === g) disconnectGatt()
                            if(candidateSearchInProgress && pendingCandidates.isNotEmpty()) {
                                bleHandler.postDelayed({ tryNextCandidate() }, 400)
                            } else {
                                bleHandler.postDelayed({
                                    if(gatt == null) connectFlipper()
                                }, 1200)
                            }
                        }
                    }

                    override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
                        if(status != BluetoothGatt.GATT_SUCCESS) {
                            setBleStatus("Service discovery failed")
                            setReady(false)
                            return
                        }

                        val service = g.getService(serialServiceUuid)
                        val writes = service?.let(::findSerialWriteChars).orEmpty()
                        val notifies = service?.let(::findSerialNotifyChars).orEmpty()
                        if(service == null || writes.isEmpty() || notifies.isEmpty()) {
                            setBleStatus("Serial UUIDs missing")
                            setReady(false)
                            if(candidateSearchInProgress) {
                                try {
                                    g.disconnect()
                                } catch(_: Throwable) {
                                }
                            }
                            return
                        }

                        writeChars = writes
                        notifyChars = notifies
                        writeChar = writes.firstOrNull()
                        flowChar = service.getCharacteristic(serialFlowUuid)
                        flowCredits = serialFlowWindow
                        targetSyncRequested = false
                        remoteTargetsBuffer.clear()
                        service.characteristics.forEach { ch ->
                            val role =
                                when(ch.uuid) {
                                    serialWriteUuid -> "RX"
                                    serialNotifyUuid -> "TX"
                                    serialFlowUuid -> "FLOW"
                                    serialStatusUuid -> "STATUS"
                                    else -> "OTHER"
                                }
                            Log.d(
                                logTag,
                                "Service char $role ${ch.uuid} props=${characteristicProps(ch)}",
                            )
                        }
                        Log.d(
                            logTag,
                            "Write chars: ${writes.joinToString { "${it.uuid}(${characteristicProps(it)})" }}",
                        )
                        Log.d(
                            logTag,
                            "Notify chars: ${notifies.joinToString { "${it.uuid}(${characteristicProps(it)})" }}",
                        )
                        candidateSearchInProgress = false
                        pendingCandidates.clear()
                        setBleStatus("Enabling notifications...")
                        enableNotifications(g, notifies)
                    }

                    override fun onDescriptorWrite(
                        g: BluetoothGatt,
                        descriptor: BluetoothGattDescriptor,
                        status: Int,
                    ) {
                        if(descriptor.uuid != cccdUuid) return
                        if(status == BluetoothGatt.GATT_SUCCESS &&
                            notifySubscribeIndex + 1 < notifyChars.size) {
                            notifySubscribeIndex += 1
                            val nextNotify = notifyChars[notifySubscribeIndex]
                            if(!subscribeOne(g, nextNotify)) {
                                setBleStatus("Notification subscribe failed")
                                setReady(false)
                            }
                            return
                        }

                        if(status == BluetoothGatt.GATT_SUCCESS) {
                            verifyTransport()
                        } else {
                            setBleStatus("Notification setup failed")
                            setReady(false)
                        }
                    }

                    override fun onCharacteristicChanged(
                        g: BluetoothGatt,
                        characteristic: BluetoothGattCharacteristic,
                    ) {
                        val payload = characteristic.value
                        when(characteristic.uuid) {
                            serialNotifyUuid -> runOnUiThread { onBlePayload(payload) }
                            serialFlowUuid -> {
                                val credits = decodeFlowCredits(payload)
                                if(credits != null) {
                                    flowCredits = credits
                                    Log.d(logTag, "Flow credits <- $credits")
                                } else {
                                    Log.d(
                                        logTag,
                                        "Flow ctrl decode failed ${payload?.joinToString(" ") { "%02X".format(it.toInt() and 0xFF) }}",
                                    )
                                }
                            }
                            serialStatusUuid -> Log.d(
                                logTag,
                                "Ignoring status notify ${payload?.joinToString(" ") { "%02X".format(it.toInt() and 0xFF) }}",
                            )
                            else -> Log.d(logTag, "Ignoring notify from ${characteristic.uuid}")
                        }
                    }

                    override fun onCharacteristicChanged(
                        g: BluetoothGatt,
                        characteristic: BluetoothGattCharacteristic,
                        value: ByteArray,
                    ) {
                        when(characteristic.uuid) {
                            serialNotifyUuid -> runOnUiThread { onBlePayload(value) }
                            serialFlowUuid -> {
                                val credits = decodeFlowCredits(value)
                                if(credits != null) {
                                    flowCredits = credits
                                    Log.d(logTag, "Flow credits <- $credits")
                                } else {
                                    Log.d(
                                        logTag,
                                        "Flow ctrl decode failed ${value.joinToString(" ") { "%02X".format(it.toInt() and 0xFF) }}",
                                    )
                                }
                            }
                            serialStatusUuid -> Log.d(
                                logTag,
                                "Ignoring status notify ${value.joinToString(" ") { "%02X".format(it.toInt() and 0xFF) }}",
                            )
                            else -> Log.d(logTag, "Ignoring notify from ${characteristic.uuid}")
                        }
                    }

                    override fun onCharacteristicWrite(
                        g: BluetoothGatt,
                        characteristic: BluetoothGattCharacteristic,
                        status: Int,
                    ) {
                        writeStatusOk?.set(status == BluetoothGatt.GATT_SUCCESS)
                        writeLatch?.countDown()
                    }

                    override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) {
                        if(status == BluetoothGatt.GATT_SUCCESS) {
                            negotiatedMtu = mtu
                        }
                        mtuLatch?.countDown()
                    }
                },
            )
    }

    private fun scanRecordHasSerialService(result: ScanResult): Boolean =
        result.scanRecord?.serviceUuids?.any { it.uuid == serialServiceUuid } == true

    @SuppressLint("MissingPermission")
    private fun connectFlipper() {
        val adapter =
            (getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter ?: run {
                setBleStatus("Bluetooth adapter unavailable")
                return
            }

        val hasScan =
            checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED
        val hasConnect =
            checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED
        if(!hasScan || !hasConnect) {
            setBleStatus("Requesting BLE permissions...")
            ensurePermissions()
            bleHandler.postDelayed({ connectFlipper() }, 1200)
            return
        }

        if(!adapter.isEnabled) {
            setBleStatus("Bluetooth is off")
            setReady(false)
            return
        }

        disconnectGatt()
        bleHandler.removeCallbacksAndMessages(null)
        candidateSearchInProgress = false
        pendingCandidates.clear()
        setBleStatus("Scanning...")
        setReady(false)

        val scanner = adapter.bluetoothLeScanner ?: run {
            setBleStatus("BLE scanner unavailable")
            return
        }

        val settings =
            ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
        val serviceFilters = emptyList<ScanFilter>()
        val candidates = linkedMapOf<String, ScanCandidate>()

        adapter.bondedDevices?.forEach { dev ->
            val address = dev.address
            candidates[address] =
                ScanCandidate(
                    device = dev,
                    bestRssi = Int.MIN_VALUE,
                    hasService = false,
                    looksLikeFlipper = dev.name?.contains("Flipper", ignoreCase = true) == true,
                    bonded = true,
                )
        }

        val callback =
            object : ScanCallback() {
                override fun onScanResult(callbackType: Int, result: ScanResult) {
                    val dev = result.device ?: return
                    val address = dev.address
                    val hasService = scanRecordHasSerialService(result)
                    val looksLikeFlipper = dev.name?.contains("Flipper", ignoreCase = true) == true
                    if(hasService) {
                        scanner.stopScan(this)
                        bleHandler.removeCallbacksAndMessages(null)
                        candidateSearchInProgress = false
                        pendingCandidates.clear()
                        setBleStatus("Found Flipper service")
                        connectToDevice(dev)
                        return
                    }

                    val current = candidates[address]
                    if(current == null) {
                        candidates[address] =
                            ScanCandidate(
                                device = dev,
                                bestRssi = result.rssi,
                                hasService = false,
                                looksLikeFlipper = looksLikeFlipper,
                                bonded = false,
                            )
                    } else {
                        if(result.rssi > current.bestRssi) current.bestRssi = result.rssi
                        current.hasService = current.hasService || hasService
                        current.looksLikeFlipper = current.looksLikeFlipper || looksLikeFlipper
                    }
                }

                override fun onScanFailed(errorCode: Int) {
                    setBleStatus("Scan failed ($errorCode)")
                }
            }

        scanner.startScan(serviceFilters, settings, callback)
        bleHandler.postDelayed({
            try {
                scanner.stopScan(callback)
            } catch(_: Throwable) {
            }

            val ranked =
                candidates
                    .values
                    .sortedWith(
                        compareByDescending<ScanCandidate> { it.hasService }
                            .thenByDescending { it.bonded }
                            .thenByDescending { it.looksLikeFlipper }
                            .thenByDescending { it.bestRssi },
                    ).take(6)
                    .map { it.device }

            if(ranked.isNotEmpty()) {
                pendingCandidates = ranked.toMutableList()
                candidateSearchInProgress = true
                setBleStatus("Trying ${ranked.size} BLE candidates...")
                tryNextCandidate()
            } else {
                setBleStatus("No Flipper found")
            }
        }, 3000)
    }

    override fun onDestroy() {
        super.onDestroy()
        disconnectGatt()
    }

    @OptIn(ExperimentalMaterial3Api::class)
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        ensurePermissions()

        val orange = Color(0xFFFF7A00)
        val scheme =
            darkColorScheme(
                primary = orange,
                onPrimary = Color.Black,
                surface = Color(0xFF0A0A0A),
                background = Color.Black,
                onBackground = Color(0xFFF2F2F2),
                onSurface = Color(0xFFF2F2F2),
            )

        setContent {
            var bleState by remember { mutableStateOf("Connecting...") }
            var bleReady by remember { mutableStateOf(false) }
            var barcode by remember { mutableStateOf(lastSelectedBarcode) }
            var savedTargets by remember { mutableStateOf<List<SavedTarget>>(emptyList()) }
            var eslSize by remember { mutableStateOf(EslSize(296, 128)) }
            var detail by remember { mutableStateOf(50f) }
            var scale by remember { mutableStateOf(1.0f) }
            var offsetX by remember { mutableStateOf(0f) }
            var offsetY by remember { mutableStateOf(0f) }
            var sending by remember { mutableStateOf(false) }
            var original by remember { mutableStateOf<Bitmap?>(null) }
            var preview by remember { mutableStateOf<Bitmap?>(null) }
            var uploadBytes by remember { mutableStateOf<ByteArray?>(null) }
            var stats by remember { mutableStateOf("No image") }
            val scanner = remember {
                val options = GmsBarcodeScannerOptions.Builder().enableAutoZoom().build()
                GmsBarcodeScanning.getClient(this, options)
            }

            MaterialTheme(colorScheme = scheme) {
                LaunchedEffect(Unit) {
                    bleStatusSink = { bleState = it }
                    bleReadySink = { bleReady = it }
                    targetListSink = { savedTargets = it }
                    selectedBarcodeSink = { nextBarcode ->
                        barcode = nextBarcode
                        lastSelectedBarcode = nextBarcode
                        eslSize = eslSizeForBarcode(nextBarcode)
                    }
                    bleState = lastBleStatus
                    connectFlipper()
                }

                LaunchedEffect(pickedImageUri.value) {
                    original = pickedImageUri.value?.let { loadBitmap(it) }
                    if(original == null) {
                        preview = null
                        uploadBytes = null
                        stats = "No image"
                    }
                }

                LaunchedEffect(original, detail, scale, offsetX, offsetY, eslSize) {
                    val nextPreview =
                        original?.let {
                            toBwPricehax(
                                it,
                                eslSize,
                                detail,
                                scale,
                                offsetX,
                                offsetY,
                            )
                        }
                    preview = nextPreview
                    uploadBytes = nextPreview?.let(::buildMonochromeBmp)
                    stats =
                        if(nextPreview != null && uploadBytes != null) {
                            "Upload ${uploadBytes!!.size} B | ${nextPreview.width}x${nextPreview.height}"
                        } else {
                            "No image"
                        }
                }

                LaunchedEffect(barcode, savedTargets) {
                    if(barcode.isNotBlank()) {
                        eslSize = eslSizeForBarcode(barcode)
                    }
                }

                val selectedTarget = savedTargets.firstOrNull { it.barcode == barcode }

                if(!bleReady) {
                    Scaffold(
                        containerColor = Color.Black,
                    ) { inner ->
                        Column(
                            modifier =
                                Modifier
                                    .fillMaxSize()
                                    .background(Color.Black)
                                    .padding(inner)
                                    .padding(16.dp),
                            verticalArrangement = Arrangement.Center,
                            horizontalAlignment = Alignment.CenterHorizontally,
                        ) {
                            Text("Connecting to Flipper...")
                            Text(bleState, color = orange, modifier = Modifier.padding(top = 8.dp))
                        }
                    }
                } else {
                    Scaffold(
                        containerColor = Color.Black,
                    ) { inner ->
                        Column(
                            modifier =
                                Modifier
                                    .fillMaxSize()
                                    .background(Color.Black)
                                    .padding(inner)
                                    .padding(12.dp)
                                    .verticalScroll(rememberScrollState()),
                            verticalArrangement = Arrangement.spacedBy(8.dp),
                        ) {
                            Card(
                                colors = CardDefaults.cardColors(containerColor = Color(0xFF101010)),
                                shape = RoundedCornerShape(16.dp),
                            ) {
                                Column(
                                    modifier = Modifier.fillMaxWidth().padding(12.dp),
                                    verticalArrangement = Arrangement.spacedBy(8.dp),
                                ) {
                                    Row(
                                        modifier = Modifier.fillMaxWidth(),
                                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                                    ) {
                                        Button(
                                            onClick = {
                                                scanner.startScan().addOnSuccessListener { code ->
                                                    barcode = normalizeBarcode(code.rawValue.orEmpty())
                                                    lastSelectedBarcode = barcode
                                                    eslSize = eslSizeForBarcode(barcode)
                                                }
                                            },
                                            modifier = Modifier.weight(1f),
                                            contentPadding = PaddingValues(vertical = 10.dp),
                                        ) {
                                            Text("Scan")
                                        }
                                        Button(
                                            onClick = { imagePicker.launch("image/*") },
                                            modifier = Modifier.weight(1f),
                                            contentPadding = PaddingValues(vertical = 10.dp),
                                        ) {
                                            Text("Photo")
                                        }
                                    }
                                    Row(
                                        modifier = Modifier.fillMaxWidth(),
                                        horizontalArrangement = Arrangement.SpaceBetween,
                                        verticalAlignment = Alignment.CenterVertically,
                                    ) {
                                        Column(modifier = Modifier.weight(1f)) {
                                            Text(
                                                when {
                                                    selectedTarget != null -> selectedTarget.name
                                                    barcode.isBlank() -> "No target selected"
                                                    else -> "New target"
                                                },
                                                style = MaterialTheme.typography.bodyMedium,
                                            )
                                            Text(
                                                if(barcode.isBlank()) {
                                                    "Choose an existing target or scan a new one"
                                                } else {
                                                    barcode
                                                },
                                                style = MaterialTheme.typography.bodySmall,
                                            )
                                        }
                                        Text(
                                            "${eslSize.width} x ${eslSize.height}",
                                            color = orange,
                                            style = MaterialTheme.typography.bodySmall,
                                        )
                                    }
                                    Text(stats, color = orange)
                                    Card(
                                        colors = CardDefaults.cardColors(containerColor = Color.Black),
                                        shape = RoundedCornerShape(12.dp),
                                    ) {
                                        Box(
                                            modifier =
                                                Modifier
                                                    .fillMaxWidth()
                                                    .aspectRatio(
                                                        eslSize.width.toFloat() /
                                                            eslSize.height.toFloat().coerceAtLeast(1f),
                                                    )
                                                    .padding(6.dp),
                                            contentAlignment = Alignment.Center,
                                        ) {
                                            if(preview != null) {
                                                Image(
                                                    bitmap = preview!!.asImageBitmap(),
                                                    contentDescription = "Preview",
                                                    modifier = Modifier.fillMaxSize(),
                                                    contentScale = ContentScale.Fit,
                                                )
                                            } else {
                                                Column(
                                                    modifier = Modifier.fillMaxSize(),
                                                    verticalArrangement = Arrangement.Center,
                                                    horizontalAlignment = Alignment.CenterHorizontally,
                                                ) {
                                                    Text("No image selected")
                                                }
                                            }
                                        }
                                    }
                                    CompactSliderRow(
                                        label = "Detail",
                                        valueText = detail.toInt().toString(),
                                        value = detail,
                                        onValueChange = { detail = it },
                                        valueRange = 0f..100f,
                                    )
                                    CompactSliderRow(
                                        label = "Zoom",
                                        valueText = "${(scale * 100f).toInt()}%",
                                        value = scale,
                                        onValueChange = { scale = it },
                                        valueRange = 0.5f..2.5f,
                                    )
                                    Row(
                                        modifier = Modifier.fillMaxWidth(),
                                        horizontalArrangement = Arrangement.SpaceBetween,
                                    ) {
                                        Column(modifier = Modifier.weight(1f)) {
                                            CompactSliderRow(
                                                label = "Move X",
                                                valueText = "${(offsetX * 100f).toInt()}%",
                                                value = offsetX,
                                                onValueChange = { offsetX = it },
                                                valueRange = -1f..1f,
                                            )
                                        }
                                        Column(modifier = Modifier.weight(1f)) {
                                            CompactSliderRow(
                                                label = "Move Y",
                                                valueText = "${(offsetY * 100f).toInt()}%",
                                                value = offsetY,
                                                onValueChange = { offsetY = it },
                                                valueRange = -1f..1f,
                                            )
                                        }
                                    }
                                }
                            }

                            Button(
                                onClick = {
                                    if(!isValidBarcode(barcode)) {
                                        Toast
                                            .makeText(
                                                this@MainActivity,
                                                "Scan a valid barcode: Letter + 16 digits",
                                                Toast.LENGTH_SHORT,
                                            ).show()
                                        return@Button
                                    }

                                    val bytes = uploadBytes
                                    if(bytes == null) {
                                        Toast
                                            .makeText(
                                                this@MainActivity,
                                                "Choose an image first",
                                                Toast.LENGTH_SHORT,
                                            ).show()
                                        return@Button
                                    }

                                    sending = true
                                    Thread {
                                        val job =
                                            ImageUploadJob(
                                                jobId = buildJobId(),
                                                barcode = barcode,
                                                width = eslSize.width,
                                                height = eslSize.height,
                                                page = 1,
                                                bmpBytes = bytes,
                                            )
                                        val ok = sendSyncOverBle(job)
                                        runOnUiThread {
                                            sending = false
                                            if(ok && savedTargets.none { it.barcode == barcode }) {
                                                val target =
                                                    SavedTarget(
                                                        barcode = barcode,
                                                        name = barcode,
                                                        width = eslSize.width,
                                                        height = eslSize.height,
                                                    )
                                                remoteTargetsCache.removeAll { it.barcode == barcode }
                                                remoteTargetsCache.add(target)
                                                savedTargets = savedTargets + listOf(target)
                                            }
                                            Toast
                                                .makeText(
                                                    this@MainActivity,
                                                    if(ok) {
                                                        "Saved on Flipper"
                                                    } else {
                                                        "BLE send failed: $lastBleStatus"
                                                    },
                                                    Toast.LENGTH_SHORT,
                                                ).show()
                                        }
                                    }.start()
                                },
                                modifier = Modifier.fillMaxWidth(),
                                contentPadding = PaddingValues(vertical = 12.dp),
                            ) {
                                Text(if(sending) "Uploading..." else "Send to Flipper")
                            }
                            Text(
                                bleState,
                                color = orange,
                                style = MaterialTheme.typography.bodySmall,
                                modifier = Modifier.fillMaxWidth(),
                            )
                        }
                    }
                }
            }
        }
    }
}
