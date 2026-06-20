# Mayan Decoder

Mayan Decoder is an external `.fap` application for Flipper Zero that converts decimal numbers into Mayan numerals using the Flipper Canvas API.

## Features

- Decimal range: `0` to `9999`.
- Vigesimal conversion with vertically stacked Mayan levels.
- `0`: simple shell glyph.
- `1..4`: dots.
- `5..19`: horizontal bars plus remaining dots.
- Direct rendering through `ViewPort` and `Canvas`.
- Intro screen with simple Mayan pyramid artwork.
- Digit-based numeric input mode for larger values.

## Controls

Intro screen:

- `OK`: enter the app.
- `Back`: exit.

Main screen:

- `Up` / `Right`: increment the number.
- `Down` / `Left`: decrement the number.
- `OK`: open numeric input.
- `Back`: exit the application.

Numeric input:

- `Left` / `Right`: select digit.
- `Up` / `Down`: increase or decrease the selected digit.
- `OK`: save the number.
- `Back`: cancel and return.

## Repository Structure

```text
.
+-- application.fam
+-- mayan_decoder.c
+-- assets
|   +-- generate_icon.py
|   +-- icon.png
+-- docs
|   +-- changelog.md
+-- screenshots
|   +-- ss0.png
|   +-- ss1.png
|   +-- ss2.png
|   +-- ss3.png
+-- README.md
```

## Build With uFBT

Install or update uFBT:

```sh
python -m pip install --upgrade ufbt
```

Clone and build:

```sh
git clone https://github.com/RogerF5-Security/Mayan_decoder.git
cd Mayan_decoder
ufbt
```

The compiled `.fap` will be generated in the `dist/` directory.

Build, upload, and run on a connected Flipper Zero:

```sh
ufbt launch
```

Update the uFBT SDK:

```sh
ufbt update
```

## Regenerate Icon

The FAP icon must be a 1-bit `10x10` PNG. To regenerate it:

```sh
python assets/generate_icon.py
```

## Catalog Assets

Screenshots are stored in `screenshots/` as unmodified qFlipper PNG captures for Flipper Application Catalog submission.

## License

MIT License.
