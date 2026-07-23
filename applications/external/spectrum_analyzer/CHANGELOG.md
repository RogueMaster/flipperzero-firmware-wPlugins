# Changelog

## 1.5

- Add peak hold: the last detected peak stays on screen ("Peak:" while the signal is live, "Last:" once it is gone) until a new peak replaces it, so short bursts can be read after the fact; the label stores the frequency measured at sweep time, so retuning or zooming cannot mislabel it
- Save the current view (frequency, spectrum width, modulation and vertical scale) on exit and restore it on the next launch, including applying the restored modulation to the radio
- Hide the peak label while the mode/modulation overlay is visible (the two used to overlap in the top-right corner)

## 1.4

- Version bump for catalog compatibility (no functional changes)

## 1.3

- Version bump for catalog compatibility (no functional changes)

## 1.2

- Version bump for catalog compatibility (no functional changes)

## 1.1

- Initial catalog release: CC1101-based spectrum analyzer with five zoom widths, adjustable vertical scale, two modulation presets and external radio module support (original app by @jolcese)
