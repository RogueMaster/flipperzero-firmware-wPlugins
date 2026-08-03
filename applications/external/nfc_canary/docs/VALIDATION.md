# Validation: Flipper Zero as a passive NFC/RFID skimming detector

**Date:** 2026-08-02
**Device under test:** Flipper Zero "Noodlasx", HW rev 12, Unleashed `unlshd-089e` (API 87.8), `/dev/ttyACM4`
**Verdict:** **Feasible for 13.56 MHz (NFC). Feasible but power-expensive for 125 kHz (LF RFID). NOT feasible simultaneously — a hardware pin conflict forces time-multiplexing.**

> **Scope decision (2026-08-02):** based on this validation, the project was scoped to **NFC only**. Dropping LF eliminates both hard constraints below (the shared-pin conflict in §4 and the forced-awake MCU in §3). See `SPEC.md` for the build plan. The LF analysis is retained here as the rationale for that cut and as reference if LF is revisited.

---

## 1. Bottom line

| Question | Answer |
|---|---|
| Can the Flipper detect a *reader's* field passively? | **Yes**, both bands, via existing public HAL APIs. |
| Can it detect both bands at once? | **No.** Single shared GPIO, physically exclusive. Must time-slice. |
| Can it alert loudly? | **Yes** — speaker + vibro + LED, unlike passive LED cards. |
| Is it practical in a back pocket all day? | **NFC-only: yes.** Dual-band: ~8–12 h realistically, not a full day. |
| Biggest risk | Not battery — it's **false positives** and **detection-range asymmetry** (see §5). |

The core premise is sound and this is worth building. The honest caveat is that the naive "watch both bands continuously" design is the one thing the hardware cannot do.

---

## 2. Evidence: 13.56 MHz (NFC) — CONFIRMED VIABLE

The ST25R3916 has a dedicated **External Field Detector** in silicon. This is not a workaround; it's a designed-in feature.

**Registers** (`lib/drivers/st25r3916_reg.h`):
```
0x2A  External Field Detector Activation Threshold Register
0x2B  External Field Detector Deactivation Threshold Register
```
Thresholds are programmable — this is what makes sensitivity tunable in §5.

**Interrupts** (`lib/drivers/st25r3916.h:31-33`):
```c
#define ST25R3916_IRQ_MASK_EON 0x00001000U  /** external field on interrupt */
#define ST25R3916_IRQ_MASK_EOF 0x00000800U  /** external field off interrupt */
```

**Already wired into the Flipper HAL** (`targets/f7/furi_hal/furi_hal_nfc_event.c:68-73`):
```c
if(irq & ST25R3916_IRQ_MASK_EON) { event |= FuriHalNfcEventFieldOn; }
if(irq & ST25R3916_IRQ_MASK_EOF) { event |= FuriHalNfcEventFieldOff; }
```

**Exposed as public API** (`targets/furi_hal_include/furi_hal_nfc.h:43-45`):
```c
FuriHalNfcEventFieldOn  = (1U << 1), /**< External field (carrier) has been detected. */
FuriHalNfcEventFieldOff = (1U << 2), /**< External field (carrier) has been lost. */
FuriHalNfcEventListenerActive = (1U << 3), /**< Reader has issued a wake-up command. */
```

And at the higher `Nfc` library level (`lib/nfc/nfc.h:37-46`):
```c
NfcEventTypeFieldOn,          /**< Reader's field was detected by the NFC hardware. */
NfcEventTypeFieldOff,         /**< Reader's field was lost. */
NfcEventTypeListenerActivated,/**< The listener has been activated by the reader. */
```

There is also a polled path (`furi_hal_nfc.c:416`, `:442`) reading the hardware `efd_o` bit:
```c
FuriHalNfcError furi_hal_nfc_field_detect_start(void);
bool furi_hal_nfc_field_is_present(void);
```

**Precedent:** stock firmware already ships **"Detect Reader"** (`applications/main/nfc/scenes/nfc_scene_mf_classic_detect_reader.c`), which uses `nfc_listener_alloc()` / `nfc_listener_start()` to sit passively and react to a reader. The passive-listen path is proven in shipping code, not theoretical.

### The critical distinction this buys us

Two different signals, and conflating them is the #1 design mistake:

- `FieldOn` — *any* 13.56 MHz carrier appeared. Fires on payment terminals, transit gates, phones, and a reader across the room. **High false-positive rate.**
- `ListenerActivated` — a reader completed anticollision and **addressed the emulated card by UID**. This is someone actively trying to *enumerate a card*, i.e. the actual threat.

A good alerter treats these as different severities. `ListenerActivated` is the high-confidence skim signal.

---

## 3. Evidence: 125 kHz (LF RFID) — VIABLE, BUT COSTLY

A field detector exists (`targets/f7/furi_hal/furi_hal_rfid.h:91-102`):
```c
void furi_hal_rfid_field_detect_start(void);
void furi_hal_rfid_field_detect_stop(void);
bool furi_hal_rfid_field_is_present(uint32_t* frequency);
```

**It is genuinely passive.** In `furi_hal_rfid_pins_field()` (`furi_hal_rfid.c:157-172`) the carrier output is driven **low**, and the antenna pin is configured as a **timer input** (`GpioModeAltFunctionPushPull ... GpioAltFn2TIM2`), not a driver. It listens; it does not transmit.

**Mechanism** — count carrier cycles into TIM2 via external clock, gate with TIM1, DMA the count out:
```c
LL_TIM_SetPrescaler(TIM1, 64000 - 1);
LL_TIM_SetAutoReload(TIM1, 100 - 1);   // 100 ms gate
```
```c
*frequency = furi_hal_rfid->field.counter * 10;   // 100ms count -> Hz
return (*frequency >= 80000) && (*frequency <= 200000);
```

**Implications:**
- **Accepted band is 80–200 kHz.** Covers 125 kHz (EM4100/HID Prox) and 134.2 kHz (animal/FDX-B). Wider than needed → more false positives from switching-supply and motor noise in that range.
- **Detection latency floor is 100 ms** (the gate period). Fine for this use case.
- **It reports frequency**, so alerts can distinguish 125 kHz from 134 kHz.

### Why LF is the expensive one

LF detection holds **TIM1 + TIM2 + two DMA channels running continuously**, and there is **no hardware wake-up interrupt** — the MCU must stay awake polling. Contrast with NFC, which has a silicon field detector that can interrupt an otherwise-sleeping MCU. This asymmetry, not the radios themselves, drives the power budget.

---

## 4. THE BLOCKER: NFC and LF cannot run simultaneously

Both radios need the **same physical pin**, `gpio_nfc_irq_rfid_pull` (`furi_hal_resources.c:50`) — the pin is literally named for its double duty (NFC IRQ *and* RFID pull).

NFC requires it as an **input interrupt** (`furi_hal_nfc_irq.c:21-23`):
```c
furi_hal_gpio_init(&gpio_nfc_irq_rfid_pull, GpioModeInterruptRise, GpioPullDown, GpioSpeedVeryHigh);
furi_hal_gpio_add_int_callback(&gpio_nfc_irq_rfid_pull, furi_hal_nfc_int_callback, NULL);
```

LF requires it as a **push-pull output** (`furi_hal_rfid.c:107-108, 142-143, 163-164`):
```c
furi_hal_gpio_init(&gpio_nfc_irq_rfid_pull, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
furi_hal_gpio_write(&gpio_nfc_irq_rfid_pull, false);
```

These are mutually exclusive configurations of one pin. Starting LF detection **destroys the NFC interrupt path**. This is a hardware routing decision, not a firmware limitation — **no firmware patch can fix it.**

> **Consequence:** any dual-band design must time-multiplex, and therefore has a duty-cycle-dependent probability of missing a scan. This must be stated honestly in the spec rather than hidden.

A real skim is not instantaneous — a reader held to a pocket typically dwells ~0.5–3 s. With a 250 ms/250 ms alternation, a 1 s dwell yields ~4 sampling opportunities per band, so detection probability is high but **not 100%**. A fast "tap-and-go" LF read could fall in a blind window.

---

## 5. Practicality: the real risks

Power is the *expected* concern; it turns out to be the manageable one. Ranked by actual severity:

### Risk 1 — False positives (HIGHEST)
The modern world is saturated with 13.56 MHz. Payment terminals, transit gates, access control, and every NFC-enabled phone emit it. A back-pocket device alarming on every checkout counter, subway turnstile, and passing phone becomes noise the user disables within a day. **This, not battery, is what kills the product.**

Mitigations:
- Prefer `ListenerActivated` over bare `FieldOn` for the loud alarm; keep `FieldOn` as a silent/log-only tier.
- Programmable EFD thresholds (regs `0x2A`/`0x2B`) allow raising the activation floor so only *close* (pocket-range) readers trigger — a genuine skimmer must be near the pocket to couple.
- Dwell/repetition filter: ignore single brief pings; alert on sustained or repeated interrogation.
- Quiet-zone / snooze so the user can suppress alerts at a known terminal.

### Risk 2 — Detection range asymmetry (physics, unavoidable)
To *read* a card, an attacker must couple enough energy into it. To *detect* the reader, we only need to sense the carrier — sensing is easier than being read. **This asymmetry is in our favor:** we can detect a reader at a range where it could not yet successfully read a card. That means alerts can fire *before* a successful skim — but it also guarantees false positives from readers that were never a threat. Tuning §5.1 is really tuning this tradeoff.

### Risk 3 — Battery (MANAGEABLE, and less bad than expected)
Measured/spec figures:
- Flipper battery: **2100 mAh**; deep-sleep idle **1.5 mA**; active ~30 mA with backlight.
- ST25R3916: **power-down 0.8–2.5 µA**, **wake-up mode 3.0–6.3 µA**, **fully active 16–23 mA**.

The ST25R3916 supports a true low-power **Wake-Up mode** (`ST25R3916_REG_WUP_TIMER_CONTROL` 0x32, plus amplitude/phase/capacitance measurement registers 0x35–0x3D and `IRQ_MASK_WAM` / `WPH` / `WCAP`) that duty-cycles measurements off a low-power RC oscillator and interrupts the MCU only on a delta.

Practical estimates (screen off, no BLE, backlight off):
| Mode | Est. draw | Est. runtime (2100 mAh) |
|---|---|---|
| NFC-only, EFD + MCU mostly asleep | ~3–6 mA | **~2 days** (conservative) |
| NFC-only, naive always-active poller | ~20–25 mA | **~3.5–4 days** theoretical; **~1 day** with real-world overhead |
| Dual-band time-sliced (LF forces MCU awake) | ~12–20 mA | **~8–12 h** |
| Any mode with backlight on | +30 mA | hours |

> Note the dominant cost in dual-band is **the MCU being unable to sleep because of LF**, not the radios. NFC-only is comfortably a full-day device; dual-band is a "conference day, then charge" device. Both are acceptable if the user is told which mode they're in.

**These are estimates from datasheet + firmware analysis, not bench measurements.** Phase 0 of the build must measure actual draw before the runtime claims are trusted.

### Risk 4 — Alert reaches the user
This is the stated advantage over passive LED cards, and it holds: the Flipper has a **speaker, vibration motor, and RGB LED**. In a back pocket, **vibration is the primary channel** — audio is muffled by fabric and useless in a loud environment (a DEF CON floor being exactly the scenario). Design vibro-first.

### Risk 5 — Operational
- Alerting *after* the fact is inherent — the card may already be read. Value is in *awareness and deterrence*, not prevention. Set expectations honestly.
- The Flipper is conspicuous. At DEF CON, fine. In a bank lobby, less so.
- Leaving an app running with the screen off can look like the device is idle; a periodic heartbeat blink confirms it's armed.

---

## 6. Testing notes (device interaction)

Verified on hardware:
- `info device` — confirmed HW rev 12, Unleashed `unlshd-089e`, radio alive.
- `help` — confirmed both `nfc` and `rfid` command trees exist.
- `nfc` → `help` — subcommands: `scanner`, `raw`, `dump`, `apdu`, `emulate`, `mfu`, `field`.
- `rfid` — only `read` / `write` / `emulate` / `raw_read` / `raw_emulate` / `raw_analyze`. **All active modes; no CLI detect primitive.** LF detection is HAL-only and requires a custom FAP.

**`nfc field` is an emitter, not a detector.** Source (`nfc_cli_command_field.c`):
```c
.name = "field", .description = "Turns NFC field on",
...
furi_hal_nfc_poller_field_on();
printf("Field is on. Don't leave device in this mode for too long.\r\n");
```
Do not mistake this for a detection command.

> **Caveat on live testing:** the CLI session became unresponsive after invoking `nfc field` together with `log level 5`, and did not recover; the device requires a manual power-cycle (hold Back + Left). Consequently the vibro/buzzer/LED alert commands were issued but **their physical firing was not visually confirmed**, and no live field-detection capture was obtained. All capability claims above rest on firmware source and vendor datasheet figures, which are authoritative for feasibility — but the empirical bring-up in Phase 0 is genuinely required, not a formality.

---

## 7. Recommended architecture

Build a custom FAP (`.fap`) — CLI is insufficient (no LF detect command, no event loop).

**Mode selector (do not hide this from the user):**
1. **NFC Guard (recommended default)** — 13.56 MHz only. Uses EFD/wake-up interrupts, MCU sleeps. Full-day battery. Catches the dominant modern threat.
2. **LF Guard** — 125 kHz only. Continuous timer/DMA polling.
3. **Dual Watch** — time-sliced (~250 ms/band), reconfiguring the shared pin each swap. **Must warn: reduced battery and a small chance of missing a fast scan.**

**Alert tiers:**
| Tier | Trigger | Response |
|---|---|---|
| Info | `FieldOn` brief/weak | LED blink, log only |
| Warn | `FieldOn` sustained / repeated | Short vibro + LED |
| **Alarm** | `ListenerActivated`, or LF field in-band sustained | **Long vibro burst + beep + red LED + screen detail** |

**Must-haves:** snooze/quiet-zone, event log with timestamps + frequency, adjustable EFD threshold, heartbeat blink when armed, `furi_hal_power_insomnia_*` handling so the app survives screen-off.

---

## 8. Conclusion

**Validated.** The Flipper Zero can do this, and it improves on passive LED indicator cards in the way that matters — it can produce a vibration alert the wearer actually notices in a pocket.

Two constraints are non-negotiable and must shape the design rather than be discovered later:
1. **The shared-pin conflict makes true simultaneous dual-band monitoring physically impossible.** Time-multiplexing is the only option, with an accepted small miss probability.
2. **LF monitoring prevents MCU sleep**, making dual-band roughly a half-day device versus a multi-day device for NFC-only.

The engineering risk that most threatens the product is **false-positive tuning**, not battery life. Build NFC Guard first, tune thresholds against real terminals, and add LF only once the alert logic is trustworthy.

**Recommended next step:** Phase 0 bring-up — a minimal FAP that logs `FieldOn` / `ListenerActivated` with timestamps and RSSI-ish threshold data, plus bench current measurements to replace the estimates in §5.3.

## References
- [ST25R3916 datasheet (ST)](https://www.st.com/resource/en/datasheet/st25r3916.pdf)
- [AN5320 — Wake-up mode for ST25R3916](https://www.st.com/resource/en/application_note/an5320-wakeup-mode-for-st25r391616b-st25r391717b-st25r3918-st25r3919b-and-st25r392020b-devices-stmicroelectronics.pdf)
- [ST community — ST25R3916 wake-up mode current figures](https://community.st.com/t5/st25-nfc-rfid-tags-and-readers/st25r3916-wake-up-mode/td-p/248799)
- [Flipper Zero power documentation](https://docs.flipper.net/zero/basics/power)
- [Flipper blog — deep sleep / 1 month battery](https://blog.flipper.net/1-month-battery-life-with-firmware-update/)
- Firmware source: `github.com/flipperdevices/flipperzero-firmware`
