package xyz.i12bp8.tagtinker.android.ble

import android.Manifest
import android.annotation.SuppressLint
import android.content.Context
import android.content.pm.PackageManager
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.util.Log
import androidx.core.content.ContextCompat
import java.util.UUID
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.channels.BufferOverflow
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.launch
import kotlinx.coroutines.withTimeout
import kotlinx.coroutines.withTimeoutOrNull
import no.nordicsemi.android.kotlin.ble.client.main.callback.ClientBleGatt
import no.nordicsemi.android.kotlin.ble.client.main.service.ClientBleGattCharacteristic
import no.nordicsemi.android.kotlin.ble.core.data.BleWriteType
import no.nordicsemi.android.kotlin.ble.core.data.BleGattConnectOptions
import no.nordicsemi.android.kotlin.ble.core.data.util.DataByteArray
import no.nordicsemi.android.kotlin.ble.core.scanner.BleScanMode
import no.nordicsemi.android.kotlin.ble.core.scanner.BleScannerSettings
import no.nordicsemi.android.kotlin.ble.scanner.BleScanner
import no.nordicsemi.android.kotlin.ble.scanner.aggregator.BleScanResultAggregator
import xyz.i12bp8.tagtinker.android.model.BlePeripheral
import xyz.i12bp8.tagtinker.android.model.UploadJob
import xyz.i12bp8.tagtinker.android.model.UploadProgress

// Official Flipper Serial UUIDs
private val serialServiceUuid = UUID.fromString("8fe5b3d5-2e7f-4a98-2a48-7acc60fe0000")
private val serialWriteUuid   = UUID.fromString("19ed82ae-ed21-4c9d-4145-228e62fe0000") // Phone -> Flipper
private val serialNotifyUuid  = UUID.fromString("19ed82ae-ed21-4c9d-4145-228e61fe0000") // Flipper -> Phone

private const val logTag = "TagTinkerBle"
private const val connectTimeoutMs = 30000L
private const val mtuSize = 247

// How many bytes per chunk we send to the Flipper.
// 128 bytes → ~172 chars base64 → well under BLE MTU with header.
private const val CHUNK_BYTES = 128

class FlipperBleClient(private val context: Context) {
    private val appScope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private val adapter: BluetoothAdapter? =
        context.getSystemService(BluetoothManager::class.java)?.adapter

    private val _scanResults = MutableStateFlow<List<BlePeripheral>>(emptyList())
    val scanResults: StateFlow<List<BlePeripheral>> = _scanResults
    private val _scanStatus = MutableStateFlow<String?>(null)
    val scanStatus: StateFlow<String?> = _scanStatus

    private var notifyJob: Job? = null
    private var client: ClientBleGatt? = null

    private var writeCharacteristic: ClientBleGattCharacteristic? = null
    private var notifyCharacteristic: ClientBleGattCharacteristic? = null

    private val ackWaiters = linkedMapOf<String, CompletableDeferred<String>>()
    private val rxBuffer = StringBuilder()

    private var helloReceived = false
    private var pongReceived = false

    fun isAvailable(): Boolean {
        return ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_CONNECT) ==
                PackageManager.PERMISSION_GRANTED
    }

    fun startScan() { _scanStatus.value = "Use System Picker" }
    fun stopScan() {}

    @SuppressLint("MissingPermission")
    suspend fun connect(address: String) {
        Log.d(logTag, "Connecting to $address...")
        disconnect()

        val bluetoothDevice = adapter?.getRemoteDevice(address) ?: error("Device not found")
        val serverDevice = no.nordicsemi.android.kotlin.ble.core.RealServerDevice(bluetoothDevice)

        val gatt = withTimeout(connectTimeoutMs) {
            ClientBleGatt.connect(context, serverDevice, appScope, BleGattConnectOptions(autoConnect = false))
        }

        if (!gatt.isConnected) error("Connection failed")
        this.client = gatt

        Log.d(logTag, "GATT connected, waiting for bond...")
        try {
            withTimeout(15000) { gatt.waitForBonding() }
        } catch (e: Exception) {
            Log.w(logTag, "Bonding stalled, continuing anyway")
        }

        gatt.requestMtu(mtuSize)
        delay(500)

        Log.d(logTag, "Discovering services...")
        gatt.discoverServices()

        val services = withTimeout(20000) {
            var found: no.nordicsemi.android.kotlin.ble.client.main.service.ClientBleGattServices? = null
            repeat(3) { attempt ->
                if (attempt > 0) {
                    Log.d(logTag, "Retrying service discovery (attempt ${attempt + 1})...")
                    gatt.discoverServices()
                }
                found = withTimeoutOrNull(5000) {
                    gatt.services.filter { it != null }.first()
                }
                if (found != null) return@withTimeout found
                delay(1000)
            }
            found
        } ?: error("BLE services unavailable")

        val serialService = services.findService(serialServiceUuid) ?: error("Serial service missing")

        notifyCharacteristic = serialService.findCharacteristic(serialNotifyUuid)
        writeCharacteristic = serialService.findCharacteristic(serialWriteUuid)

        helloReceived = false
        pongReceived = false

        val notifyProps = notifyCharacteristic?.properties
        Log.d(logTag, "Notify char props: $notifyProps")

        notifyJob = appScope.launch {
            if (notifyProps?.contains(no.nordicsemi.android.kotlin.ble.core.data.BleGattProperty.PROPERTY_NOTIFY) == true ||
                notifyProps?.contains(no.nordicsemi.android.kotlin.ble.core.data.BleGattProperty.PROPERTY_INDICATE) == true) {
                notifyCharacteristic?.getNotifications(bufferOverflow = BufferOverflow.SUSPEND)?.collect {
                    handleNotify(it.value)
                }
            } else {
                Log.e(logTag, "Notify characteristic has no NOTIFY or INDICATE property!")
            }
        }

        Log.d(logTag, "Subscribed to notifications, waiting for handshake...")
        sendHandshake()
    }

    private suspend fun sendHandshake() {
        // Wait briefly to see if Flipper auto-sends TT_HELLO
        delay(3000)
        if (helloReceived) return

        Log.d(logTag, "Flipper quiet — sending TT_PING poke...")
        repeat(3) { attempt ->
            runCatching {
                sendLine("TT_PING", ack = "TT_PONG")
            }.onSuccess {
                Log.d(logTag, "Handshake OK via PING")
                return
            }.onFailure {
                Log.w(logTag, "Handshake attempt ${attempt + 1} failed: ${it.message}")
                delay(1200)
            }
        }

        if (!helloReceived && !pongReceived) {
            throw IllegalStateException("Handshake failed — Flipper not responding")
        }
    }

    suspend fun disconnect() {
        notifyJob?.cancel()
        notifyJob = null
        runCatching { client?.disconnect() }
        runCatching { client?.close() }
        client = null
        ackWaiters.values.forEach { it.cancel() }
        ackWaiters.clear()
        rxBuffer.clear()
    }

    private fun handleNotify(bytes: ByteArray) {
        if (bytes.isEmpty()) return
        Log.d(logTag, "RX Packet: ${bytes.joinToString("") { "%02X".format(it) }}")

        String(bytes, Charsets.UTF_8).forEach { char ->
            if (char == '\n' || char == '\r') {
                val line = rxBuffer.toString().trim()
                rxBuffer.clear()
                if (line.isNotBlank()) {
                    Log.d(logTag, "RX Line: $line")
                    if (line == "TT_HELLO") helloReceived = true
                    if (line == "TT_PONG")  pongReceived = true
                    ackWaiters.remove(line)?.complete(line)
                }
            } else if (char.code in 32..126) {
                rxBuffer.append(char)
            }
        }
    }

    /**
     * Sends a line to the Flipper and optionally waits for a specific ACK response.
     *
     * Flow control is entirely ACK-based: the phone sends one chunk, waits for
     * TT_ACK|N from the Flipper, then sends the next. There is no credit/window
     * mechanism — that approach was unreliable for large images.
     *
     * We retry up to [maxAttempts] times with [ackTimeoutMs] timeout each.
     */
    suspend fun sendLine(
        line: String,
        ack: String? = null,
        ackTimeoutMs: Long = 8000L,
        maxAttempts: Int = 3,
    ) {
        val char = writeCharacteristic ?: error("Not connected")
        val payload = "$line\n".encodeToByteArray()

        repeat(maxAttempts) { attempt ->
            val deferred = ack?.let { CompletableDeferred<String>() }
            if (ack != null) ackWaiters[ack] = deferred!!

            Log.d(logTag, "TX (attempt ${attempt + 1}/$maxAttempts): $line")
            char.write(DataByteArray(payload), BleWriteType.NO_RESPONSE)

            if (ack == null) return

            try {
                withTimeout(ackTimeoutMs) { deferred!!.await() }
                return // ACK received — success
            } catch (e: Exception) {
                Log.w(logTag, "Timeout waiting for '$ack' (attempt ${attempt + 1})")
                ackWaiters.remove(ack)
                if (attempt < maxAttempts - 1) delay(200) // brief pause before retry
            }
        }

        error("Timed out after $maxAttempts attempts sending: $line")
    }

    /**
     * Uploads a BMP image to the Flipper via the TT_BEGIN / TT_DATA / TT_END protocol.
     *
     * Each chunk is sent and confirmed via ACK before the next chunk is sent.
     * Chunk size is [CHUNK_BYTES] bytes (128), encoded as URL-safe base64.
     */
    suspend fun upload(job: UploadJob, onProgress: (UploadProgress) -> Unit) {
        onProgress(UploadProgress(0, job.bmpBytes.size, "Preparing..."))

        // Verify connection is alive
        sendLine("TT_PING", ack = "TT_PONG")

        // Begin upload — Flipper opens file and responds TT_ACK|BEGIN
        val beginCmd = "TT_BEGIN|${job.jobId}|${job.barcode}|${job.width}|${job.height}|${job.page}|${job.bmpBytes.size}"
        sendLine(beginCmd, ack = "TT_ACK|BEGIN", ackTimeoutMs = 10000L)

        // Stream chunks — each chunk waits for its individual ACK
        var offset = 0
        var sequence = 1
        while (offset < job.bmpBytes.size) {
            val end = minOf(job.bmpBytes.size, offset + CHUNK_BYTES)
            val chunk = job.bmpBytes.copyOfRange(offset, end)
            val b64 = android.util.Base64.encodeToString(
                chunk,
                android.util.Base64.NO_WRAP or android.util.Base64.URL_SAFE
            )

            onProgress(UploadProgress(offset, job.bmpBytes.size, "Chunk $sequence / ${(job.bmpBytes.size + CHUNK_BYTES - 1) / CHUNK_BYTES}"))
            sendLine("TT_DATA|$sequence|$b64", ack = "TT_ACK|$sequence", ackTimeoutMs = 10000L)

            offset = end
            sequence++
        }

        // Finalize — Flipper renames file and marks ready
        sendLine("TT_END|${job.jobId}", ack = "TT_ACK|END", ackTimeoutMs = 15000L)
        onProgress(UploadProgress(job.bmpBytes.size, job.bmpBytes.size, "Sync complete!"))
    }
}
