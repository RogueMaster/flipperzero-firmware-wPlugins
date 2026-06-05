# Samsung AC Remote (Flipper Zero FAP)

Infrared remote for Samsung air conditioners. Every button press sends the full Samsung
A/C state frame: power, mode, temperature, fan and swing.

The protocol (timings, byte-field layout, the dual-section checksum) comes from the reverse
engineering in [IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266)
`ir_Samsung.cpp`, originally worked out in
[issue #1538](https://github.com/crankyoldgit/IRremoteESP8266/issues/1538).

## Controls

- **Power**: on / off
- **Mode**: cool, heat, dry, fan, auto
- **Temp +/-**: 16 to 30 °C (locked in fan mode)
- **Fan**: auto, low, med, high
- **Swing**: vertical swing on / off

Samsung A/C is stateful. There are no one-shot command codes, so the whole current state
gets retransmitted on every press.

## Protocol notes

- Carrier 38 kHz, 50% duty
- Header 690 / 17844 µs, then two 7-byte sections
- Per section: 3086 / 8864 µs lead, 56 bits LSB-first (bit mark 586, one 1432, zero 436),
  footer mark 586 plus a 2886 µs gap
- Each section carries its own checksum: count the set bits over the section (excluding the
  checksum nibbles) and negate it. Verified against IRremoteESP8266's `validChecksum` test
  vectors.

## Build

The flake gives you the pinned fbt toolchain (reused from
[wlipurk-appkit](https://github.com/dappermint/wlipurk-appkit)) plus a `just` harness that
symlinks this app into a firmware tree and builds the fap:

```sh
nix develop                 # toolchain, just, python(pyserial), usb tools
just build                  # f6/az0v fork (default): TARGET_HW=6 FIRMWARE_ORIGIN=az0v
just target=7 origin= build # stock f7: TARGET_HW=7
just install                # build and deploy over USB to /ext/apps/Infrared/
```

Each step on its own: `just link`, `just build`, `just deploy`, `just clean`. Override the
firmware path or device port inline, e.g. `just fw=/path/to/fw port=/dev/cu.usbmodemX build`.

The toolchain doesn't care about the target. The same gcc/python bundle builds f6 or f7,
only `TARGET_HW` changes. The fap lands at
`<fw>/build/f<target>-firmware-D/.extapps/samsung_ac_remote.fap`, copy it to
`apps/Infrared/` on the SD card.

Without the flake: symlink the app into a firmware tree's `applications_user/` and run
`./fbt fap_samsung_ac_remote`.

UI and panel code are adapted from
[flipperzero-midea-ac-remote](https://github.com/xakep666/flipperzero-midea-ac-remote)
(GPL), see `LICENSE`.
