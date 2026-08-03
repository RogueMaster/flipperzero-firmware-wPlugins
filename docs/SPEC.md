# SPEC: `nfc_canary` — Flipper Zero NFC skim alarm

**Scope: 13.56 MHz NFC only.** LF/125 kHz is explicitly out of scope (see `VALIDATION.md` §3–4 for why it was cut: shared-pin conflict + it blocks MCU sleep).
**Status:** Validated, ready to build. Read `VALIDATION.md` first.
**Target:** Flipper Zero HW rev 12, Unleashed `unlshd-089e`, API 87.8. Prefer stock-OFW-compatible APIs; avoid Unleashed-only calls.
**Deliverable:** a custom `.fap` application.

---

## 0. What this is

A passive detector carried in a pocket. When a reader interrogates it over NFC, an **audible alarm** sounds (configurable in-app) and the device wakes.

**It detects; it does not prevent.** By the time the alarm fires, a read may already have succeeded. The value is awareness and deterrence. State this in the UI.

Dropping LF removes every hard constraint from validation: no shared-pin conflict, no forced-awake MCU. This is now a single-radio app with a silicon field detector behind it — the easy version of the problem.

---

## 1. READ THIS BEFORE TESTING: what actually triggers the alarm

**A passive NFC tag/card will NOT trigger this app.** A tag has no power source and emits no field — it only backscatters energy from a reader. This app listens for a **reader's carrier**, so a tag is invisible to it. Do not use a tag as your test source; you will conclude the app is broken when it is working correctly.

Valid test sources, in order of convenience:

| Source | Reliability | Notes |
|---|---|---|
| **Android phone, NFC enabled, unlocked** | **Best** | Polling loop emits a 13.56 MHz carrier continuously. Hold to Flipper's back. Primary crawl-phase source. |
| Payment terminal / transit gate / badge reader | High | Real-world validation; also the main false-positive source. |
| Second Flipper running `nfc field` | High | That CLI command is an emitter (`furi_hal_nfc_poller_field_on()`). |
| iPhone | Medium | Polls opportunistically, not continuously. Works but is intermittent — don't debug against it. |

**Antenna location:** the Flipper's NFC antenna is on the **back of the device**. Present the reader there, not to the screen.

---

## 2. Detection: the two signals

From `lib/nfc/nfc.h:37-46`:

| Event | Meaning | Confidence |
|---|---|---|
| `NfcEventTypeFieldOn` | *Any* 13.56 MHz carrier detected | **Low** — fires on every terminal, gate, and nearby phone |
| `NfcEventTypeFieldOff` | Carrier lost | — |
| `NfcEventTypeListenerActivated` | A reader completed anticollision and **addressed our emulated UID** | **High** — this is someone actively enumerating a card |

Conflating these is the single biggest design mistake available here. The world is saturated with 13.56 MHz; an alarm on bare `FieldOn` will scream at every checkout counter and get switched off within a day.

**Crawl uses `FieldOn`** (simplest, proves the pipeline). **Walk introduces tiering** so `ListenerActivated` is the loud one.

---

## 3. API reference

High-level listener — the proven path, mirrors stock "Detect Reader" (`applications/main/nfc/scenes/nfc_scene_mf_classic_detect_reader.c`):
```c
#include <lib/nfc/nfc.h>

Nfc* nfc = nfc_alloc();
NfcListener* listener = nfc_listener_alloc(nfc, NfcProtocolIso14443_3a, &data);
nfc_listener_start(listener, callback, ctx);
// ... callback receives NfcEventTypeFieldOn / FieldOff / ListenerActivated
nfc_listener_stop(listener);
nfc_listener_free(listener);
nfc_free(nfc);
```

Lower-level polled alternative (`targets/f7/furi_hal/furi_hal_nfc.c:416,442`):
```c
#include <furi_hal_nfc.h>
furi_hal_nfc_field_detect_start();
bool present = furi_hal_nfc_field_is_present();  // reads hardware efd_o bit
furi_hal_nfc_field_detect_stop();
```

**Never call `furi_hal_nfc_poller_field_on()`** — that transmits. It would make this device behave like the thing it's meant to detect.

Always tear down with `furi_hal_nfc_low_power_mode_start()` on exit so no field is left energized.

---

## 4. CRAWL — proof of concept

**Goal:** hold an NFC-emitting device near the Flipper, hear an alarm. Nothing else.

**Build:**
- Minimal FAP, single screen, text only.
- Start an ISO14443-3A listener with a random/dummy UID.
- On `NfcEventTypeFieldOn` → play an alarm tone + turn LED red.
- On `NfcEventTypeFieldOff` → stop tone, LED off.
- One button: exit (clean radio teardown).
- Display a live counter: `Detections: N`.

**Alarm (crawl version):** hardcoded tone via the speaker.
```c
#include <furi_hal_speaker.h>
if(furi_hal_speaker_acquire(1000)) {
    furi_hal_speaker_start(2000.0f, 1.0f);  // 2 kHz, full volume
    // ... stop + release when field drops
    furi_hal_speaker_stop();
    furi_hal_speaker_release();
}
```
Speaker access is arbitrated — **always check `furi_hal_speaker_acquire()` and pair every acquire with a release**, or the speaker will be left held.

**Test procedure:**
1. Power-cycle the Flipper (**Back + Left**) — clears the wedged CLI state from validation.
2. Launch the FAP.
3. Unlock an Android phone with NFC enabled, hold it to the **back** of the Flipper.
4. Alarm sounds, counter increments, LED red.
5. Withdraw phone → alarm stops.

**Crawl is done when:** step 4 works repeatably. Do not add features until it does.

> **Build status (2026-08-03):** crawl phase **complete and verified on hardware** — a phone in reader mode reliably triggers the alarm. Walk phase (v0.2) is also implemented and field-tested: tiering, configurable vibro/sound alerts, event log, session view, history strip, and first-run splash. Deployment tooling and the API-pinning procedure are in the [README](../README.md).
>
> Detection tuning note: the original 100 ms symmetric debounce never fired, because readers *pulse* their carrier rather than holding it. Replaced with an asymmetric latch (fire on first rising edge, clear only after a 750 ms hold-off). See §4 of this document and `FIELD_HOLDOFF_MS`.

**Expected friction:** if nothing fires, confirm you're using an *emitter* (§1), not a tag, and that you're presenting to the **back** of the device.

---

## 5. WALK — usable daily-carry app

**Goal:** something worth actually keeping armed.

### 5.1 Alarm tiering (the thing that makes it usable)
| Tier | Trigger | Response |
|---|---|---|
| **Info** | `FieldOn` brief (< ~300 ms) | LED blink, log only. **Silent.** |
| **Warn** | `FieldOn` sustained > ~1 s, or repeated in a short window | Short beep + amber LED |
| **Alarm** | `ListenerActivated` | **Full alarm** + red LED + wake screen |

Dwell/repetition thresholds must be named constants, tuned from §5.5 field data.

### 5.2 Configurable alarm (required by scope)
In-app settings, persisted to SD across reboots:
- **Volume** — off / low / med / high (`furi_hal_speaker_start(freq, volume)`, volume 0.0–1.0)
- **Tone frequency** — e.g. 1000 / 2000 / 4000 Hz
- **Pattern** — continuous / pulsing / rising two-tone
- **Duration** — while-field-present, or fixed N seconds
- **Vibration** — on/off, independent of sound
- **LED** — on/off
- **Silent mode** — vibro + LED only (for when a beep would be socially awkward)

Provide a **"Test alarm"** menu item so settings can be auditioned without a reader present. This matters more than it sounds — otherwise tuning requires a second device in hand.

### 5.3 Wake behavior
On Alarm tier: wake the screen and display band, tier, and timestamp. Use `furi_hal_power_insomnia_enter()` / `_exit()` so the app survives screen-off, but release insomnia when idle so the NFC sleep path can actually save power.

### 5.4 UI
- **Main:** armed/disarmed, session detection count, battery, last event time.
- **Heartbeat:** periodic LED blink while armed, so a dark-screen device is visibly alive.
- **Event log:** timestamp + tier, persisted to SD. Record *that* an interrogation happened — never card data.
- **Snooze:** suppress alarms for N minutes at a known-safe terminal. Essential; without it the app gets disabled at the first coffee shop.

### 5.5 Field tuning (do this during walk, not after)
Carry it through a normal day — transit, retail, office. Log what triggers each tier. If **Alarm** tier fires on things that aren't skim attempts, retune before proceeding to run.

Sensitivity lever: the ST25R3916 EFD thresholds (regs `0x2A` activation / `0x2B` deactivation) can be raised so only close, pocket-range readers register. Expose as Low/Med/High sensitivity.

---

## 6. RUN — open-source release

- **Battery optimization:** investigate ST25R3916 **Wake-Up mode** (`REG_WUP_TIMER_CONTROL` 0x32, amplitude/phase/capacitance regs 0x35–0x3D, `IRQ_MASK_WAM`/`WPH`/`WCAP`) — duty-cycled measurement at 3–6 µA vs 16–23 mA fully active. Biggest remaining win.
- **Measure actual current draw** in idle/armed/alarm and publish real runtime numbers. The figures in `VALIDATION.md` §5.3 are datasheet estimates and were **not** bench-confirmed.
- **Compatibility:** build against stock OFW, Unleashed, and Momentum; CI via `ufbt`.
- **Packaging:** README with the §1 tag-vs-reader explanation prominently placed, screenshots, demo video, LICENSE, and a Flipper Application Catalog submission.
- **Robustness:** long-run soak (≥24 h armed), graceful SD-absent handling, recovery if the speaker can't be acquired.
- **Config:** export/import settings.

---

## 7. Acceptance criteria

**Crawl:** Android phone at the Flipper's back reliably triggers an audible alarm; withdrawing it stops the alarm; clean exit leaves no field energized.

**Walk:**
1. `FieldOn` and `ListenerActivated` drive different tiers.
2. Volume, tone, pattern, and silent mode are adjustable in-app and persist across reboot.
3. "Test alarm" works with no reader present.
4. Alarm wakes the screen.
5. Snooze suppresses alarms for its full duration.
6. After a full day of carry, **Alarm**-tier false positives are rare enough that a user would leave it armed. *(This is the real bar.)*

**Run:** ≥24 h soak without crash or stuck speaker; published battery numbers come from measurement, not estimate; builds clean on stock OFW.

---

## 8. Scope boundaries

Defensive only. This app detects interrogation aimed at the wearer and notifies them. It must not capture, store, clone, or replay credentials — the Flipper has separate tooling for that, and merging it here would turn a personal-safety device into a skimmer. Logs record *that* an interrogation occurred (timestamp, tier), never card contents.

---

## 9. Key file references

| Purpose | Path |
|---|---|
| Listener events enum | `lib/nfc/nfc.h:37-46` |
| HAL event mapping (EON/EOF) | `targets/f7/furi_hal/furi_hal_nfc_event.c:68-73` |
| HAL field detect / is_present | `targets/f7/furi_hal/furi_hal_nfc.c:416,442` |
| Public HAL event enum | `targets/furi_hal_include/furi_hal_nfc.h:43-45` |
| Working listener precedent | `applications/main/nfc/scenes/nfc_scene_mf_classic_detect_reader.c` |
| EFD threshold registers | `lib/drivers/st25r3916_reg.h:144-148` |
| Wake-up mode registers | `lib/drivers/st25r3916_reg.h:173-193` |
