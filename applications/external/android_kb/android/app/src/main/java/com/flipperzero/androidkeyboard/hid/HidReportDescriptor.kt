package com.flipperzero.androidkeyboard.hid

/**
 * Combo HID descriptor: Report ID 1 = boot-style keyboard, Report ID 2 = mouse.
 * Used with [android.bluetooth.BluetoothHidDevice].
 */
object HidReportDescriptor {
    const val REPORT_ID_KEYBOARD = 1
    const val REPORT_ID_MOUSE = 2

    val DESCRIPTOR: ByteArray = byteArrayOf(
        // Keyboard
        0x05, 0x01, // Usage Page (Generic Desktop)
        0x09, 0x06, // Usage (Keyboard)
        0xA1.toByte(), 0x01, // Collection (Application)
        0x85.toByte(), REPORT_ID_KEYBOARD.toByte(),
        0x05, 0x07, //   Usage Page (Key Codes)
        0x19, 0xE0.toByte(), //   Usage Minimum (224)
        0x29, 0xE7.toByte(), //   Usage Maximum (231)
        0x15, 0x00, //   Logical Minimum (0)
        0x25, 0x01, //   Logical Maximum (1)
        0x75, 0x01, //   Report Size (1)
        0x95.toByte(), 0x08, //   Report Count (8)
        0x81.toByte(), 0x02, //   Input (Data,Var,Abs) ; Modifier byte
        0x95.toByte(), 0x01, //   Report Count (1)
        0x75, 0x08, //   Report Size (8)
        0x81.toByte(), 0x01, //   Input (Const) ; Reserved
        0x95.toByte(), 0x06, //   Report Count (6)
        0x75, 0x08, //   Report Size (8)
        0x15, 0x00, //   Logical Minimum (0)
        0x25, 0x65, //   Logical Maximum (101)
        0x05, 0x07, //   Usage Page (Key Codes)
        0x19, 0x00, //   Usage Minimum (0)
        0x29, 0x65, //   Usage Maximum (101)
        0x81.toByte(), 0x00, //   Input (Data,Array) ; Key array
        0xC0.toByte(), // End Collection

        // Mouse
        0x05, 0x01, // Usage Page (Generic Desktop)
        0x09, 0x02, // Usage (Mouse)
        0xA1.toByte(), 0x01, // Collection (Application)
        0x85.toByte(), REPORT_ID_MOUSE.toByte(),
        0x09, 0x01, //   Usage (Pointer)
        0xA1.toByte(), 0x00, //   Collection (Physical)
        0x05, 0x09, //     Usage Page (Buttons)
        0x19, 0x01, //     Usage Minimum (1)
        0x29, 0x03, //     Usage Maximum (3)
        0x15, 0x00, //     Logical Minimum (0)
        0x25, 0x01, //     Logical Maximum (1)
        0x95.toByte(), 0x03, //     Report Count (3)
        0x75, 0x01, //     Report Size (1)
        0x81.toByte(), 0x02, //     Input (Data,Var,Abs)
        0x95.toByte(), 0x01, //     Report Count (1)
        0x75, 0x05, //     Report Size (5)
        0x81.toByte(), 0x01, //     Input (Const)
        0x05, 0x01, //     Usage Page (Generic Desktop)
        0x09, 0x30, //     Usage (X)
        0x09, 0x31, //     Usage (Y)
        0x09, 0x38, //     Usage (Wheel)
        0x15, 0x81.toByte(), //     Logical Minimum (-127)
        0x25, 0x7F, //     Logical Maximum (127)
        0x75, 0x08, //     Report Size (8)
        0x95.toByte(), 0x03, //     Report Count (3)
        0x81.toByte(), 0x06, //     Input (Data,Var,Rel)
        0xC0.toByte(), //   End Collection
        0xC0.toByte(), // End Collection
    )
}
