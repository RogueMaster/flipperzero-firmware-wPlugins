# FliBuddy macOS: UI specification from the design

Source: the Claude Design project "FliBuddy mobile app design"
(`FliBuddy Spec.dc.html`, `FliBuddy macOS.dc.html`, `flibuddy-core.js`),
imported on 2026-10-26; the new-spool flow, Save and the test wording follow
the design synced on 2026-11-05. This file is the implementation-ready
distillation for the macOS app; the iPhone, iPad and watch have their own
files. Where this file and the shipped code disagree, this file
wins, except for the deviations recorded in "Implemented differently" at
the end; where it is silent, keep the shipped behaviour. The approved design
document (`docs/superpowers/specs/…design.md`) outranks this file on
behaviour; this file only says how that behaviour looks and feels.

## Assumptions the designer asked the owner to confirm

Taken from the spec page's "Assumptions to confirm". The owner answered the
first on 2026-10-26 (see "Owner decisions" below); 2–6 are adopted as the
designer proposed.

1. Answered: see "Owner decisions".
2. The suggestion is gross (remaining + core). Switching Net/Gross converts
   an untouched suggestion and keeps a number the user typed.
   **Overridden (2026-10-27):** switching converts the number whether typed
   or not (see "Weight rules").
3. Saved rows reopen on click for a re-weigh. Last save wins; the status
   says who saved.
4. macOS keeps the explicit Save section in Settings; iPhone and iPad apply
   settings on change. **Changed in the design (2026-11-05):** settings
   apply on change on every platform, the Mac too, with no Save button.
   The Mac app still has Save and Revert (see "Implemented differently").
   Also new on the page (2026-11-05): after **New** the spool is simply
   added at its label weight ("Added to Bambuddy"), with no weight prompt;
   click the row to weigh it anyway. See "Owner decisions".
5. "Link to existing" warns when the chosen spool already carries a
   different tag and offers to move the link.
6. The watch creates Bambu Lab tags in one tap but cannot fill the form;
   for other tags it says "Create it on iPhone or Mac". Watch is dark only.

## Owner decisions (2026-10-26)

The owner described the intended flow; it replaces the earlier "New is one
action, no weight entry":

- **Main flow is one spool at a time.** Scan, the row for that spool takes
  focus with a suggested weight, type the scale reading, Save sends it.
- **Unknown Bambu tags are created on scan** when the Flipper's **Auto-save
  new** is on (amended by the Flipper UI plan, 2026-10-26; the flag travels
  in every `scan`, default off). Off, the row reads `Not in Bambuddy` with
  the weight entry pre-filled to label + core, and **New** and **Link to
  existing…** under it; nothing reaches Bambuddy until New, Link or a Save,
  which creates the spool and then records the weight. On, when the lookup
  finds nothing for a Bambu tag, the host creates the spool at once with the defaults
  (material, subtype, colour and label weight from the tag; core weight and
  catalog entry from Settings) and the row opens as `Created in Bambuddy`
  with the field pre-filled to label + core and focused. Save sends the
  weight as a separate request. The owner's reasoning (2026-10-26): the
  number of requests does not matter here; spool ids are unique and the
  by-tag lookup makes a rescan find the spool created the first time, so
  scanning never creates a second one. Corrections after creation are
  separate edits: a different weight (Save), or **Link to existing…**, which
  moves the tag to the chosen spool and deletes the spool created on scan
  (builder's call; `DELETE /inventory/spools/{id}` exists). If the delete
  fails the row says the spool was left in the inventory.
  **Superseded twice:** on 2026-10-27 by a new spool card (Save as new,
  Save with weight…, Set weight…), and on 2026-11-05 by the design's flow,
  the next decision.
- **A tag not in Bambuddy (design, 2026-11-05): add at the label weight,
  weigh by choice.** With Auto-save new off, an unknown Bambu tag (one with
  material) reads `Not in Bambuddy` (`questionmark.circle`, secondary) with
  **New** and **Link to existing…**; no field, no focus. **New** adds the
  spool from the tag at its label weight and the row reads `Added to
  Bambuddy` (`plus.circle.fill`, green); with Auto-save new on the row
  reads that at once. An added row does not wait for a weight; a click
  reopens it as Enter weight, pre-filled to label + core, like a saved
  row. New is prominent unless the Flipper asked to link
  (`link_on_host`). The Flipper's **Relink on host** on a row this session
  added offers **Link to existing…** alone, which moves the tag and
  deletes the spool just added.
- **Known spools go straight to weight entry**, as today.
- **Several scans per session are allowed** and every row stays editable.
  This is a secondary flow with more room for human error (identical
  colours), kept for those who prefer it.
- **Rows are in scan order, newest at the top.** Scanning a spool that is
  already in the feed moves its row to the top and reopens it for a weight;
  no duplicate row.
- The Flipper offers the same corrections on its own screens: weight entry
  for new and known spools.

## Stance (from the spec)

- **One number per row.** The weight is the critical path. The newest row that
  needs it takes focus with the suggestion pre-filled and selected; typing
  replaces it. A correct suggestion costs one keystroke (⌘S or Return).
- **Readable at a metre.** The weight value is the only custom-sized type:
  22 pt semibold on macOS, monospaced digits. Everything else is a system text
  style.
- **Status is a word first.** Every state is a short sentence-case phrase plus
  an SF Symbol. Colour reinforces it and never carries meaning alone. The
  swatch always sits next to the title and colour name.
- **Stock parts only.** `List`, `Form`, toolbar, sheets,
  `ContentUnavailableView`, `confirmationDialog`. On macOS 14 the same code
  renders the classic look; no special-casing.

## Window structure

```swift
WindowGroup("FliBuddy") {
  FeedView()
    List(selection: $focusedRow) { ForEach(feed) { FeedRow($0) } }
      .listStyle(.inset)
    .toolbar {
      Button("Clear", systemImage: "trash").keyboardShortcut(.delete)   // ⌘⌫
      SettingsLink { Label("Settings", systemImage: "gearshape") }      // ⌘,
    }
    .safeAreaInset(edge: .bottom) { StatusBar() }   // Flipper · Bambuddy
    .overlay { ContentUnavailableView }              // empty or blocking
    .sheet(item: $sheet) { NewSpoolForm / LinkPicker }
    .confirmationDialog("Clear the feed?")
}
Settings { SettingsForm().formStyle(.grouped) }
```

Main window default 1000 × 600, minimum 760 × 420. Settings window 640 wide,
minimum height 600 (already shipped).

## Feed row (macOS: one line, `.controlSize(.large)`, row height 60)

Left to right: swatch · title · colour line · status · weight field 96 × 34
· Net/Gross segmented (≈150) · Save. Rows that are not in an
entry state have no field, mode or Save; the space stays empty so columns
line up.

| Part | Spec |
|---|---|
| Swatch | 28 pt `Circle()` filled with the tag RGBA, 0.5 pt `.separator` stroke. No colour: dashed `.tertiary` outline. Hidden from VoiceOver; the colour name is read instead. |
| Title | material + subtype, `.headline` (15 sb). Blank tags: "NTAG 7-byte UID 04A2…". |
| Colour line | `.subheadline`, `.secondary` (13): colour name, Bambu colour code, amount, joined by ` · `: "Bambu Green · 10501 · 590 g left of 1000", "Bambu Green · 10501 · label 1000 g" for unknown tags with prefill, empty for a blank tag. Unknown parts are left out; never the hex as a name. The code is the spool's `material_number`, else the one the tag's filament and variant ids name. |
| Status | symbol + phrase, `.subheadline` (12) `.secondary`; `.red` only for errors. |
| Weight field | `TextField` 96 × 34, value `.system(.largeTitle, design: .rounded).bold().monospacedDigit()` scaled to 22 pt via `@ScaledMetric(relativeTo: .largeTitle)`, digits only, 0…9999; unit "g" `.title2.weight(.semibold)` `.secondary` (15) after it. Focus ring on focus. Accessibility label "Gross weight, grams" / "Net weight, grams". |
| Net / Gross | segmented picker; ← → switch when focused. |
| Readout | `.footnote.monospacedDigit()` `.secondary` (11) under the field: "Saves 590 g left · 832 − 242 g core" (gross), "Saves 590 g left" (net), "Type the scale reading" (empty), "Below the core weight (242 g)" in `.red` (negative net). |
| Save | tinted, not filled (the weight is the focus): the tint at 14 % (24 % dark) with tinted text, 28 high; filled (`.borderedProminent`) with Increase Contrast. "Save" / "Saving…"; disabled when empty, negative or saving. |
| Timeout note | `.footnote` under the status: "Flipper timed out after 10 s and showed an error; the save went through." |

VoiceOver reads the header as one element: "PLA Basic, Bambu Green, 590 grams
left of 1000, Enter weight" (the colour code is not read).

**Tag details** (owner decision, 2026-10-27): the UID left the row. A
right-click on a row (context menu, `info.circle`) or the VoiceOver action
"Tag details" opens a sheet titled with the row title: UID, Bambuddy tag
UID (Bambu tags, with the `00000100` suffix), tray UUID, colour, colour
code, filament ID, RGBA, diameter (`1.75 mm`), nozzle (`190–230 °C`), label
weight, core weight and spool (`#42`, `· archived`), each only when known;
identifiers monospaced and selectable. Done closes it. The same
`TagDetails` model feeds the iPhone, iPad and watch.

### Row states and copy

| State | Status text | Symbol | Tint | Entry block |
|---|---|---|---|---|
| Looking up | `Looking up` | `ProgressView` | secondary | no |
| Enter weight | `Enter weight` | `scalemass` | blue | yes, focused when newest |
| Saving | `Saving…` | `ProgressView` | secondary | yes, disabled |
| Saved | `Saved 590 g left · by host` / `· by Flipper` | `checkmark.circle.fill` | green | no; click reopens for a re-weigh |
| Timeout | `Saved 590 g left · by host` + note | `checkmark.circle.fill` | green | no; reopenable |
| Not in Bambuddy (Bambu tag, Auto-save new off) | `Not in Bambuddy` | `questionmark.circle` | secondary | no, never focused; **New** (prominent, adds the spool from the tag at its label weight) · **Link to existing…** |
| Added to Bambuddy (after New, or Auto-save new on) | `Added to Bambuddy` | `plus.circle.fill` | green | no, never focused; click reopens it as Enter weight, pre-filled label + core |
| Not in Bambuddy (no material) | `Not in Bambuddy; tag has no material` | `questionmark.circle` | secondary | **New…** (prominent, the form) · **Link to existing…** |
| Error | `Error: Bambuddy unreachable` | `exclamationmark.triangle.fill` | red text | on a spool yes, value kept for retry; after a failed New or Link the row's buttons stay as the retry |
| Updating | `Updating…` (Mark as new / empty or Delete from the Flipper) | `ProgressView` | secondary | yes, disabled |
| Marked as new | `Marked as new · 1000 g left · by Flipper` | `arrow.counterclockwise.circle.fill` | green | no; reopenable |
| Marked as empty | `Marked as empty · archived · by Flipper`; the colour column reads `Jade White · archived` | `archivebox` | secondary | no; reopenable (a Save restores the spool) |
| Deleted | `Deleted from Bambuddy · by Flipper` | `trash` | secondary | **New** (tag with material) or **New…** (without) · **Link to existing…** |
| Archived spool scanned again | `Enter weight`; the colour column reads `… · archived` | `scalemass` | blue | yes; Save restores the spool, then records the weight |
| Flipper asks to create | `Flipper asks to create it here` | `dot.radiowaves.left.and.right` | blue | **New** (or **New…** without material) prominent · **Link to existing…** |
| Flipper asks to link | `Flipper asks to link it here` | `dot.radiowaves.left.and.right` | blue | **Link to existing…** first and prominent · **New** / **New…** |
| Flipper asks to relink a spool added here | `Flipper asks to link it here` | `dot.radiowaves.left.and.right` | blue | **Link to existing…** alone (moves the tag, deletes the spool just added); no entry |

### Weight rules (from the prototype model)

- Suggestion = `base + (mode == gross ? core : 0)` where `base` is the spool's
  remaining grams (for a new spool: the label weight). Shown selected; the
  first keystroke replaces it.
- Switching Net/Gross converts the number on screen, typed or not (owner,
  2026-10-27): to net `n − core`, clamped at 0 as on the Flipper; to gross
  `n + core`, at most 9999. An untouched suggestion stays untouched (still
  selected, the first key replaces it); a typed number stays typed; an
  empty field stays empty.
- `net = gross − core` in gross mode; Save sends gross to Bambuddy as today.
- After a successful save the row shows `Saved n g left · by host`, keeps the
  value as the new base, and focus moves to the next newest row that still
  waits for a weight (Enter weight); rows not in Bambuddy and added rows
  never take focus. If none, the selection stays and the next scan takes
  focus.
- Focus never steals from a field the user is typing in (`touched`): the new
  row gets a blue focus hint and waits.
- Digits only, at most four; leading zeros drop, so a digit typed after a
  lone `0` replaces it (shared with the iPhone's number pad). While a sheet
  is open (New, Link, Tag details) no weight field holds the keyboard; the
  focused row's field takes it back when the sheet closes.
- A save from the Flipper on a row the host shows as waiting turns it into
  `Saved … · by Flipper` and clears the host's draft.
- Rows are ordered by their last scan, newest first. A scan of a tag already
  in the feed moves that row to the top and reopens it for a weight instead
  of adding a second row.

### Keyboard

| Action | Keys |
|---|---|
| Save focused row | ⌘S or Return in the field |
| Clear feed (with confirmation) | ⌘⌫ |
| Settings | ⌘, |
| Field → mode → Save | ⇥ (⇧⇥ back) |
| Switch Net/Gross when the picker has focus | ← → |
| Next / previous row | ⌘↓ / ⌘↑ |

Clear asks "Clear the feed?" with "This clears the session log only. Nothing
changes in Bambuddy." Buttons Clear (destructive) · Cancel.

## Status bar (bottom safe-area inset)

Two items, each symbol + phrase in `.footnote.weight(.medium)` (11), plus one
optional action button on the right of the item:

| Flipper | Text | Symbol | Tint | Action |
|---|---|---|---|---|
| none | `No Flipper · searching` | `antenna.radiowaves.left.and.right.slash` (spinning progress) | gray | |
| usb | `Flipper · USB` | `cable.connector` | green | |
| not started | `Flipper not connected` | `antenna.radiowaves.left.and.right` | orange | Settings |
| ble | `Flipper · Bluetooth` | `antenna.radiowaves.left.and.right` | green | |
| nohello | `Open FliBuddy on the Flipper` | `app.dashed` | orange | |
| other version | `Flipper speaks protocol 1, host speaks 2` (tooltip: the side to update) | `exclamationmark.triangle` | orange | |

| Bambuddy | Text | Symbol | Tint | Action |
|---|---|---|---|---|
| not set up | `Bambuddy not set up` | `server.rack` | orange | Settings |
| unchecked | `Bambuddy · not checked` | `server.rack` | gray | Check |
| ready | `Bambuddy ready` | `server.rack` | green | |
| unreachable | `Bambuddy unreachable` | `wifi.exclamationmark` | red | Retry |
| rejected | `Bambuddy rejected the API key` | `lock.trianglebadge.exclamationmark` | red | Settings |
| noperm | `API key lacks inventory permission` | `lock.trianglebadge.exclamationmark` | red | Settings |
| spoolman | `Bambuddy is in Spoolman mode` | `nosign` | red | Retry |
| nocolors | `Bambuddy ready · colour names unavailable` | `paintpalette` | orange | |

The bar is `.bar` material on macOS 27, a plain bar with a 0.5 pt top
separator on macOS 14 (system does this).

## Empty and blocking states (`ContentUnavailableView`)

- Empty feed: symbol `wave.3.right`, "No scans yet", "Hold a spool's tag to
  the Flipper running FliBuddy."
- Spoolman mode (blocking, replaces the list even with rows): `nosign` in
  red, "Bambuddy is in Spoolman mode", "FliBuddy works with Bambuddy's own
  inventory. Switch Bambuddy's inventory back from Spoolman in its settings,
  then retry.", button Retry.

## Sheets

**New spool** (only for tags without prefill; Bambu tags create directly):
title "New spool", subtitle "NTAG 7-byte · <uid>". Fields: Material (required,
menu: Choose… / PLA, PETG, ABS, ASA, TPU, PA, PC, PVA), Subtype, Colour (name),
Brand, Label weight (g, digits, default 1000). Footer: "Material is required.
The spool is added at its label weight." Buttons Cancel
· Create (disabled without material).

**Link to existing spool**: title "Link to existing spool", "Tag <uid>", filter
field (placeholder "material, colour, brand or note"), list rows: swatch,
title, "colour · brand · n g left", note, "Tag <uid>" when already tagged;
single selection; empty: "No spools match “<q>”." When the chosen spool
carries a different tag, a warning line: "<title> <colour> is linked to tag
<tag>. Linking moves it to <uid>." and the button reads "Move link" instead
of "Link". Buttons Cancel · Link/Move link (disabled without a pick).

## Settings window (grouped Form)

The design (2026-11-05) draws four sections that apply on change, with no
Save row, and the version footer `FliBuddy 1.0 (42)` under them; the app
keeps the save row below (see "Implemented differently").

```swift
Form {
  Section("Bambuddy") { TextField("Server URL"); SecureField("API key")
    Button("Test connection") + result line }
  Section("New items") { Picker("Empty spool"); TextField("Core weight") + "g" }
    footer: "Weight of everything that is not filament; 242 g = Bambu reusable spool 215 g + refill core 27 g."
  Section("Weighing") { Picker("I type the weight as", Net | Gross).pickerStyle(.segmented) }
    footer: "Gross is the whole spool on the scale. Each row can still switch."
  Section("Flipper") { Picker("Connection", USB | Bluetooth)
    USB: Picker("Port", ports)      Bluetooth: LabeledContent("Flipper", name | "Searching…") + Button("Scan for Flipper")
    connection line: "Connected over USB" / "Connected over Bluetooth" / "No Flipper paired"
    Button("Forget", role: .destructive) }
  Section { HStack { Text(dirty ? "Unsaved changes" : "All changes saved").secondary; Spacer(); Button("Revert"); Button("Save").keyboardShortcut(.defaultAction) } }
}.formStyle(.grouped)
```

Test connection results: "Not tested" · "Testing…" (progress) ·
"Ready · inventory OK" (`checkmark.circle.fill`, green, one wording on
every platform) ·
"Unreachable: no answer within 5 s" (`xmark.circle.fill`, red). Save is
disabled while nothing changed; Revert restores the saved values. The
Bluetooth parts exist once Phase 2 lands; before that the Connection picker
shows USB only.

## Tokens

Spacing (pt): 4 symbol↔text in a status line · 8 between controls in a
group · 12 swatch↔text, header↔entry, between rows · 16 row padding ·
20 sheet and Form padding · 24 between Form sections · 32 empty-state stack.

Radii: system shapes only (`Circle()`, `.buttonBorderShape(.capsule)`, weight
field `.rect(cornerRadius: 8)` on macOS). Never hard-code the 27 vs 14 look.

Type roles (macOS sizes at default content size): weight 22 sb rounded
monospaced digits · row title `.headline` 15 sb · colour/remaining
`.subheadline` 13 · status `.subheadline` 12 · readout `.footnote.monospacedDigit()` 11 · status bar
`.footnote.weight(.medium)` 11 · Save `.headline` on the tinted button ·
unit "g" `.title2.weight(.semibold)` 15.

Colours: `.primary`, `.secondary`, `.tertiary`, `.tint` (blue: Save, focus,
Flipper requests), `.red` (errors, Forget, Clear), `.green` (saved, added,
ready), `.orange` (warnings: timeout, advertising, no hello), `.gray`
(searching, not checked); backgrounds and fills are the system grouped
backgrounds and `tertiarySystemFill`; `.separator` for the swatch stroke.

SF Symbols: `scalemass`, `checkmark.circle.fill`, `plus.circle.fill`,
`questionmark.circle`, `exclamationmark.triangle.fill`, `xmark.circle.fill`,
`dot.radiowaves.left.and.right`, `antenna.radiowaves.left.and.right`,
`antenna.radiowaves.left.and.right.slash`, `cable.connector`, `app.dashed`,
`server.rack`, `wifi.exclamationmark`, `lock.trianglebadge.exclamationmark`,
`nosign`, `paintpalette`, `wave.3.right`, `gearshape`, `trash`, `link`,
`arrow.left.arrow.right`.

## Differences from the shipped macOS app

All twelve are implemented (plan `docs/plans/20261026-macos-ui-from-design.md`).
Since then (plan `docs/plans/20261027-apple-ui-from-design.md`, shared with
the iPhone, iPad and watch) the UID moved from under the title to Tag
details, the colour line gained Bambu's colour code, and Net/Gross converts
a typed number too (items 1 and 3 below are superseded there).


1. Row layout on one line with fixed column widths; UID under the title;
   readout line; "g" unit; `.controlSize(.large)`.
2. Focus management: newest waiting row takes focus with the suggestion
   selected; focus does not steal from a touched field; moves on after a
   save; ⌘↑/⌘↓ navigation; Tab order field → mode → Save.
3. Net/Gross conversion rule for untouched suggestions.
4. Row states: `Saving…`, timeout note, reopen a saved row by click,
   Created pre-fills label + core and keeps the row open.
5. Status bar: symbols, tints, phrases and the inline actions (Connect,
   Check, Retry, Settings); the `nohello` state.
6. Blocking Spoolman `ContentUnavailableView` with Retry; empty state copy.
7. Clear with confirmation dialog and ⌘⌫.
8. New spool sheet: material menu, footer text, "New…" vs "New" labels.
9. Link picker: filter placeholder, row subtitle format, "Tag <uid>" badge,
   move-link warning and button label.
10. Settings: "Server URL" label, test result phrases, Weighing footer,
    "Unsaved changes / All changes saved" row with Revert, Forget
    (destructive).
11. Unknown Bambu tags are created on scan with the defaults when the
    Flipper's Auto-save new is on; the row opens as `Created in Bambuddy`
    with label + core pre-filled; Save sends the weight separately. With
    Auto-save new off the row reads `Not in Bambuddy` (see the row table).
    Superseded on 2026-10-27 by a new spool card and on 2026-11-05 by the
    design's flow (Task 10 of `docs/plans/20261105-release-1.0.md`): `Not
    in Bambuddy` with New and Link to existing…, `Added to Bambuddy`.
12. Rescanning a tag moves its row to the top and reopens it; newest first.
13. Save is tinted, filled with Increase Contrast (2026-11-05); the test
    result reads `Ready · inventory OK`.

## Implemented differently (2026-10-26, updated 2026-11-05)

Where the app departs from this file or fills in what it leaves open, and
why. Items marked "by eye" still wait for the owner's check on screen.

Window and keyboard
- The main window is a single `Window`, not a `WindowGroup`: the feed and
  the Flipper link are app-wide, so a second window would show the same
  session. Clear is disabled while the feed is empty.
- ⌘S, ⌘↑ and ⌘↓ are invisible buttons at the feed level, so each shortcut
  exists once rather than on every row. Selecting the suggestion on focus
  and the Tab order field → mode → Save are AppKit's (a field selects its
  text when it becomes first responder; Tab follows view order and reaches
  the picker and Save only with keyboard navigation on). By eye.
- "Touched" is the focused row's typed draft. A row that lost focus does
  not hold it, and writing back the unchanged suggestion does not count.
  The row that waits behind a touched field gets a tinted outline.

Feed row
- The one-line layout is macOS only; iOS shares the row and stacks the
  entry block under the header until the iPhone/iPad spec lands.
- Title, colour and status columns share the width equally and the entry
  block is a fixed 360 pt, so columns line up across states.
- The weight value is bold, as in the row table; the Stance line says
  semibold.
- The scan time is the row's tooltip, not a column. Saved rows reopen on a
  click; VoiceOver gets a "Re-weigh" action for the same thing.
- "Saved … by host" after a Flipper timeout is approximate: the host
  measures its own latency from the intent to the `result` against the
  Flipper's 10 s, since it cannot see the Flipper's clock.
- A row whose spool is being created on scan stays "Looking up" until the
  create returns, rather than flashing "Not in Bambuddy" with its buttons.
  A failed create shows "Error: <reason>" with New and Link to existing…
  as the retry.
- "Not in Bambuddy" also covers a tag with material that has no spool any
  more (e.g. the link after deleting the added spool failed): the same
  buttons. "Error: <reason>" carries the session's message, of which
  "Bambuddy unreachable" is one.
- **Link to existing…** on an added row (offered only when the Flipper
  asks to relink it) deletes the spool added here first and then links
  the tag to the chosen spool. The other order cannot
  work: Bambuddy answers `PATCH /inventory/spools/{id}/link-tag` with 409
  `tag_already_linked` while another active spool carries the tag. If the
  delete fails, nothing changes and the row reads "Spool <id> created on
  scan could not be deleted, so the tag stays on it: <reason>" (Save and
  Link stay); if the link fails after the delete, the row has no spool and
  offers New and Link again. A created spool already gone from Bambuddy
  counts as deleted. Once a weight is saved on it, or Settings point at
  another Bambuddy, the spool is no longer deleted and Link is not offered
  on that row.

Status bar
- A bottom `safeAreaInset` on `.bar` with a top `Divider()`. By eye: drop
  the divider if macOS 27 draws a double separator.
- "No Flipper · searching" shows a spinner in place of the symbol, not
  `antenna.radiowaves.left.and.right.slash` beside a spinner. By eye.
- Each item's tooltip carries the detail the phrase leaves out: the USB
  port and the firmware, the link's error, Bambuddy's message.
- States the tables leave open: waiting for `hello` under 2 s and "No
  Flipper found" count as searching; any other reason the link is down
  ("Bluetooth is off", "Resource busy") is shown as the phrase itself,
  orange, with the slash symbol; "Checking Bambuddy…" with a spinner while
  a check runs. (Not set up, the rejected key and the other version are in
  the tables above since the Spec page drew them, 2026-11-05.)
- "Flipper not connected" (Settings) is shown while the link is not
  started. The Mac and the watch start it at launch, so it shows only
  before that; on the iPhone and iPad it is the state until a Flipper is
  chosen (creating the Bluetooth central asks for permission, which
  belongs to Scan for Flipper), and Forget This Flipper stops the link
  there. The host does not probe for advertising Flippers, so the design's
  "nearby" is not claimed.

- New and Link to existing… stand in the 360 pt entry column, bordered,
  the prominent one `.borderedProminent`, sized to their titles; the
  2026-10-27 `ViewThatFits` fallback to smaller sizes went with the third
  button. By eye.
- While the Flipper weighs a tag not in Bambuddy (its Add and weigh) the
  row reads `Saving…` with no field and no buttons. While New or Link runs
  from the host the buttons dim and a small spinner sits beside them; the
  status keeps its phrase. A failed one keeps the buttons for a retry.
- A deleted row offers New (or New… without material) and Link, no
  weight.
- Save is a custom tinted button style on the Mac (`SaveButtonStyle`): a
  bordered Mac button does not take the tint. 28 high and at least 64
  wide, a 14 pt corner on macOS 26 and 6 pt before, as the macOS page
  draws for each; 12 pt sides rather than the page's 16, so field, mode
  and Save stay in the 360 pt entry column; dimmed to 45 % when disabled. By eye: the focus ring when Tab
  reaches Save with keyboard navigation on.
- After New, Link, or a Save that created the spool, the host sends the
  Flipper a fresh `lookup` for the row's scan (PROTOCOL.md, Session), so a
  Flipper still showing that tag shows the spool.
- `Marked as empty` and `Deleted` say which side did it, as `Saved` and
  `Marked as new` do; the host has no buttons for Mark or Delete, so it is
  the Flipper.
- The watch has one New button and no Link (`watchos-ui-spec.md`).

Sheets
- New spool: no Core weight or Tag UID rows; the spec lists five fields and
  the UID is in the subtitle. A Bambu tag's subtitle reads "Bambu tag ·
  <uid>". The Material menu adds a material outside the list (the tag's
  own), so the menu always shows the selection.
- "New…" opens the form for a tag without material; a tag with material
  gets "New", one step from the tag's data, instead. The action the
  Flipper asked for is the prominent button.
- Link picker: the row's own spool (the one created on scan) is left out of
  the list. "Carries a different tag" compares the spool's `tag_uid` with
  the tag's Bambuddy UID (Bambu UIDs carry the AMS `00000100` suffix) and
  its tray UUID, so a spool already on this tag does not warn. With no
  spools at all the list reads "No spools in Bambuddy."; the spool id is
  the row's tooltip.

Settings
- Test connection: "Unreachable: no answer within 5 s" for a timeout; any
  other failure keeps the session's message with `xmark.circle.fill` in
  red, since the spec names only the timeout.
- Phase 2 has landed, so the Connection picker shows USB | Bluetooth
  wherever both exist. "Core weight" is a labelled field with a "g" suffix;
  the connection line sits left of Forget.
- Forget is a draft edit like every other field of this explicit-Save
  screen: it clears the remembered Flipper of the chosen transport (USB
  port back to Automatic, Bluetooth peripheral to the first found), takes
  effect on Save, is disabled when nothing is remembered, and does not stop
  the link.
- The disconnected connection line reads "No Flipper paired" for both
  transports, although neither pairs.
- The design now applies settings on change on every platform, the Mac
  too (2026-11-05). The app does that on the iPhone and iPad
  (`SettingsViewModel.appliesOnChange`, see `ios-ui-spec.md`); the Mac
  form keeps the explicit Save and Revert row, with "Unsaved changes" /
  "All changes saved", and Forget is a draft edit until Save. Not changed
  in Task 11 of the 1.0 plan, which brought the shared behaviour only.
- The design's Mac form has no Reload catalog and ends with the version
  footer `FliBuddy 1.0 (42)`. The Mac form keeps **Reload catalog** under
  the core weight and has no version footer; the catalog loads each time
  the Settings window opens (`SettingsViewModel.opened()`).
- A found Flipper's signal reads `Nearby · strong` / `Nearby` (shared with
  the iPhone and the watch), not dBm.
