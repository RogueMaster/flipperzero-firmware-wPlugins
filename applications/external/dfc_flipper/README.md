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

The app stores bounded DESFire file payloads suitable for Flipper NFC
emulation and uses only keys embedded in the selected credential when reading
or emulating saved credentials.

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
submodule at `lib/core`, using `git@github.com:cindersocket/dfc-core.git`.
Initialize the dependencies after cloning or pulling:

```sh
git submodule sync --recursive
git submodule update --init --recursive
```

The SSH submodule URLs require GitHub SSH access. For an HTTPS-only checkout,
use Git's per-command URL rewrite when initializing the dependencies:

```sh
git -c 'url.https://github.com/.insteadOf=git@github.com:' \
  submodule update --init --recursive
```

Run the engine unit suites and compile checks for all supported build profiles:

```sh
make -C lib/core/tests test TINY_AES_DIR=../../tiny-AES-c TINY_DES_DIR=../../tiny-DES-c
```

The explicit paths also work on case-sensitive systems. Build the Flipper
application with an installed `ufbt` SDK:

```sh
ufbt
```

`application.fam` compiles the engine and tiny crypto sources into the app;
`port/` supplies the Flipper platform services. Host tests and host platform
implementations are excluded from the firmware build.

The Flipper build uses the full EV1 profile and the pinned tiny AES/DES
backends. It retains D40, ISO 3DES and AES authentication, ISO 7816 commands,
2K/4K/8K storage profiles, and standard, backup, value and record files.


## License boundary

The **Flipper application is AGPL v3** (`AGPL-3.0-only`). It inherits that
license from `bettse/seos_compatible`; inherited code and assets retain their
upstream notices. Original CinderSocket contributions use the same license.
See [LICENSE](LICENSE). Seader is credited above as a related project.

The portable **`lib/core` remains GPL-2.0-or-later** under
[its own license](lib/core/LICENSE). It does not depend on Flipper firmware.
Its GPL v3 option permits combination with the AGPL v3 application without
changing the core's separate license. The two tiny crypto libraries retain
the Unlicense.

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
