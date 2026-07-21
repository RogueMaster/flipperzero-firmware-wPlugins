# Changelog

## 1.4

- Fix decimal results: `1/12` now shows `0.083333` instead of `.8`. The leading `0`, leading fractional zeros, and any digit past the second decimal place are no longer dropped
- Increase result precision to 6 decimal places, with correct rounding (carrying into the integer part) and trailing-zero trimming (`0.5`, not `0.500000`)
- Show `Error` for malformed expressions and division by zero instead of garbage output
- Add exponentiation and square root without changing the keypad, via long-press: long-press `X` (multiply) inserts `^` (e.g. `2^3`), long-press `/` inserts `sqrt(` (e.g. `sqrt(9)`), alongside the existing long-press `(` -> `)`. Roots of any degree via `x^(1/n)`, e.g. `27^(1/3)`.
- Cursor position and text scrolling now use exact font metrics

## 1.3

- Version bump for catalog compatibility (no functional changes)

## 1.2

- Version bump for catalog compatibility (no functional changes)

## 1.1

- Version bump for catalog compatibility (no functional changes)

## 1.0

- Initial catalog release: expression calculator powered by TinyExpr with an on-screen keypad, supporting `+`, `-`, `*`, `/`, `%` (modulo), parentheses and decimals
