package com.flipperzero.androidkeyboard

import android.content.Context
import android.content.Intent
import android.os.Build
import com.flipperzero.androidkeyboard.ble.FlipperBleClient
import com.flipperzero.androidkeyboard.hid.DirectHidClient
import com.flipperzero.androidkeyboard.hid.MouseButtons
import com.flipperzero.androidkeyboard.prefs.AppPreferences

/**
 * Routes keyboard/touchpad input to either Flipper BLE Serial or direct Bluetooth HID.
 */
object BridgeSession : InputSink {

    enum class State {
        DISCONNECTED,
        CONNECTING,
        /** Direct BT: waiting for the PC to pair / connect. */
        WAITING_PAIRING,
        CONNECTED,
        READY,
        ERROR,
    }

    fun interface Listener {
        fun onStateChanged(state: State, message: String)
    }

    fun interface DiscoverableRequestListener {
        fun onDiscoverableNeeded(intent: Intent)
    }

    private var flipper: FlipperBleClient? = null
    private var direct: DirectHidClient? = null
    private var listener: Listener? = null
    private var discoverableListener: DiscoverableRequestListener? = null
    private var prefs: AppPreferences? = null

    @Volatile
    var state: State = State.DISCONNECTED
        private set

    @Volatile
    var statusMessage: String = "Disconnected"
        private set

    fun init(context: Context) {
        val appContext = context.applicationContext
        prefs = AppPreferences(appContext)
        if (flipper == null) {
            flipper = FlipperBleClient(appContext)
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P && direct == null) {
            direct = DirectHidClient(appContext)
        }
        wireListeners()
    }

    fun setListener(listener: Listener?) {
        this.listener = listener
        listener?.onStateChanged(state, statusMessage)
    }

    fun setDiscoverableRequestListener(listener: DiscoverableRequestListener?) {
        discoverableListener = listener
    }

    fun connect(context: Context) {
        init(context)
        val mode = prefs?.outputMode ?: OutputMode.FLIPPER
        when (mode) {
            OutputMode.FLIPPER -> {
                val mac = prefs?.flipperMac
                if (mac.isNullOrBlank()) {
                    publish(State.ERROR, "Set Flipper MAC in Settings")
                    return
                }
                disconnectOther(OutputMode.DIRECT_BT)
                flipper?.connectAddress(mac)
            }
            OutputMode.DIRECT_BT -> {
                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
                    publish(State.ERROR, "Direct Bluetooth needs Android 9+")
                    return
                }
                disconnectOther(OutputMode.FLIPPER)
                // Try saved host; if missing/unreachable → wait for pairing.
                direct?.start(prefs?.hostMac, prefs?.hidDeviceName)
            }
        }
    }

    fun disconnect() {
        flipper?.disconnect()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            direct?.disconnect()
        }
        publish(State.DISCONNECTED, "Disconnected")
    }

    override fun sendTap(keyCode: Byte, modifiers: Byte): Boolean {
        return when (prefs?.outputMode) {
            OutputMode.DIRECT_BT -> direct?.sendTap(keyCode, modifiers) == true
            else -> flipper?.sendTap(keyCode, modifiers) == true
        }
    }

    override fun sendMouseMove(dx: Int, dy: Int): Boolean {
        return when (prefs?.outputMode) {
            OutputMode.DIRECT_BT -> direct?.sendMouseMove(dx, dy) == true
            else -> flipper?.sendMouseMove(dx, dy) == true
        }
    }

    override fun sendMouseButton(down: Boolean, button: Byte): Boolean {
        val mask = if (button.toInt() == 0) MouseButtons.LEFT else button
        return when (prefs?.outputMode) {
            OutputMode.DIRECT_BT -> direct?.sendMouseButton(down, mask) == true
            else -> flipper?.sendMouseButton(down, mask) == true
        }
    }

    override fun sendMouseScroll(delta: Int): Boolean {
        return when (prefs?.outputMode) {
            OutputMode.DIRECT_BT -> direct?.sendMouseScroll(delta) == true
            else -> flipper?.sendMouseScroll(delta) == true
        }
    }

    override fun isReady(): Boolean = state == State.READY

    private fun disconnectOther(mode: OutputMode) {
        when (mode) {
            OutputMode.FLIPPER -> flipper?.disconnect()
            OutputMode.DIRECT_BT -> if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                direct?.disconnect()
            }
        }
    }

    private fun wireListeners() {
        flipper?.setListener(object : FlipperBleClient.Listener {
            override fun onStateChanged(state: FlipperBleClient.State, message: String) {
                if (prefs?.outputMode != OutputMode.FLIPPER) return
                publish(mapFlipper(state), message)
            }
        })
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            direct?.setListener(object : DirectHidClient.Listener {
                override fun onStateChanged(state: DirectHidClient.State, message: String) {
                    if (prefs?.outputMode != OutputMode.DIRECT_BT) return
                    publish(mapDirect(state), message)
                }
            })
            direct?.setHostConnectedListener(object : DirectHidClient.HostConnectedListener {
                override fun onHostConnected(address: String, name: String?) {
                    prefs?.hostMac = address
                }
            })
            direct?.setDiscoverableRequestListener(
                object : DirectHidClient.DiscoverableRequestListener {
                    override fun onDiscoverableNeeded(intent: Intent) {
                        discoverableListener?.onDiscoverableNeeded(intent)
                    }
                },
            )
        }
    }

    private fun mapFlipper(state: FlipperBleClient.State): State = when (state) {
        FlipperBleClient.State.DISCONNECTED -> State.DISCONNECTED
        FlipperBleClient.State.CONNECTING -> State.CONNECTING
        FlipperBleClient.State.CONNECTED -> State.CONNECTED
        FlipperBleClient.State.READY -> State.READY
        FlipperBleClient.State.ERROR -> State.ERROR
    }

    private fun mapDirect(state: DirectHidClient.State): State = when (state) {
        DirectHidClient.State.DISCONNECTED -> State.DISCONNECTED
        DirectHidClient.State.CONNECTING -> State.CONNECTING
        DirectHidClient.State.WAITING_PAIRING -> State.WAITING_PAIRING
        DirectHidClient.State.CONNECTED -> State.CONNECTED
        DirectHidClient.State.READY -> State.READY
        DirectHidClient.State.ERROR -> State.ERROR
    }

    private fun publish(newState: State, message: String) {
        state = newState
        statusMessage = message
        listener?.onStateChanged(newState, message)
    }
}
