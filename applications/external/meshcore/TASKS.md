# Task list

Working order and current state, against the consolidated nine-stage plan.
New requests are folded in here; ones that change earlier work say so.

Legend: `[x]` done · `[~]` partial · `[ ]` not started · `[!]` blocked

## Gate policy

Three gates, and they are not interchangeable:

- **Build gate** — `ufbt` builds clean and `./test/run.sh` is green. Enforced
  before anything is called done. Runs in CI on every push.
- **Emulator gate** — the software stack is exercised against
  `tools/node-emulator/` over TCP. This is what replaced most of the old
  "waiting for hardware" list. Also in CI.
- **Node gate** — verified against a real MeshCore node. **Still unreachable:
  the boards are in transit.** Everything needing one is collected at the
  bottom and will be run as a batch.

Remote UI driving is not available: the Flipper CLI accepts `input send` but
the events never reach a running app, confirmed with debug mode on. Scenes
cannot be exercised from the desk at all — only their building blocks can.

## The nine stages

- [x] 1 — ufbt skeleton and the main menu (Configurator / Messenger / Logger)
- [x] 2 — UART layer + connect: self-info, model and firmware version, plus a
      passthrough serial log
- [~] 3 — meshcore_c integrated ✔; Configurator radio editor + apply + verify ✘
- [ ] 4 — presets: built-in *City/daily*, SD presets, save-from-node
- [~] 5 — Logger: `rx_log.csv` ✔; telemetry, ping and events ✘
- [ ] 6 — auto-detect {companion|repeater} × {t114|v4}, CSV tags, `LogNode`
- [ ] 7 — second UART (LPUART) and two nodes at once
- [~] 8 — Messenger: contacts ✔, chat ✔, receive ✔; compose ✘, channels ✘
- [ ] 9 — Logger UI: live SNR/RTT, a button for `type=mark`, threshold colouring

### Stage 4 groundwork — already answered

The spec asked whether `path_hash_bytes` is compile-time. **It is not** — it is
runtime, via its own command (61), and the mapping is
`path_hash_bytes = path_hash_mode + 1`. The full trace is in AGENTS.md under
"Settings layers". So every field in a preset is editable; none needs to be a
read-only label.

Built-in preset to ship (shown first, present with no SD card):

```
City/daily   freq 868.731018 MHz   bw 62.5 kHz   sf 7   cr 7   path_hash_bytes 2
```

## Node emulator (`tools/node-emulator/`)

- [x] 1 — protocol core + TCP: framing, self-info, battery, radio and packet
      stats, contacts. *Gate: reference `meshcore` client — 15 checks.*
- [x] 2 — events and ping: adverts, RX log with drifting SNR, incoming mail via
      MSG_WAITING + SYNC_NEXT_MESSAGE, ACK with delay and loss, scenarios.
      *Gate: the user's `meshlog.py` fills all four CSVs — 18 checks.*
- [ ] 3 — serial transport end to end through the devboard bridge
- [ ] self-info should also carry the network profile / path hash, so presets
      and the read-only question can be exercised against the fixture
- [ ] `devboard-bridge/` — ESP32-S2 passthrough sketch. **Needs the real
      U_TX/U_RX GPIOs from the Flipper WiFi Devboard schematic, not a guess.**
- [ ] `HANDOFF.md`

## Infrastructure

- [x] Host test suite for the protocol layer, Zig toolchain, POSIX + PowerShell
- [x] Flipper firmware brought to Unleashed `unlshd-089` (was `080e`; API 79 →
      87, the app could not have loaded on the old one)
- [x] Build against the Unleashed SDK, documented both directions
- [x] `application.fam` lists sources per directory so `test/` cannot leak into
      the firmware
- [x] Everything pushed; branches → PR → CI → squash merge
- [x] CI: host tests, both emulator gates, and the FAP build on every push

## Known gaps

- `scene_chat` right-aligns outgoing messages with `\er` but cannot invert
  them: the text scroll element has no per-line colour. Real inversion needs a
  custom view with its own draw callback.
- `MC_PUSH_NEW_ADVERT` carries a full contact record and could refresh the
  contact list live. Currently ignored.
- Delivery confirmation (`MC_PUSH_SEND_CONFIRMED`) is not tracked yet — needed
  once compose can send, to tell "sent" from "delivered".
- **Stale reply after a timeout.** The protocol puts no request id on the wire,
  so a reply arriving after its request gave up cannot be told from the next
  request's reply. Back-to-back mailbox drains are the realistic case. Not
  fixed speculatively — a sequence tag cannot help without wire correlation.
  **If bring-up on a flaky link shows duplicated or misattributed messages,
  look here first.**
- The session's threaded mechanism has no automated coverage — host tests reach
  the routing policy, not the slot/event-flag dance. The loopback jumper is the
  cheapest way to exercise it.
- Logger timestamps are local ISO-8601 **without a zone offset**, while
  `meshlog.py` writes one. The Flipper RTC holds no timezone, so an offset
  would be invented. Say the word and it becomes a constant.

## Waiting for hardware

- [!] **Loopback check — the cheapest pre-node test.** A jumper between pins 13
      and 14. Connect should report *"Node refused the handshake"* and the
      serial log should show identical `TX`/`RX` lines; that counterintuitive
      message is the success signal. It exercises the threaded session
      machinery that host tests cannot reach: the echo drives poll → assemble →
      parse → route → flag → scene completion. The 5 s mailbox drain adds a
      bonus, since `SYNC_NEXT_MESSAGE` (command 10) echoes back as
      `NO_MORE_MESSAGES` (response 10), exercising multi-code routing.
- [!] Connect against a real node: model, firmware, radio settings
- [!] Logger: a walk near the mesh fills `rx_log.csv`
- [!] Messenger: a message from another node appears; one typed here arrives
- [!] Whether a node accepts a runtime role change at all
- [!] Confirm the Heltec V4 works on stock `heltec_v4_companion_radio_usb`
- [!] Node wiring, from the pinouts: **T114** header P1 — UART1_RX GPIO9,
      UART1_TX GPIO10, GND pin 4. **V4** header J3 — RX GPIO47, TX GPIO48,
      GND pin 1 of J2.
