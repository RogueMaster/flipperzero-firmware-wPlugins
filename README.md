# Pod

A Flipper-to-Flipper encounter game over sub-GHz radio. Carry your Flipper
around with Pod open and it quietly broadcasts your dolphin and listens for
others. Cross paths with another Flipper running Pod and it logs the
**encounter**; linger near each other and you can **battle** — a live 10-second
tap race. Best at meetups, cons, and hackerspaces where Flippers gather.

No add-on hardware required — Pod uses the Flipper's built-in sub-GHz radio and
its real dolphin level. You just need **two Flippers** to play for real.

---

## How it works

- **Beacon.** While Walk Mode is open, your Flipper broadcasts a tiny "calling
  card" (your callsign, a random ID, and your dolphin level) on 433.92 MHz every
  few seconds, and listens for others.
- **Encounter.** Catching someone's beacon logs an encounter — a passive
  "we crossed paths" record. A quick walk-by is *just* an encounter.
- **Battle.** If two people are in range and **both opt in** (one challenges,
  the other accepts), a live battle starts: a 3-2-1 countdown, then **mash OK for
  10 seconds** — most taps wins. The winner earns XP.
- **Progress.** Wins/losses/XP persist on the SD card and drive a rank, from
  **Minnow** up through Guppy, Tuna, Dolphin, Orca, to **Kraken**.

Your real Flipper dolphin's level seeds your beacon and battles, but Pod never
modifies your actual pet — all game progression is stored separately.

---

## Requirements

- A Flipper Zero on firmware with sub-GHz support (built against API 87.1 /
  fw 1.4.3, portable to Official/Momentum/Unleashed/RogueMaster).
- **Two Flippers** for real encounters and battles. Solo, you can use **Practice**
  mode to play the tap race against a simulated opponent.
- Uses **433.92 MHz** — a region-legal ISM frequency. Transmit briefly and
  respect your local regulations.

---

## Install

### Build with ufbt (recommended)

```bash
# from the project root
ufbt
ufbt launch    # build, upload, and run on the connected Flipper
```

Or copy `dist/pod.fap` into `apps/Games/` on your Flipper's microSD to sideload.

### Firmware tree

Clone into `applications_user/pod/` of a firmware checkout and build as usual.
Appears under **Apps → Games → Pod**.

---

## Usage

Main menu:

| Item | What it does |
|------|--------------|
| **Walk Mode** | Broadcast + listen. Press **OK** to see who's in range and challenge them. Leave it open as you move around. |
| **Encounters** | Log of everyone you've crossed paths with (newest first), with battle results, XP, and "seen X ago". Includes **Clear log**. |
| **Practice** | Solo tap race vs a simulated opponent — try the battle game with one Flipper. |
| **Profile** | Your callsign, ID, dolphin level, XP, rank, and win/loss. Press **OK** to edit your callsign. |
| **About** | In-app explanation. |

**Battle controls:** on a challenge prompt, **OK** accepts / **BACK** declines.
During the race, **mash OK**. On the result screen, **BACK** continues.

---

## Data & storage

Everything lives under `/ext/apps_data/pod/` on the SD card:

- `profile.bin` — your identity: random 32-bit ID, callsign, wins/losses/XP.
- `encounters.bin` — the encounter log (up to 64), with per-rival battle result
  and XP.

---

## Radio protocol

Pod uses the firmware's `subghz_tx_rx_worker` (the same machinery as the built-in
sub-GHz chat) at 433.92 MHz. Messages are self-framed with a 1-byte type, CRC-8,
and de-duplication:

- **Beacon** — presence broadcast (id, callsign, dolphin level), sent frequently.
- **Challenge / Accept / Decline** — the battle handshake between two Flippers.
- **Taps** — final tap counts exchanged at the end of a battle to decide a winner.

The wire format is host-tested for round-tripping and CRC rejection.

---

## Architecture

```
flipper-pod/
├── application.fam     # FAP manifest (appid "pod", Games)
├── pod.c               # app: view_dispatcher, menus, Walk Mode, battle state machine
├── radio.c/.h          # sub-GHz client: starting the radio, sending, receiving
├── pod_wire.c/.h       # the on-air format: framing, CRC, encode/decode, dedup
├── profile.c/.h        # identity persistence + reading the real dolphin level
├── encounters.c/.h     # encounter log load/save
├── pod_10px.png        # app icon
└── test/test_wire.c    # two simulated peers, run on a host
```

The app is built on `view_dispatcher` + views (submenus, widgets, and custom
canvas views for Walk Mode and battles). Incoming radio messages arrive on the
worker thread and are handed to the GUI thread via a message queue + custom
event.

---

## Testing

`pod_wire.c` holds the entire on-air format and has no Flipper dependency, so
the code that runs on the device can be exercised on a computer:

```bash
cd test
cc -Wall -Wextra -fsanitize=address -I.. -o test_wire test_wire.c ../pod_wire.c
./test_wire
```

It runs two simulated peers against each other — one's encoder feeding the
other's parser — over a channel that repeats, fragments, corrupts and
interleaves frames, covering all five message types, the full
challenge/accept/taps handshake, resynchronisation after noise, CRC rejection,
name edges and the duplicate window.

## Status & notes

- **Encounters and battles work between two Flippers running Pod.** The wire
  format is additionally covered by host tests that run two peers against each
  other over a lossy channel, and the challenge → accept → countdown → tap race
  → result sequence has been exercised on hardware.
- Range depends on surroundings: 433.92 MHz carries well in the open, less well
  through walls and crowds. Both Flippers need Walk Mode open to find each
  other.
- Use **Practice** to play the tap race with a single Flipper.

---

## License

Released under the [MIT License](LICENSE).
