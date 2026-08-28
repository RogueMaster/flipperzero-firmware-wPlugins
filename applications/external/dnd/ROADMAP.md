# Dungeons & Dolphins roadmap

Current baseline: **3.2.5**. This roadmap lists future features only. Completed work belongs in `CHANGELOG.md`.

## Pack management

- Transactional uninstall and export for installed campaign and monster packs.
- Stable-ID conflict preview before pack changes are committed.
- On-device manifest preview with record count, compatibility range, source files, and destination.
- Rename and notes fields for installed packs while keeping pack files manually editable.

## Performance & diagnostics

- Compact persistent summary indexes so favorites, recents, and saved encounters can reopen without rebuilding unchanged state.
- Batched adjacent Bestiary state updates with immediate flush on app switch or exit.
- Low-memory diagnostics showing heap headroom, peak transient allocation, cache sizes, and SD transaction recovery state.

## Campaign enhancements

- Typed campaign variables without executable scripting.
- Conditional campaign choices based on stored campaign state.
- Campaign objectives linked with journal milestones and inventory rewards.
- Conflict-safe campaign progress import/export with compatibility preview.

## Accessibility & controls

- Compact, standard, and large-text row layouts with per-screen previews.
- Configurable long-press shortcuts and Left/Right behavior.
- Reset-to-default control mapping.
- Reduced-motion option for marquee and dice animations.

## Encounter history

- Compact completed-encounter history with date, rounds, party state, and surviving opponents.
- Clone a completed encounter into a new named encounter without retaining old initiative state.
- Streamed encounter-history export and pruning without loading the entire history into RAM.
