#!/usr/bin/env python3
"""Source-level regression checks for the explicit macOS HID setup flow."""
from pathlib import Path

source = Path("ck42x_passvault.c").read_text(encoding="utf-8")

required = (
    "CkEventMacKeyboardSetup",
    '"macOS Keyboard Setup"',
    "static void ck_show_mac_keyboard_setup(CkApp* app)",
    "static void ck_send_mac_ansi_keys(CkApp* app)",
    "static void ck_finish_mac_keyboard_setup(CkApp* app)",
    "static void ck_stop_mac_keyboard_setup(CkApp* app)",
    "furi_hal_usb_set_config(&usb_hid, NULL)",
    "furi_hal_hid_kb_press(HID_KEYBOARD_Z)",
    "furi_hal_hid_kb_press(HID_KEYBOARD_SLASH)",
    "furi_hal_usb_set_config(app->previous_usb, NULL)",
    "app->mac_setup_view",
    "app->mac_setup_keys_sent",
    "app->mac_hid_active",
    "app->mac_previous_usb",
)
for contract in required:
    assert contract in source, contract

signature = "static void ck_send_mac_ansi_keys(CkApp* app)"
send_start = source.index(signature, source.index(signature) + len(signature))
send_end = source.index("static void ck_hid_type_string", send_start)
send = source[send_start:send_end]
assert send.index("HID_KEYBOARD_Z") < send.index("HID_KEYBOARD_SLASH")
assert "password" not in send.lower()
assert "0x05ac" not in source.lower()
assert "0x021e" not in source.lower()

finish_signature = "static void ck_finish_mac_keyboard_setup(CkApp* app)"
finish_start = source.index(
    finish_signature, source.index(finish_signature) + len(finish_signature)
)
finish_end = source.index("static void ck_show_mac_keyboard_setup", finish_start)
finish = source[finish_start:finish_end]
assert "app->mac_hid_active = true;" in finish
assert "app->mac_setup_view = false;" in finish
assert "furi_hal_usb_set_config" not in finish

inject_start = source.index("static bool ck_inject_selected")
inject_end = source.index("static void ck_begin_auth", inject_start)
inject = source[inject_start:inject_end]
assert "app->previous_usb != &usb_hid" in inject
assert "app->previous_usb && app->previous_usb != &usb_hid" in inject

cleanup = source[source.index("static void ck_stop_mac_keyboard_setup") :]
assert cleanup.count("ck_stop_mac_keyboard_setup(app);") >= 3

print("OK: macOS keyboard setup lifecycle contract checks passed")
