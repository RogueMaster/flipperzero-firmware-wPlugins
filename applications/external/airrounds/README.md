# AirRounds for Flipper Zero

AirRounds controls passive Wi-Fi sessions on the ESP32-S2 Wi-Fi Devboard.
Scan for nearby 2.4 GHz networks and select the approved points to capture.
The app and its public firmware are receive-only and add no transmit path.

## Build and connect

Run `make embedded` from the repository root with the [pinned SDK][sdk].
Copy `artifacts/quality/flipper/airrounds.fap` to `apps/GPIO/` on the Flipper
SD card and open **Apps → GPIO → AirRounds**.

Use the [AirRounds firmware and wiring guide][board] for the Devboard.
Close the Flipper USB-to-UART Bridge before opening AirRounds. Keep
the Devboard powered during a session.

## Scan and run a round

1. Open **Scan area**. Back cancels the scan. **Cancel failed** means the
   board reported an error while shutting down the scan receiver.
2. In **Beacons**, use Up and Down to move and OK to select one to five
   points. Press Right on **Start** to begin. Left runs another scan.
3. **Capture** shows the current network and how many points were captured.
   Press OK on **Stop** to end the round. A save already in progress can finish.
4. The app returns to **Beacons**. **Saved** identifies a verified beacon PCAP
   and receipt. **Not seen** identifies a finished visit without a matching
   beacon. Select that row to retry it. After Stop, unvisited rows stay selected.

Each row shows a network name. Identical names receive a number, and the scan
screen keeps BSSIDs and channels out of the interface. The list preserves
matching selections and outcomes during rescans while AirRounds is open.

**Capture mode** chooses the mode of the next round. Beacon is always available;
**EAPOL** is available once the board lists it. **Flash board hardware** and
**Test board** are empty menu entries. **About** shows the app version and the
board firmware version. An older board with scan support shows **Unknown**.
About is part of the scan menu only, so a board without scan support never shows
it. Back returns to the menu.

[Workflow and interactive mockup][scan-flow]

The current `Saved` row confirms a beacon record.

## Manual profiles on older boards

Use both updated builds for Scan area and five points. An older board still
uses the manual profile. Original single-visit boards can run one point.
Boards with the original round capability can run up to three.

1. Open **Set up points** or **Access points**.
1. Choose **Add point** and enter the six BSSID bytes with the hex keyboard.
1. Select **Save** on the keyboard. Use Left or Right to choose the channel.
1. Press OK on **Save**. Add up to three points.

Open a point to edit or remove it. Back from the channel screen returns
to BSSID entry. Back from BSSID entry discards the draft. The app saves
one profile on the Flipper card and loads it when the app opens again.
If the card cannot confirm a write, the app shows **Save not confirmed**.
If the saved profile changed while the app was open, reopen AirRounds to
load it before making edits.

## Run a session

1. Select **Start session**. Check every BSSID and channel on **Review session**.
1. Press OK on **Start**. **Observing** shows the active point and channel.
1. Press OK on **Stop** to stop the session. **Stopping...** waits for the
   board. **Reception stopped** means reception ended and the board is
   still finishing the file. **Saving point** means later visits can follow.
1. Open **Results** to see each point. Select a **Captured** point, then press
   Right for its file name. Read the lines as one name at the root of the
   Devboard card.

**Captured** means the board confirmed the closed file and its receipt.
**Not seen** means the board found no matching beacon. **Stopped** means
the stop left an unfinished point with no result. A card error or radio
error has its own label. The session summary shows **Session log failed**
when the Devboard reports a separate microSD journal failure.

## Recover a session

In the manual flow, Back leaves the status screen. Use **Session status** to return.
AirRounds opens a confirmed active session when the app starts.

If UART drops, the board may finish the session on its own. Reconnect the
cable to read the board's current state. AirRounds waits for that state
before offering a new Start or Stop. **Command unconfirmed** means the app
is still checking what happened. **Last result** shows the latest confirmed
result available to the app, including after a board restart.

A slow microSD card can block the app while it saves the profile. The app
records when each UART byte arrives, so a board reply that arrived in time
still counts after the save.

[Design and prototype][design] · [UART protocol][uart]

## Test without a device

Run `python3 tests/flipper/run_ui_emulator.py` from the repository root.
It builds with the LLVM from the [quality check setup][quality] and fails on
a memory leak. The headless emulator runs the native app source with simulated Flipper
services. It uses the AirRounds board protocol and scheduler with a fake
receiver. It covers scan cancellation, five-point selection, Stop, retry,
profile reload, slow profile saves, UART recovery, and older board firmware. It does not run
the ARM package or open a USB port.

[sdk]: ../docs/t0.md#flipper-zero
[board]: ../firmware/esp32/README.md
[scan-flow]: ../docs/ui-ux/scan-flow.md
[design]: ../docs/ui-ux/requirements.md
[uart]: ../docs/uart.md
[quality]: ../docs/quality.md#install-the-host-tools
