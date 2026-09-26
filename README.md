# DFC

Flipper app for reading and emulating generic MIFARE DESFire-compatible
cards/fobs, using the native DESFire command set (SelectApplication,
Authenticate legacy DES / ISO 3DES / AES, ReadData).

> **Note:** This project is a fork of Eric Betts' excellent
> [seos_compatible](https://github.com/bettse/seos_compatible) Flipper Zero
> application. We thank bettse and other contributors for laying the groundwork
> for NFC credential reading and emulation on the Flipper. See also Eric Betts'
> [Seader](https://github.com/bettse/seader), a related Flipper credential-reading
> project. Seader is credited separately from this app's `seos_compatible` ancestry.

## 🔑 Keys

**The app uses an all-zero DES key at the PICC level by default** (the
standard DESFire factory default). If you know a card's application AID,
ISO AID, key settings, authentication mode, and keys, save them as a
`.dfc` credential file (see `example.dfc`) and place it into
`SD Card/apps_data/dfc/`. A `.dfc` file is compiled to the compact binary
encoding when it is loaded; a pre-compiled `.dfcb` file is also accepted.

- **No saved credential**: the app uses the all-zero DES key at key 0, PICC
  level (AID `00 00 00`) for blank-card emulation.
- **Saved credentials** (`.dfc` files): once a card has been read (or a
  credential authored by hand from known keys), it's saved with its own
  AID, key settings, authentication mode, and per-slot keys/files — see
  `example.dfc`.
- **Blank credentials**: choose **Blank Card** to emulate a writable blank
  DESFire credential. If a reader writes data during emulation, leaving the
  emulation screen prompts to save it as a `.dfc` credential.

## Credential Limitations

`.dfc` credentials must use a 7-byte UID beginning with `04`.

The Flipper build uses the full EV3 core profile, including EV1 and EV2
commands. It includes D40 DES, ISO 3DES, AES and EV2 authentication, ISO 7816
commands, and standard, backup, value, and record files. A credential can hold
at most 4 applications, 16 files across the
card, 3 KiB of file payload in total, 2 KiB in one file, and 512 bytes of key
material. `.dfc` and `.dfcb` files are limited to 16 KiB on the Flipper. Large
text credentials can exceed the available heap during on-device compilation;
compile them to `.dfcb` on the host with `tests/compile_dfcb.py`.

The NFC listener supports a 64 byte ISO-DEP frame, 106 kbit/s, FWI 8 or
greater, CID, and no NAD. The app rejects an ATS that advertises more than the
listener can deliver. EV2 and EV3 hardware command coverage is still being
measured with physical readers.

The reader currently authenticates with key 0 in the primary application and
reads its configured files. Emulation uses only keys embedded in the selected
credential. These limits are build capacities; the hardware cases in
[tests.md](tests.md) still need physical verification.

## Disclaimer

DFC is an independent, third-party implementation of the MIFARE DESFire
native command protocol. It is not developed, authorized, licensed, or
endorsed by NXP Semiconductors. "DESFire" is used here solely to describe
protocol compatibility, not to claim affiliation.

No guarantee of compatibility or functionality is made. This implementation
may not work with all DESFire-enabled systems, and its performance,
security, and reliability are not assured. Users assume all risks
associated with its use.

MIFARE and DESFire are trademarks of NXP B.V. This software is not
associated with, sponsored by, or endorsed by NXP in any way.

## Build and tests

The portable engine is the pinned [CinderSocket dfc-core](https://github.com/cindersocket/dfc-core)
submodule at `lib/core`.
Initialize the dependencies after cloning or pulling:

```sh
git submodule sync --recursive
git submodule update --init --recursive
```

Run the engine unit suites and compile checks for all supported build profiles:

```sh
make -C lib/core/tests test CRYPTO_BACKEND=tiny TINY_CRYPTO_DIR=../../tiny-crypto-c
```

Probe a `.dfcb` credential with the same core feature and capacity settings as
the Flipper build. The default APDUs read the version and application IDs;
additional hex APDUs run in order in one card session:

```sh
python3 tests/probe_dfcb.py path/to/card.dfcb
python3 tests/probe_dfcb.py path/to/card.dfcb 9060000000 90AF000000
python3 tests/compile_dfcb.py path/to/card.dfc path/to/card.dfcb
```

The probe exercises the core's virtual card on a host computer. It reports
unsupported credentials and APDU responses, but does not run the FAP UI, NFC
listener, radio timing, or a physical reader.

Build the Flipper application with an installed `ufbt` SDK:

```sh
ufbt
```

`application.fam` compiles the engine and tiny-crypto-c sources into the app;
`port/` supplies the Flipper platform services. Host tests and host platform
implementations are excluded from the firmware build.

The Flipper build uses the pinned tiny-crypto-c backend. The `.dfc` parser and
writer ship as two embedded plugins. Each loads only for its operation and
unloads before emulation, leaving more RAM for the reader session. The plugins
are packed into the app by `ufbt`.

The firmware linker assigns 192 KiB of SRAM1, shared with the firmware and
loaded apps; its heap is smaller after the firmware starts. The emulator
allocates transaction snapshots and chained-command storage only when needed.
Text loading releases the parser plugin and source before allocating its binary
round-trip copy.
With qFlipper screen streaming active, a four-application EV3 credential with
3 KiB of file data loaded and answered a CCID reader; 4 KiB binary credentials
could not be loaded reliably. Larger capacities need device measurements before
enabling them.


## License boundary

The **Flipper application is AGPL v3** (`AGPL-3.0-only`). It inherits that
license from `bettse/seos_compatible`; inherited code and assets retain their
upstream notices. Original CinderSocket contributions use the same license.
See [LICENSE](LICENSE). Seader is credited above as a related project.

The portable **`lib/core` remains GPL-2.0-or-later** under
[its own license](lib/core/LICENSE). It does not depend on Flipper firmware.
Its GPL v3 option permits combination with the AGPL v3 application without
changing the core's separate license. The tiny-crypto-c library remains
GPL-2.0-or-later under its own license.

Flipper firmware/SDK code is separately licensed under GPL v3, with component
exceptions. Copied or adapted Flipper code and artwork retain their upstream
licenses and attribution. This application's license does not relicense SDK
components, artwork, logos, or trademarks. The inherited artwork in `images/`
comes from the official Flipper Zero firmware repository and remains under its
GPL v3 license.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for component notices and
upstream sources. Distribute the applicable license texts, notices, and
corresponding source for the application and included dependencies with a
binary release, including the exact submodule revisions and build instructions.
