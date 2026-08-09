# Contributors

FlipDeFlock is maintained by [@ReconGrunt](https://github.com/ReconGrunt).

The people below made it better. Bug reports and field data count as much as code
here — arguably more. This project can only be verified against hardware and
radio environments the maintainer does not have, so a careful report from someone
who *does* have that board is often the only way a bug gets found at all.

## Reports and findings

- **[@h00die](https://github.com/h00die)** — found that the companion firmware no
  longer compiled on Arduino ESP32 core 3.x
  ([#4](https://github.com/ReconGrunt/FlipDeFlock/issues/4)). CI pinned core 2.0.17
  and only built on release tags, so the project had been shipping a companion
  that would not build for anyone with a current Arduino install, with every check
  green. Also caught the companion README pointing at build files that were not
  release downloads. Prompted the core-3.x compatibility work and the ESP32-C5
  dual-band target in v0.48.

## Code

- **[@nickk02](https://github.com/nickk02)** — release and build engineering across
  v0.70. Found that the project shipped a single `.fap` built against API 87.1 while
  Unleashed and RogueMaster are on 88.2, so those users were being told the app was
  out of date when it was the API that had moved; releases now carry a build per
  firmware family. Also caught that the checksum job hashed the release before both
  companion images had landed, meaning a `SHA256SUMS.txt` could look complete while
  omitting the C5 build, and that the API drift check had been comparing a status
  column to a version number and so could never have reported real drift. Bounded the
  label formatting so the app builds on SDKs with `-Wformat-truncation`, pinned every
  action to a commit SHA, and made a release rehearsable before the tag is pushed.

## Upstream research this project builds on

Detection signatures and methods come from open counter-surveillance work.
Credited in the source next to the data they contributed:

- [`colonelpanichacks/flock-you`](https://github.com/colonelpanichacks/flock-you)
- [`0xXyc/flock-you-wifi-recon`](https://github.com/0xXyc/flock-you-wifi-recon)
- [`nitekry/nite-oui-collection`](https://github.com/nitekry/nite-oui-collection) —
  the curated per-prefix OUI table with confidence/status columns
- [`JakeSwiz/WatchFlock`](https://github.com/JakeSwiz/WatchFlock) — the
  hidden-SSID/probe-request finding and the SoundThinking prefix
- [ryanohoro](https://github.com/ryanohoro)'s Falcon teardown — the BLE
  external-battery advert layout
- The [DeFlock](https://deflock.me) community

## Contributing

See [CONTRIBUTING.md](.github/CONTRIBUTING.md). Note that **data-only contributions** —
field reports, new OUIs, SSID/BLE patterns, probe fingerprints,
board-compatibility reports, bug reports, test cases — carry no copyright
entanglement, so they need no sign-off and are the easiest way to help. Code
contributions require the DCO sign-off and licensing grant described there.
