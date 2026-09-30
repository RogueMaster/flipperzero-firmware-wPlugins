# Cribbage Calc for Flipper Zero

An external Flipper App Package (FAP) that scores a starter, non-dealer hand, dealer hand, and crib according to standard cribbage counting rules.

## Controls

- `UP`/`DOWN`: select rank.
- `UP ♥`, `RIGHT ♦`, `DOWN ♣`, `LEFT ♠`: select suit on the suit screen.
- `OK`: advance, save a card, or start a new deal from results.
- `BACK`: return to the previous entry or leave the app from the welcome screen.
- On the results overview, use `LEFT`/`RIGHT` to open score details; there, use them to move between hands. `BACK` returns to the scores, and `OK` begins a new deal.

Duplicate cards are rejected with the selected card and the earlier conflicting slot shown on-screen. Results include hand scores, the crib score, category breakdowns, and the dealer's two-point his-heels bonus when the starter is a Jack.

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

## References

- [Flipper App Development documentation](https://developer.flipper.net/flipperzero/doxygen/applications.html)
- [Bicycle Cribbage rules](https://bicyclecards.com/how-to-play/cribbage)
