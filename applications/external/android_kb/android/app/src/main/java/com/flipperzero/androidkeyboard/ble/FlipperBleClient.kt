package com.flipperzero.androidkeyboard.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import java.util.ArrayDeque
import java.util.UUID
import java.util.concurrent.atomic.AtomicReference

class FlipperBleClient(private val context: Context) {

    enum class State {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        READY,
        ERROR,
    }

    interface Listener {
        fun onStateChanged(state: State, message: String)
    }

    private val mainHandler = Handler(Looper.getMainLooper())
    private val listenerRef = AtomicReference<Listener?>(null)
    private var gatt: BluetoothGatt? = null
    private var txCharacteristic: BluetoothGattCharacteristic? = null
    private val writeQueue = ArrayDeque<ByteArray>()
    private var writeInFlight = false

    @Volatile
    var state: State = State.DISCONNECTED
        private set

    @Volatile
    var statusMessage: String = "Disconnected"
        private set

    fun setListener(listener: Listener?) {
        listenerRef.set(listener)
    }

    @SuppressLint("MissingPermission")
    fun connect(device: BluetoothDevice) {
        disconnect()
        updateState(State.CONNECTING, "Connecting to ${device.name ?: device.address}...")
        gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
    }

    @SuppressLint("MissingPermission")
    fun connectAddress(address: String) {
        val manager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val adapter: BluetoothAdapter = manager.adapter
            ?: run {
                updateState(State.ERROR, "Bluetooth unavailable")
                return
            }
        if (!adapter.isEnabled) {
            updateState(State.ERROR, "Bluetooth is off")
            return
        }
        val normalized = address.trim().uppercase()
        connect(adapter.getRemoteDevice(normalized))
    }

    @SuppressLint("MissingPermission")
    fun disconnect() {
        synchronized(writeQueue) {
            writeQueue.clear()
            writeInFlight = false
        }
        gatt?.close()
        gatt = null
        txCharacteristic = null
        updateState(State.DISCONNECTED, "Disconnected")
    }

    fun sendKeyEvent(down: Boolean, keyCode: Byte, modifiers: Byte = 0): Boolean {
        if (state != State.READY) return false
        enqueueWrite(BridgeProtocol.encodeKeyEvent(down, keyCode, modifiers))
        return true
    }

    /** Tap: key_down, then key_up after a short hold. */
    fun sendTap(keyCode: Byte, modifiers: Byte = 0): Boolean {
        if (!sendKeyEvent(down = true, keyCode = keyCode, modifiers = modifiers)) return false
        mainHandler.postDelayed({
            sendKeyEvent(down = false, keyCode = keyCode, modifiers = modifiers)
        }, TAP_RELEASE_MS)
        return true
    }

    fun sendMouseMove(dx: Int, dy: Int): Boolean {
        if (state != State.READY) return false
        if (dx == 0 && dy == 0) return true
        enqueueWrite(
            BridgeProtocol.encodeMouseMove(
                clampToByte(dx),
                clampToByte(dy),
            ),
        )
        return true
    }

    fun sendMouseButton(down: Boolean, button: Byte): Boolean {
        if (state != State.READY) return false
        enqueueWrite(BridgeProtocol.encodeMouseButton(down, button))
        return true
    }

    fun sendMouseScroll(delta: Int): Boolean {
        if (state != State.READY) return false
        if (delta == 0) return true
        enqueueWrite(BridgeProtocol.encodeMouseScroll(clampToByte(delta)))
        return true
    }

    private fun enqueueWrite(payload: ByteArray) {
        synchronized(writeQueue) {
            writeQueue.addLast(payload)
        }
        drainWriteQueue()
    }

    @SuppressLint("MissingPermission")
    private fun drainWriteQueue() {
        mainHandler.post {
            val characteristic = txCharacteristic ?: return@post
            val gattRef = gatt ?: return@post
            if (state != State.READY) return@post

            val payload: ByteArray
            synchronized(writeQueue) {
                if (writeInFlight) return@post
                val next = writeQueue.pollFirst() ?: return@post
                writeInFlight = true
                payload = next
            }

            val ok = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                gattRef.writeCharacteristic(
                    characteristic,
                    payload,
                    BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE,
                ) == BluetoothStatusCodes.SUCCESS
            } else {
                @Suppress("DEPRECATION")
                characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
                @Suppress("DEPRECATION")
                characteristic.value = payload
                @Suppress("DEPRECATION")
                gattRef.writeCharacteristic(characteristic)
            }

            if (!ok) {
                synchronized(writeQueue) {
                    writeQueue.addFirst(payload)
                    writeInFlight = false
                }
                mainHandler.postDelayed({ drainWriteQueue() }, 20L)
                return@post
            }
            // Some stacks never callback for WRITE_NO_RESPONSE — release after a tick.
            mainHandler.postDelayed({
                synchronized(writeQueue) {
                    writeInFlight = false
                }
                drainWriteQueue()
            }, 15L)
        }
    }

    private fun updateState(newState: State, message: String) {
        state = newState
        statusMessage = message
        mainHandler.post {
            listenerRef.get()?.onStateChanged(newState, message)
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                updateState(State.ERROR, "GATT error: $status")
                gatt.close()
                return
            }

            when (newState) {
                BluetoothProfile.STATE_CONNECTED -> {
                    updateState(State.CONNECTED, "Discovering services...")
                    gatt.discoverServices()
                }
                BluetoothProfile.STATE_DISCONNECTED -> {
                    txCharacteristic = null
                    synchronized(writeQueue) {
                        writeQueue.clear()
                        writeInFlight = false
                    }
                    updateState(State.DISCONNECTED, "Disconnected")
                    gatt.close()
                }
            }
        }

        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                updateState(State.ERROR, "Service discovery failed: $status")
                return
            }

            val service = gatt.getService(FlipperBleUuids.SERIAL_SERVICE)
            if (service == null) {
                updateState(State.ERROR, "Serial service not found. Is bridge FAP running?")
                return
            }

            val tx = service.getCharacteristic(FlipperBleUuids.TX_CHARACTERISTIC)
            if (tx == null) {
                updateState(State.ERROR, "TX characteristic not found")
                return
            }

            txCharacteristic = tx

            val rx = service.getCharacteristic(FlipperBleUuids.RX_CHARACTERISTIC)
            if (rx != null) {
                gatt.setCharacteristicNotification(rx, true)
                val cccd = rx.getDescriptor(CLIENT_CONFIG_DESCRIPTOR)
                cccd?.let {
                    @Suppress("DEPRECATION")
                    it.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                    @Suppress("DEPRECATION")
                    gatt.writeDescriptor(it)
                }
            }

            updateState(State.READY, "Ready — start typing")
        }

        override fun onCharacteristicWrite(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int,
        ) {
            synchronized(writeQueue) {
                writeInFlight = false
            }
            if (status == BluetoothGatt.GATT_SUCCESS) {
                drainWriteQueue()
            } else {
                mainHandler.postDelayed({ drainWriteQueue() }, 20L)
            }
        }
    }

    companion object {
        private val CLIENT_CONFIG_DESCRIPTOR: UUID =
            UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

        private const val TAP_RELEASE_MS = 60L

        private fun clampToByte(value: Int): Byte {
            return value.coerceIn(-127, 127).toByte()
        }

        fun bondedFlipperDevices(context: Context): List<BluetoothDevice> {
            val manager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
            val adapter: BluetoothAdapter = manager.adapter ?: return emptyList()
            return adapter.bondedDevices
                .filter { device ->
                    val name = device.name?.lowercase() ?: ""
                    name.contains("flipper") || name.contains("akb")
                }
                .sortedBy { it.name ?: it.address }
        }
    }
}
