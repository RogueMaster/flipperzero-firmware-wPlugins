# Changelog

## 1.5

- Fix a winning hand at a very large bet overflowing the bank, which could pay out the wrong amount or end the game on the "You have run out of money!" screen
- Cap the bank at $1,000,000,000 so the bet controls cannot overflow either

## 1.4

- Add Up/Down press to double or halve the bet (replaces Up = all-in; all-in is still reachable by doubling past half the bank or wrapping Left at the minimum)
- Add Up/Down hold to bet half the bank
- Make Left/Right +/-$10 steps clamp cleanly when the bet is not a multiple of $10
- Fix the bet re-entering the betting screen below the table minimum after a low-bank win

## 1.3

- Version bump for catalog compatibility (no functional changes)

## 1.2

- Add all-in on Up press and bet wraparound on Left/Right
- Fix the game state mutex declaration

## 1.1

- Version bump for catalog compatibility (no functional changes)

## 1.0

- Initial release: Jacks or Better video poker with betting, hold selection and payouts
