#!/usr/bin/env python3
"""Keep the three USB transport identities distinct and consistently wired."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]
IDS = (ROOT / "src/firmware/usb/morse_usb_ids.h").read_text()
APP = (ROOT / "src/firmware/morse_flipper_app.c").read_text()
TRANSPORT = (ROOT / "src/firmware/morse_flipper_transport.c").read_text()
MIDI = (ROOT / "src/firmware/usb/morse_usb_midi.c").read_text()


def value(name: str) -> int:
    match = re.search(rf"^#define\s+{name}\s+0x([0-9A-Fa-f]+)U$", IDS, re.MULTILINE)
    assert match is not None, f"missing {name}"
    return int(match.group(1), 16)


assert value("MORSE_USB_VID") == 0x1209
assert value("MORSE_USB_KEYBOARD_PID") == 0x6900
assert value("MORSE_USB_MOUSE_PID") == 0x6901
assert value("MORSE_USB_MIDI_PID") == 0x6902

assert ".vid = MORSE_USB_VID" in APP
assert ".pid = MORSE_USB_KEYBOARD_PID" in APP
assert "app->hid_cfg.pid = MORSE_USB_KEYBOARD_PID" in TRANSPORT
assert "app->hid_cfg.pid = MORSE_USB_MOUSE_PID" in TRANSPORT
assert ".idVendor = MORSE_USB_VID" in MIDI
assert ".idProduct = MORSE_USB_MIDI_PID" in MIDI
assert "dev_descr->idVendor = MORSE_USB_VID" in MIDI
assert "dev_descr->idProduct = MORSE_USB_MIDI_PID" in MIDI

for obsolete in ("0x6666", "0x434B", "0x4357"):
    assert obsolete not in APP
    assert obsolete not in TRANSPORT
    assert obsolete not in MIDI

print("test_usb_ids: PASS")
