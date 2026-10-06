# Cribbage Calc for Flipper Zero

An external Flipper App Package (FAP) for quickly scoring one cribbage hand or crib at a time. Choose whether you are counting a hand or a crib, enter its four cards and the starter, and get a category-by-category total.

Choosing `Crib` applies the crib's five-card-only flush rule; choosing `Hand` allows a four-card flush. This keeps the normal counting workflow to five cards instead of requiring an entire deal.

## Controls

- `UP`/`DOWN`: select rank.
- `UP ♥`, `RIGHT ♦`, `DOWN ♣`, `LEFT ♠`: select suit on the suit screen.
- On the count-type screen, `UP`/`DOWN` chooses `Hand` or `Crib`; `OK` starts card entry.
- `OK`: advance, save a card, or start another count from results.
- `BACK`: return to the previous entry or leave the app from the welcome screen.
- Enter four hand/crib cards, then the starter. `BACK` from results reopens the starter for correction.

Duplicate cards are rejected with the selected card and the earlier conflicting slot shown on-screen. Results include fifteens, pairs, runs, flush, and nobs.

## Build

Install [uFBT](https://github.com/flipperdevices/flipperzero-ufbt), then run:

```sh
ufbt
```

With a connected Flipper whose firmware SDK matches uFBT, build, upload, and launch with:

```sh
ufbt launch
```

Run pure scoring tests on the host with:

```sh
make test
```

## Release files

Catalog-facing release notes are in [docs/changelog.md](docs/changelog.md), and the catalog description is kept separately in [docs/description.md](docs/description.md) so it contains only the Markdown supported by the Flipper Apps Catalog. The required 10×10 1-bit app icon is [icon.png](icon.png).

## References

- [Flipper App Development documentation](https://developer.flipper.net/flipperzero/doxygen/applications.html)
- [Bicycle Cribbage rules](https://bicyclecards.com/how-to-play/cribbage)
