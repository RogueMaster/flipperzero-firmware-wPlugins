# Architecture Notes

This document describes how the Hold 'em app is organized so contributors can extend it safely.

## Runtime Model

- Single-threaded event loop in `holdem.c`
- Rendering and input share one app state (`HoldemApp`)
- Game state (`HoldemGame`) is pure data and updated by deterministic game functions
- Save/load operates on full game state snapshots so interrupted sessions resume without reconstructing hidden cards or board state

## UI Modes

- `UiModeTable`: Main table render and in-hand interaction
- `UiModeActionPrompt`: Human action selection state
- `UiModePause`: Result, confirmation, and modal pause screens
- `UiModeBigWin`: Timed or persistent interstitial win/loss presentation
- `UiModeHelp`: Multi-page help
- `UiModeBlindEdit`: Blind editing sub-menu
- `UiModeBotCountEdit`: Bot count editor
- `UiModeRestartConfirm`: Confirm restart after bot-count change
- `UiModeExitPrompt`: Exit/save prompt
- `UiModeStartChoice`: Startup load/new choice
- `UiModeStartReady`: Clean startup state waiting for `OK`
- `UiModeSplash`: Startup splash + audio

## Hand Flow

1. Reset/deal/post blinds
2. Run betting rounds by stage
3. Resolve fold-win or showdown payout map
4. Show inspect/result screens
5. Apply payout only after result acknowledgement
6. Show endgame or restart-ready screen when a champion is decided
7. Advance hand counter and continue

## Payout Timing

Payout is intentionally split into two phases:

- Resolve phase (`resolve_fold_win`, `resolve_showdown`) computes payout only
- Commit phase (`apply_payout`) mutates stacks/pot after result confirmation

This keeps the inspect/result UX consistent and avoids premature stack updates.

## Save/Load

- Single save slot: `/ext/apps_data/holdem/save.bin`
- Save contains the full `HoldemGame` state for deterministic resume
- Starting a new game from startup choice clears prior save
- Legacy save migration is handled in `holdem_storage.c` and should stay conservative

## Extension Guidance

- Keep UI text short for 128x64 constraints
- Preserve mode-based input handling in `process_global_event`
- Treat short-vs-long `Back` behavior as a compatibility surface, not a casual UX tweak
- Keep display ordering separate from gameplay seat ordering
- Add new gameplay features by extending data first, then render/input paths
- Add comments around state transitions when behavior depends on ordering
