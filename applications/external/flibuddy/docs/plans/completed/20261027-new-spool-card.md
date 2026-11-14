# New spool card: add by default, weight optional

## Overview
- Owner request (2026-10-27): when a new filament is scanned, the hosts
  open a card for adding it, not a weight entry. The default is to add the
  spool as is (full); entering a weight is an option.
- Applies to the shared view models and every host UI: macOS, iPhone, iPad
  and Apple Watch. The Flipper side is unchanged (it already offers OK Save
  and ▶ Weigh on a "New spool").

## Behaviour
- **Unknown Bambu tag with material (Auto-save new off), no spool:** status
  `New spool` (replaces `Not in Bambuddy` for this case), symbol
  `plus.circle`, tint secondary. The row does **not** wait for a weight: it
  never takes focus, never raises the number pad, and shows no weight field.
  Actions:
  - **Save as new** (prominent): creates the spool from the tag, full
    (today's one-step New / `IntentHandler` create path).
  - **Save with weight…** (secondary): reveals the weight entry on that row
    (field, Net/Gross, readout, Save) and focuses it; its Save creates the
    spool and records the weight (today's `savesNewSpool` path). Esc /
    Cancel hides the entry again without saving.
  - **Link to existing…** (secondary), unchanged.
  - A Flipper request (`create_on_host` / `link_on_host`) keeps making the
    asked action prominent, as today.
- **Created on scan (Auto-save new on):** status `Created in Bambuddy`
  stays, but the row no longer waits for a weight: no focus, no field.
  Actions: **Set weight…** (reveals the entry as above; Save records the
  weight) and **Link to existing…** (unchanged semantics).
- **Tag without material** (`Not in Bambuddy; tag has no material`): as
  today (New… form, Link).
- **Known spools:** unchanged — the entry opens and takes focus.
- Focus rules (`FocusModel`): only rows whose entry is shown take part;
  after a save focus moves to the newest row that waits (known spools and
  revealed entries), as today.
- **Watch:** an unknown-with-material detail shows the title, the colour
  line, `label 1000 g`, a 40 pt blue **Save as new** and a secondary
  **With weight** that opens the crown entry (Save creates and weighs). A
  created-on-scan row's detail shows `Created in Bambuddy`, the label
  weight and **Set weight** (opens the crown entry). Auto-open on the feed
  applies only to rows that wait for a weight, so new spools do not open.
- Wording is the same on every platform: `New spool`, `Save as new`,
  `Save with weight…` (watch: `With weight`), `Set weight…` (watch:
  `Set weight`), `Link to existing…`.

## Context
- `host/Sources/FliBuddyCore/ViewModels/FeedItem.swift` (`isEntry`,
  `savesNewSpool`, `isWaiting`, `offersUnknownActions`, `newAction`,
  `prominentAction`, status texts), `FeedViewModel.swift`, `FocusModel.swift`.
- UIs: `host/Sources/FliBuddyUI/FeedView.swift`, `UnknownRowActions.swift`,
  `Watch/WatchFeedView.swift`, `Watch/WatchRowDetail.swift`, macOS views in
  `host/Sources/FliBuddyMac/` (check where the Mac row lives).
- Specs to update: `docs/design/macos-ui-spec.md`, `ios-ui-spec.md`,
  `ipados-ui-spec.md`, `watchos-ui-spec.md` (row-states tables and
  actions).
- Commands: `make test`, `make lint`, `make coverage`, `make apple-build`.

## Development Approach
- TDD for view-model changes; UIs checked by `swift build` and
  `make apple-build`.
- Every task includes tests; all tests pass before the next task.
- All repository text in English.

## Implementation Steps

### Task 1: View model for the new spool card
- [x] `FeedItem`: new status `New spool` for an unknown tag with material; `isWaiting` false for it and for a created-on-scan row; a per-row "entry revealed" state (in `FeedViewModel`, like drafts) that makes `isEntry` / `isWaiting` true; actions model exposing Save as new / Save with weight… / Set weight… / Link to existing… with the prominent one
- [x] `FeedViewModel`: `saveAsNew(row)` (the one-step create), `revealEntry(row)` (focuses it), `hideEntry(row)`; Save on a revealed new-spool entry creates and weighs; Save on a revealed created entry weighs
- [x] `FocusModel`: new spools and created rows never take focus unless revealed; after a save focus skips them
- [x] write tests for every state, the reveal/hide flow, focus rules and both Save paths (success and failure)
- [x] run `make test` — must pass before Task 2

### Task 2: macOS, iPhone and iPad rows
- [x] macOS row: New spool / Created rows show the actions instead of the field; Save with weight… / Set weight… reveal the entry inline and focus it; Esc hides it
- [x] iPhone card and iPad one-line row: same, with the iOS sizes from the specs (Save as new full-width 50 pt prominent; the two secondaries as 44 pt capsules)
- [x] write view-model-level tests for anything new the views bind to
- [x] run `make test`, `swift build`, `make apple-build` — must pass before Task 3

### Task 3: Watch
- [x] detail screens per the Behaviour section; auto-open only for waiting rows
- [x] write tests for the screen-state selection and auto-open rule
- [x] run `make test` and `make apple-build` — must pass before Task 4

### Task 4: Verify and document
- [x] update the four specs' row-states tables and actions; note the change under "Implemented differently" where it departs from the design
- [x] run `make test`, `make lint`, `make coverage`, `make apple-build`; fix all issues
