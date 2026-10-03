# Host unit tests

Plain-gcc unit tests for FlipDeFlock's **pure-logic** modules — the ones whose
headers advertise "no firmware dependencies, host-testable." They run off-device
so the detection/scoring contracts can't silently regress.

## Run

```sh
make -C test        # build + run
make -C test clean
```

On this Windows development environment use `mingw32-make -C test`; `make` is not
on PATH. CI uses `make -C test` on Ubuntu.

Any failed check prints a `FAIL file:line` line and the runner exits non-zero
(so CI gates on it — see `.github/workflows/build.yml`, job `host-tests`).
The current runner executes **16 suites and 5,148 checks**.

## Coverage

| Suite | Module | Locks in |
|-------|--------|----------|
| `test_flock_db.c`   | `helpers/flock_db.c`   | Confidence truth table; **B6** strict `^Flock-[0-9A-Fa-f]{6}$` anchoring; OUI-only never confirms; UNVERIFIED user IE-fp cap |
| `test_esp_parser.c` | `helpers/esp_parser.c` | Companion UART grammar, malformed-line rejection, backward compatibility |
| `test_detect_rules.c` | `helpers/detect_rules.c` | Geotag hysteresis; the **issue #1** alert gate — one alert per device, nothing below "Likely", cooldown |
| `test_report_escape.c` | `helpers/report_escape.c` | CSV/KML escaping of hostile radio strings |
| `test_gps_parser.c` | `helpers/gps_parser.c` | NMEA parsing, fix validity, and lock loss |
| `test_gps_rpc_convert.c` | `helpers/gps_rpc_convert.c` | Phone-location conversion and accuracy rejection |
| `test_report_fmt.c` | `helpers/report_fmt.c` | Report formatting and privacy redaction |
| `test_flock_store.c` | `helpers/flock_store.c` | **issue #2** hit-record round trip: SSIDs with commas/quotes/control chars; "no fix" stays distinct from a real 0,0; malformed lines rejected, not half-parsed; eviction ordering |
| `test_flock_ble.c` | `helpers/flock_ble.c` | BLE evidence classification and malformed adverts |
| `test_oui_vendor.c` | `helpers/oui_vendor.c` | Vendor attribution without increasing confidence |
| `test_marauder_scan.c` | `helpers/marauder_scan.c` | Generic-backend parsing and precision gates |
| `test_fast_trig.c` | `helpers/fast_trig.c` | Map trigonometry approximation bounds |
| `test_open_drone_id.c` | `helpers/open_drone_id.c` | Remote ID decoding and hostile payload rejection |
| `test_survey_rank.c` | `helpers/survey_rank.c` | Survey ordering and candidate ranking |
| `test_deflock_url.c` | `helpers/deflock_url.c` | Bounded map URL generation |
| `test_preflight.c` | `helpers/preflight.c` | Fail-closed Health/Preflight state transitions, grace period, and generic-backend limits |

Some helpers pull in `<core/string.h>` (FuriString); `mocks/` provides a minimal
host FuriString, selected ahead of the SDK via `-Imocks`.

## Adding a suite

Add `test_<x>.c` with a `void suite_<x>(void)` using the `CHECK_*` macros in
`test.h`, call it from `test_main.c`, and add the production module and test file
to `MODULES` and `TESTS` in the `Makefile`.
