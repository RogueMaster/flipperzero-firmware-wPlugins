"""Configuration for the host bridge daemon."""

import os
import sys
from pathlib import Path

SOCKET_PATH = os.environ.get(
    "FLIPPER_BRIDGE_SOCKET", "/tmp/claude-flipper-bridge.sock"
)

SERIAL_BAUD = 115200

SERIAL_PORT = os.environ.get("FLIPPER_SERIAL_PORT", "")

PING_INTERVAL = 5.0

DICTATION_POLL_INTERVAL = 2.0

SPACE_REPEAT_INTERVAL = 0.01

# ---------------------------------------------------------------------------
# Dictation backend
# ---------------------------------------------------------------------------
# "macos"  — macOS native dictation via AppleScript + pmset (default on macOS)
# "custom" — user-supplied shell commands (see DICTATION_*_CMD below)
# "none"   — disabled (default on non-macOS)
_default_dictation = "macos" if sys.platform == "darwin" else "none"
DICTATION_BACKEND = os.environ.get("FLIPPER_DICTATION_BACKEND", _default_dictation)

# Shell command to START dictation (required when DICTATION_BACKEND="custom")
DICTATION_START_CMD = os.environ.get("FLIPPER_DICTATION_START_CMD", "")

# Shell command to STOP dictation (optional; if empty, only ESC is sent)
DICTATION_STOP_CMD = os.environ.get("FLIPPER_DICTATION_STOP_CMD", "")

# Shell command that exits 0 while dictation is active (optional).
# If empty, the bridge assumes dictation is still running until the user
# presses UP again (manual toggle mode).
DICTATION_CHECK_CMD = os.environ.get("FLIPPER_DICTATION_CHECK_CMD", "")

# Transport selection: "auto" (default), "usb", or "ble"
# "auto" tries USB first, then BLE — matching Flipper app behaviour.
TRANSPORT = os.environ.get("FLIPPER_TRANSPORT", "auto")

# Path to cache file for auto-detected BT name (set by plugin session-start hook)
_plugin_data = os.environ.get("FLIPPER_PLUGIN_DATA", "")
BT_NAME_CACHE = os.path.join(_plugin_data, "bt_name") if _plugin_data else ""

# Bluetooth settings (used when TRANSPORT="ble")
# 0x3082 — the 16-bit service UUID Flipper advertises in every advertisement
# packet (observed on both official and Momentum firmware).
# Used as the primary scan filter so the bridge finds the Flipper even when
# it has been renamed.
FLIPPER_ADV_UUID = "00003082-0000-1000-8000-00805f9b34fb"
# BT_DEVICE_NAME is a fallback for the rare case where the advertisement
# does not include service UUIDs (e.g. OS-level advertisement caching).
BT_DEVICE_NAME   = os.environ.get("FLIPPER_BT_NAME", "Flipper")
BT_SCAN_TIMEOUT  = float(os.environ.get("FLIPPER_BT_SCAN_TIMEOUT", "10"))
BT_WRITE_CHUNK   = 128  # max bytes per BLE write; capped to negotiated MTU-3 at runtime

# Dual CDC mode: Flipper exposes two serial ports.
# Channel 0 = CLI (first port), Channel 1 = our app (second port).
# On macOS the second port has a higher suffix; on Linux both appear as ttyACM*.
SERIAL_GLOB_PATTERN = "/dev/ttyACM*" if sys.platform == "linux" else "/dev/cu.usbmodem*"

# Project root for scanning .claude/commands/
PROJECT_DIR = os.environ.get(
    "FLIPPER_PROJECT_DIR",
    str(Path(__file__).parent.parent.parent),
)

# Custom commands files (project-level overrides user-level)
CUSTOM_COMMANDS_FILES = [
    str(Path.home() / ".claude" / "flipper-commands.txt"),
    os.environ.get("FLIPPER_PROJECT_DIR", str(Path(__file__).parent.parent.parent))
    + "/.claude/flipper-commands.txt",
]
