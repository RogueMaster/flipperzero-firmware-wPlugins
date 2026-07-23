# LGT ISP (SWD) — Flipper Zero App

**Version 0.7.0** · Category: GPIO · Bilingual (English / Deutsch)

Flashes an **LGT8F328P** over its proprietary SWD protocol (GPIO bit-bang),
straight from the Flipper Zero — standalone from the SD card, over **USB
with avrdude**, or over **BLE-serial with avrdude** (via a PC-side bridge).
Handling modelled on the official AVR ISP Programmer app.

The UI language (English / German) is switchable in the menu and remembered on
the SD card. All drawn screens follow the selected language.

---

## English

### Status
- The **SWD core** (`lgt_swd.c`) is a faithful port of the hardware-verified
  `ft2232_lgtisp` (C232HD) — the framing sequences are byte-for-byte identical.
- **SD flashing**, **USB flashing** and **BLE flashing** are hardware-verified:
  the BLE STK500 round-trip (`30 20` -> `14 10`) was confirmed with nRF Connect. avrdude
  (`-c stk500v1 -p m328p`) writes and verifies an LGT8F328P over the Flipper.
- The bilingual UI, drawn graphics, and everything else are built on top of that
  proven core.

### Supported chips
Built and hardware-verified for the **LGT8F328P** (32 KB flash). The whole
**LGT8FX8P** family (LGT8F88P / 168P / 328P) shares the *same* proprietary SWD
programming interface and pinout, so the protocol layer applies family-wide — but
pick the target in the menu (**Chip:** 328P / 168P / 88P). Selecting a chip sets
the flash size shown and the signature reported to avrdude, and is remembered on
the SD card. Only the **328P is hardware-verified**; 168P (16 KB) and 88P (8 KB)
share the identical SWD protocol but are **untested** (flagged "untested!" on the
chip-ID and USB screens). The runtime "chip ID" detection stays family-level: the
SWDID (`3E`/`3F`) only reports that an LGT is present and its lock state, not the
exact part — so that title says "LGT", not the model.

### Wiring (Flipper GPIO → LGT8F328P, 3.3 V)
| Signal | Flipper pin | Port | → LGT |
|--------|-------------|------|-------|
| SWC    | 2           | PA7  | SWC   |
| SWD    | 3           | PA6  | SWD (bidirectional) |
| RESET  | 4           | PA4  | RSTN  |
| +3V3   | 9           | —    | VCC   |
| GND    | 11          | —    | GND   |

Pins can be changed at the top of `lgt_swd.c`. The app also draws this pinout
under **Wiring**.

**Target voltage.** The Flipper drives its I/O at 3.3 V; inputs are 5 V-tolerant
(via 51 Ω series resistors) **but only in input mode** — a pin configured as an
output must not see 5 V.
- **3.3 V target (recommended):** power the LGT from pin 9. Simplest and fully
  safe. The LGT8F328P runs at 1.8–5.5 V, so flashing at 3.3 V is fine even if the
  chip later runs at 5 V in-circuit.
- **5 V target:** possible, but note that SWD is *bidirectional* — the SWD pin
  alternates between output and input, and the 5 V-tolerance is input-only. Unlike
  the (unidirectional) AVR-ISP app, where MISO is always an input, a 5 V LGT can
  briefly put 5 V onto a pin that is momentarily an output during bus turnaround.
  If you must flash at 5 V, add a **series resistor (~330 Ω–1 kΩ) in the SWD line**
  to limit turnaround contention, and power from pin 1 (enable "5V on GPIO" in the
  Flipper's GPIO menu first).

### Build & install
With **ufbt** (recommended) or `fbt` in a firmware tree:

    ufbt              # builds lgt_isp.fap
    ufbt launch       # build + run on the attached Flipper

Or copy the built `lgt_isp.fap` to `SD:/apps/GPIO/`. The app shows up under
**Apps → GPIO → LGT ISP (SWD)**.

### Usage
Menu: Flash from SD · Flash + verify · Read chip ID · Dump to SD · Crack · USB (avrdude) · BLE (avrdude) · Wiring · About · Language.

- **Flash from SD** — pick a `.hex`, then unlock + erase + write.
- **Flash + verify** — additionally reads back and compares (same session).
- **Read chip ID** — reads the SWDID and the lock-independent GUID (no erase). `3E`/`3F` = LGT detected.
- **Dump to SD (crack)** — reads flash to `/ext/lgt_dump.hex` (sacrifices the first 1 KB — see below).
- **Crack (read-protect)** — breaks read protection (erases the first 1 KB); see below.
- **USB (avrdude)** — turns the Flipper into a virtual COM port. A **second**
  COM port appears — use that one, not the CLI port:

      avrdude -c stk500v1 -p m328p -P <COM> -U flash:w:sketch.hex:i

  Use the `-p` that matches the selected chip (`m328p` / `m168p` / `m88p`) — the
  Flipper reports the corresponding signature. **Back** ends USB mode.
- **BLE (avrdude)** — same STK500 slave, but over a BLE-serial channel instead of
  USB (**hardware-verified**: `30 20` -> `14 10` round-trip via nRF Connect). The screen
  shows connection state and a live **RX counter/activity bar**. Needs the channel reclaim
  on connect (the Bt service re-activates RPC otherwise) — handled in `ble_isp.c`.
  avrdude still needs a serial port, so on the PC a **BLE-serial ↔ virtual COM
  bridge** is required (Nordic-UART style); then:

      avrdude -c stk500v1 -p m328p -P <bridged-COM> -U flash:w:sketch.hex:i

  The bit-banging runs locally on the Flipper — BLE only carries STK500 page
  commands, never individual SWD bits (so it stays reliable, if slow). **Back**
  ends BLE mode and restores the default (mobile-app) BLE profile.
  *Note:* the firmware BLE-serial API is version-sensitive; the four `<<BLE>>`
  calls in `ble_isp.c` may need matching to your firmware, and `requires=["bt"]`
  must be present. See the header comment in `ble_isp.c`.
- **Language** — toggles English/German; stored on the SD card.

### Dump / read-out (via crack)
Existing firmware **can** be read back — but not for free. The LGT re-locks on
every reset, and its read (EEE) engine only comes up after an unlock that erases
flash. The trick (from the `ftdude` work) is the **crack**: it erases *only the
first page* (1 KB, `0x000`-`0x3FF`) instead of the whole chip, which is enough to
enable reading. A dump therefore recovers everything from `0x400` onward; the
first 1 KB comes back as `FF`.

- **Dump to SD (crack)** — cracks the chip, reads all 32 KB, and writes
  `/ext/lgt_dump.hex`. The first 1 KB is lost (reads `FF`); `0x400`+ is the
  original code. Use only when you accept sacrificing the first page.
- **Crack (read-protect)** — performs the crack alone (breaks read protection,
  erases the first 1 KB), then shows the SWDID. Handy before dumping, or to clear
  a protected part.

Same behaviour as `ftdude -c lgt --dump out.hex -F` / `--crack`.

### Versioning
- App version in `application.fam` as `fap_version=(MAJOR, MINOR)`.
- Patch level + on-screen version: `LGT_ISP_VERSION` in `lgt_isp.c` (About).
- SemVer 2.0.0: MAJOR = incompatible, MINOR = feature, PATCH = fix.

### Files
| File | Purpose |
|------|---------|
| `application.fam` | Flipper app manifest (version, category, icon) |
| `lgt_isp.c` | App: GUI (menu / progress / drawn screens / USB / BLE) + worker + i18n |
| `lgt_swd.c/.h` | LGT-SWD protocol (GPIO bit-bang), port of ft2232_lgtisp |
| `stk500.c/.h` | STK500v1 slave (avrdude commands → LGT), SDK-independent |
| `usb_isp.c/.h` | USB-CDC glue + STK500 worker |
| `ble_isp.c/.h` | BLE-serial glue + STK500 worker (drop-in twin of `usb_isp`) |
| `ihex.c/.h` | Intel HEX parser/builder |
| `lgt_isp_10px.png` | App icon (10×10, 1-bit) |

---

## Deutsch

### Status
- Der **SWD-Kern** (`lgt_swd.c`) ist ein originalgetreuer Port des hardware-
  bestätigten `ft2232_lgtisp` (C232HD) — die Framing-Sequenzen sind Byte für
  Byte identisch.
- **SD-Flashen** und **USB-Flashen** sind hardware-bestätigt: avrdude
  (`-c stk500v1 -p m328p`) schreibt und verifiziert einen LGT8F328P über den
  Flipper.
- Die zweisprachige Oberfläche, die gezeichnete Grafik und der Rest bauen auf
  diesem erprobten Kern auf.

### Unterstützte Chips
Gebaut und hardware-bestätigt für den **LGT8F328P** (32 KB Flash). Die ganze
**LGT8FX8P**-Familie (LGT8F88P / 168P / 328P) nutzt *dieselbe* proprietäre SWD-
Schnittstelle und Pinbelegung, die Protokoll-Schicht gilt also familienweit —
das Ziel wählst du im Menü (**Chip:** 328P / 168P / 88P). Die Wahl setzt die
angezeigte Flash-Größe und die an avrdude gemeldete Signatur und wird auf der SD
gemerkt. Nur der **328P ist hardware-bestätigt**; 168P (16 KB) und 88P (8 KB)
nutzen dasselbe SWD-Protokoll, sind aber **ungetestet** (auf dem Chip-ID- und
USB-Screen mit „ungetestet!" markiert). Die Laufzeit-Erkennung bleibt familien-
weit: die SWDID (`3E`/`3F`) sagt nur, dass ein LGT dranhängt und wie der Lock-
Status ist — nicht das Modell. Darum steht dort „LGT", nicht das Modell.

### Verdrahtung (Flipper GPIO → LGT8F328P, 3,3 V)
| Signal | Flipper-Pin | Port | → LGT |
|--------|-------------|------|-------|
| SWC    | 2           | PA7  | SWC   |
| SWD    | 3           | PA6  | SWD (bidirektional) |
| RESET  | 4           | PA4  | RSTN  |
| +3V3   | 9           | —    | VCC   |
| GND    | 11          | —    | GND   |

Pins oben in `lgt_swd.c` änderbar. Die App zeichnet diesen Pinout auch unter
**Verdrahtung**.

**Target-Spannung.** Der Flipper treibt seine I/O mit 3,3 V; Eingänge sind
5-V-tolerant (über 51-Ω-Serienwiderstände) **aber nur im Eingangsmodus** — ein
als Ausgang konfigurierter Pin darf keine 5 V sehen.
- **3,3-V-Target (empfohlen):** LGT aus Pin 9 versorgen. Am einfachsten und sicher.
  Der LGT8F328P läuft 1,8–5,5 V, Flashen mit 3,3 V ist also selbst dann in Ordnung,
  wenn der Chip später mit 5 V im Einsatz ist.
- **5-V-Target:** möglich, aber SWD ist *bidirektional* — der SWD-Pin wechselt
  zwischen Aus- und Eingang, und die 5-V-Toleranz gilt nur für Eingänge. Anders als
  bei der (unidirektionalen) AVR-ISP-App, wo MISO immer Eingang ist, kann ein 5-V-
  LGT beim Bus-Turnaround kurz 5 V auf einen gerade als Ausgang geschalteten Pin
  legen. Wenn du bei 5 V flashen musst: einen **Serienwiderstand (~330 Ω–1 kΩ) in
  die SWD-Leitung** (begrenzt den Kontentionsstrom) und aus Pin 1 versorgen (vorher
  „5V on GPIO" im GPIO-Menü aktivieren).

### Bauen & Installieren
Mit **ufbt** (empfohlen) oder `fbt` im Firmware-Baum:

    ufbt              # baut lgt_isp.fap
    ufbt launch       # bauen + auf dem angeschlossenen Flipper starten

Oder die gebaute `lgt_isp.fap` nach `SD:/apps/GPIO/` kopieren. App erscheint
unter **Apps → GPIO → LGT ISP (SWD)**.

### Bedienung
Menü: Flash von SD · Flash + Verify · Chip-ID lesen · Dump → SD · Crack · USB (avrdude) · Verdrahtung · About · Language.

- **Flash von SD** — `.hex` wählen, dann Unlock + Erase + Write.
- **Flash + Verify** — zusätzlich Rücklesen & Vergleich (gleiche Session).
- **Chip-ID lesen** — SWDID und die lock-unabhängige GUID auslesen (ohne Erase). `3E`/`3F` = LGT erkannt.
- **Dump → SD (Crack)** — liest Flash nach `/ext/lgt_dump.hex` (opfert das erste 1 KB — siehe unten).
- **Crack (Leseschutz)** — bricht den Leseschutz (löscht das erste 1 KB); siehe unten.
- **USB (avrdude)** — Flipper wird virtueller COM-Port. Es erscheint ein
  **zweiter** COM-Port — den nehmen, nicht den CLI-Port:

      avrdude -c stk500v1 -p m328p -P <COM> -U flash:w:sketch.hex:i

  Nimm das `-p`, das zum gewählten Chip passt (`m328p` / `m168p` / `m88p`) — der
  Flipper meldet die passende Signatur. **Zurück** beendet den USB-Modus.
- **Language** — schaltet Deutsch/Englisch um; wird auf der SD gespeichert.

### Dump/Auslesen (per Crack)
Bestehende Firmware lässt sich **doch** auslesen — aber nicht umsonst. Der LGT
sperrt bei jedem Reset, und seine Lese-Engine (EEE) kommt erst nach einem Unlock
hoch, der Flash löscht. Der Kniff (aus der `ftdude`-Arbeit) ist der **Crack**: er
löscht *nur die erste Seite* (1 KB, `0x000`-`0x3FF`) statt des ganzen Chips, und
das genügt zum Freischalten des Lesens. Ein Dump rettet damit alles ab `0x400`;
das erste 1 KB kommt als `FF` zurück.

- **Dump → SD (Crack)** — crackt den Chip, liest alle 32 KB und schreibt
  `/ext/lgt_dump.hex`. Das erste 1 KB geht verloren (liest `FF`); ab `0x400`
  steht der Originalcode. Nur nutzen, wenn das Opfern der ersten Seite in Ordnung
  ist.
- **Crack (Leseschutz)** — führt nur den Crack aus (bricht den Leseschutz,
  löscht das erste 1 KB) und zeigt dann die SWDID. Praktisch vor dem Dumpen oder
  um einen geschützten Chip zu entsperren.

Gleiches Verhalten wie `ftdude -c lgt --dump out.hex -F` / `--crack`.

### Versionierung
- App-Version in `application.fam` als `fap_version=(MAJOR, MINOR)`.
- Patch-Stand + Anzeige: `LGT_ISP_VERSION` in `lgt_isp.c` (About-Screen).
- SemVer 2.0.0: MAJOR = inkompatibel, MINOR = Feature, PATCH = Fix.

---

## Changelog
- **0.7.0** — Dump/read-out via crack (from the `ftdude` findings): new
  **Dump → SD** (writes `/ext/lgt_dump.hex`, sacrifices the first 1 KB, recovers
  `0x400`+) and **Crack** (breaks read protection alone). **Read chip ID** now
  also shows the lock-independent GUID. Reverses the 0.3.1 "dump impossible"
  note — a partial dump *is* possible. / Dump per Crack + GUID-Anzeige.
- **0.6.3** — BLE: reclaim the serial channel on every connect (re-set event callback +
  `set_rpc_active(false)`); the Bt service re-activates RPC on connect otherwise. This is what
  made it work — the BLE STK500 round-trip `30 20` -> `14 10` is hardware-verified, and avrdude
  flashes + verifies over BLE (via `ble_bridge.py`).
- **0.6.2** — BLE: `set_rpc_active(false)` on open (first attempt to claim the channel from RPC;
  not sufficient alone — see 0.6.3).
- **0.6.1** — BLE: fixed include path to the `ble_glue` SDK layout
  (`profiles/serial_profile.h`) + `<string.h>`; both `ble_isp.c` and `lgt_isp.c` verified to
  compile cleanly against the ufbt SDK (arm-none-eabi-gcc, 0 warnings).
- **0.6.0** — **BLE (avrdude) mode**: STK500 slave over BLE-serial (`ble_isp.c`, drop-in twin of
  `usb_isp.c`) — same SWD/STK500 core, only the transport changes. New menu item + screen with
  live RX counter/activity bar. Manifest `requires=["bt"]`. Bit-banging stays local on the
  Flipper; BLE carries only STK500 page commands.
- **0.5.0** — Chip selection in the menu (328P / 168P / 88P): sets displayed flash
  size + avrdude signature, stored on SD. Only 328P is verified; 168P/88P share
  the protocol but are untested (marked "untested!"). / Chip-Auswahl im Menü.
- **0.4.2** — Clarified chip scope: built/verified for the LGT8F328P (32K); the
  LGT8FX8P family (88P/168P) shares the SWD protocol but is untested (flash size +
  signature are 328P-specific). / Chip-Umfang präzisiert.
- **0.4.1** — Corrected target-voltage guidance: 3.3 V recommended, 5 V possible
  with a series resistor on SWD (bidirectional line, input-only 5 V-tolerance). /
  Korrigierte Spannungshinweise: 3,3 V empfohlen, 5 V mit Serien-R an SWD.
- **0.4.0** — Bilingual UI (English / Deutsch), switchable in the menu and stored
  on the SD card (`/ext/apps_data/lgt_isp/lang`); all drawn screens localised. /
  Zweisprachige Oberfläche, im Menü umschaltbar und auf SD gemerkt.
- **0.3.2** — More drawn graphics: DIP chip on "chip detected", drawn pinout,
  "ISP mode active" screen (connector → SWD → USB). / Mehr gezeichnete Grafik.
- **0.3.1** — Dump removed (impossible on the LGT: read engine only comes up
  after the erasing unlock). / Dump entfernt.
- **0.3.0** — (withdrawn) dump attempt; progress bar + chip mascot added.
- **0.2.4** — Fix: hang when leaving USB mode (start/stop moved to view
  enter/exit callbacks). / Fix Haenger beim Verlassen des USB-Modus.
- **0.2.3** — USB after the official AVR-ISP pattern (usb_cdc_dual +
  furi_hal_usb_lock + CLI-VCP, channel 1); hardware-verified.
- **0.2.0–0.2.2** — USB mode via STK500v1 slave (early, unstable USB handling).
- **0.1.0** — First cut: SD flash/verify + chip ID, LGT-SWD core ported.
