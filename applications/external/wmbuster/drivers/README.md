# Driver layout

This folder is the meter-decoding layer of wM-Buster. It is
deliberately small and data-driven so contributors can add a new
meter by writing one C file and editing one list.

```
drivers/
├── engine/
│   ├── driver.h        public WmbusDriver descriptor + dispatch API
│   ├── driver.c        registry walker, OMS-walker fallback
│   └── registry.c      single source of truth: every supported driver
└── europe/
    ├── techem/         FHKV HCA, MK-Radio water, Compact V / Vario heat
    ├── qundis/         Qundis Q-Caloric / ista / Brunata HCA + Q-Water/Heat
    ├── diehl/          Hydrus, Sharky, Aerius, IZAR (PRIOS stub)
    ├── kamstrup/       MULTICAL, flowIQ, OmniPower, Kampress
    ├── sensus/         iPERL, PolluCom F, Spanner-Pollux
    ├── itron/          BM+M, Cyble, heat, gas
    ├── apator/         amiplus, 162/172, 08, ELF, OP-041A, Ultrimis, …
    ├── engelmann/      Sensostar, Waterstar, HCA-E2, DWZ Waterstar M
    ├── zenner/         Minomess, C5-ISF, CalToSe HCA
    ├── bmeters/        HYDRODIGIT, IWM-TX5, Hydrocal, Hydroclima, RFM-AMB
    ├── elster/         gas / water / heat
    ├── sontex/         868 HCA, Supercal, Supercom 587
    ├── aquametro/      Topas ESKR, CALEC heat
    ├── maddalena/      water, Microclima, EVO868
    ├── landis_gyr/     Ultraheat, L+S water, Q-Heat 5
    ├── electricity/    ABB, EBZ, EMH, EasyMeter, Gavazzi, Schneider, IME, …
    ├── sensors/        Lansen, Weptech Munia, ELV/eQ-3, EI smoke
    └── misc/           long-tail vendors with one product family
```

When wM-Buster expands beyond Europe, sibling folders
(`drivers/north_america/`, `drivers/asia/`) follow the same shape.
The engine itself has zero geographic assumptions.

## What does the engine do?

`drivers/engine/driver.{h,c}` exposes three things:

- `wmbus_engine_find(M, V, T)` — returns the first driver whose
  MVT list matches the link-header triple, or a sentinel
  "unknown OMS" driver when nothing claims it.
- `wmbus_engine_decode(M, V, T, apdu, len, out, cap)` — calls
  the driver's custom `decode()` if present, else renders the
  driver's `title` followed by the standard OMS DIF/VIF walk.
- `wmbus_engine_model_name(M, V, T)` — friendly string used by
  the meter-list subtitle and the detail header. **The same
  data backs the dispatcher** so the list and the decoder can
  never drift apart.

There is exactly one curated array (`wmbus_engine_registry[]` in
`engine/registry.c`). Order matters: the first matching MVT row
wins, so specific drivers must precede brand-wide fallbacks.

## How to add a new meter

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the full PR
walkthrough. The short version:

1. Find the meter's manufacturer code (3 letters) and the
   `(version, medium)` bytes from a captured frame, or copy them
   from `_refs/wmbusmeters/drivers/src/<name>.xmq`.
2. Drop a new C file under the right vendor folder (or create a
   new vendor folder). Define a `WmbusMVT[]` list and a
   `WmbusDriver` global. If the payload is OMS-conformant set
   `decode = NULL`; the engine handles the rest.
3. Add one `extern` line and one array entry to
   `engine/registry.c`.
4. Build with `ufbt`, capture a telegram, confirm the meter
   shows up with the expected title and rows.
