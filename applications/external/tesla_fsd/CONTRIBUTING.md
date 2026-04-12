# Contributing

## How to test before opening a PR

This project writes to a real vehicle CAN bus. The cost of an untested
patch landing in main is "the next user's car does something unexpected on
the road". Please be conservative.

### Step 1 — Build it

```bash
git clone --recursive https://github.com/flipperdevices/flipperzero-firmware.git
cd flipperzero-firmware
git clone https://github.com/hypery11/flipper-tesla-fsd.git applications_user/tesla_fsd
./fbt fap_tesla_fsd
```

The output `.fap` lands in `dist/<arch>-<api>/apps/GPIO/tesla_fsd.fap`.

### Step 2 — Test in Listen-Only mode first

Since v2.4 the app boots in **Listen-Only** mode. The MCP2515 is put into
its hardware listen-only register, which is **physically incapable of TX
even on bus error frames**. This is the right starting point for any new
behaviour.

1. Plug the Flipper into the car
2. Open the app, leave Mode = Listen
3. Watch the RX counter — if it stays at 0 after 5 seconds, your wiring is
   wrong; the app will display a "No CAN traffic" warning
4. Confirm BMS dashboard reads sensible values (SoC, voltage, current)
5. Switch to **Service** mode if you want to test TX paths in a controlled
   way; switch to **Active** for normal operation

If you're adding a new TX feature, the PR description should say what you
verified in Listen-Only first (what frames you saw, what your handler
would have written).

### Step 3 — On-vehicle confirmation

For any change that modifies what we write to CAN, we need at least one
on-vehicle confirmation before merging. State in the PR:

- Tesla model + year
- HW version (HW3 / HW4 / Legacy)
- Firmware version (Settings → Software)
- Region (this matters for which UI features are exposed)
- What you changed
- What you observed (UI behaviour, dashboard messages, error counters)

If you don't have a Tesla yourself, that's fine — open the PR and tag it
`needs-on-car-test`. Other contributors will pick it up.

## Code style

The C side is plain C99, no C++ features, no allocations on the worker
thread. Match the style of the surrounding file. Concretely:

- 4-space indent, no tabs
- snake_case for functions and locals, PascalCase for types and enum values
- Mutex discipline: lock the app mutex when reading or writing `app->fsd_state`,
  unlock immediately, work on a stack copy in the worker
- Bounds-check every byte access against `frame->data_lenght` (yes,
  `data_lenght`, that's the upstream MCP2515 lib spelling — don't fix it)
- Add new CAN ID `#define`s to `fsd_logic/fsd_handler.h` with the decimal
  value and a one-line comment

## Branching

- `main` — only release-tagged code, never broken
- `feat/*` — feature branches, merged via PR with at least one review
- `fix/*` — bug fixes, smaller turnaround OK

Don't push directly to `main`.

## What to avoid

- AI-generated commit messages, AI-generated PR bodies, AI-style README
  prose. We get filtered out by readers if it reads like ChatGPT slop.
  Write what you actually did, in your own voice, in whatever language
  you're comfortable with — Chinese, English, German, Korean, all welcome
  in PR conversations.
- Adding features that need a feature flag "for safety" — if it's not
  safe enough to default on, the design isn't ready
- Touching brakes, steering, or powertrain CAN IDs without a long
  conversation in an issue first
- Bumping the version in your PR — the maintainer does that at release
  time

## What we love

- Real on-car test reports with firmware version, HW, region, and what
  worked / didn't
- New hardware variants (other CAN modules, other MCUs) — just add a
  `HARDWARE.md` row
- Bug fixes with a one-sentence reproduction
- Translations of `README.md` into more languages
- CAN frame templates from public Tesla CAN research, with the source
  cited (opendbc, mikegapinski, talas9, tuncasoftbildik, etc.)

## Code of conduct

Be civil. We have users from a lot of countries who don't share a first
language. Don't pile on, don't gatekeep, don't reply in a way you wouldn't
say to someone in person.
