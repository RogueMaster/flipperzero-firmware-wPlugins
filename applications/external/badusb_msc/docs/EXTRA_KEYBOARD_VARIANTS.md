# Extra keyboard variants

BadUSB MSC bundles additional **real, distinct** keyboard mappings generated from
`xkeyboard-config`/XKB (`xkb-data` 2.42-1, physical keyboard model `pc105`).
These are not arbitrary renamings of the stock layouts, and **they are not
guaranteed to match a similarly named Windows keyboard layout**.

The existing `fr-CA-Legacy.kl` is preserved byte-for-byte. Other countries
do not automatically have a Windows keyboard layout named "Legacy". The exact
variants are listed below under their actual names.

| Map | XKB definition | ASCII characters unsupported by the 256-byte `.kl` format |
| --- | --- | --- |
| `en-US-Colemak.kl` | `us(colemak)` | None (95/95 printable ASCII mapped) |
| `en-US-ColemakDH.kl` | `us(colemak_dh)` | None (95/95 printable ASCII mapped) |
| `en-US-Workman.kl` | `us(workman)` | None (95/95 printable ASCII mapped) |
| `en-US-DvorakLeft.kl` | `us(dvorak-l)` | None (95/95 printable ASCII mapped) |
| `en-UK-Colemak.kl` | `gb(colemak)` | None (95/95 printable ASCII mapped) |
| `en-UK-Dvorak.kl` | `gb(dvorak)` | None (95/95 printable ASCII mapped) |
| `fr-FR-Bepo.kl` | `fr(bepo)` | None (95/95 printable ASCII mapped) |
| `de-DE-Dvorak.kl` | `de(dvorak)` | `backtick` |
| `de-DE-Qwerty.kl` | `de(qwerty)` | `caret`, `backtick` |
| `es-ES-Dvorak.kl` | `es(dvorak)` | None (95/95 printable ASCII mapped) |
| `es-ES-NoDead.kl` | `es(nodeadkeys)` | None (95/95 printable ASCII mapped) |
| `it-IT-Dvorak.kl` | `it(dvorak)` | None (95/95 printable ASCII mapped) |
| `it-IT-NoDead.kl` | `it(nodeadkeys)` | None (95/95 printable ASCII mapped) |
| `pt-PT-Colemak.kl` | `pt(colemak)` | `caret`, `backtick`, `tilde` |
| `pt-BR-Dvorak.kl` | `br(dvorak)` | None (95/95 printable ASCII mapped) |

**Important limitations:** The classic Flipper `.kl` format stores only one
keystroke per ASCII character. It cannot perform a dead key followed by space,
or type arbitrary Unicode; certain variants therefore cannot encode every
printable ASCII character. A character listed as unsupported has mapping 0
(no key), so scripts relying on it will not type that character.

**Verification:** The new maps were checked against `libxkbcommon` by simulating
every encoded ASCII key/modifier combination. This confirms the XKB mapping,
**not compatibility with a Windows PC or a real Flipper**. For Windows, run a
Notepad character test before attempting a PowerShell transfer script.

**Installation:** `.kl` files are embedded into the FAP using
`fap_file_assets="keyboard_layouts"`; app startup copies only **missing** files
into `/ext/badusb/assets/layouts/`. Existing user mappings, the current
selection, and the `drive.img` MSC image are not overwritten.

**Provenance:** XKB symbols from `/usr/share/X11/xkb/symbols/` in xkb-data
2.42-1. The script `tools/generate_xkb_variants.py` documents conversion.
Upstream xkeyboard-config licenses and attribution are retained in
`docs/XKB_DATA_LICENSE.txt`.

**Regenerate on Linux:** `python3 tools/generate_xkb_variants.py` requires
`libxkbcommon.so.0` and xkeyboard-config symbol files. Do not regenerate
on a different XKB release without reviewing possible mapping changes.
