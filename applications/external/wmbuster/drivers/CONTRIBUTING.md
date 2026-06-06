# Contributing a meter driver

Thanks for helping make wM-Buster recognise more meters. The
guide below is the same checklist a maintainer follows when
reviewing a PR — if your change ticks every box, the merge is
usually mechanical.

## TL;DR

1. **Capture a telegram** with the meter you want to support.
2. **Identify the (M, V, T) triple** — manufacturer 3-letter
   code, version byte, medium byte from the wM-Bus link header.
3. **Drop a new C file** under
   `drivers/europe/<manufacturer>/<product>.c`.
4. **Register it** in `drivers/engine/registry.c` (one extern,
   one array entry).
5. **Build, test, document**.
6. **Open a PR** with the captured frame in the description.

## 1. Capture a telegram

Run wM-Buster, open _Settings → Logging_, and let the meter send
at least one frame. The SD logger writes `wmbus_<date>.log`
where each line contains the raw 868 MHz bytes. Pick one frame
that decoded cleanly (no CRC error in the log) and paste its
hex into your PR description — reviewers will replay it offline.

## 2. Identify the manufacturer / version / medium

For each frame the engine prints something like:

```
Hdr  KAM 12345678 V19 T04
```

That's M = `KAM`, V = `0x19`, T = `0x04`. If you don't have a
frame yet, look the meter up in the wmbusmeters reference
(`_refs/wmbusmeters/drivers/src/<name>.xmq`). The `mvt =` lines
there use the same `manufacturer, version, medium` order.

## 3. Author the driver file

The minimum is ~25 lines. Pattern:

```c
/* <Vendor> <Product> — short description.
 * wmbusmeters reference: drivers/src/<name>.xmq           */

#include "../../engine/driver.h"

static const WmbusMVT k_mvt[] = {
    {"SEN", 0x68, 0x06},
    {"SEN", 0x68, 0x07},
    {"SEN", 0x7C, 0x07},
    { {0}, 0, 0 }                /* terminator */
};

const WmbusDriver wmbus_drv_sensus_iperl = {
    .id     = "sensus-iperl",    /* short kebab id, used in logs */
    .title  = "Sensus iPERL water",
    .mvt    = k_mvt,
    .decode = NULL,              /* NULL → use generic OMS walker */
};
```

### When do you need a custom `decode()`?

Only when the payload is **not** OMS DIF/VIF-conformant. Examples
in this repo:

- Techem compact frames (`drivers/europe/techem/_compact.c`) —
  fixed offsets, custom date packing.
- Qundis / ista / Brunata HCA (`qundis/hca_compact.c`) — custom
  12-byte preamble before the OMS payload.
- Diehl IZAR (`diehl/diehl.c`) — PRIOS-encrypted, currently
  rendered as a labelled hex dump until somebody ports the
  cipher.

If you do write a custom decoder, follow these rules:

- First line of the output is the **title** (rendered as the
  detail-view subtitle).
- Each subsequent line is `Label  Value` where the label is
  ≤ 11 characters and the value is ≤ 19 characters. The detail
  canvas truncates anything longer.
- Cite the byte layout you decoded from. wmbusmeters drivers
  under `_refs/wmbusmeters/src/driver_*.cc` are the canonical
  source — link by file name and line number.

## 4. Register the driver

Edit `drivers/engine/registry.c`:

```c
extern const WmbusDriver wmbus_drv_sensus_iperl;
...
const WmbusDriver* const wmbus_engine_registry[] = {
    ...
    &wmbus_drv_sensus_iperl,
    ...
};
```

Order matters — specific MVT triples must come before brand-wide
wildcard entries (`{"SEN", 0xFF, 0x07}`) so the specialised
driver wins on a tie.

## 5. Build and verify

```sh
ufbt
ufbt launch          # if your Flipper is connected
```

Then either capture a fresh frame or replay your captured frame.
Confirm:

- The meter appears in the list with the expected title.
- The detail screen shows numeric fields, not just hex rows.
- The first column in the meter list is the manufacturer code,
  the second is your `title`.

## 6. Documentation

If your manufacturer folder doesn't have a `README.md` yet,
add one — three lines is enough:

```md
# <Vendor>

Sources of MVT triples: wmbusmeters drivers/src/<name>.xmq.

Known limitations:
- Variant <X> ships frames with a non-standard CI byte; not yet
  supported.
```

Add the model to the list in `README.md` at the project root if
the meter is a brand-new family.

## 7. Open the PR

Title format: `drivers/<manufacturer>: add <product>`.

Body checklist:

- [ ] Captured frame (hex, ≤ 80 chars/line) included
- [ ] (M, V, T) triple from the captured frame
- [ ] wmbusmeters reference file (when applicable)
- [ ] Build is clean (`ufbt` exits 0)
- [ ] Tested on hardware OR replayed from log offline

That's it. Reviewers care about correctness over cleverness:
two extra MVT rows are a much smaller cost than one mis-decoded
field on a real meter.
