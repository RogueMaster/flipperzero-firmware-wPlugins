# Host unit tests

Plain-gcc unit tests for FlipDeFlock's **pure-logic** modules — the ones whose
headers advertise "no firmware dependencies, host-testable." They run off-device
so the detection/scoring contracts can't silently regress.

## Run

```sh
make -C test        # build + run
make -C test clean
```

On Windows without `make`, use the MSYS2/MinGW `mingw32-make` (same targets), or
invoke `cc` directly with the flags in the `Makefile`.

Any failed check prints a `FAIL file:line` line and the runner exits non-zero
(so CI gates on it — see `.github/workflows/build.yml`, job `host-tests`).

## Coverage

| Suite | Module | Locks in |
|-------|--------|----------|
| `test_flock_db.c`   | `helpers/flock_db.c`   | Confidence truth table; **B6** strict `^Flock-[0-9A-Fa-f]{6}$` anchoring; OUI-only never confirms; UNVERIFIED user IE-fp cap |
| `test_detect_rules.c` | `helpers/detect_rules.c` | Geotag hysteresis; the **issue #1** alert gate — one alert per device, nothing below "Likely", cooldown |
| `test_flock_store.c` | `helpers/flock_store.c` | **issue #2** hit-record round trip: SSIDs with commas/quotes/control chars; "no fix" stays distinct from a real 0,0; malformed lines rejected, not half-parsed; eviction ordering |

Some helpers pull in `<core/string.h>` (FuriString); `mocks/` provides a minimal
host FuriString, selected ahead of the SDK via `-Imocks`.

## Adding a suite

Add `test_<x>.c` with a `void suite_<x>(void)` using the `CHECK_*` macros in
`test.h`, call it from `test_main.c`, and add it to `TESTS` in the `Makefile`.
Escaping (B12) and NMEA (B10/B23) tests land once R8/R2 extract those pure
functions out of the firmware-coupled `recon_report.c` / `gps_link.c`.
