# Dungeons & Dolphins 4.19.1 audit status

Audit date: 2026-10-04 (America/New_York).

The corrected suite passes the available source and host gates. All eleven manifests use numeric FAP version `(4, 19)` and the common `4.19.1` release label. See [INTEGRATION_AUDIT.md](INTEGRATION_AUDIT.md) for findings, evidence and application instructions.

## Completed

- Removed the redundant editor-release prototypes that failed in the three shared-core FAP builds.
- Corrected null runtime access in common input, profile lifecycle, Language/Proficiency first-use and Settings paths; preserved Catalog return state before teardown.
- Restored Favorite Spells index initialization and casting return ownership; retained Combat Roll state through casting statistics.
- Saved selected Feats before handing dependent review to DNDGrants. The Hub's linked entry root excludes grant staging/review and Combat workflow roots.
- Removed the inactive `.inc` and three legacy wrappers; updated every test/audit/layout consumer to the compiled core and actual FAP modes.
- Passed strict `-Wall -Wextra -Werror -Wstrict-prototypes -Wredundant-decls` compilation at `-O1` and `-Os`, eleven manifest links, eleven balanced startup/teardown smoke tests and fourteen ASan/UBSan regression executables.
- Regenerated the 32-bit common app state: **3,416 B**, versus the prior **4,676 B**. Optional runtimes retain the documented **48/64/76/24/248/308 B** Catalog/cache/Roll/Grant Review/Combat/profile measurements.
- Regenerated the same-compiler entry-rooted Hub host proxy: **122,588 B text+rodata**, **2,536 B** below the uploaded core's **125,124 B** proxy. This is not an ARM target-size claim.
- Retained all catalog assets, persisted schemas, eleven FAPs and current stack reservations. No `dist/` or generated firmware binary is packaged.

## Remaining target gates

A current RogueMaster/ARM build and physical-device checks remain unverified here. The environment lacks the ARM compiler/matching SDK, uFBT and clang-format. Historical `tests/sdk` output is preserved as historical evidence only. Device checks remain in [DEVICE_TEST_MATRIX.md](DEVICE_TEST_MATRIX.md).
