# Flipper Space Calculators

A pair of minimalist space and physics calculators for the Flipper Zero. No games, no splash screens -- just the numbers.

## Applications

### [Space Travel Calculator](./space-travel-calculator/)

Interplanetary trajectory planner with real-time orbital visualization. Select a destination (Mars, Venus, Jupiter, Europa, Titan, and more), scroll through launch dates, and see Hohmann transfer orbits drawn on screen. Outputs delta-v requirements, flight time, and phase angles.

### [Dilate](./dilate/)

Time dilation calculator for special relativity. Set a velocity (fraction of c) and a duration, and see how much time passes for you versus an observer on Earth. Auto-scales units from seconds to years.

## Installation

### Pre-built .fap files (easiest)

1. Download `space_travel_calculator.fap` and/or `dilate.fap` from the [latest release](https://github.com/ejfox/flipper-space-calculators/releases/latest).
2. Connect your Flipper Zero via qFlipper or mount the SD card directly.
3. Copy the `.fap` files to `SD Card/apps/Tools/` on the Flipper.
4. On the Flipper, open **Apps > Tools** and select the calculator you want.

### Firmware compatibility

The pre-built `.fap` files in releases are compiled for **Momentum firmware mntm-009** (API 79.2). If you are running a different firmware or API version, you will need to build from source (see below).

## Building from Source

The recommended way to build Flipper Zero apps is with [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) (micro Flipper Build Tool).

### 1. Install ufbt

```bash
pip install ufbt
```

### 2. Set up the SDK

For **Momentum firmware** (recommended if you use Momentum):

```bash
ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json
```

For **stock Flipper firmware**:

```bash
ufbt update
```

### 3. Clone and build

```bash
git clone https://github.com/ejfox/flipper-space-calculators.git
cd flipper-space-calculators
```

Each app is built separately from its own directory:

```bash
# Space Travel Calculator
cd space-travel-calculator
ufbt
cd ..

# Dilate
cd dilate
ufbt
cd ..
```

Compiled `.fap` files will be in each app's `dist/` directory.

### 4. Deploy directly to Flipper (optional)

From inside either app directory:

```bash
ufbt launch
```

## Testing

Both calculators include unit tests for their core calculation functions, run via GitHub Actions on every push and PR.

- Space Travel Calculator: 14 tests (orbital mechanics, Hohmann transfers)
- Dilate: 15 tests (Lorentz factor, time dilation)

Run tests locally from each app's directory:

```bash
cd space-travel-calculator && make -C tests && cd ..
cd dilate && make -C tests && cd ..
```

## Project Structure

```
flipper-space-calculators/
  space-travel-calculator/    # Orbital transfer calculator
    application.fam
    calculations.h/c          # Core physics functions
    tests/                    # Unit tests
    ...
  dilate/                     # Time dilation calculator
    application.fam
    calculations.h/c          # Core physics functions
    tests/                    # Unit tests
    ...
  .github/workflows/          # CI test runner
```

## License

MIT where applicable. See individual app directories.
