# PC Health Remote — wire protocol v1

All multi-byte integers are **little-endian**. Frames are fixed-size, packed (no padding).
The same frames are used over BLE and over USB CDC.

## Common header (5 bytes)

| off | size | field   | value                                  |
|-----|------|---------|----------------------------------------|
| 0   | 2    | magic   | `0x50 0x48` ("PH")                     |
| 2   | 1    | version | `1`                                    |
| 3   | 1    | type    | see below                              |
| 4   | 1    | seq     | rolling counter, sender-local          |

Every frame ends with **CRC16-CCITT-FALSE** (poly `0x1021`, init `0xFFFF`, no reflection,
xorout `0x0000`) over all preceding bytes of the frame, stored little-endian.
Test vector: CRC of ASCII `"123456789"` = `0x29B1`.

Receivers drop frames with wrong magic, unknown version, wrong length or bad CRC.

## Type 0x01 — TELEMETRY (PC → Flipper), 48 bytes, sent every 1 s

| off | size | field          | notes                                                        |
|-----|------|----------------|--------------------------------------------------------------|
| 0   | 5    | header         | type = 0x01                                                  |
| 5   | 1    | flags          | bit0 on_battery, bit1 charging, bit2 cpu_temp_valid, bit3 gpu_present, bit4 gpu_temp_valid, bit5 battery_present, bit6 fan_valid, bit7 reserved(0) |
| 6   | 1    | cpu_load       | % 0..100                                                     |
| 7   | 1    | cpu_temp       | °C (valid only if bit2)                                      |
| 8   | 1    | gpu_load       | % (valid only if bit3)                                       |
| 9   | 1    | gpu_temp       | °C (valid only if bit4)                                      |
| 10  | 1    | ram_load       | %                                                            |
| 11  | 1    | vram_load      | % (valid only if bit3)                                       |
| 12  | 1    | disk_load      | % used of the system drive                                   |
| 13  | 1    | battery        | % (valid only if bit5)                                       |
| 14  | 2    | ram_total_dgb  | total RAM in 0.1 GB units (160 = 16.0 GB)                    |
| 16  | 2    | vram_total_dgb | total VRAM in 0.1 GB units                                   |
| 18  | 2    | cpu_clock_mhz  | current CPU clock, 0 = unknown                               |
| 20  | 2    | fan_rpm        | valid only if bit6                                           |
| 22  | 1    | top_cpu_pct    | CPU % of the heaviest process (normalised to all cores)      |
| 23  | 1    | top_ram_pct    | RAM % of the process using most memory                       |
| 24  | 12   | top_cpu_name   | ASCII, NUL-padded, no ".exe", truncated                      |
| 36  | 8    | top_ram_name   | ASCII, NUL-padded, no ".exe", truncated                      |
| 44  | 2    | uptime_h       | PC uptime in hours                                           |
| 46  | 2    | crc            |                                                              |

## Type 0x81 — HELLO (Flipper → PC), 12 bytes, sent every 1 s until TELEMETRY arrives, then every 10 s

| off | size | field        | notes                                       |
|-----|------|--------------|---------------------------------------------|
| 0   | 5    | header       | type = 0x81                                 |
| 5   | 2    | app_version  | major*100 + minor (e.g. 100 = 1.0)          |
| 7   | 1    | transport    | 0 = BLE, 1 = USB                            |
| 8   | 1    | interval_s   | requested telemetry interval, 1..10         |
| 9   | 1    | reserved     | 0                                           |
| 10  | 2    | crc          |                                             |

### USB rule (important)
The Flipper CLI uses the same USB VID/PID (`0x0483:0x5740`). The backend **must not write
anything** to a Flipper COM port until it has read a valid HELLO frame from it.
If no HELLO within 3 s, close the port and retry later.

### BLE
Flipper exposes the standard Flipper serial GATT service (UUIDs from the firmware SDK
`services/serial_service`). PC writes TELEMETRY to the RX characteristic (write without
response preferred) and subscribes to TX notifications to receive HELLO.
Advertised local name starts with `PCHealth`. The app uses its own bonding-key file and a
MAC distinct from the stock Flipper so the bond does not clash with the mobile app.
Pairing method: numeric comparison with bonding, done once. The Flipper shows "Verify code"
and the user presses OK; the PC backend accepts the request programmatically, so Windows
shows no dialog. After that, reconnects are automatic and silent.

## Timeouts
Flipper marks the link lost if no valid TELEMETRY for 5 s (shows "PC lost" + optional alert).
