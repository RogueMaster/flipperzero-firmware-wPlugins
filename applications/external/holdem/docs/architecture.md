# Architecture Notes

This document describes how the Hold 'em app is organized so contributors can extend it safely.

## Runtime Model

- Single-threaded event loop rooted in `holdem.c`
- Rendering and input share one app state (`HoldemApp`)
- Game state (`HoldemGame`) is pure data and updated by deterministic game functions
- Save/load operates on full game state snapshots so interrupted sessions resume without reconstructing hidden cards or board state

## Code Layout

- `holdem.c`: app bootstrap, lifecycle, and top-level loop
- `holdem_ui_common.c`: shared glyphs, centering helpers, and common display-state helpers
- `holdem_ui_render.c`: all draw code and table/menu/result rendering
- `holdem_ui_flow.c`: input routing, staged editor commits, startup flow, and back-button behavior
- `holdem_gameplay.c`: interstitial/result flow plus betting-round and hand orchestration
- `holdem_engine.c`: chip movement, dealing, payout resolution, and showdown helpers
- `holdem_ai.c`: bot decision heuristics
- `holdem_eval.c`: hand scoring and card formatting
- `holdem_storage.c`: save/load and legacy migration

This split is intentional. The project treats contributor readability as a feature and keeps modules focused so UI, gameplay, and storage work can evolve without crowding each other.

## UI Modes

- `UiModeTable`: Main table render and in-hand interaction
- `UiModeActionPrompt`: Human action selection state
- `UiModePause`: Result, confirmation, and modal pause screens
- `UiModeBigWin`: Timed or persistent interstitial win/loss presentation
- `UiModeHelp`: Multi-page help
- `UiModeBlindEdit`: Blind editing sub-menu
- `UiModeBotCountEdit`: Bot count editor
- `UiModeNewGameConfirm`: Shared new-game confirmation screen used by both the game menu and bot-count changes that require a fresh table
- `UiModeExitPrompt`: Exit/save prompt
- `UiModeStartChoice`: Startup load/new choice
- `UiModeStartReady`: Clean startup state waiting at the explicit start gate
- `UiModeSplash`: Startup splash + audio

## Hand Flow

1. Reset/deal/post blinds
2. Show short hand-boundary notifications such as progressive blind increases and `Hand Start`
3. Run betting rounds by stage
4. Resolve fold-win or showdown payout map
5. Show inspect/result screens
6. Apply payout only after result acknowledgement
7. Show endgame or restart-ready screen when a champion is decided
8. Advance hand counter and continue

## Payout Timing

Payout is intentionally split into two phases:

- Resolve phase (`resolve_fold_win`, `resolve_showdown`) computes payout only
- Commit phase (`apply_payout`) mutates stacks/pot after result confirmation

This keeps the inspect/result UX consistent and avoids premature stack updates.
Before either payout path resolves winners, unmatched top contributions are refunded so uncalled excess chips do not leak into bogus side pots.
If a hand continues to another street or to showdown after a short all-in leaves unmatched excess behind, that excess is normalized before the next board state is shown so visible pot and stack numbers stay truthful mid-hand.
When a hand pays multiple players, the current result screen intentionally shows one representative payout recipient plus a `[Split Pot]` cue until richer split-pot detail work lands.
Three-line and four-line result layouts intentionally use different vertical anchors so split-pot screens can gain a fourth centered line without shifting the legacy three-line result presentation.

## Save/Load

- Single save slot: `/ext/apps_data/holdem/save.bin`
- Save contains the full `HoldemGame` state plus persisted gameplay settings such as bot difficulty and blind configuration
- Persisted settings include table size (`player_count`), the live blind values, the configured base small blind for future fresh games, bot difficulty, and the full progressive-blind schedule state
- Starting or playing a fresh unsaved game does not clear the previously stored save slot until a later save overwrites it
- Legacy save migration is handled in `holdem_storage.c` and should stay conservative
- Progressive blind scheduling persists via the next scheduled raise hand number, so a loaded save can trigger the next blind increase at the correct future hand boundary

## Bot AI

- Bot behavior is driven by a lightweight heuristic layer in `holdem_ai.c`
- The player-facing difficulty tiers map to internal AI levels: `Easy` (20), `Medium` (50), `Hard` (80), and `Extreme` (110)
- `Medium` is the default setting and acts as the balance baseline for new tuning work
- Core decision inputs include preflop hand quality, postflop made-hand strength, board wetness, pot odds, betting pressure, and stack-commitment risk
- Heads-up early-street defense now has a small dedicated pressure discount so Easy and Medium bots do not over-fold to repetitive oversized opens and continuation bets
- Bot raise sizing is rounded onto the table's small-blind increment so AI-generated bet sizes stay on the same visible chip grid as the human controls
- `Extreme` adds stronger preflop hand-shape reading, postflop draw awareness, tighter bluff discipline, and more assertive value raising without abandoning the single-threaded lightweight runtime budget

## Release 1.1 Scope

- Five total players are now supported, with one human and up to four bots
- The denser table layout depends on footer compaction and tighter menu spacing across the split UI modules
- Player rows on the main table now use fixed visual columns for names, role/status markers, stack text, and hidden-card placeholders
- Folded pre-showdown bot rows keep their `XX XX` placeholders and add a narrow strike cue across the pair so fold state reads faster without exposing cards
- Inline bitmap glyphs now carry part of the UI language for confirm, back, navigation, left/right actions, and folded-autoplay fast-forward
- The board row now stands without a `Table:` label and centers the visible community cards as a unit
- Bot-count and difficulty settings are treated as persistent gameplay settings and survive save/load plus new-game resets
- Blind editing now has an optional progressive mode that stays disabled by default and advances blinds only between hands
- Fixed blind settings are treated as table configuration, so custom non-progressive `SB/BB` values persist across both user-initiated and automatic new-game resets
- Blind and bot editors now use staged values with conditional save prompts so temporary menu edits do not imply an immediate commit
- Bot-count changes now reuse the shared `Start new game?` confirmation screen so the user sees one consistent restart flow
- When a progressive increase is due, the app shows a short non-animated blind-level notification before the next `Hand Start` beat
- Every newly dealt hand shows a short `Hand Start` beat after fresh cards are in place so the visible table state is stable before action starts
- Foreground gameplay prompts are queued behind open menus so live turns and results do not steal focus from active menu screens
- Elimination state is now treated separately from temporary zero-chip all-in state so busted markers and busted-row cues only appear once a player is truly out of the game
- Betting rounds now distinguish full raises from short all-in raises so action is not reopened illegally after a partial all-in bump

## Extension Guidance

- Keep UI text short for 128x64 constraints
- Preserve mode-based input handling in `process_global_event`
- Treat short-vs-long `Back` behavior as a compatibility surface, not a casual UX tweak
- Keep display ordering separate from gameplay seat ordering
- Add new gameplay features by extending data first, then render/input paths
- Add comments around state transitions when behavior depends on ordering
- Keep shared UI math in common helpers instead of duplicating centering or glyph-placement logic screen by screen
