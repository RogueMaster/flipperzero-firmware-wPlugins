# CFS Tag Writer

Read, write, clone, and erase CFS filament spool NFC tags on the Flipper.

CFS filament spools identify themselves with a pair of MIFARE Classic 1K NFC
tags that store the brand, material, color, weight/length, and serial in an
encrypted format. CFS Tag Writer decodes those tags and lets you write them
back — clone a spool as-is, edit its fields, build one from scratch, or wipe
a tag blank.

The physical tags are standard MIFARE Classic 1K, so you don't need a genuine
Creality spool tag — writing from scratch or cloning works onto any writable
MIFARE Classic 1K tag, and a blank/generic 1K tag works as the medium.

Compatible with Creality CFS spool tags and any MIFARE Classic 1K tag.
"Creality" is used only to describe tag compatibility and the spool brand; this
app is not affiliated with or endorsed by Creality.

## Features

- **Read** a tag and view brand, material, color, weight, length, serial, and
  UID, plus a Raw / Details view showing the decoded 48-byte payload as ASCII
  and hex. A successful read plays a confirmation sound.
- **Write from scratch** with a weight-driven wizard (brand, material, color,
  weight; the weight sets the encoded length). Materials come from the
  built-in filament catalog, filtered by the selected brand.
- **Clone / Edit & Write** from a read or saved tag — copy as-is
  (byte-for-byte), or tweak first.
- **x2 dual-tag writing** — write both of a spool's tags, with a swap prompt
  in between.
- **Saved Tags** — tags are saved as standard Flipper MIFARE Classic .nfc
  files; open them through the file browser (browse anywhere, including files
  from the stock NFC app), then re-write, edit, or delete.
- **Erase** a tag back to blank.
- **Settings** — default brand, serial mode, editable weight presets, and a
  crypto/data self-test.

## Safety

Only clone or write tags for spools you own. This app is for managing your
own filament.
