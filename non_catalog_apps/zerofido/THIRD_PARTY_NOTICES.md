# Third-Party Notices

This project is licensed under the GNU General Public License, version 3 or later
(`GPL-3.0-or-later`). This file records the third-party license information checked before
declaring the project license.

This is not a complete legal review. Before public release, keep this file in sync with
`pyproject.toml`, `uv.lock`, the Flipper firmware target, generated artifacts, and any copied
or vendored source.

## Runtime and Build Dependencies

| Component | Observed license metadata | Notes |
| --- | --- | --- |
| Flipper Zero firmware reference | GPLv3 | Checked against the Flipper Zero firmware license for the target SDK. |
| uFBT (`ufbt`) | GPL-3.0 / GPLv3+ classifier | Build tool dependency from `pyproject.toml` and `uv.lock`. |
| cbor2 | MIT | Python host-tool dependency. |
| cryptography | Apache-2.0 OR BSD-3-Clause | Python host-tool dependency. |
| hidapi | BSD and GPLv3 classifiers | Python host-tool dependency. |
| pyserial | BSD | Python host-tool dependency. |
| cffi | MIT | Transitive Python dependency of `cryptography`. |
| pycparser | BSD-3-Clause | Transitive Python dependency of `cffi`. |
| oslex | MIT classifier | Transitive Python dependency of `ufbt`. |
| mslex | Apache-2.0 | Transitive Python dependency of `oslex`. |

## Vendored Source

| Component | Observed license metadata | Notes |
| --- | --- | --- |
| micro-ecc | BSD-2-Clause | Vendored under `src/crypto/micro_ecc/` as the fixed P-256 backend. Source fetched from `github.com/kmackay/micro-ecc`; local build enables only `secp256r1`. |

## Native Test Headers

The headers under `tests/native/include/` are native-regression shims that mirror the shape of
Flipper firmware and mbedTLS APIs closely enough for host-side tests. Their provenance and SPDX
policy should still be reviewed before publication.

## Trademarks

ZeroFIDO is not affiliated with, endorsed by, or certified by Flipper Devices, the FIDO Alliance,
browser vendors, or any other named third-party project unless explicitly stated in release
materials.
