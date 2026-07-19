# CCID Emulator — Sample Cards

Ready-to-use `.ccid` card profiles for the [CCID Emulator](../ccid_emulator/) app.
Each file defines a smartcard's ATR plus a set of **APDU command → response**
rules, so the Flipper presents as a USB CCID smartcard reader with that card
inserted. A PC/SC host (`pcsc_scan`, `opensc-tool`, `gpg`, `ykman`, an EMV
analysis tool, …) can then exchange APDUs with it for reader/parser testing.

## Installing

Copy the `.ccid` files to your Flipper SD card:

```
/ext/ccid_emulator/cards/
```

Then open **CCID Emulator → Card Browser**, pick a card, and start emulation.
(The app also writes a few built-in samples there on first run.)

## Included cards

| File | AID / standard | What it exercises |
|------|----------------|-------------------|
| `test_card.ccid` | generic | Minimal SELECT / GET DATA smoke test |
| `iso7816_memory.ccid` | ISO 7816-4 | Raw MF/EF SELECT, READ BINARY, VERIFY, GET CHALLENGE — best starting point |
| `piv_emulator.ccid` | PIV `A0000003080000100001` | FIPS-201 PIV: applet select, CHUID, Discovery Object, PIN verify |
| `emv_visa_contactless.ccid` | Visa `A0000000031010` | Contactless EMV: **PPSE** (2PAY.SYS.DDF01), AID select, GPO, READ RECORD |
| `emv_mastercard_contact.ccid` | Mastercard `A0000000041010` | Contact EMV: **PSE** (1PAY.SYS.DDF01) directory, AID select, GPO |
| `openpgp_card.ccid` | OpenPGP `D27600012401` | `gpg --card-status` flow: SELECT + GET DATA AID/login/cardholder |
| `yubikey_oath.ccid` | YKOATH `A0000005272101` | YubiKey OATH: SELECT, LIST credentials, CALCULATE |

## Quick test from a PC

With the emulated card running and the Flipper plugged in via USB:

```sh
# List the reader + card ATR
pcsc_scan

# Drive a specific APDU (ISO7816 memory card: SELECT MF)
opensc-tool -s 00:a4:00:00:02:3f:00

# OpenPGP card
gpg --card-status
```

## File format

```ini
[card]
name = My Card
description = ...
atr = 3B 88 80 01 00 ...      ; space-separated hex, ≤ 33 bytes

[rules]                        ; ≤ 24 rules
# COMMAND_HEX = RESPONSE_HEX   ; command ≤ 64 B, response ≤ 128 B
00 A4 04 00 02 3F 00 = 90 00
00 B0 00 ?? 10 = ...           ; ?? matches any single byte (wildcard)

[default]
response = 6A 82               ; sent when no rule matches
```

Lines beginning with `#` or `;` are comments. A rule whose command or response
exceeds its length bound is **skipped with a log warning** rather than silently
truncated (see issue #59), so keep responses within 128 bytes.

## A note on the data

These profiles use **published standard AIDs and illustrative test data** — not
dumps of anyone's real card. They reproduce the *structure* of each card's
application-selection / data-object flow so readers and parsers can be exercised
safely. Use them for testing software and hardware you are authorized to test.
