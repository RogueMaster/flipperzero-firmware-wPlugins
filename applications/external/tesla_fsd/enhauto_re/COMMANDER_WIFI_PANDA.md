# Major find: S3XY Commander has a WiFi + Panda UDP mode

Source: `tumik/S3XY-candump` (GitHub, 7★, last update 2026-04-02).
Confirmed by `nmullaney/candash` Android client (`PandaService.kt`) and the
ScanMyTesla iOS app — both connect to a real Commander over WiFi.

## What this changes

Until now we assumed Commander only spoke BLE (to the phone app) + CAN (to the
car). It actually has a **third interface**: an onboard WiFi AP that exposes both
CAN buses via the comma.ai **Panda UDP protocol**.

That means:

- The phone app is not required at all once WiFi is enabled. A laptop / Pi /
  Flipper-with-WiFi-devboard can talk straight to the dongle over UDP and read
  every frame on Vehicle CAN + Chassis CAN in real time.
- We don't actually need the firmware to understand what the dongle can do —
  the WiFi side gives us a clean, documented data plane.
- ScanMyTesla, SavvyCAN, candash, openpilot Cabana all already work against it.
- Architectural implication: enhauto either bundled comma's Panda firmware or
  re-implemented the protocol. Either way they leaked enough surface that the
  in-car CAN data path is fully observable from outside without any RE.

## Wire protocol (from `s3xycandump/panda.py`)

Transport: UDP, both directions, default Commander IP/port hardcoded by user.

### Client → Commander

| Bytes | Meaning |
|-------|---------|
| `"ehllo"` (5 bytes) | Hello / keepalive. Send every <1 s to keep session alive. |
| `0x0f` then N × 3 bytes `(0xff, frame_id_hi, frame_id_lo)` | Subscribe. Up to 43 ID pairs per packet; send multiple packets to subscribe to more. Empty subscribe = nothing forwarded. |

### Commander → Client

A UDP packet contains 1..512 frame records, **16 bytes each**:

```
offset 0..3   uint32 LE  header0   →  frame_id  = header0 >> 21
offset 4..7   uint32 LE  header1   →  bus_id    = header1 >> 4
                                       length    = header1 & 0x0F  (1..8)
offset 8..15  up to 8 bytes data
```

Special: `bus_id == 15 && frame_id == 6` is the Commander's ACK to a subscribe.
On receiving the ACK the client should re-send its subscribe list.

Failure modes observed by tumik:
- No data within 5 s after `ehllo` → reconnect
- After 5 retries → wait 120 s for the Commander-side session to time out
- Only **one client** can be connected at a time

## How to use this

1. Buy / borrow a Commander once.
2. In the S3XY phone app, enable Wi-Fi mode in Commander settings (one-time).
3. Join the Commander's AP from any device.
4. Run `tumik/S3XY-candump` (or any Panda-compatible tool) to start streaming
   both CAN buses.
5. From here, log in candump / SavvyCAN / openpilot Cabana format and analyse
   exactly which frames the dongle injects when you press Disable Front Motor /
   Track Mode / etc., by diffing logs with the feature on vs off.

That gives us the full set of "secret" Commander CAN frames **without ever
extracting the firmware**. Firmware is no longer load-bearing for the
signal-observation goal — it'd just be confirmation.

## Other notable repos surfaced in the same sweep

- **dzid26/ESP32-DualCAN** ("Dorky Commander — open source alternative to S3XY
  Commander"). Last commit 2026-04-07 (today). Currently only PCB / KiCad
  files in the public repo, no firmware yet — but the project name and
  description make clear dzid26 is actively building an open clone. Worth
  watching, and worth contacting (overlaps Task #44).
- **EzeLLM/fsd-spoofing** — community FSD-region-spoofing guide that bundles
  the original CanFeather Adafruit RP2040 sketch + ESP32/Nano ports. Same
  technique we ported, slightly different downstream presentation.
- **peepsnet/tesla_anitNag_SexyButton** — RP2040 sketch that uses an Enhance
  Commander + S3XY Buttons specifically to kill the Autopilot nag. Tiny but
  shows another point of intersection.
- **tumik/S3XY-candump** itself — the main find above.

## See also

- `COMMANDS.md` — the full Commander command catalogue, with each action
  classified by delivery channel (Tesla Fleet HTTP API vs CAN injection vs
  read-only sniff)
- `CAN_DICTIONARY.md` — cross-source Tesla CAN signal map
