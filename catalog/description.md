Carry your Flipper in a pocket or bag and it will alert you — by vibration first — when someone points an NFC reader at you.

It uses the NFC chip's hardware field detector to listen for a reader's carrier, and never transmits anything of its own.

## Detects readers, not tags

A card, badge, or fob will never trigger this. Passive tags have no transmitter — they only reflect energy from a reader. To set it off you need something that emits a field: a phone with NFC enabled (unlocked), or a payment or access-control terminal. Present it to the **back** of the Flipper, where the antenna is.

## Uses

- Keep it in a pocket and get alerted if someone tries to sniff your wallet
- Drop it in a bag to count how many things tried to scan you at an airport or a conference
- Anywhere you want to know that something is probing you at 13.56 MHz

## Threat tiers

Ambient 13.56 MHz is everywhere — payment terminals, transit gates, phones — so a bare carrier is not treated as an alarm.

- **Info** — a brief carrier. Logged, silent.
- **Warn** — a sustained carrier. Vibration.
- **Alarm** — the loudest tier. Vibration and sound.

## Alerts

Vibration is the primary channel, because a pocket alert should be felt before it is heard. Vibration and sound each get their own pattern (Off, Single, Double, SOS, Pulse, Constant), and you can set the tone, the volume, and the minimum tier for each channel independently. There is a silent mode, and a test button so you can audition the configuration without needing a reader.

## Also included

- Timestamped event log, saved to the SD card as CSV
- Session summary: counts by tier, first and last event, longest exposure
- History strip showing the last few minutes at a glance
- Diagnostics screen with the live field-detect bit and an alarm self-test

## Scope

This is a defensive tool. It tells you that you are being scanned; it does not capture, store, clone, or replay anything. The log records only that an interrogation happened — never card contents.

It detects, it does not prevent: by the time it fires, a read may already have succeeded. The value is knowing.

Source, documentation, and issues: https://github.com/antitree/nfc_alerter
