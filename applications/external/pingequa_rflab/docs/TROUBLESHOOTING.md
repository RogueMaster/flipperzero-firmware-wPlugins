# Troubleshooting

Quick reference for common issues. If your problem isn't here, [file a bug](../../../issues/new?template=bug.yml).

## Error screen — "Board Not Detected"

The error screen shows diagnostic register dumps from both chips. Find your pattern below:

### Pattern: `RF=02 SET=0F CFG=08`

```
NRF24 FAIL
RF=02 SET=0F CFG=08
AW=03 ST=0E
```

✅ **NRF24 is alive** — these are the datasheet default values for RF_CH, RF_SETUP, CONFIG, SETUP_AW, STATUS at power-on.

If you see this but `Cur:N` always reads 0 in scanner mode, the chip is fine but:
- Your antenna is unplugged or damaged
- You're in an RF-quiet area (no signals to detect)
- Dwell time too low for the actual signals' duration

### Pattern: All zeros — `RF=00 SET=00 CFG=00 AW=00 ST=00`

```
NRF24 FAIL
RF=00 SET=00 CFG=00
AW=00 ST=00
```

❌ **NRF24 not responding** — either:
1. **No power** to the chip — check 5V OTG is enabled (Flipper Settings → System → Power)
2. **PC3 not connected** — verify the board is fully seated on the GPIO header
3. **Damaged chip** — rare, but possible if voltage was wrong on the supply

### Pattern: All `0xFF` — `RF=FF SET=FF CFG=FF`

❌ **MISO floating** — no chip pulling MISO low. Either:
1. Board not connected at all
2. Defective board (unlikely if recently purchased)

### Pattern: Mixed — `RF=02 SET=0F` but `AW≠03 ST=0E`

⚠️ **Firmware bug** — should not happen on v0.2.0+. If you see this:
1. Make sure you're on the latest release
2. File a bug with full register dump

### CC1101 also missing

```
CC1101 FAIL (PN=00 V=00)
NRF24 FAIL ...
```

Both chips down → board likely not connected. If only CC1101 fails but NRF24 works:
- Confirm your board has CC1101 (some early prototypes may not)
- Check for shorts on PA4 / SPI lines

## App freezes / unresponsive

### Buttons stop working after several presses

**Should not happen on v0.2.0+** — fixed by worker yield + volatile `dwell_us`. If you see it:
1. Update to latest release
2. File a bug with: firmware version, hardware variant, time-to-freeze, what keys were pressed in what sequence

### Display frozen but bars still updating

Sounds like the scanner_view background updates work but input thread is stuck. File a bug.

### Display frozen AND bars not updating

Probably auto MAX HOLD mode — top right shows `MAX!`. Press OK to clear and rescan.

If `MAX!` is not shown and display is genuinely frozen, it's a real freeze — file a bug.

## Bars never grow

### All bars stay at 0 height

- **No nearby 2.4 GHz activity?** Try near a known WiFi access point
- **Dwell too low?** Press ↑ a few times to increase
- **Antenna issue?** If your board has a U.FL connector, ensure antenna attached
- **Hardware doesn't actually work?** Run [Method 1 verification](#hardware-verification) below

### Some bars grow, but not where I expect

The frequency map of NRF24 channels to MHz is `ch N ↔ 2400+N MHz`. Verify:
- Move cursor onto the tall bar — header shows the actual frequency
- Compare to known protocols (see [HARDWARE.md](HARDWARE.md) channel reference)

### Bars saturate too fast in busy areas

Lower dwell time with ↓. Less dwell = lower sensitivity = harder to saturate.

Or accept that your environment is congested and use MAX HOLD as a snapshot tool.

## Flipper Sub-GHz / NFC apps stop working after using PINGEQUA RF Lab

**Should not happen on v0.2.0+** — the app cleans up all pins on exit.

If you see this:
1. Reboot Flipper (Power off → on)
2. If issue persists, file a bug — that's a regression we want to fix

## Build issues (from source)

### `ufbt` fails with "API mismatch"

```
APPCHK error: API version 87.x doesn't match
```

Update SDK: `ufbt update --channel=dev`. This pulls the latest mntm-dev SDK.

### Build succeeds but FAP crashes on Flipper

Make sure the SDK target matches the firmware on your Flipper. If your Flipper runs `mntm-012`, build with `--channel=release` or pin a specific branch.

```bash
ufbt update --channel=release   # for stable mntm
ufbt update --channel=dev       # for latest dev
```

### "spi_config.c not found" / linker errors

You're trying to call functions outside the Flipper public API. Stick to documented HAL functions — this app only uses public APIs and builds clean.

If you forked and added something, the offending symbol is in your changes.

## Hardware verification

If you suspect the hardware is faulty, run this checklist:

1. **Visual inspection**: chips clean, no scorch marks, all GPIO pins seated
2. **Use a known-good app first**: try [hot tools / firmware-stock NRF24 scanner](https://github.com/htotoo/NRF24ChannelScanner) — if it also fails, hardware issue
3. **Continuity check** (multimeter): from Flipper PC3 to nRF24's CSN pin — should be 0 ohms
4. **Power check**: with board connected, measure 3.3V on the regulator output

If 3 fails, board is defective. Email [support@pingequa.com](mailto:support@pingequa.com) for warranty support.

## Still stuck?

- **GitHub Issues**: [bug template](../../../issues/new?template=bug.yml)
- **GitHub Discussions**: [ask the community](../../../discussions)
- **Email**: [support@pingequa.com](mailto:support@pingequa.com)

Include in any bug report:
- Firmware (Momentum / Unleashed / OFW) and version
- Hardware (PINGEQUA 2-in-1 / other — describe pin mapping)
- App version (top of `application.fam` or future About screen)
- Exact reproduction steps
- Error screen text if any (full register dump)
