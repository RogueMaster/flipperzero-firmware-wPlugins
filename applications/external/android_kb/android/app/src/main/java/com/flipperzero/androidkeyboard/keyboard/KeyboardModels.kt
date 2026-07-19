package com.flipperzero.androidkeyboard.keyboard

data class LayoutInfo(
    val id: String,
    val title: String,
    val file: String,
)

data class KeyboardLayout(
    val id: String,
    val name: String,
    val rows: List<List<KeyboardKey>>,
)

data class KeyboardKey(
    val label: String,
    val hid: Byte,
    val mods: Byte = 0,
    val span: Float = 1f,
    /** If true, tapping toggles sticky mods instead of sending a key. */
    val stickyMod: Boolean = false,
    /** Special role, e.g. "space" enables swipe-to-switch layouts. */
    val role: String? = null,
) {
    val isSpace: Boolean
        get() = role == ROLE_SPACE || hid == HID_SPACE
}

const val ROLE_SPACE = "space"
const val HID_SPACE: Byte = 0x2C
