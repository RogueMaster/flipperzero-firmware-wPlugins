package com.flipperzero.androidkeyboard

/**
 * Transport-agnostic input sink used by UI (keyboard / touchpad).
 * Implemented by [BridgeSession]; keeps Views free of Flipper/Direct clients.
 */
interface InputSink {
    fun sendTap(keyCode: Byte, modifiers: Byte = 0): Boolean
    fun sendMouseMove(dx: Int, dy: Int): Boolean
    fun sendMouseButton(down: Boolean, button: Byte): Boolean
    fun sendMouseScroll(delta: Int): Boolean
    fun isReady(): Boolean
}
