# Flipper Cribbage Calculator v1

## Summary

Build a standalone External FAP with `ufbt`, following Flipper’s custom-app guidance. It will guide the user through one complete deal—starter, non-dealer hand, dealer hand, and dealer crib—validate all 13 cards are unique, then show each show score and a scoring breakdown.

## References

- [Flipper App Development documentation](https://developer.flipper.net/flipperzero/doxygen/applications.html)
- [Bicycle Cribbage rules](https://bicyclecards.com/how-to-play/cribbage)

The Flipper documentation is the source of truth for FAP structure and build workflow. Bicycle’s rules are the source of truth for game scoring behavior.

## Key Changes

- Create a native C/C++ Flipper app packaged as a FAP and built with `ufbt`.
- Guide card entry through 13 slots: starter, four non-dealer cards, four dealer cards, then four crib cards.
  - Rank: `UP`/`DOWN` cycles Ace through King; `OK` continues.
  - Suit: `UP ♥`, `RIGHT ♦`, `DOWN ♣`, `LEFT ♠`; `OK` confirms.
  - `Back` returns to the prior step; at the first screen it exits.
- Reject duplicate physical cards, identify the prior conflicting slot, and keep the current card editable.
- Implement standard hand-counting rules:
  - Score fifteens, pairs, all run multiplicities, flushes, and his nobs.
  - Ace is low only; face cards count as 10 toward fifteens.
  - Score the starter in all three five-card counts.
  - Require a five-card crib flush; allow four- or five-card hand flushes.
  - Show “his heels” as a separate dealer `+2` when the starter is a Jack.
- Provide results screens:
  - Overview: non-dealer score, dealer-hand score, crib score, and separate heels bonus.
  - Detail pages: fifteens, pairs, runs, flush, and nobs for each count.
  - `Back` returns to the entered deal; `OK` starts a cleared new deal.

## Interfaces and Structure

- Define compact internal `Card`, `Suit`, `Rank`, `Hand`, and `ScoreBreakdown` types.
- Keep scoring independent from the Flipper UI for host-side testing.
- Use a native scene/view state machine for landing, rank selection, suit selection, duplicate errors, overview, and score details.
- Include the FAP manifest and build/install instructions in the README.

## Test Plan

- Unit-test canonical and boundary scores: the 29-point hand, fifteens, pairs, all duplicated-run patterns, flush variants, nobs, and Ace-low behavior.
- Verify crib flush behavior differs from hand flush behavior.
- Verify starter Jack produces only the separate dealer heels bonus.
- Verify duplicate cards are rejected, valid 13-card deals reach results, results remain editable via Back, and New Deal clears all slots.

## Assumptions

- v1 calculates post-play hand/crib counts only; pegging and game-to-121 tracking are out of scope.
- Native text and suit symbols are preferred over card artwork for fast, clear use on the Flipper display.
- No saved deals or statistics are included in v1.
