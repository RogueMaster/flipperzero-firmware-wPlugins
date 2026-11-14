# iPhone, iPad and Apple Watch UI from the design

## Overview
- Bring the iPhone, iPad and Watch apps to the Claude Design pages, as the
  macOS app was brought to its page (`docs/plans/completed/20261026-macos-ui-from-design.md`).
- The specs are the source of truth: `docs/design/ios-ui-spec.md`,
  `docs/design/ipados-ui-spec.md`, `docs/design/watchos-ui-spec.md`
  (design sources in `docs/design/source/`). Each has a "Differences from
  what ships" list; this plan turns those lists into tasks. Where a spec and
  this plan disagree, the spec wins; where the spec is silent, keep the
  shipped behaviour.
- Shared behaviour changes that also reach the Mac (view models in
  `FliBuddyCore`): Net/Gross converts the number on screen, typed or not
  (gross − core floored at 0, net + core); the Bambu colour code shows with
  the colour name; technical tag facts move to a Tag details view.

## Owner decisions (2026-10-26/27, already in the specs)
- Rows newest first; a rescan moves the row to the top; any row editable.
- Auto-save new lives on the Flipper; hosts have no switch. A "Not in
  Bambuddy" Bambu row offers New, Link to existing… and an editable weight
  whose Save creates the spool and records the weight.
- Net/Gross always converts; below the core it clamps to 0.
- Portrait iPad shows the iPhone cards (decided by width, ≥ 900 pt detail
  column for the one-line table).
- The iPhone banner shows each side in trouble: one line for one side, two
  lines (Flipper, then Bambuddy, each with its chip) when both are down.

## Context (from discovery)
- Shared SwiftUI: `host/Sources/FliBuddyUI/` (`FeedView.swift`,
  `SettingsForm.swift`, `QRScanner.swift`, `UnknownRowActions.swift`,
  `SwatchView.swift`, `Watch/*`).
- View models: `host/Sources/FliBuddyCore/ViewModels/` (`FeedViewModel`,
  `FeedItem`, `CrownWeight`, `StatusItem`, `NewSpoolForm`,
  `LinkPickerModel`, `SettingsViewModel`, `HostModel`).
- Apps: `host/Apple/` (XcodeGen `project.yml`, `iOS/App.swift`,
  `Watch/App.swift`); `make apple-project`, `make apple-build`.
- Settings sync: `host/Sources/FliBuddyCore/Session/WatchSettingsSync.swift`.
- Colour code data: `ParsedTag.bambuColor`, `TagInfo.code`,
  `Spool.materialNumber` (Bambuddy field), `tools/data/bambu_color_codes.json`.
- Commands: `make test`, `make lint`, `make coverage`, `make apple-build`.

## Development Approach
- **Testing approach**: TDD for view-model and core logic (FliBuddyCore
  tests); SwiftUI views are checked by building every target
  (`swift build`, `make apple-build`) and by view-model tests of what they
  bind to.
- Complete each task fully before moving to the next; small focused commits.
- **CRITICAL: every task MUST include new/updated tests** for code changes.
- **CRITICAL: all tests must pass before starting the next task.**
- **CRITICAL: update this plan file when scope changes.**
- Keep the macOS app working and its spec (`docs/design/macos-ui-spec.md`)
  current when a shared change reaches it.
- All repository text in English.

## Testing Strategy
- Swift Testing suites in `host/Tests/FliBuddyCoreTests` for every view-model
  change (conversion, banner lines, row texts, focus rules, settings
  apply-on-change, watch sync cadence, crown rate, watch screen state).
- `make apple-build` (iOS and watchOS simulators) and `swift build`
  (macOS) after every UI task.

## Progress Tracking
- Mark completed items with `[x]` immediately; ➕ for new tasks, ⚠️ for
  blockers; record deviations in an "Implemented differently" section of
  the spec they touch.

## Implementation Steps

### Task 1: Shared view-model changes
- [x] `FeedViewModel.setMode` converts the number on screen (gross − core floored at 0 / net + core), typed or not; macOS gets it too
- [x] `FeedItem`: colour line `<colour name> · <code>` when the spool or tag has a Bambu colour code (`Spool.materialNumber`, else `ParsedTag.bambuColor`); UID leaves the row
- [x] Tag details model (UID, tray UUID, filament id, colour code, RGBA, label, diameter, nozzle range) for a row, used by all platforms
- [x] ➕ `Spool.materialNumber` decoded from Bambuddy; shared `TagDetailsSheet` behind a row context menu / VoiceOver action (Mac now; iOS gets it from the shared row, Task 3 checks it); the watch crown follows a converted value
- [x] update `docs/design/macos-ui-spec.md` for the Mac-visible parts (conversion, colour code, Tag details entry point)
- [x] write tests for conversion (both directions, clamp, untouched pre-fill), colour line, details model
- [x] run `make test` — must pass before Task 2

### Task 2: iPhone connection banner
- [x] banner view model: healthy line, or one line per side in trouble with the iPhone short phrases, symbols, tints and actions (ios spec, "Connection banner"), including the extra `StatusItem` states
- [x] top glass capsule in `FeedView` on iOS (replaces the bottom `StatusBar` there); actions Connect / Check / Retry / Settings wired
- [x] write tests for every phrase/action and the two-line case
- [x] run `make test` and `make apple-build` — must pass before Task 3

### Task 3: iPhone row card, number pad and focus
- [x] stacked card per ios spec "Feed row": status under the colour line, 64 pt field with the mode word, 34 pt value, full-width segmented, readout, full-width 50 pt Save, 44 pt New / Link capsules; one `Section` per row, compact spacing
- [x] number pad: `.numberPad`, select-all on focus, up to four digits, a lone `0` replaced by the next key; scroll to the focused row; tab bar hides while typing; pad lowers when no row waits; tap on the list background dismisses
- [x] focus rules (ios spec "Number pad and focus"): focus moves to the newest row waiting for a weight after a save; never steals from a touched field (2 pt tint outline instead)
- [x] haptics: success after a save, error on a failure
- [x] Tag details reachable from the row (sheet) on iPhone
- [x] write view-model tests for the focus rules, digit entry rules and haptic events
- [x] run `make test` and `make apple-build` — must pass before Task 4

### Task 4: iPhone settings, QR, sheets, Clear
- [x] Settings apply on change (no Save / Revert on iOS); labels, rows and footers per ios spec; keep Reload catalog; Connection picker stays hidden with one transport
- [x] watch settings sync after each applied change (`WatchSettingsSender`), debounced
- [x] QR scanner as a full-screen cover with the bracket frame and hint; Scan row dimmed with its footer without a camera; a read applies and starts Test connection; messages updated
- [x] New spool sheet: colour picker, Required / Optional sections, `xmark` / `checkmark` toolbar buttons, iOS footer; Link picker: `.searchable`, confirmation alert for moving a tag, toolbar buttons, wording per spec
- [x] Clear: `trash` icon button, dialog button `Clear feed`
- [x] write tests for apply-on-change, sync cadence, QR apply flow, form/picker models
- [x] run `make test` and `make apple-build` — must pass before Task 5

### Task 5: iPad layout
- [x] layout switch by width (≥ 900 pt detail → one-line table, else iPhone cards), not size class alone (`host/Apple/iOS/App.swift`)
- [x] sidebar: Connections section with the two status rows and their chips, Feed with count badge, Settings with the ⌘, hint, Clear feed at the bottom
- [x] one-line row grid per ipados spec (28 · 150 · 112 · 140 · flexible · auto; field 100 × 44 at 28 pt; segmented 120 × 34; Save 44); readout under the status; colour and remaining in their columns
- [x] keyboard: ⌘S, ⌘↑/⌘↓, ⌘⌫, ⌘, as menu commands (listed in the ⌘ overlay)
- [x] link warning wording `Saving moves the link to <uid>.`
- [x] write tests for the layout decision and sidebar model
- [x] run `make test` and `make apple-build` — must pass before Task 6

### Task 6: Watch feed, navigation and status
- [x] one `NavigationStack` with Settings behind the leading connection glyph (watchos spec); glyph from the merged status
- [x] feed row: 16 pt swatch, 15 sb title, short sub line (`590 g left`, `label 1000 g`, `Unknown tag`), 19 pt trailing glyph; empty text per spec
- [x] auto-open the detail of a new row that waits for a weight only while the feed list shows; pop back to the feed 1 s after a successful save
- [x] Always-On (`isLuminanceReduced`): title and remaining only, no taps
- [x] write tests for auto-open/pop rules and row texts
- [x] run `make test` and `make apple-build` — must pass before Task 7

### Task 7: Watch detail, crown, haptics, settings
- [x] entry: 50 pt rounded value + 20 pt unit, readout line (`Saves … g left` / `Turn the crown` / errors in red), Net / Gross pills (converting), 40 pt blue Save
- [x] crown: `.low` sensitivity, 5 g per detent above about 6 detents/s (`CrownWeight`)
- [x] haptics `.success` / `.failure`
- [x] saved screen: icon, 34 pt remaining, status, `Weigh again`; unknown screen: one-tap New only for tags with material, hints `Create it on iPhone or Mac.` / `Link it on iPhone or Mac.`; a Not-in-Bambuddy Bambu row keeps the entry (Save creates and weighs); busy screen
- [x] settings read-only (Server, Core weight, Weight as, `Received … Change these on iPhone.`), Flipper platter, separate Scan for Flipper screen with `Searching…` and signal words
- [x] write tests for the crown rate, screen-state selection and settings texts
- [x] run `make test` and `make apple-build` — must pass before Task 8

### Task 8: Verify acceptance criteria
- [x] walk each spec's "Differences from what ships" list: every item done or recorded under "Implemented differently"
- [x] run `make test`, `make lint`, `make coverage`, `make apple-build`, `swift build`
- [x] fix all issues (four unused-result / redundant `try` warnings in the host tests)

### Task 9: [Final] Update documentation
- [x] update the three specs' "Differences" sections to what remains (ideally nothing) and the READMEs for the new behaviour

## Post-Completion
- Manual check on the iPhone (`sokophone14`), an iPad (simulator or device)
  and an Apple Watch with the Flipper over Bluetooth: banner states, number
  pad flow, Save → next row, Tag details, settings sync to the watch,
  crown entry and haptics. Device installs need `host/Apple/Signing.xcconfig`
  with the owner's Team ID.
