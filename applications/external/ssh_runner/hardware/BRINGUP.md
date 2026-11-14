# SSH Runner rev 0.1: bring-up

[← Hardware](README.md)

The bring-up procedure for an assembled board, and what the rev 0.1 boards measured. The design
reasons behind each check are in [`DESIGN.md`](DESIGN.md) (section numbers below).

## Boards

- Two rev 0.1 boards, assembled by JLCPCB, were brought up. The measurements below are from the
  first; the second confirmed LAN.
- D103 (power LED) and D201 (status LED) are not fitted on either board. Their light holes are
  too small for the ordered parts (erratum E1, `DESIGN.md` §8); they will be fitted by hand.
- Firmware: v0.2.1, the Waveshare ESP32-S3-ETH build. Its pin map is the same as this board's,
  so no board-specific build is needed for bring-up.

## Procedure

Measure with a multimeter against GND: Flipper header pin 8, 11 or 18, or any GND pad.

1. **Visual check:** no solder bridges on the W5500, the module or the USB-C; D103 and D201
   empty (E1).
2. **Flipper power only** (USB-C unplugged, board on the Flipper, "5V on GPIO" off):
   - +3V3 at TP104: 3.3 V expected. Rev 0.1 has no power LED (E1), so this reading is the
     power check.
   - Flipper 3V3 in, at J402 pin 1 (Flipper pin 9). The difference to TP104 is the mux drop.
   - PR1 across R102: well below the TPS2116's 0.92 V threshold. A higher reading means the
     mux could pick a dead USB input (the phantom VBUS that R104 bleeds off, §4.5).
3. **USB only** (board off the Flipper, USB-C plugged in):
   - +3V3 at TP104: 3.3 V.
   - PR1: 56k / (100k + 56k) × 3.3 V ≈ 1.18 V, above the threshold, so the mux picks USB.
   - Current from USB: blank flash, then the firmware idle, then WiFi busy. U101 should not
     run hot (§4.5).
4. **LED pads**, with no LED fitted, firmware running: the anode pad carries a voltage and the
   cathode pad reads 0 V (GND). This tells the hand-fitter which way round each LED goes.
5. **Flash over USB** and read the boot log: PSRAM test, flash ID, W5500 found over SPI, and
   the reset counters (brownout, watchdog, crash).
6. **WiFi:** join an access point, scan, ping the gateway.
7. **Flipper power under load:** run from the Flipper alone, SSH over WiFi to a test sshd, and
   pull a large output.
8. **LAN:** a cable to a switch; DHCP lease and an SSH session over Ethernet.
9. **LEDs** (after hand-fitting, E1): power LED on with +3V3; status LED driven from GPIO21 by
   the add-on firmware build (`sdkconfig.addon`, [Hardware and Wiring](../docs/HARDWARE.md)).

## Results, rev 0.1

### Rails (2026-11-02, multimeter)

| Condition | Point | Reading | Verdict |
|---|---|---|---|
| Flipper power only, USB-C unplugged | +3V3 at TP104 | 3.30 V | pass |
| | Flipper 3V3 in (J402 pin 1) | 3.32 V | mux drop about 20 mV |
| | PR1 across R102 | 0.02 V | pass: well below 0.92 V, the phantom-VBUS fix (R104) works |
| USB only | +3V3 at TP104 | 3.29 V | pass |
| | PR1 across R102 | 1.17 V | pass: 1.18 V expected, the mux picks USB |

### LED pads, no LED fitted (2026-11-02)

Seen from the parts side, with the Flipper header at the bottom:

| LED | Left pad | Right pad | Anode |
|---|---|---|---|
| D103 (power, white) | 0 V (GND) | 3.29 V (PWR_LED, through R103) | right |
| D201 (status, green) | 0.06 V (LED_STATUS_A, through R203) | 0 V (GND) | left |

This matches the footprints: D201 is turned 180° from D103. When fitting by hand, put the
anode on the pad that carries a voltage (E1). D201's anode follows GPIO21 through R203, which
the v0.2.1 firmware holds low, so it reads near 0 V; D201's anode is the left pad.

### Current from USB (2026-11-01/02)

| State | Supply | Current |
|---|---|---|
| Blank flash | 5.08 V | 67 mA |
| Firmware, idle | 5.08 V | 88 mA |
| Firmware, WiFi associated and pinging | 5.08 V | about 106 mA |

U101 (the LDO) was not noticeably warm. A temperature reading with LAN and WiFi both busy is
still to do (§4.5).

### Boot

- PSRAM: 8 MB, test OK.
- Flash ID 0x184046: 16 MB.
- W5500 found over SPI.
- Reset counters: brownout 0, watchdog 0, crash 0.

### WiFi

- RSSI −26 dBm at the test access point.
- Scan: 29 networks, the weakest at −93 dBm.
- Ping to the gateway: 255 of 255 replies, 0–8 ms.

### Flipper power under load

- About 30 minutes on Flipper power alone, stable, with SSH over WiFi to a test sshd on the
  local network.
- A 2 MB output (`yes 0123456789 | head -c 2000000`) arrived byte-exact, exit 0, in 77 s:
  about 26 KB/s. That matches the prototype's bench test (about 25 KB/s, §4.6).

### LAN (second board)

- Flashed, and Ethernet over a cable to a switch works.

## Open

- D103 and D201: hand-fit (E1), then the LED check with the add-on firmware build.
- LDO temperature on USB with LAN and WiFi busy (§4.5).
- Current from USB with LAN only, and with LAN and WiFi together; current from the Flipper.
- From the prototype's bench test (§4.6): the heap low-water mark across a battery boot, and a
  scope trace of the 3V3 droop if an oscilloscope becomes available.
- The full functional test suite (`tools/hil/`) on the board.
