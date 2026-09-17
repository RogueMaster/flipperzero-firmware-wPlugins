# Fixtures

Raw serial captures from a real Scout Lite board running ESP32 Marauder, recorded during T0.3.
Each capture is stored twice with identical content: `.txt` for reading in a diff, `.bin` for the
tests to load byte for byte. `test_parse_marauder.c` asserts the exact size of every file it
loads, so **do not reformat, re-wrap, or normalise line endings** — the mixed CRLF/LF in
`wardrive_no_fix` is part of what is being tested.

| File | Bytes | What it captures |
|---|---|---|
| `wardrive_no_fix` | 6373 | A `wardrive` session with no GPS fix: Wi-Fi rows, BLE rows, and interleaved firmware error lines |
| `startup_info` | 230 | The `#info` banner: firmware, version, hardware, ESP-IDF, MACs, SD card |
| `gpsdata` | 479 | One `gpsdata` block |
| `stop_sequence` | 104 | A `stopscan` and the lines it tears through |

## Anonymisation

`wardrive_no_fix` and `startup_info` are **anonymised**. The original capture contained 62 real
BSSIDs plus the SSIDs and BLE device names of third parties near the recording location, and
BSSIDs are geolocatable through public wardriving databases. Publishing them would expose people
who never agreed to be recorded.

What was changed, and what deliberately was not:

- **MAC addresses** went through a fixed bijective permutation of the 16 hex digits. Being a
  bijection, it preserves the properties the tests and the parser actually depend on: every MAC
  stays unique, MACs that were equal stay equal (a BLE row with no name repeats its own MAC as
  the name), and virtual-AP pairs that differed by one nibble still differ by one nibble.
  The case convention is preserved per field — uppercase for Wi-Fi, lowercase for BLE — because
  that is what the firmware emits.
- **SSIDs and BLE names** were replaced one for one, each new name having exactly the same UTF-8
  byte length as the one it replaced. Names that embedded part of their own MAC still do.
- **Nothing else moved.** Byte counts, line counts, line endings, field counts, channels, RSSI
  values and firmware messages (including the `Maurauder` typo the firmware really prints) are
  untouched. Every coverage number the test suite prints is bit-identical to the run against the
  original capture.
- The Chinese SSIDs were replaced with **accented Latin, not ASCII**. This is the only fixture in
  the repository containing non-ASCII bytes, so it is the only real non-ASCII input the parser is
  tested against; flattening it to ASCII would have silently deleted that coverage.
