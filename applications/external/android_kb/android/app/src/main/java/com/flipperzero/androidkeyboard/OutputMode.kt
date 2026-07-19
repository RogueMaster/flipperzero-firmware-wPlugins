package com.flipperzero.androidkeyboard

enum class OutputMode {
    /** Phone → Flipper BLE Serial → USB HID → PC */
    FLIPPER,

    /** Phone → Bluetooth HID → PC (no Flipper) */
    DIRECT_BT,
}
