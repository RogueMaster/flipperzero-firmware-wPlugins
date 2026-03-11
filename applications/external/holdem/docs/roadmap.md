# Roadmap

This roadmap tracks the work intentionally deferred beyond the current release of Hold 'em.

## Status Key

- Planned: agreed direction, not started
- In Progress: actively implemented
- Blocked: waiting on dependency, design decision, or validation
- Deferred: intentionally out of the current release scope
- Done: shipped and verified

## Current Release Status

The current release is feature-complete for its scope. Remaining work should be handled as validation, bug fixing, documentation cleanup, or next-release planning rather than new feature expansion.

## Next Release Candidates

### F-001 Lifetime Hands Completed Counter
- Status: Deferred
- Summary: Track total completed hands for the currently installed app instance after the current release.
- Future requirements:
  - Counter persists across app restarts.
  - Counter increments only when a hand reaches a completed result state.
  - Save format/versioning must allow future extension without data loss.
  - Behavior for save deletion, reinstall, and firmware migration must be documented.

### F-002 Configurable Progressive Blinds
- Status: Deferred
- Summary: Add optional progressive blind structures after the current release.
- Future requirements:
  - Add enable/disable control and basic structure tuning.
  - Apply increases only at hand boundaries.
  - Preserve save/load correctness with an active structure.

### F-003 Configurable Bot Difficulty
- Status: Deferred
- Summary: Add a player-facing bot difficulty setting after the current release.
- Future requirements:
  - Preserve the current default balance as the initial setting.
  - Persist the chosen difficulty in save/config state.
  - Keep the AI path extensible for richer future behavior.

### F-004 Winner Celebration Polish
- Status: Deferred
- Summary: Expand the winner presentation after the current release with audio, LED effects, and richer animation treatment.
- Future requirements:
  - Add an optional winner tune.
  - Add celebratory LED behavior.
  - Expand the current fireworks treatment without blocking core flow excessively.

### F-005 Game Over Polish
- Status: Deferred
- Summary: Expand the game-over presentation after the current release with audio, LED effects, and richer animation treatment.
- Future requirements:
  - Add an optional game-over tune distinct from the winner tune.
  - Add a subdued LED pattern for losses.
  - Expand the current game-over presentation without obscuring restart flow.

### F-006 Busted Player Strikethrough Experiment
- Status: Deferred
- Summary: Evaluate drawing a strike-through treatment for busted bot rows to improve scanability.
- Future requirements:
  - Prototype a lightweight strike-through overlay for busted bot names or full rows.
  - Confirm readability on-device with the current font and row spacing.
  - Keep the effect optional until the visual result is validated.
  - Avoid cluttering active-player rows or reducing card readability.

### F-007 New Startup Art
- Status: Deferred
- Summary: Replace or expand the current startup art with a more distinctive title-screen presentation.
- Future requirements:
  - Preserve legibility on the 128x64 display.
  - Keep startup flow fast and input-safe.
  - Revalidate spacing around prompt text and title lockup on device.

### F-008 Sub-GHz Multiplayer Investigation
- Status: Deferred
- Summary: Evaluate whether synchronous multiplayer across multiple Flipper devices is feasible and worth pursuing.
- Future requirements:
  - Define a connection model that is reliable within Flipper hardware and regional radio constraints.
  - Determine how to synchronize shuffles, betting actions, and reconnect behavior without trust issues.
  - Confirm whether latency, packet loss, and user setup burden are acceptable for Texas Hold'em pacing.
  - Keep the investigation separate from the single-device core game until feasibility is proven.

## Notes

- Keep this file concise and implementation-focused.
- Add new post-v1 items only when they are meaningfully defined.
- Revisit deferred items after the current release is tagged and stabilized.
