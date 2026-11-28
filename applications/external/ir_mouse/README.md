# IR & Wii Mouse

[![Flipper Zero](https://img.shields.io/badge/Flipper%20Zero-External%20App-2ea44f)](https://flipperzero.one/)
[![Wireless](https://img.shields.io/badge/Connection-Bluetooth%20LE%20HID-0078D4)](https://flipperzero.one/)
[![PC Tracker](https://img.shields.io/badge/PC%20Tracker-Windows-0078D4)](https://www.microsoft.com/windows)
[![Python](https://img.shields.io/badge/Python-3.8%2B-3776AB)](https://www.python.org/)

Use a Flipper Zero as a wireless BLE mouse and keyboard, or pair it with the Windows webcam tracker for a Wii-style pointer. The firmware app has three modes: map a TV remote in Learn Mode, use those mappings in Run Mode, or pulse the Flipper's IR LEDs in Beacon Mode.

## Features

- **Run Mode — TV Remote:** Turn learned infrared remote buttons into BLE mouse movement, left/right clicks, and Spacebar.
- **Learn Mode — On-Device Button Programmer:** Assign remote buttons to actions using the Flipper's directional buttons; mappings persist on the device.
- **Beacon Mode — Wii-Style Webcam Pointer:** Pulse the IR LEDs for webcam tracking and use Flipper buttons for wireless clicks.
- **Windows companion tracker:** Calibrate five screen points and move the cursor from the webcam's detected IR position.

## Flipper Zero Installation

1. Download `ir_mouse.fap` from the project's GitHub **Releases**.
2. Transfer it to the Flipper's SD card at `apps/Tools/ir_mouse.fap`.
3. On the Flipper, open **Apps → Tools → IR & Wii Mouse**.
4. Pair the Flipper's Bluetooth HID device with your computer in the computer's Bluetooth settings. The app header shows `BT: Ready` while waiting and `BT: Connected` when connected.

Press Left or Right to switch modes. Press Back to exit.

### Run Mode — TV Remote

Run Mode listens for signals from a TV remote and sends the action assigned to each learned button over BLE. Supported actions are Mouse Up, Down, Left, Right, Left Click, Right Click, and Spacebar. Program the mappings in Learn Mode first.

### Learn Mode — On-Device Button Programmer

Use Up and Down to select an action, then point the remote at the Flipper and press the remote button to assign. The Flipper saves the keymap to:

```text
/ext/apps_data/ir_mouse/keymap.txt
```

The mapping is loaded automatically the next time the app starts. `Saved!` confirms that the assignment was written.

### Beacon Mode — Wii-Style Webcam Pointer

Beacon Mode sends a short 38 kHz IR pulse every 60 ms for webcam tracking. Hold OK for a BLE left-click or hold Down for a BLE right-click; holding either button supports dragging. Run the PC tracker below while the Flipper is in Beacon Mode.

## PC Companion App Setup

The companion script requires Windows, Python, and a webcam. From the project folder, install its dependencies and start it:

```powershell
python -m pip install -r requirements.txt
python wii_tracker.py
```

If `python` is not on your PATH, use the Windows Python launcher (`py`) for these commands.

**Camera compatibility:** The tracker currently opens camera index `0` using OpenCV's default backend. It may work with other webcams, but it has not been tested with every camera; if your webcam is assigned a different index or is not detected, you may need to adjust the camera index in `wii_tracker.py`.

On first launch, follow the five targets—Center, Top-Left, Top-Right, Bottom-Left, and Bottom-Right—and press Space at each target to calibrate. Press C at any time to recalibrate; calibration is saved to `calibration.json` beside the script.

**Keep the room lights off while tracking.** The `Max Room Light` control defaults to 45 and can be adjusted from 10 to 120. If room brightness exceeds the selected limit, cursor movement pauses and a warning appears; tracking resumes automatically when the room is dark enough. Use the `Threshold` control to adjust IR detection. Press Q or Esc to quit.

The Flipper and PC must remain Bluetooth-connected for the Flipper's OK/Down click controls. Cursor movement is performed locally by `pyautogui`.

## Building from Source

Install uFBT and build the external app from the project directory:

```powershell
ufbt fap_ir_mouse
```

Alternatively, if uFBT is installed as a Python module:

```powershell
python -m ufbt fap_ir_mouse
```

The build output reports where `ir_mouse.fap` was created.
