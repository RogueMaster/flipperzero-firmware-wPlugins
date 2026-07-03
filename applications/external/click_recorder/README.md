# Click Recorder for Flipper Zero

Turn your Flipper Zero into a powerful mouse automation tool. Record, replay, randomize, or spam mouse clicks over USB or Bluetooth. No software needed on the target computer just connect your flipper zero.

# Modes

## Record Clicks

Capture the exact timing of your mouse clicks, then replay them perfectly on the dot.

1. Choose a recording duration (1–60 seconds)
2. A 3-second countdown gives you time to get ready
3. Press **OK** for left clicks, **Back** for right clicks - both at the same moment records a dual click (up to 128 clicks)
4. **Tune the track** - stretch or compress the timing after recording and watch keyframes shift in real time
5. Position your cursor, pick **Play Once** or **Loop Forever**, and go

## Shuffle Clicks

Generate random click patterns within a time window. Each playback produces a brand new distribution - great for randomized stress testing or making automation look human.

1. Set a time window (1–60 seconds)
2. Choose how many clicks (1–128, default 25)
3. Position your cursor, then pick **Shuffle Once** or **Shuffle Forever**

## Spam Click

Rapid-fire a burst of clicks at 100ms intervals.

1. Set a time window (1–60 seconds)
2. Choose how many clicks (1–255) and which button - **Left**, **Middle**, or **Right**
3. Position your cursor, then pick **Spam Once** or **Spam Forever**
4. Press **OK** to pause/resume mid-spam, **Back** to stop

## Computer Mouse

Use your Flipper as a full wireless or wired mouse.

- **Arrow keys** move the cursor (tap = 5px, hold = 20px)
- **OK** = left click (hold for drag)
- **Back** = right click (hold for drag)
- On-screen indicators show which buttons are currently pressed

# Highlights

- **USB + Bluetooth** - switch between wired and wireless from the main menu with Left/Right
- **BLE auto-discovery** - advertises as "ClickRec" or as the Flipper name configured with a blue LED when a device connects
- **Track tuning** - after recording, adjust the duration and all your clicks scale proportionally
- **Pause / resume** - pause playback or spam clicking at any time with OK
- **Loop modes** - play once, loop forever, shuffle forever, or spam forever
- **Works on any OS** - standard HID mouse protocol, no drivers or software required
- **In-app menu** - long-press Back from any screen to access Resume, Menu, About, or Quit

# Controls

| Screen | Key | Action |
|---|---|---|
| **Main Menu** | Up / Down | Navigate modes |
| | Left / Right | Toggle USB / BLE |
| | OK | Select mode |
| | Long Back | Exit the app |
| **Duration** | Up / Down | Adjust seconds (1–60) |
| | OK | Continue |
| | Back | Go back |
| **Recording** | OK | Record left click |
| | Back | Record right click |
| **Tune Track** | Left / Right | Shrink / expand duration |
| | OK | Continue to positioning |
| **Spam Config** | Up / Down | Adjust click count (1–255) |
| | Left / Right | Choose button (L / M / R) |
| **Positioning** | Arrows | Move cursor |
| | OK | Continue to play mode |
| **Play Mode** | Up / Down | Toggle between once / forever |
| | OK | Start |
| **Playback** | OK / Back | Pause |
| | Left / Right | Toggle loop mode |
| **Spamming** | OK | Pause / resume |
| | Back | Stop |
| **Computer Mouse** | Arrows | Move cursor |
| | OK (hold) | Left click |
| | Back (hold) | Right click |
| **Any screen** | Long Back | Open menu |

# Troubleshooting

| Problem | Solution |
|---|---|
| No mouse movement | Check that USB HID is connected (look at the Flipper's notification bar) |
| BLE not connecting | Make sure Bluetooth is enabled on both devices - look for "ClickRec" or the flipper name configured in your device's Bluetooth settings |
| App not showing on Flipper | Verify the .fap file is at apps/USB/mouse_click_recorder.fap on the SD card |
| Build errors | Run ufbt update to fetch the latest SDK |

## Author

Created by **0x78f1935** aka **undeƒined**
