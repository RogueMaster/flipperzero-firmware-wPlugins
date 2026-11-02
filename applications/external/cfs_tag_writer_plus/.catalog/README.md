# CFS Tag Writer+

Read, write, clone, and erase CFS filament spool NFC tags on the Flipper, with
redrawn screens and a color picker of 240 named shades.

CFS Tag Writer+ is a fork of CFS Tag Writer by P. Schattenberg (MIT). It keeps
everything the original does and adds:

- **Redrawn screens** — a main menu with icons and a spool logo, a spool card
  for read results and the write confirm screen, and animated read / write /
  erase screens: the spool winds up filament while a tag is being written.
- **One-screen color picker** — Left/Right switch the color family (Whites &
  Grays, Reds, ... Browns & Beiges, then Basic colors and Custom hex), Up/Down
  move through its shades from light to dark, OK picks. The spool fills with a
  dot pattern that shows how light or dark the shade is.
- **Templates** — save a spool you write again and again (e.g. Generic PLA,
  Black, 1 kg); pick it from Templates in the main menu and write it with a
  fresh serial in a couple of presses, then go straight on to the next tag.
- **Retry** after a failed read, write, or erase, without starting over.
- **Your saved tags come along** — on first run, saved tags and settings from
  the original CFS Tag Writer are copied over (the original keeps working).

CFS filament spools identify themselves with a pair of MIFARE Classic 1K NFC
tags that store the brand, material, color, weight/length, and serial in an
encrypted format. The app decodes those tags and lets you write them back —
clone a spool as-is, edit its fields, build one from scratch, or wipe a tag
blank. Writing works onto any writable MIFARE Classic 1K tag, so a blank or
generic 1K tag works as the medium.

Compatible with Creality CFS spool tags and any MIFARE Classic 1K tag.
"Creality" is used only to describe tag compatibility and the spool brand; this
app is not affiliated with or endorsed by Creality.

## Features

- **Read** a tag and view brand, material, color, weight, length, and serial,
  plus a Raw / Details view with the UID and the decoded 48-byte payload as
  ASCII and hex.
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
