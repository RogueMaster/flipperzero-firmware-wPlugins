# InfraFi

Transmit WiFi credentials from a Flipper Zero to a Linux server via IR.

## Project Structure

```
infrafi/
  application.fam          # Flipper app manifest (must stay in root for ufbt/VS Code extension)
  flipper/                 # Flipper Zero app source
    wi_fir.h/c             # App entry point, ViewDispatcher + SceneManager
    wfr_encode.h/c         # RC-6 IR message encoder + transmitter
    wfr_decode.h/c         # RC-6 ACK decoder (IR receive from daemon)
    wfr_nfc.h/c            # NFC NDEF WiFi tag parser
    wfr_storage.h/c        # SD card credential + settings storage (FlipperFormat)
    protocol/
      wfr_protocol.h       # Shared protocol constants, structs, function declarations
      wfr_protocol.c       # CRC-8, WiFi QR string builder/parser
      version.h            # Shared version (semver, single source of truth)
    scenes/                # Flipper UI scenes (main_menu, edit_*, confirm, transmit, scan_nfc, saved, settings, about)
    images/                # App icon (10x10 1-bit PNG)
  daemon/                  # Linux daemon (infrafid)
    main.c                 # Entry point, CLI args, main loop
    wfr_lirc.h/c           # LIRC scancode reader (/dev/lirc0, LIRC_MODE_SCANCODE)
    wfr_evdev.h/c          # evdev input reader (/dev/input/eventN, NEC via MSC_RAW)
    wfr_decode.h/c         # IR scancode reassembler
    wfr_network.h/c        # NetworkManager/systemd-networkd/ifupdown WiFi connector with rollback
    wfr_ack.h/c            # IR ACK transmitter (LIRC TX, RC-6 scancodes)
    Makefile               # Build with `make` (requires gcc, linux headers)
    infrafid.service       # systemd unit file
    install.sh             # Build + install + enable service
  debian/                  # Debian packaging (dpkg-buildpackage)
  rpm/                     # RPM packaging (rpmbuild)
  .github/workflows/      # CI/CD (ci.yml, release.yml)
```

## Protocol

Uses **RC-6 IR protocol** (36kHz carrier, native to ITE8708 CIR receivers found in Intel NUCs).

Each RC-6 message carries 16 bits: 8-bit address + 8-bit command.

**Address byte layout:**
- Bits 7-4: Magic `0xA` (identifies InfraFi messages)
- Bits 3-2: Frame type (`00`=START, `01`=DATA, `10`=END, `11`=ACK)
- Bits 1-0: Pass number (retransmission attempt 0-3)

**Message sequence:** START (len) → DATA × N (one byte each) → END (CRC-8)

Payload is a WiFi QR string: `WIFI:T:<type>;S:<ssid>;P:<pass>;H:<hidden>;;`

The Flipper sends the full sequence (retransmission configurable). The daemon assembles bytes per-pass and verifies CRC-8 at END.

**ACK response (daemon → Flipper):** Uses same START/DATA/END framing. Payload is `OK:<ip>` on success or `FAIL` on failure. Requires IR TX hardware (not all CIR receivers have a TX emitter — use `--ack-device` for an external USB IR blaster).

## Building

**Flipper app:** Run `ufbt` from the project root. The `application.fam` must be in root (required by ufbt and the VS Code Flipper extension). The `daemon/` directory is excluded from the Flipper build.

**Linux daemon:** `cd daemon && make`. Requires Linux headers for `<linux/lirc.h>`. Install with `sudo ./install.sh`.

## ITE8708 CIR Notes

- The ITE8708 (found in Intel NUCs) is designed for RC-6 at 36kHz
- Raw LIRC_MODE_MODE2 has a tiny hardware FIFO that overflows with custom protocols
- Using LIRC_MODE_SCANCODE lets the kernel decode RC-6 natively — no overflow issues
- Ensure rc-6 decoder is enabled: `cat /sys/class/rc/rc0/protocols` should show `[rc-6]`
- The Flipper app uses `InfraredProtocolRC6` which handles the 36kHz carrier automatically
- The ITE8708 chip supports TX but many NUCs (e.g. NUC8i5BEK) have no physical IR LED wired for transmission. ACK TX requires an external IR blaster connected via USB or the CIR TX header (if exposed).

## Daemon Usage

```bash
sudo ./infrafid -f -v                       # foreground, verbose
sudo ./infrafid -d /dev/lirc1               # different RX device
sudo ./infrafid -a /dev/lirc1               # separate TX device for ACK
sudo ./infrafid -d /dev/lirc0 -a /dev/lirc1 # RX on lirc0, ACK TX on lirc1
```

Runs as root (needed to write network config). Logs to syslog/journald.

## Key Design Decisions

- `application.fam` in root (not `flipper/`) because ufbt and VS Code extension expect it there
- Flipper uses `ViewDispatcherTypeFullscreen` to avoid the status bar eating a keyboard row
- RC-6 over raw IR because ITE8708 hardware FIFO is too small for custom pulse-distance encoding
- CRC-8 (not CRC-32) for payload integrity — sufficient for the small payloads
- 20ms inter-message delay for reliable RC-6 reception
- ACK is opt-in on the Flipper (Settings → Wait for ACK) because not all servers have IR TX hardware
- Version shared between Flipper and daemon via `flipper/protocol/version.h`
- Internal C code uses `wi_fir_`/`WiFir`/`wfr_`/`WFR_` prefixes (legacy, not worth renaming)
- Flipper TextInput keyboard is hardcoded in firmware — missing `\ | { } [ ] < > ~`. NFC tags are the workaround for passwords with special characters.
