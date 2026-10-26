# Manual hardware test matrix

## Memory limit run

Generate the credential ladder, then copy the `.dfcb` files into
`SD Card/apps_data/dfc/`:

```sh
python3 tests/generate_stress_dfcb.py /tmp/dfc-stress
```

Build and launch the app with `ufbt launch`. The file picker accepts both
`.dfc` and `.dfcb` files. Compile larger text credentials on the host to keep
the Flipper's heap available for emulation:

```sh
python3 tests/compile_dfcb.py card.dfc card.dfcb
```

The Flipper build accepts at most 2 applications, 8 files, 8 keys per
application or at the PICC level, 2 KiB of total file payload, and 1 KiB in one
file. Its ATS must advertise a 64 byte frame size, 106 kbit/s, FWI at least 8,
and no NAD support. A 4 byte random UID needs ATQA `04 03` on air; a 7 byte UID
uses `44 03` by default.

Keep qFlipper screen streaming active throughout the run. For each credential
in ascending payload and application count, load it, start emulation, send the
same reader APDUs, and leave emulation. Record whether the app loads, emulates,
responds, or resets. Repeat each passing case five times. The `DfcMemory` log
lines report free heap, minimum free heap, and largest free block before and
after the allocations; a final "before" line without its matching "after"
identifies the failing stage.

The ladder reaches the current compiled cap of 2 KiB payload and 2
applications. After selecting the generated `stress-a2-d2048.dfcb` on the
Flipper, repeat the file-read check with `python3 tests/probe_pcsc_stress.py`
on macOS.

With qFlipper streaming active on 2026-09-25, an earlier higher-capacity build
loaded and emulated a four-application EV3 credential with 3 KiB of data and
returned four AIDs through an ACR1552 reader. Its 2 KiB file also read correctly
in 36 frames. The 4 KiB binary cases were rejected at load on that build. This
is historical evidence for the memory boundary, not the current supported
capacity or a proven crash threshold. Increase the build capacities and repeat
before claiming a larger supported configuration.

The 1,428-byte `H10301-FC69-CN420-field.dfc` loaded through the import plugin
with qFlipper connected and emulated through an ACR1552 on 2026-09-25. PC/SC
completed all three GetVersion frames, listed and selected AID `4F49D3`, listed
file `0F`, and read its file settings. Unauthenticated ReadData returned `91AE`
because this file requires authentication.

Protocol framing, crypto, and credential-file parsing are covered by the
automated unit test suite (see the core test command in README). The scenarios
below require a physical DESFire-compatible card and Flipper hardware, so
they're tracked manually here instead.

| Auth cipher         | Key length | Comm mode  | Result |
| -------------------- | ---------- | ---------- | ------ |
| Legacy DES (0x0A)    | 8 bytes    | Plain      |        |
| Legacy DES (0x0A)    | 8 bytes    | MAC        |        |
| 2K3DES (0x0A)        | 16 bytes   | MAC        |        |
| ISO 3DES (0x1A)      | 16 bytes   | Enciphered |        |
| 3K3DES (0x1A)        | 24 bytes   | Enciphered |        |
| AES (0xAA)           | 16 bytes   | MAC        |        |
| AES (0xAA)           | 16 bytes   | Enciphered |        |

Also verify:
- Reading a card with the default (all-zero) PICC-level key succeeds when no
  saved credential is loaded.
- Emulating a saved credential is accepted by a real reader configured with
  the same AID/key.
- Emulating a saved credential triggers DESFire authentication and file reads
  instead of fallback application probing.
- `hf 14a info` on Proxmark3 identifies the emulator as the selected EV1, EV2,
  or EV3 generation.
