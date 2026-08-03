# How to test NFC Canary on the device

"Nothing triggered it" almost always means the **test source wasn't emitting a
field**, not that the app is broken. This procedure separates those two cases
in a fixed order so you never have to guess which one you're looking at.

---

## Step 0 — The single most common mistake

**A passive NFC tag, card, badge, or fob will NEVER trigger this app.**

Passive tags have no power source. They contain no transmitter. They only
reflect (backscatter) energy that a *reader* supplies. This app detects a
**reader's carrier wave**. Waving a tag at the Flipper is like waving a mirror
at a light sensor in a dark room — there is nothing to reflect.

If you tested with a tag, a hotel keycard, a transit card, or an access badge:
**that is why nothing happened, and the app may well be working fine.**

You need something that **actively emits** 13.56 MHz. See Step 2.

---

## Step 1 — Prove the app itself is alive (30 seconds, no reader needed)

This confirms the radio came up and the alarm hardware works, independent of
any test source.

1. Launch **Apps → NFC → NFC Canary**.
2. Press **Right** to open the **Diagnostics** screen.
3. Read the two lines that matter:

| Line | Meaning |
|---|---|
| `NFC radio: OK` | HAL acquired, External Field Detector running. **Required.** |
| `NFC radio: FAILED` | Another app holds the NFC hardware. Exit any NFC/Reader app, or reboot the Flipper, and relaunch. |
| `EFD bit now: 0 (quiet)` | No field right now — correct in a quiet room. |
| `EFD bit now: 1 (FIELD)` | A carrier is present *right now*. |

4. Press **OK** to fire the **alarm self-test** — you should hear a ~0.6 s tone
   and see the LED go red.

**Interpretation:**
- Self-test is silent → speaker/notification problem, not a detection problem.
- `NFC radio: FAILED` → the radio never started; nothing will ever trigger.
- `NFC radio: OK` + audible self-test → **the app is fine.** Any failure to
  trigger from here on is your test source. Continue to Step 2.

Leave this Diagnostics screen open for Step 2 — the `EFD bit` and `Raw edges`
counters update live and will move even if the debounce filter suppresses the
alarm.

---

## Step 2 — Trigger it with a real emitter

Ranked by how reliably they work. **Present the source to the BACK of the
Flipper** — that's where the NFC antenna is, not the screen side.

### A. Android phone (best, most people have one)

1. Settings → search "NFC" → make sure **NFC is ON**.
2. **Unlock the phone and keep the screen on.** A locked/asleep Android usually
   stops polling — this alone explains many "it didn't work" reports.
3. Hold the phone's NFC antenna flat against the **back** of the Flipper.
   - The antenna is usually the **upper third** of the phone's back, near the
     camera. It varies by model; sweep slowly across the whole back panel.
4. Hold for 2–3 seconds, then pull away, then repeat.

Expect: `EFD bit` flips to `1 (FIELD)`, `Raw edges` climbs, alarm sounds.

> Tip: opening any NFC-using app on the phone (payment app, "NFC Tools", or a
> tag-reader app) forces continuous polling and makes this much more reliable.

### B. Payment terminal / transit gate / office badge reader (most realistic)

These emit strongly and continuously. Hold the Flipper's back near the reader's
tap zone. This is also the honest real-world false-positive test — it's exactly
the situation the alarm tiering in SPEC.md §5 exists to handle.

**Only do this with readers you're authorized to be near.** Standing at your
own office badge reader or a self-checkout you're actually using is fine;
loitering at someone else's terminal to test a device is not.

### C. Second Flipper (most definitive)

If you can borrow another Flipper, this is the cleanest possible test — a known
emitter under your control:

```
# On the OTHER Flipper's CLI:
nfc field
```

That command turns its NFC field ON (validated: it calls
`furi_hal_nfc_poller_field_on()`). Hold the two Flippers back-to-back. Your
alerter should fire immediately and stay firing until you Ctrl+C that command.

> Don't leave `nfc field` running long — the firmware itself warns against it,
> and it drains the emitting device.

### D. iPhone (works, but don't debug with it)

iPhones poll opportunistically rather than continuously, so results are
intermittent. If an iPhone doesn't trigger it, that proves nothing. Use A or C.

---

## Step 3 — Reading the result

| What you see | What it means |
|---|---|
| Alarm sounds, banner shows, counter climbs | **Working.** Crawl phase passes. |
| `EFD bit` flips to 1 but no alarm | Field is real but too brief — lower `FIELD_DEBOUNCE_MS` (default 100 ms). |
| `Raw edges` climbing, `Detections` flat | Same as above: field is flickering below the debounce window. |
| `EFD bit` stays 0 with a phone pressed to the back | Wrong spot on the phone (sweep the whole back), phone locked, or NFC off. |
| `NFC radio: FAILED` | Another app owns the radio. Reboot and relaunch. |
| Nothing at all, screen frozen | App crashed — see Troubleshooting. |

---

## Troubleshooting

**Flipper not detected by the computer**
```bash
ls /dev/serial/by-id/ | grep -i flipper
```
Nothing listed → it's unplugged, powered off, or the cable is charge-only. Try
a different USB cable; many cheap cables carry power but no data.

**Serial CLI stops responding** (port opens, commands return nothing)

This happened repeatedly during development. Power-cycle the Flipper: hold
**Back + Left** for ~5 seconds until it reboots.

**App won't launch: `Preload failed: Invalid file`**

SDK/firmware API mismatch. See `nfc_canary/README.md` — the SDK must be pinned
to match `info device` exactly (this device: API 87.8 / `unlshd-089e`).

**Watching logs while testing**

Connect a serial terminal at 230400 baud and run `log`. The app emits
`reader field detected` / `reader field lost` at INFO level, which confirms
detection even if the audio is hard to hear.

---

## What "passing" means at crawl stage

Crawl is done when **Step 2A or 2C reliably triggers the alarm, repeatably.**

That's the whole bar. Battery life, false-positive tuning, and alert tiering
are deliberately deferred to walk phase (SPEC.md §5) — don't hold crawl to
them.
