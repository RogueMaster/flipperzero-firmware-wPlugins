# Changelog

All notable changes to this project should be documented in this file.

This changelog follows a lightweight Keep a Changelog style.

## [Unreleased]

## [1.1]

### Added
- Support for a fourth bot, expanding tables to five total players.
- Configurable bot difficulty tiers with Medium as the default baseline.
- Optional progressive blinds with period and increase tuning from the blind editor.

### Changed
- The app/controller code was split out of the old monolithic `holdem.c` into dedicated UI/common, render, flow, and gameplay modules so contributors can work in smaller, purpose-specific files.
- Main table and menu layout were revised to preserve readability for the larger five-player table.
- Result-screen prompts and in-hand footer layout were tightened up to free space for a fourth bot row.
- Player rows now use fixed columns so turn markers, role/status flags, stacks, and hidden-card placeholders stay visually aligned.
- Folded pre-showdown bot rows now add a compact strike cue across the hidden `XX XX` placeholders instead of changing the rest of the row.
- Table header and board placeholder alignment are now measured dynamically so hand, stage, pot, and preflop board state stay visually centered and stable.
- Help, menu, footer, and cancel prompts now use compact bitmap control glyphs where screen space is tight.
- Visible money amounts now use the shared chip-style money glyph instead of text `$` signs.
- The board row no longer uses a `Table:` label and now centers the visible community cards as a unit.
- Startup, exit, result, and gameplay prompts now use the shared OK glyph treatment instead of mixed text-only `OK` labels.
- Bot decision logic now accounts for betting pressure, tones down low pocket-pair aggression preflop, and avoids weak stack-off lines.
- Extreme AI now plays a sharper heuristic game with stronger pressure handling, draw awareness, and more disciplined bluffing.
- Bot-count and difficulty settings now persist cleanly through restart, save/load, and new-game flow.
- Blind editing now supports a progressive-blinds mode that stays off by default and only advances at hand boundaries.
- Progressive blind periods now step in five-hand increments, with clearer blind-editor labeling.
- Progressive blind increases now surface a short centered notification before `Hand Start` when the saved schedule says a new level is due.
- Custom non-progressive blinds now persist across both manual new-game resets and automatic post-win/post-loss fresh games.
- Progressive-blind saves now preserve the configured base `SB/BB` separately from the live in-hand blind level, so future fresh games still restart from the intended base blinds.
- Freshly dealt hands now pause briefly on a stable `Hand Start` beat before action begins.
- Heads-up Easy and Medium bots now defend oversized preflop and flop pressure more realistically, so simple bully raises no longer print money nearly every hand.
- Bot raise sizing now snaps to the small-blind increment, eliminating oddball AI-generated call amounts such as `$22` on a `10/20` table.
- Blind and bot editors now behave like staged settings screens with dynamic save affordances instead of always-on control legends.
- Split-pot result screens now show a representative payout winner plus a `[Split Pot]` callout instead of pretending every paid hand has a single undisputed winner.
- Bot-only split-pot results now prefer the largest paid bot when the human player was not part of the split.
- Non-split result screens now preserve the full visible pot amount for display even when payout resolution refunds unmatched excess first.
- Three-line result screens keep their pre-split-pot vertical balance, while four-line split-pot results use a separate centered layout.
- Default bot action pacing was tightened slightly so autoplay remains readable without feeling quite as slow.
- Bot-count changes that require a fresh table now reuse the shared `Start new game?` confirmation screen instead of a separate restart-only prompt.

### Fixed
- Fresh-table resets triggered from bot-count changes now stay behind the explicit start gate until a real hand is dealt, preventing transient undealt table states with zero pots, duplicate placeholder cards, or folded bot rows.
- Folded autoplay no longer flashes the generic help footer, and `OK` now skips ahead to the next showdown or win step cleanly.
- Folded autoplay now keeps its dedicated fast-forward affordance through bot turns without leaking the generic idle hint before results.
- Live gameplay events no longer kick the player out of open menus before they choose to return to the table.
- Canceling `Exit Hold 'em` no longer resurrects the old idle footer hint on the game board.
- Showdown and fold-win resolution now refund uncalled excess chips before payout is computed, preventing bogus side pots and phantom split payouts in short-stack all-in spots.
- Short all-in raises no longer reopen raising incorrectly for players who already acted, and the action footer now drops its bet-adjustment glyph when only call/fold is legal.
- Players forced all in by blinds now keep the fast-forward spectator affordance while bots finish the hand, and short all-in bot calls now announce `is All In` instead of pretending they covered the full call.
- Shared centering and glyph-placement math is now centralized instead of being reimplemented screen by screen, reducing future UI drift.
- Forced all-in calls now say `All In` on the action footer, max raises keep their down-arrow cue, and split-pot result blocks keep their centered text grouped more cleanly.
- Exiting or starting a fresh unsaved game no longer deletes the existing save slot until a later save explicitly overwrites it.
- Straight result displays now order the winning cards ascending for easier scanability.
- Holding Right on the action footer now snaps the current bet to all in, and Controls Help reflects the new hold gesture.
- The Game Menu blind row once again shows the `P - ` marker while progressive blinds are enabled.
- Hold Right now uses its own shorter 1-second all-in hold threshold instead of sharing the longer Back-hold timer.
- Continuing hands now normalize unmatched excess before later streets and showdown, keeping visible stack and pot numbers honest after short all-ins.
- Big-blind walks once again preserve the full blind pot on the result screen instead of incorrectly refunding the posted blind difference.

## [1.0] - 2026-03-11

### Added
- Initial public release of Hold 'em for Flipper Zero.
- Full single-player Texas Hold'em gameplay from preflop through showdown.
- Support for 1 to 3 CPU opponents.
- Single-slot save/load with full game-state persistence.
- In-game blind editor, bot-count editor, controls help, and new-game reset.
- Big Win celebration and persistent win/loss end screens.

### Changed
- README, architecture notes, roadmap, and contributor docs were aligned with the shipped release.
- Catalog submission assets were added under `.catalog/`.

### Fixed
- Final release icon clarity and screenshot presentation polish for public release materials.
