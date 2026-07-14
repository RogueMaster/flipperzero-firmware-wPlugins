"""FlipDeck Flipper communication – auto-detect, upload config, create dirs."""

import glob
import os
import sys
import time

CONFIG_REMOTE_DIR = "/ext/apps_data/flipdeck"
CONFIG_REMOTE_PATH = f"{CONFIG_REMOTE_DIR}/config.txt"


def find_flipper_port():
    """Auto-detect Flipper Zero serial port."""
    patterns = []
    if sys.platform == "darwin":
        patterns = ["/dev/cu.usbmodemflip_*"]
    elif sys.platform == "linux":
        patterns = ["/dev/ttyACM*", "/dev/serial/by-id/*flipper*"]
    elif sys.platform == "win32":
        # Windows: would need pyserial to enumerate
        return None

    for pattern in patterns:
        matches = glob.glob(pattern)
        if matches:
            return matches[0]
    return None


def is_flipper_connected():
    """Check if a Flipper is connected."""
    return find_flipper_port() is not None


def _open_serial(port, baud=230400, timeout=2):
    """Open serial connection to Flipper CLI."""
    import serial

    return serial.Serial(port, baud, timeout=timeout)


def _send_cli(ser, command, wait=0.3):
    """Send a CLI command and return response."""
    # Send Ctrl+C to cancel any running command
    ser.write(b"\x03")
    time.sleep(0.1)
    ser.read(ser.in_waiting or 1)

    ser.write(f"{command}\r\n".encode())
    time.sleep(wait)
    response = ser.read(ser.in_waiting or 1).decode(errors="replace")
    return response


def _ensure_dir(ser, path):
    """Create directory on Flipper if it doesn't exist."""
    _send_cli(ser, f"storage mkdir {path}", wait=0.2)


def upload_config(config_path, port=None):
    """Upload a config.txt file to Flipper SD card."""
    if not os.path.exists(config_path):
        raise FileNotFoundError(f"Config file not found: {config_path}")

    with open(config_path) as f:
        data = f.read()

    if not port:
        port = find_flipper_port()
    if not port:
        raise ConnectionError("Flipper not found. Is it connected via USB?")

    upload_config_data(data, port)
    print(f"  \u2713 Uploaded {config_path} to {CONFIG_REMOTE_PATH}")


def upload_config_data(config_text, port):
    """Upload config text directly to Flipper."""
    import serial

    ser = _open_serial(port)
    try:
        # Ensure directory exists
        _ensure_dir(ser, "/ext/apps_data")
        _ensure_dir(ser, CONFIG_REMOTE_DIR)

        # Write file using storage write command
        # Flipper CLI: storage write_chunk <path> <size>
        data = config_text.encode("utf-8")
        _send_cli(ser, f"storage remove {CONFIG_REMOTE_PATH}", wait=0.2)

        # Use storage write command
        ser.write(f"storage write_chunk {CONFIG_REMOTE_PATH} {len(data)}\r\n".encode())
        time.sleep(0.2)
        ser.write(data)
        time.sleep(0.5)

        response = ser.read(ser.in_waiting or 1).decode(errors="replace")

        if "error" in response.lower() and "exist" not in response.lower():
            # Fallback: try line-by-line echo approach
            _send_cli(ser, f"storage remove {CONFIG_REMOTE_PATH}", wait=0.2)
            for line in config_text.split("\n"):
                line = line.strip()
                if line:
                    # Escape special chars
                    safe = line.replace('"', '\\"')
                    _send_cli(
                        ser, f'storage write {CONFIG_REMOTE_PATH} "{safe}"', wait=0.1
                    )

    finally:
        ser.close()


def read_flipper_config(port=None):
    """Read config.txt from Flipper."""
    if not port:
        port = find_flipper_port()
    if not port:
        return None

    import serial

    ser = _open_serial(port)
    try:
        response = _send_cli(ser, f"storage read {CONFIG_REMOTE_PATH}", wait=0.5)
        # Parse out the file content from CLI response
        lines = response.split("\n")
        content_lines = []
        capture = False
        for line in lines:
            if "storage read" in line:
                capture = True
                continue
            if capture and line.strip().startswith(">"):
                break
            if capture:
                content_lines.append(line)
        return "\n".join(content_lines).strip()
    except Exception:
        return None
    finally:
        ser.close()
