package com.flipperzero.androidkeyboard.ble

/**
 * Wire frames (payload always 3 bytes after magic+len):
 *
 * Key:   FB 4B 03 [01|02] [mods] [keycode]
 * Mouse: FB 4B 03 [10|11|12|13] [a] [b]
 *
 * 0x01/0x02 = key down/up
 * 0x10 = mouse move (a=dx, b=dy as signed bytes)
 * 0x11/0x12 = mouse button down/up (a=button mask, b=0)
 * 0x13 = mouse scroll (a=delta signed, b=0)
 */
object BridgeProtocol {
    private const val MAGIC_0: Byte = 0xFB.toByte()
    private const val MAGIC_1: Byte = 0x4B
    private const val PAYLOAD_LEN: Byte = 0x03

    const val EVENT_KEY_DOWN: Byte = 0x01
    const val EVENT_KEY_UP: Byte = 0x02
    const val EVENT_MOUSE_MOVE: Byte = 0x10
    const val EVENT_MOUSE_DOWN: Byte = 0x11
    const val EVENT_MOUSE_UP: Byte = 0x12
    const val EVENT_MOUSE_SCROLL: Byte = 0x13

    fun encodeKeyEvent(down: Boolean, keyCode: Byte, modifiers: Byte = 0): ByteArray {
        return byteArrayOf(
            MAGIC_0,
            MAGIC_1,
            PAYLOAD_LEN,
            if (down) EVENT_KEY_DOWN else EVENT_KEY_UP,
            modifiers,
            keyCode,
        )
    }

    fun encodeMouseMove(dx: Byte, dy: Byte): ByteArray {
        return byteArrayOf(MAGIC_0, MAGIC_1, PAYLOAD_LEN, EVENT_MOUSE_MOVE, dx, dy)
    }

    fun encodeMouseButton(down: Boolean, button: Byte): ByteArray {
        return byteArrayOf(
            MAGIC_0,
            MAGIC_1,
            PAYLOAD_LEN,
            if (down) EVENT_MOUSE_DOWN else EVENT_MOUSE_UP,
            button,
            0,
        )
    }

    fun encodeMouseScroll(delta: Byte): ByteArray {
        return byteArrayOf(MAGIC_0, MAGIC_1, PAYLOAD_LEN, EVENT_MOUSE_SCROLL, delta, 0)
    }
}
