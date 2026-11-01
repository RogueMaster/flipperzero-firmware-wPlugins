## v1.5

- Templates: save a spool you write again and again (Save, then Template, on
  the write confirm screen or a read result). Templates in the main menu
  lists them; picking one opens the confirm card with a new serial, and after
  writing the app comes back to the list, ready for the next tag. Templates
  are .nfc files in apps_data/cfs_tag_writer_plus/templates; delete them from
  the list's last item.
- New app icon: a spool with a plus, so CFS Tag Writer+ is easy to tell apart
  from the original CFS Tag Writer in the app list.

## v1.4

- Published as its own app, CFS Tag Writer+ (app id cfs_tag_writer_plus), a
  fork of CFS Tag Writer by P. Schattenberg. Versions now use major.minor.
- On first run, saved tags and settings from the original CFS Tag Writer are
  copied over; the original app keeps its own copy and keeps working.

## v1.3.1

- Slower animations (one frame every 180 ms, the working spool turns once
  every ~4 s and winds up over ~6 s).
- The main menu spool no longer turns by itself: it works like a dial and
  turns a little with each move up or down.

## v1.3.0

- The spool moves: it winds up filament while a tag is being written, turns
  while reading and erasing, turns slowly in the main menu, and makes a short
  turn in the color picker after each move (in the direction you scroll).
- The spool hub now has three spokes, which is what shows it turning.

## v1.2.0

- New look: the app's main screens are drawn by the app itself instead of
  plain text lists.
- Main menu: spool logo on the left, list with icons on the right.
- Color picker on one screen: Left/Right switch family (shade families, then
  Basic colors, then Custom hex), Up/Down move through shades, OK picks. The
  spool fills with a dither pattern showing how light or dark the color is.
- Spool card for the write confirm screen and read results: material, brand,
  color name, weight and length, serial.
- Reading, writing and erasing show a spool with animated NFC waves; results
  show a check or a cross. Failed reads, writes and erases offer Retry
  instead of dropping back to the menu.

## v1.1.0

- Color picker by family: pick a hue family (Reds, Greens, Whites & Grays, ...),
  then a named shade, listed light -> dark (240 shades, generated from
  meodai/color-names by scripts/gen_color_shades.py).
- Classic palette moved under "Basic colors"; "Custom hex..." starts from the
  picked shade, so it can be fine-tuned.
- Confirm screen shows the nearest color name next to the hex.

## v1.0.1

- Correct the built-in Generic PET catalog temperature range from 210–230 °C
  to 250–270 °C to match Creality's material database.

## v1.0

- Initial release
