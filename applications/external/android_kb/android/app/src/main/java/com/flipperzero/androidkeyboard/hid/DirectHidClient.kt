package com.flipperzero.androidkeyboard.hid

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothHidDevice
import android.bluetooth.BluetoothHidDeviceAppSdpSettings
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.Handler
import android.os.Looper
import androidx.annotation.RequiresApi
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicReference

/**
 * Phone acts as a Bluetooth HID keyboard/mouse toward a PC (no Flipper).
 *
 * Connect flow:
 * 1. Register HID app
 * 2. If a previously paired host MAC is known — try reconnect
 * 3. Otherwise (or on failure) — become discoverable and wait for the PC to pair/connect
 *
 * Requires API 28+ and OEM support for the HID Device profile.
 */
@RequiresApi(Build.VERSION_CODES.P)
class DirectHidClient(private val context: Context) {

    enum class State {
        DISCONNECTED,
        CONNECTING,
        /** HID app registered; phone is discoverable / waiting for PC to initiate pairing. */
        WAITING_PAIRING,
        CONNECTED,
        READY,
        ERROR,
    }

    fun interface Listener {
        fun onStateChanged(state: State, message: String)
    }

    fun interface HostConnectedListener {
        fun onHostConnected(address: String, name: String?)
    }

    fun interface DiscoverableRequestListener {
        /** UI should launch [createDiscoverableIntent]. */
        fun onDiscoverableNeeded(intent: Intent)
    }

    private val mainHandler = Handler(Looper.getMainLooper())
    private val listenerRef = AtomicReference<Listener?>(null)
    private val hostConnectedRef = AtomicReference<HostConnectedListener?>(null)
    private val discoverableRef = AtomicReference<DiscoverableRequestListener?>(null)
    private val executor = Executors.newSingleThreadExecutor()

    private var adapter: BluetoothAdapter? = null
    private var hidDevice: BluetoothHidDevice? = null
    private var hostDevice: BluetoothDevice? = null
    private var registered = false
    private var mouseButtons: Byte = 0
    private var preferredHostMac: String? = null
    private var deviceName: String = DEFAULT_DEVICE_NAME
    private var registeredName: String? = null
    private var savedAdapterName: String? = null
    private var adapterNameOverridden = false
    private var reconnectAttempted = false

    private val reconnectTimeoutRunnable = Runnable {
        if (state == State.CONNECTING) {
            enterWaitingForPairing("No response from saved PC — waiting to pair")
        }
    }

    @Volatile
    var state: State = State.DISCONNECTED
        private set

    @Volatile
    var statusMessage: String = "Disconnected"
        private set

    fun setListener(listener: Listener?) {
        listenerRef.set(listener)
    }

    fun setHostConnectedListener(listener: HostConnectedListener?) {
        hostConnectedRef.set(listener)
    }

    fun setDiscoverableRequestListener(listener: DiscoverableRequestListener?) {
        discoverableRef.set(listener)
    }

    /**
     * Start Direct BT session: reconnect to [preferredHostMac] if possible, else wait for pairing.
     * [deviceName] is how the phone appears as a Bluetooth HID keyboard on the PC.
     */
    fun start(preferredHostMac: String?, deviceName: String? = null) {
        this.preferredHostMac = preferredHostMac?.trim()?.uppercase()?.takeIf { it.isNotBlank() }
        this.deviceName = normalizeDeviceName(deviceName)
        reconnectAttempted = false
        mainHandler.removeCallbacks(reconnectTimeoutRunnable)

        prepare { ok, message ->
            if (!ok) {
                updateState(State.ERROR, message)
                return@prepare
            }
            applyAdapterName()
            tryResumeOrWait()
        }
    }

    @SuppressLint("MissingPermission")
    private fun tryResumeOrWait() {
        val hid = hidDevice ?: return

        val already = hid.connectedDevices.firstOrNull()
        if (already != null) {
            hostDevice = already
            rememberHost(already)
            updateState(State.READY, "Connected to ${already.name ?: already.address}")
            return
        }

        val mac = preferredHostMac
        val bonded = if (mac != null) findBonded(mac) else null
        if (bonded != null) {
            reconnectAttempted = true
            hostDevice = bonded
            updateState(State.CONNECTING, "Reconnecting to ${bonded.name ?: bonded.address}…")
            val started = hid.connect(bonded)
            if (!started) {
                enterWaitingForPairing("Could not reach saved PC — waiting to pair")
                return
            }
            mainHandler.postDelayed(reconnectTimeoutRunnable, RECONNECT_TIMEOUT_MS)
            return
        }

        enterWaitingForPairing(
            if (mac == null) {
                "Ready to pair — connect from the PC"
            } else {
                "Saved PC not bonded — waiting to pair"
            },
        )
    }

    @SuppressLint("MissingPermission")
    private fun enterWaitingForPairing(message: String) {
        mainHandler.removeCallbacks(reconnectTimeoutRunnable)
        hostDevice = null
        updateState(State.WAITING_PAIRING, message)
        requestDiscoverable()
    }

    @SuppressLint("MissingPermission")
    private fun requestDiscoverable() {
        val bt = adapter ?: return
        // Already discoverable — nothing to ask.
        if (bt.scanMode == BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE) {
            return
        }
        val intent = createDiscoverableIntent()
        mainHandler.post {
            discoverableRef.get()?.onDiscoverableNeeded(intent)
        }
    }

    fun createDiscoverableIntent(): Intent {
        return Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE).apply {
            putExtra(BluetoothAdapter.EXTRA_DISCOVERABLE_DURATION, DISCOVERABLE_SECONDS)
        }
    }

    fun prepare(onReady: (Boolean, String) -> Unit) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            onReady(false, "Direct Bluetooth HID needs Android 9+")
            return
        }
        val manager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        val bt = manager.adapter
        if (bt == null || !bt.isEnabled) {
            onReady(false, "Bluetooth is off")
            return
        }
        adapter = bt
        if (hidDevice != null && registered && registeredName == deviceName) {
            onReady(true, "Already prepared")
            return
        }
        if (hidDevice != null && registered && registeredName != deviceName) {
            // Name changed — re-register HID SDP with the new name.
            try {
                hidDevice?.unregisterApp()
            } catch (_: Exception) {
                // ignore
            }
            registered = false
            registeredName = null
            registerHidApp { success, message ->
                mainHandler.post { onReady(success, message) }
            }
            return
        }
        if (hidDevice != null && !registered) {
            registerHidApp { success, message ->
                mainHandler.post { onReady(success, message) }
            }
            return
        }
        val ok = bt.getProfileProxy(
            context,
            object : BluetoothProfile.ServiceListener {
                override fun onServiceConnected(profile: Int, proxy: BluetoothProfile) {
                    if (profile != BluetoothProfile.HID_DEVICE) return
                    hidDevice = proxy as BluetoothHidDevice
                    registerHidApp { success, message ->
                        mainHandler.post { onReady(success, message) }
                    }
                }

                override fun onServiceDisconnected(profile: Int) {
                    if (profile == BluetoothProfile.HID_DEVICE) {
                        hidDevice = null
                        registered = false
                        registeredName = null
                        updateState(State.DISCONNECTED, "HID service disconnected")
                    }
                }
            },
            BluetoothProfile.HID_DEVICE,
        )
        if (!ok) {
            onReady(false, "HID Device profile unavailable on this phone")
        }
    }

    @SuppressLint("MissingPermission")
    private fun registerHidApp(done: (Boolean, String) -> Unit) {
        val hid = hidDevice ?: run {
            done(false, "HID proxy missing")
            return
        }
        if (registered && registeredName == deviceName) {
            done(true, "Already registered")
            return
        }

        val sdp = BluetoothHidDeviceAppSdpSettings(
            deviceName,
            "Phone keyboard and touchpad",
            "AndroidKeyboard",
            BluetoothHidDevice.SUBCLASS1_COMBO,
            HidReportDescriptor.DESCRIPTOR,
        )

        val registeredOk = hid.registerApp(
            sdp,
            null,
            null,
            executor,
            object : BluetoothHidDevice.Callback() {
                override fun onAppStatusChanged(pluggedDevice: BluetoothDevice?, registered: Boolean) {
                    this@DirectHidClient.registered = registered
                    if (!registered) {
                        updateState(State.DISCONNECTED, "HID app unregistered")
                    } else if (pluggedDevice != null) {
                        hostDevice = pluggedDevice
                        rememberHost(pluggedDevice)
                        updateState(State.READY, "Connected to ${pluggedDevice.name ?: pluggedDevice.address}")
                    }
                }

                override fun onConnectionStateChanged(device: BluetoothDevice?, connectionState: Int) {
                    when (connectionState) {
                        BluetoothProfile.STATE_CONNECTING -> {
                            if (state != State.WAITING_PAIRING) {
                                updateState(
                                    State.CONNECTING,
                                    "Connecting to ${device?.name ?: device?.address ?: "PC"}…",
                                )
                            }
                        }
                        BluetoothProfile.STATE_CONNECTED -> {
                            mainHandler.removeCallbacks(reconnectTimeoutRunnable)
                            if (device != null) {
                                hostDevice = device
                                rememberHost(device)
                                updateState(State.READY, "Connected to ${device.name ?: device.address}")
                            }
                        }
                        BluetoothProfile.STATE_DISCONNECTED -> {
                            if (hostDevice?.address == device?.address) {
                                hostDevice = null
                            }
                            // Stay discoverable if user started a session and PC dropped.
                            if (state == State.READY || state == State.CONNECTING) {
                                enterWaitingForPairing("PC disconnected — waiting to pair")
                            }
                        }
                    }
                }
            },
        )

        if (registeredOk) {
            registered = true
            registeredName = deviceName
            done(true, "HID app registered")
        } else {
            done(false, "Failed to register HID app")
        }
    }

    @SuppressLint("MissingPermission")
    private fun applyAdapterName() {
        val bt = adapter ?: return
        val desired = deviceName
        try {
            val current = bt.name
            if (!adapterNameOverridden) {
                savedAdapterName = current
                adapterNameOverridden = true
            }
            if (current != desired) {
                bt.name = desired
            }
        } catch (_: SecurityException) {
            // Missing BLUETOOTH_CONNECT — SDP name still applies.
        }
    }

    @SuppressLint("MissingPermission")
    private fun restoreAdapterName() {
        if (!adapterNameOverridden) return
        val bt = adapter ?: return
        val original = savedAdapterName
        adapterNameOverridden = false
        savedAdapterName = null
        if (original.isNullOrBlank()) return
        try {
            if (bt.name != original) {
                bt.name = original
            }
        } catch (_: SecurityException) {
            // ignore
        }
    }

    @SuppressLint("MissingPermission")
    private fun findBonded(mac: String): BluetoothDevice? {
        val bt = adapter ?: return null
        return bt.bondedDevices.firstOrNull { it.address.equals(mac, ignoreCase = true) }
    }

    @SuppressLint("MissingPermission")
    private fun rememberHost(device: BluetoothDevice) {
        preferredHostMac = device.address.uppercase()
        mainHandler.post {
            hostConnectedRef.get()?.onHostConnected(device.address, device.name)
        }
    }

    @SuppressLint("MissingPermission")
    fun disconnect() {
        mainHandler.removeCallbacks(reconnectTimeoutRunnable)
        val hid = hidDevice
        val host = hostDevice
        if (hid != null && host != null) {
            hid.disconnect(host)
        }
        mouseButtons = 0
        hostDevice = null
        restoreAdapterName()
        updateState(State.DISCONNECTED, "Disconnected")
    }

    fun release() {
        disconnect()
        val hid = hidDevice
        if (hid != null && registered) {
            hid.unregisterApp()
            registered = false
        }
        adapter?.closeProfileProxy(BluetoothProfile.HID_DEVICE, hid)
        hidDevice = null
    }

    fun sendTap(keyCode: Byte, modifiers: Byte = 0): Boolean {
        if (state != State.READY) return false
        if (!sendKeyboardReport(modifiers, keyCode)) return false
        mainHandler.postDelayed({ sendKeyboardReport(0, 0) }, TAP_RELEASE_MS)
        return true
    }

    fun sendMouseMove(dx: Int, dy: Int): Boolean {
        if (state != State.READY) return false
        return sendMouseReport(clamp(dx), clamp(dy), 0)
    }

    fun sendMouseButton(down: Boolean, button: Byte): Boolean {
        if (state != State.READY) return false
        mouseButtons = if (down) {
            (mouseButtons.toInt() or button.toInt()).toByte()
        } else {
            (mouseButtons.toInt() and button.toInt().inv()).toByte()
        }
        return sendMouseReport(0, 0, 0)
    }

    fun sendMouseScroll(delta: Int): Boolean {
        if (state != State.READY) return false
        return sendMouseReport(0, 0, clamp(delta))
    }

    @SuppressLint("MissingPermission")
    private fun sendKeyboardReport(modifiers: Byte, keyCode: Byte): Boolean {
        val hid = hidDevice ?: return false
        val host = hostDevice ?: return false
        val report = byteArrayOf(
            modifiers,
            0,
            keyCode,
            0, 0, 0, 0, 0,
        )
        return hid.sendReport(host, HidReportDescriptor.REPORT_ID_KEYBOARD, report)
    }

    @SuppressLint("MissingPermission")
    private fun sendMouseReport(dx: Byte, dy: Byte, wheel: Byte): Boolean {
        val hid = hidDevice ?: return false
        val host = hostDevice ?: return false
        val report = byteArrayOf(mouseButtons, dx, dy, wheel)
        return hid.sendReport(host, HidReportDescriptor.REPORT_ID_MOUSE, report)
    }

    private fun updateState(newState: State, message: String) {
        state = newState
        statusMessage = message
        mainHandler.post {
            listenerRef.get()?.onStateChanged(newState, message)
        }
    }

    companion object {
        private const val TAP_RELEASE_MS = 60L
        private const val RECONNECT_TIMEOUT_MS = 8_000L
        private const val DISCOVERABLE_SECONDS = 120
        private const val DEFAULT_DEVICE_NAME = "Flipper KB Bridge"
        private const val MAX_DEVICE_NAME_LEN = 32

        private fun clamp(value: Int): Byte = value.coerceIn(-127, 127).toByte()

        private fun normalizeDeviceName(raw: String?): String {
            val cleaned = raw?.trim()?.take(MAX_DEVICE_NAME_LEN).orEmpty()
            return cleaned.ifBlank { DEFAULT_DEVICE_NAME }
        }

        @SuppressLint("MissingPermission")
        fun bondedHosts(context: Context): List<BluetoothDevice> {
            val manager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
            val adapter = manager.adapter ?: return emptyList()
            return adapter.bondedDevices
                .sortedBy { it.name ?: it.address }
        }

        fun isSupported(context: Context): Boolean {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return false
            val manager = context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
            return manager.adapter != null
        }
    }
}
