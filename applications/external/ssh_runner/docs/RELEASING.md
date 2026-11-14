# Releasing

[← Documentation index](README.md)

How a release of SSH Runner is cut. One version covers the Flipper app, the ESP32 firmware and
the `sshrunner` PC tool; GitHub Actions builds and publishes the files from the version tag.

## Contents

- [Steps](#steps)
- [Release Files](#release-files)
- [Factory and App Images](#factory-and-app-images)
- [Verifying a Release](#verifying-a-release)
- [Dry Run](#dry-run)
- [Flipper Apps Catalog](#flipper-apps-catalog)

## Steps

1. **Checks:** `tools/check.sh` (or `tools\check.ps1`) passes on the commit to release.
2. **Version:** set the same version in the four places `tools/release_assets.py check X.Y.Z`
   compares:
   - `CHANGELOG.md`: the `## [Unreleased]` changes move under `## [X.Y.Z] - YYYY-MM-DD`, with
     the compare links at the bottom updated;
   - `firmware-esp32/CMakeLists.txt`: `set(PROJECT_VER "X.Y.Z")` (the board reports it);
   - `flipper-app/application.fam`: `fap_version="X.Y"`;
   - `flipper-app/ssh_runner_i.h`: `#define SSH_RUNNER_VERSION "X.Y"`.

   The Flipper app carries the major.minor only. Run the checks again if anything changed.
3. **Merge** the version change into `main` through a pull request.
4. **Tag:** on the merged commit of `main`, push the tag `vX.Y.Z`:

   ```sh
   git tag -a vX.Y.Z -m "SSH Runner vX.Y.Z" && git push origin vX.Y.Z
   ```

   The [release workflow](../.github/workflows/release.yml) checks the version, builds both
   Flipper app builds, the firmware for each board and the PC tool, collects the files with
   `tools/release_assets.py`, attests their provenance and creates the release
   "SSH Runner vX.Y.Z", with the version's CHANGELOG section and the install steps as notes.

## Release Files

| File | What |
|---|---|
| `ssh_runner-official.fap`, `ssh_runner-unleashed.fap` | the Flipper app: the official build runs on both firmwares, the Unleashed build is optional |
| `flipper-ssh-runner-esp32-waveshare-vX.Y.Z.zip` | the firmware for the Waveshare ESP32-S3-ETH |
| `flipper-ssh-runner-esp32-addon-vX.Y.Z.zip` | the firmware for the SSH Runner add-on board, rev 0.1 and later |
| `sshrunner_X.Y.Z_<os>_<arch>.tar.gz` / `.zip` | the PC tool for Linux, macOS (`.tar.gz`) and Windows (`.zip`), amd64 and arm64 |
| `sshrunner_X.Y.Z_<arch>.deb` / `.rpm` | the PC tool as Linux packages (`/usr/bin/sshrunner`) |
| `SHA256SUMS` | the one checksum file, over every file above |

Each firmware zip holds (`<board>` is `waveshare` or `addon`):

```text
README.txt                                     which image to flash when, with the commands
flipper-ssh-runner-<board>-vX.Y.Z-factory.bin  bootloader, partition table and app from 0x0
flipper-ssh-runner-<board>-vX.Y.Z-app.bin      the app alone, for 0x20000
licenses/                                      LICENSE, COPYING.libssh, BSD.libssh,
                                               LICENSE.{esp-idf,lwip,freertos,mbedtls},
                                               THIRD_PARTY_NOTICES.txt
```

Each PC tool archive holds `sshrunner` (`sshrunner.exe`), `LICENSE`, a short `README.md` and
`licenses/THIRD_PARTY_NOTICES.txt` (the license of every linked Go module, written by
`cli/scripts/archive_files.sh`).

`tools/release_assets.py` refuses a firmware build with any eFuse-burning option (flash
encryption, secure boot, NVS encryption), a build for the wrong board, a factory image that is
not the bootloader through the app, and a version that differs in any of the four places.

## Factory and App Images

The partition table puts the settings (`nvs`, WiFi credentials and trusted host keys) between
the partition table and the app:

| Offset | Partition |
|---|---|
| `0x0` | bootloader |
| `0x8000` | partition table |
| `0x9000` | `nvs` (settings) |
| `0x20000` | the app |

The factory image is the three parts merged from `0x0` (`esptool.py merge_bin @flash_args` on
the build, the same format ESP Web Tools and ESPHome use), so it covers `nvs` and erases the
board's settings: it is for a first install, after an erase. An update writes only the app
image at `0x20000` and keeps the settings. The partition layout must keep the app at
`0x20000`; `tools/release_assets.py` checks the build's `flash_args` for it.

## Verifying a Release

```sh
sha256sum -c SHA256SUMS
gh attestation verify <file> --repo SeanChangX/flipper-ssh-runner
```

The attestation covers every file listed in `SHA256SUMS`.

## Dry Run

The workflow builds the same files without publishing them when it runs by hand (Actions >
release > Run workflow, with an optional version) or on a pull request that changes
`CHANGELOG.md`, `.github/workflows/release.yml`, `tools/release_assets.py`,
`cli/.goreleaser.yaml` or `cli/scripts/archive_files.sh`. The files and the notes are kept as
the run's artifact `release-X.Y.Z`.

Every relative link in the notes becomes the file at the tag (`blob/vX.Y.Z/...`). A link to a
file that is not in the repository, or one that would not open on the release page (an
anchor-only link, plain `http://`), stops the build before anything is published;
`tools/tests/test_release_assets.py` renders every CHANGELOG section the same way, so such a
link also fails the checks of the pull request that adds it.

## Flipper Apps Catalog

The app is listed in the [Flipper Apps Catalog](https://github.com/flipperdevices/flipper-application-catalog)
from this repository's source, at the commit its manifest names.

1. In this repository, raise `fap_version` in `flipper-app/application.fam` (done with the
   version above) and add a `vX.Y:` section at the top of `flipper-app/changelog.md`, the
   catalog's changelog of the app; merge into `main`.
2. In the fork of the catalog, create the branch `SeanChangX/ssh_runner_<version>` and change
   only `commit_sha` in `applications/GPIO/ssh_runner/manifest.yml` to the merged commit of
   `main`.
3. Open the pull request against the catalog; its checks build the app from that commit.
