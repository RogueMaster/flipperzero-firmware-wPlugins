# Flipper Cribbage Calculator v1

## Summary

Build a standalone External FAP with `ufbt`, following Flipper’s custom-app guidance. It will let the user choose a normal hand or crib, enter that group's four cards plus the starter, validate the five cards are unique, then show a score and scoring breakdown.

## References

- [Flipper App Development documentation](https://developer.flipper.net/flipperzero/doxygen/applications.html)
- [Bicycle Cribbage rules](https://bicyclecards.com/how-to-play/cribbage)

The Flipper documentation is the source of truth for FAP structure and build workflow. Bicycle’s rules are the source of truth for game scoring behavior.

## Key Changes

- Create a native C/C++ Flipper app packaged as a FAP and built with `ufbt`.
- Choose `Hand` or `Crib`, then guide card entry through four group-card slots followed by the starter.
  - Rank: `UP`/`DOWN` cycles Ace through King; `OK` continues.
  - Suit: `UP ♥`, `RIGHT ♦`, `DOWN ♣`, `LEFT ♠`; `OK` confirms.
  - `Back` returns to the prior step; at the first screen it exits.
- Reject duplicate physical cards within the count, identify the prior conflicting slot, and keep the current card editable.
- Implement standard hand-counting rules:
  - Score fifteens, pairs, all run multiplicities, flushes, and his nobs.
  - Ace is low only; face cards count as 10 toward fifteens.
  - Score the starter in the selected five-card count.
  - Require a five-card crib flush; allow four- or five-card hand flushes.
- Provide results screens:
  - One score with fifteens, pairs, runs, flush, and nobs.
  - `Back` returns to the entered cards; `OK` begins another count.

## Interfaces and Structure

- Define compact internal `Card`, `Suit`, `Rank`, `Hand`, and `ScoreBreakdown` types.
- Keep scoring independent from the Flipper UI for host-side testing.
- Use a native scene/view state machine for landing, rank selection, suit selection, duplicate errors, overview, and score details.
- Include the FAP manifest and build/install instructions in the README.

## Test Plan

- Unit-test canonical and boundary scores: the 29-point hand, fifteens, pairs, all duplicated-run patterns, flush variants, nobs, and Ace-low behavior.
- Verify crib flush behavior differs from hand flush behavior.
- Verify duplicate cards are rejected, valid five-card counts reach results, results remain editable via Back, and New Count clears all slots.

## Assumptions

- v1 calculates one post-play hand/crib count at a time; pegging, his heels, and game-to-121 tracking are out of scope.
- Native text and suit symbols are preferred over card artwork for fast, clear use on the Flipper display.
- No saved deals or statistics are included in v1.
