# macOS app: bring the feed, status bar, sheets and settings to the design

## Overview
Implement `docs/design/macos-ui-spec.md` in the macOS app. The spec is the
distillation of the Claude Design project "FliBuddy mobile app design"
(macOS page and spec page). It changes layout, copy, focus behaviour and a
few view-model rules; it does not change the wire protocol or the Bambuddy
calls. Run this plan after Phase 2 (`phase2-ble-ios`) and the FliBuddy
rename have merged, because both touch the same view and view-model files.

What changes:
- the feed row becomes a one-line macOS row with fixed columns, UID under the
  title, a readout line and a "g" unit;
- focus and keyboard behaviour: newest waiting row focused with the
  suggestion selected, no focus stealing from a touched field, Tab order,
  ⌘S/Return, ⌘⌫ with confirmation, ⌘↑/⌘↓;
- Net/Gross conversion rule; `Saving…` and timeout states; reopening a saved
  row; Created keeps the row open with label + core pre-filled;
- status bar with symbols, tints, phrases and inline actions; blocking
  Spoolman state; empty-state copy;
- New spool sheet and Link picker per the spec;
- Settings labels, test-result phrases, footers, Revert and the
  saved/unsaved line.

## Context (from discovery)
- Views (after Phase 2 and the FliBuddy rename):
  `host/Sources/FliBuddyUI/FeedView.swift` (feed, row, status
  bar), `UnknownRowActions.swift` (New form, Link picker), `SettingsForm.swift`,
  `host/Sources/FliBuddyMac/{App,SettingsView}.swift`.
- View models: `host/Sources/FliBuddyCore/ViewModels/FeedViewModel.swift`,
  `FeedItem.swift` (status text, `suggested(mode)`, `savedByText`,
  `requestText`), `FlipperLink.swift` (`statusText`), `HostModel.swift`,
  `SettingsViewModel.swift`, `NewSpoolForm.swift`.
- Session: `host/Sources/FliBuddyCore/Session/{Session,FeedRow,IntentHandler,LookupBuilder}.swift`
  (`FeedRow.Status`: scanned, awaitingWeight, unknown(prefill), saved,
  created, error; `Side`; `HostRequest`).
- Weight math: `host/Sources/FliBuddyCore/Bambuddy/WeightMath.swift`.
- Tests: `host/Tests/FliBuddyCoreTests/{FeedViewModelTests,SessionTests,FlipperLinkTests,SettingsViewModelTests}.swift`.
- The spec's prototype model (`flibuddy-core.js` in the design project) is
  the reference for the rules; its copy is quoted in the spec file.

## Development Approach
- **Testing approach**: TDD. Write the failing test, then the code.
- Complete each task fully before moving to the next
- Make small, focused changes
- **CRITICAL: every task MUST include new/updated tests** for code changes in that task
  - tests are not optional - they are a required part of the checklist
  - write unit tests for new functions/methods
  - write unit tests for modified functions/methods
  - add new test cases for new code paths
  - update existing test cases if behavior changes
  - tests cover both success and error scenarios
- **CRITICAL: all tests must pass before starting next task** - no exceptions
- **CRITICAL: update this plan file when scope changes during implementation**
- Run tests after each change
- View logic lives in view models and small pure helpers so it is testable;
  SwiftUI views stay thin
- Follow `AGENTS.md`: English everywhere, one commit per task

## Testing Strategy
- **Unit tests**: required for every task (see Development Approach above)
- `cd host && swift test`; `make test`, `make lint`, `make coverage` at the end
- No UI test framework; the acceptance task runs the app and checks the
  windows by eye against the spec (recorded in the plan)

## Progress Tracking
- Mark completed items with `[x]` immediately when done
- Add newly discovered tasks with ➕ prefix
- Document issues/blockers with ⚠️ prefix
- Update plan if implementation deviates from original scope
- Keep plan in sync with actual work done

## What Goes Where
- **Implementation Steps** (`[ ]` checkboxes): tasks achievable within this codebase - code changes, tests, documentation updates
- **Post-Completion** (no checkboxes): items requiring external action - manual testing, changes in consuming projects, deployment configs, third-party verifications
- **Checkbox placement**: Checkboxes belong only in Task sections (`### Task N:` or `### Iteration N:`). Do not put checkboxes in Success criteria, Overview, or Context — they cause extra loop iterations.

## Implementation Steps

### Task 1: Row view-model rules from the spec
- [x] `FeedRow.Status`: add `saving` and `savedAfterTimeout` (saved by host after the Flipper reported a timeout); `Session` sets `saving` while the Bambuddy call runs and `savedAfterTimeout` when the `result` went out later than about 10 s after the intent arrived (approximate: the host measures its own latency against the Flipper's constant)
- [x] `FeedItem`: `suggestion(mode:)` per the spec (base + core for gross, base for net; after Created base = label); `readout(value:mode:core:)` strings; `isEntry` (enter, saving, created, error); `isReopenable` (saved, timeout); `timeoutNote`; status symbol name and tint per state; accessibility header text
- [x] `FeedViewModel`: a per-row draft (`value`, `touched`, `mode`); Net/Gross switch converts an untouched suggestion and keeps a typed value; `reopen(row)`; Save disabled rules (empty, negative net, saving)
  - ➕ `reopen` goes through `Session.reopen(row:)`, which takes a saved row back to `awaitingWeight`; `FeedViewModel.createFromTag` is the one-step New used as the retry after a failed create on scan
  - ➕ Interim view wiring until Tasks 3 and 5: `FeedView` and the watch row show the entry block from `isEntry`, a "Re-weigh" button on saved rows, and "Link to existing…" on Created rows
- [x] `Session`: when the lookup finds no spool for a Bambu tag with prefill, create it at once with the defaults (the existing New path, no button) and show the row as `created` (entry state, suggestion = label + core, focused); Save sends the weight as today; a rescan goes through the by-tag lookup and must not create again; a failed create shows the error with **New** as the retry
- [x] **Link to existing…** on a `created` row: delete the spool created on scan, then link the tag to the chosen spool (`SpoolInventory.deleteSpool`, `DELETE /inventory/spools/{id}` in `BambuddyClient`); if the delete fails nothing changes and the row shows "Spool <id> created on scan could not be deleted, so the tag stays on it: <reason>" (Save and Link stay available); if the link fails after the delete, the row has no spool and offers New / Link again
  - ⚠️ Order changed from "link, then delete": Bambuddy's `PATCH /inventory/spools/{id}/link-tag` answers 409 `tag_already_linked` while another active spool (the one created on scan) carries the tag, so linking first always fails. `FakeInventory.linkTag` now refuses such a tag the same way. `FeedRow.createdSpoolId` remembers which spool the session created.
- [x] `FeedViewModel` order: newest scan first; a scan whose tag (tray UUID or UID) is already in the feed moves that row to the top and reopens it for a weight instead of appending a duplicate
- [x] write tests: suggestion and readout for gross/net/empty/negative; conversion on mode switch (untouched vs touched); state → symbol/tint/text table; reopen; an unknown Bambu tag is created on scan and the row opens as `created` with label + core; a rescan does not create again; Link on a created row deletes the created spool then links, and reports a failed delete; rescan moves the row to the top without a duplicate; a Flipper save clears the host draft and shows `by Flipper`
- [x] run tests - must pass before task 2

### Task 2: Focus and keyboard model
- [x] `FeedViewModel.FocusModel` (pure): `focusedRow`, `nextWaitingRow()`, `onRowBecameWaiting(id)` (takes focus unless another row is touched, then marks a hint), `onSaved(id)` (moves to the next waiting row), `moveFocus(.up/.down)`
  - ➕ `FeedViewModel` feeds it from `apply` by diffing snapshots (rows that started waiting, oldest first; rows that went from entry to saved), so a refresh never moves focus. "Waiting" is `FeedItem.isWaiting`: Enter weight or Created only. "Touched" is the focused row's typed draft; a touched row that lost focus does not hold it; a write of the unchanged suggestion (a field echoing its value) does not touch the row. The hinted row gets a tinted outline.
- [x] `FeedView`: `@FocusState` bound to the model, the field selects its text on focus, Tab order field → mode → Save, Return saves, `.keyboardShortcut("s", modifiers: .command)` saves the focused row, ⌘↑/⌘↓ move, ← → switch the segmented picker when focused
  - ➕ ⌘S, ⌘↑ and ⌘↓ are invisible buttons at the `FeedView` level, so each shortcut exists once. Select-all on focus and the Tab order rely on AppKit (a field selects its text when it becomes first responder; Tab follows view order, reaching the picker and Save with keyboard navigation on); Task 7 checks both by eye. ← → use `onKeyPress` on the picker. The list selection binds to `focusedRow` on macOS only.
- [x] write tests for `FocusModel`: new row focuses when nothing is touched; waits with a hint when a field is touched; after save moves to the newest waiting row; no waiting row keeps the selection; up/down bounds
- [x] run tests - must pass before task 3

### Task 3: One-line row layout, swatch, readout, unit
- [x] `FeedView` row: columns swatch 28 · title + UID · colour · remaining · status · field 96 × 34 · segmented ≈150 · Save; `.controlSize(.large)`; row height 60; entry controls only in entry states, space reserved otherwise; swatch with `.separator` stroke and dashed `.tertiary` outline when absent, hidden from VoiceOver; weight value 22 pt rounded bold monospaced digits via `@ScaledMetric`; "g" unit; readout line; timeout note; red status text only for errors
  - ➕ Model: `FeedItem.colorName` is never the hex (the swatch shows the colour); `detailText` is the colour column ("Jade White · 484 g left of 1000", "label 1000 g", empty); a blank tag is titled `blankTitle(uid:)` ("NTAG 7-byte UID 0411…"); `weightFieldLabel(mode:)`; `FeedViewModel.saveTitle(for:)` / `isSaving(_:)`.
  - ➕ View decisions: the one-line layout is macOS only; iOS shares `FeedRowView` and stacks the entry block under the header until the iPhone/iPad spec lands. Title, colour and status columns share the width equally and the entry block is a fixed 360 pt, so columns line up. Saved rows reopen on click (the interim Re-weigh button is gone; VoiceOver gets a "Re-weigh" action). **Link to existing…** on a Created row sits right of the readout line. The scan time moved from the row to its tooltip. The weight field is a plain field on a rounded rect with its own focus ring.
  - ➕ Task 7 checks by eye: columns line up across states at 760 pt and 1000 pt; the click on a saved row both selects and reopens it without stealing a field's focus.
- [x] write tests: `FeedItem` display strings for every state in the spec table (snapshot-style string tests); the row's accessibility header text
- [x] run tests - must pass before task 4

### Task 4: Status bar with symbols, tints and actions; empty and blocking states
- [x] `FlipperLink` and `FeedViewModel.bambuddy`: expose a `StatusItem` (text, symbol, tint, optional action) per the spec tables, including the `nohello` state (link open, no hello within 2 s) and `nocolors`
  - ➕ `StatusItem` also carries `detail` (the reason, port, firmware) for the tooltip. Mappings the spec leaves open: `waitingForHello` (under 2 s) and "No Flipper found" count as searching; any other reason the link is down ("Bluetooth is off", "Resource busy") is the phrase itself, orange, with the slash symbol; Bambuddy `notConfigured` is "Bambuddy not set up" (orange, Settings) and a rejected key "Bambuddy rejected the API key" (red, Settings); a check under way shows "Checking Bambuddy…" with a spinner (`SessionSnapshot.isCheckingBambuddy`).
  - ⚠️ `adv` ("Flipper nearby, not connected", Connect) is shown when the link is stopped, since every other down state already retries every 2 s and Connect would add nothing; the host does not scan for advertising Flippers while disconnected, so "nearby" is not probed. Nothing stops the link today, so the state appears once something (Task 6's Forget) does.
- [x] `StatusBar` view: two items in `.footnote.weight(.medium)`, progress indicator for searching/checking, action buttons (Connect, Check, Retry, Settings) wired to `HostModel`
  - ➕ `HostModel.perform(_:)` runs Connect (the saved transport choice), Check and Retry (`checkBambuddy()`); Settings is the view's: `SettingsLink` on macOS, an `openSettings` closure on iOS (the iOS tab view now has a selection). `FeedView(host:openSettings:)` replaces `FeedView(feed:link:)`. The bar is a bottom `safeAreaInset` on `.bar` with a top divider; the port moved into the Flipper tooltip. The watch keeps its `statusText` lines.
- [x] `ContentUnavailableView`s: empty feed copy; blocking Spoolman state with Retry that re-runs the startup check; the list hidden while blocking
- [x] write tests: state → `StatusItem` for all Flipper and Bambuddy states; Retry triggers the check; blocking flag
- [x] run tests - must pass before task 5

### Task 5: Clear with confirmation, New sheet, Link picker
- [x] Clear: toolbar button with `.keyboardShortcut(.delete, modifiers: .command)` opening `confirmationDialog("Clear the feed?")` with the spec copy; destructive Clear
- [x] New spool sheet: title/subtitle, Material menu with the spec list and "Choose…", Subtype, Colour, Brand, Label weight (digits, default 1000), footer copy, Cancel · Create (disabled without material); Bambu tags have no New button (created on scan, Task 1); the no-prefill row keeps "New…" (form) and "Link to existing…"
  - ➕ `FeedItem.newAction`: "New…" opens the form for a tag without material; a tag with material (only after a failed create on scan, or a Flipper request) gets the one-step "New" (`createFromTag`). `FeedItem.prominentAction` makes the Flipper's request (create or link) the `.borderedProminent` button. The Material menu appends a material outside the spec list (a tag's own), so the menu always shows the selection. The sheet drops the Core weight and Tag UID rows: the spec lists five fields, and the UID is in the subtitle ("NTAG 7-byte · <uid>"; a Bambu tag reads "Bambu tag · <uid>").
- [x] Link picker: filter placeholder, row subtitle "colour · brand · n g left", note, "Tag <uid>" badge, empty copy, move-link warning text and "Move link" label when the pick carries another tag, Link disabled without a pick
  - ➕ `LinkPickerModel` (pure) owns the filter (`FeedViewModel.filter` delegates). "Another tag" compares the spool's `tag_uid` with `ParsedTag.bambuddyTagUid` (Bambu UIDs carry the AMS `00000100` suffix) and its tray UUID, so a spool already carrying this tag does not warn. The row's own spool (the one created on scan) is left out of the list. Link needs a pick the filter still shows. With no spools at all the list reads "No spools in Bambuddy." (the spec gives only the no-match copy). The spool id moved from the row to its tooltip.
- [x] write tests: `NewSpoolForm` validation and label defaults; `LinkPickerModel` filtering over material/colour/brand/note, warning text and button label, empty state
- [x] run tests - must pass before task 6

### Task 6: Settings per the spec
- [x] `SettingsForm`: labels "Server URL", "API key"; test result phrases ("Not tested", "Testing…", "Ready · inventory access OK", "Unreachable: no answer within 5 s") with symbols; New items footer; Weighing title "I type the weight as" and footer; Flipper section with the Connection picker (USB only until Phase 2's Bluetooth lands; then USB | Bluetooth with Scan for Flipper and the connection line) and Forget (destructive); save row with "Unsaved changes" / "All changes saved", Revert and Save (default action, disabled when clean)
  - ➕ Model: `SettingsViewModel.Connection` is `testing`, `ready`, `unreachable(reason)` or `failed(message)`; `result(_:)` maps it to a `StatusItem` (not tested: `circle.dashed` gray; testing: spinner; ready: `checkmark.circle.fill` green; unreachable and every other failure: `xmark.circle.fill` red, the failure keeping the session's message since the spec names only the timeout). `BambuddyClient` reports `URLError.timedOut` as `network("no answer within 5 s")`, so the status bar's tooltip and row errors read the same way. `Session.check` is split into `state(spoolmanEnabled:)` and `state(after:)` so the test can keep the bare network reason. `saveStatusText`, `canSave`, `canRevert`, `revert()` (also clears the test result, the save error and the QR status), `connectionLine` from `FlipperLink.connectedTransport` (passed in by `HostModel`), `bluetoothFlipperText` (chosen name, "Searching…" while a scan runs, else "First found"), `canForget` / `forget()`.
  - ➕ View decisions: Phase 2 has landed, so the Connection picker shows USB | Bluetooth wherever both exist. "Core weight" is a labelled field with a "g" suffix. The connection line sits left of Forget at the bottom of the Flipper section. The iPhone keeps the same form, Save row included, until its own spec lands.
  - ⚠️ Forget stays a draft edit, like the rest of this explicit-Save screen (owner decision 4): it clears the remembered Flipper of the chosen transport (the USB port goes back to Automatic, the Bluetooth peripheral to the first found) and takes effect on Save; it is disabled when nothing is remembered. It does not stop the link, so Task 4's "Flipper nearby, not connected" (Connect) state still has no trigger. The disconnected connection line uses the spec's "No Flipper paired" for both transports, although neither pairs.
- [x] write tests: `SettingsViewModel.hasChanges` drives the text and buttons; Revert restores the saved draft; test-result mapping
- [x] run tests - must pass before task 7

### Task 7: Verify acceptance criteria
- [x] every item of "Differences from the shipped macOS app" in the spec is implemented
  - ➕ Checked item by item against the views and view models: 1 `FeedRowView`/`WeightEntry`; 2 `FocusModel` + `FeedKeyboardShortcuts`; 3 `FeedViewModel.setMode`; 4 `saving`/`savedAfterTimeout`/`reopen`/`created`; 5 `StatusItem` tables incl. `nohello` and `nocolors`; 6 the two `ContentUnavailableView`s; 7 Clear + ⌘⌫ + dialog copy; 8 `NewSpoolSheet` and `FeedItem.newAction`; 9 `LinkSpoolSheet`/`LinkPickerModel`; 10 `SettingsForm`; 11 `Session.createOnScan`; 12 `Session` rescan move. Every phrase and SF Symbol in the spec's tables is in the sources.
  - ➕ Gap found and fixed: the main window had no default size; `Window` now has `.defaultSize(width: 1000, height: 600)` (minimum 760 × 420 was already set).
  - ➕ Fixed instead of checked by eye: an unknown Bambu tag flashed "Not in Bambuddy" with New and Link while the create on scan ran. `Session.lookup` now leaves the row in "Looking up" until the create returns (test: `aRowBeingCreatedOnScanStaysLookingUpUntilTheCreateReturns`, with a `FakeInventory.onCreate` hook).
- [x] `swift run FliBuddyMac`: main window and Settings compared with the spec by eye in light and dark; findings recorded in this plan (skipped - not automatable: the app builds and launches, but this session has no Screen Recording permission (`screencapture` fails with "could not create image from display") and no computer-use tools, so nothing was compared by eye. The checks below move to Post-Completion for the owner.)
  - ➕ status bar checks from Task 4: the `.bar` background plus the top `Divider()` does not draw a double separator on macOS 27 (drop the divider if it does); the searching item shows only a spinner where the spec row names `antenna.radiowaves.left.and.right.slash` "(spinning progress)", so confirm it reads right; action buttons fit at 760 pt
  - ➕ sheet checks from Task 5: ⌘⌫ with the insertion point in a weight field opens the Clear dialog rather than deleting text; the Material menu renders as a pop-up in the grouped Form; the Link picker's "Tag <uid>" badge and warning line fit at the sheet's 520 pt
  - ➕ ~~check the "Not in Bambuddy" flash during a create on scan~~: fixed in code (see above), no by-eye check needed
  - ➕ focus checks from Task 2: a field selects its text when focus lands on it; Tab goes field → mode → Save; focus a field, Tab away without typing, scan → the new row takes focus (no hint); ⌘↑/⌘↓ with the insertion point in a field move rows rather than the cursor; focus on a row scrolled off-screen lands in its field once it scrolls in
  - ➕ Settings checks from Task 6: at 640 pt the "I type the weight as" segmented picker and its label fit; the Core weight field and its "g" line up in the grouped Form; both footers show; Forget is red, disabled with nothing remembered; the connection line turns to "Connected over USB" when the Flipper says hello while Settings is open; the test result line fits next to Test connection
- [x] `make test`, `make lint`, `make coverage` pass (555 Swift tests, 321 Python tests, C suite, lint clean; FliBuddyCore line coverage 89.9 %, tools 96.5 %)

### Task 8: Documentation
- [x] `host/README.md`: keyboard shortcuts, row states, status bar actions
  - ➕ New section "The Mac window" (focus rules, keyboard table, row states, status bar items and their buttons); the stale feed paragraph is gone and "Forget Flipper" now reads Forget, a draft edit applied on Save
- [x] `docs/design/macos-ui-spec.md`: mark anything implemented differently and why
  - ➕ New closing section "Implemented differently (2026-10-26)" collects the ➕/⚠️ notes of Tasks 1–7 with their reasons, marking the by-eye items; the intro now exempts it from "this file wins"; the twelve differences are marked implemented

## Technical Details
See `docs/design/macos-ui-spec.md` for every string, symbol, size and rule.
Core weight for the readout comes from the matched spool (`core_weight`), not
from the settings, except for Created rows where it is the settings value
just sent.

## Post-Completion
*Items requiring manual intervention or external systems - no checkboxes, informational only*

- Owner: run the app on the bench with the Flipper; check that focus lands
  on the newest row after a scan, that ⌘S saves without touching the mouse,
  and that the readout matches the scale arithmetic.
- Owner: the by-eye checks listed under Task 7 (status bar, sheets, focus,
  Settings at 640 pt, light and dark), which this run could not do without
  screen access.
- The iPhone, iPad and watch pages of the design get their own spec files
  and plans once Phase 2's targets exist.
