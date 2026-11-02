# Windows Legacy keyboard maps (ASCII-only)

**These are not full language support!** The BadUSB `.kl` format is a fixed
128 x uint16 table indexed by ASCII characters (0..127). It cannot encode
Arabic, Armenian, Georgian, Cyrillic, or Uyghur letters; those mappings are
intentionally unrepresented, rather than being falsely mapped to QWERTY.
Do not use them for general text or PowerShell scripts written in English:
many ASCII letters are not directly typeable under these layouts.

The eight added maps encode only *directly representable ASCII keystrokes*
from Windows keyboard DLL layout shift-state tables. No language-specific
Unicode extension or layout auto-detection is claimed.

| File | Windows keyboard | Windows layout file | ASCII coverage |
| --- | --- | --- | --- |
| `ro-RO-Legacy.kl` | Romanian (Legacy) | `KBDRO.DLL` | 92/95 printable ASCII |
| `hsb-Legacy.kl` | Sorbian Standard (Legacy) | `KBDSORST.DLL` | 91/95 printable ASCII |
| `ar-SA-101-Legacy.kl` | Arabic (101, Legacy) | `KBDA4.DLL` | 39/95 printable ASCII |
| `hy-AM-Eastern-Legacy.kl` | Armenian Eastern (Legacy) | `KBDARME.DLL` | 12/95 printable ASCII |
| `hy-AM-Western-Legacy.kl` | Armenian Western (Legacy) | `KBDARMW.DLL` | 12/95 printable ASCII |
| `ka-GE-Legacy.kl` | Georgian (Legacy) | `KBDGEO.DLL` | 24/95 printable ASCII |
| `tt-Cyrl-Legacy.kl` | Tatar (Legacy) | `KBDTAT.DLL` | 38/95 printable ASCII |
| `ug-Arab-Legacy.kl` | Uyghur (Legacy) | `KBDUGHR.DLL` | 37/95 printable ASCII |

Source tables and layout metadata:
- https://learn.microsoft.com/en-us/globalization/windows-keyboard-layouts
- https://kbdlayout.info/KBDRO/shiftstates
- https://kbdlayout.info/KBDSORST/shiftstates
- https://kbdlayout.info/KBDA4/shiftstates
- https://kbdlayout.info/KBDARME/shiftstates
- https://kbdlayout.info/KBDARMW/shiftstates
- https://kbdlayout.info/KBDGEO/shiftstates
- https://kbdlayout.info/KBDTAT/shiftstates
- https://kbdlayout.info/KBDUGHR/shiftstates

**Validation:** byte size, modifier/HID usage bounds, all existing files
unchanged, bundle asset packaging, and ZIP integrity were checked statically.
Windows and Flipper hardware behavior has NOT been tested.

**Installation:** The existing `fap_file_assets="keyboard_layouts"` packaging
and first-launch copy routine automatically add any missing `.kl` files to
`/ext/badusb/assets/layouts/` and never overwrite user-customized maps.
They will only appear after compiling and installing the updated FAP.

**Windows caveat:** The keys interpreted by Windows follow the current active
Windows keyboard input layout. The Flipper USB connection cannot detect it.
A `.kl` file cannot send an ASCII character that lacks a direct key and cannot
automatically switch Windows from a non-Latin script to English.

**Preserved:** `fr-CA-Legacy.kl`, the previous 47 maps, the app menus, HID/MSC,
USB/BLE, custom mappings, all existing `drive.img` behavior and user data.
