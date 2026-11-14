# FliBuddy iPhone: UI specification from the design

Source: the Claude Design project "FliBuddy mobile app design"
(`docs/design/source/FliBuddy Spec.dc.html` and
`docs/design/source/FliBuddy iPhone.dc.html`), imported on 2026-10-27 and
synced again on 2026-11-05; the Spools tab (the read-only inventory) was
added to the page on 2026-11-05 for Task 12 of the 1.0 plan and waits for
the owner's look. The page imports the shared prototype model
`flibuddy-core.js` (session, row view-model, connection texts:
`docs/design/source/flibuddy-core.js`). The row copy comes from the core's `vm`, the banner from its
`statusVM` (`FLIP` / `BAMBU` tables), the Link picker rows from `invVM`, the
keypad and focus behaviour from its `Session`.

This file is the implementation-ready distillation for the iPhone app
(`host/Apple/iOS`, views in `host/Sources/FliBuddyUI`). The iPad uses it for
its compact-width layout (a narrow multitasking window) and shares the
row states with it, see `ipados-ui-spec.md`; the watch has
`watchos-ui-spec.md`; the Mac has `macos-ui-spec.md`.

Precedence: where this file and the shipped code disagree, this file wins;
where it is silent, keep the shipped behaviour. The owner decisions below
outrank the design pages. The approved design document
(`docs/superpowers/specs/…design.md`), `protocol/PROTOCOL.md` (wire v2) and
`docs/design/flipper-ui-spec.md` outrank this file on behaviour; this file
only says how that behaviour looks and feels on the iPhone. Deviations made
while building it go into an "Implemented differently" section at the end,
as in the macOS spec.

## Assumptions the designer asked the owner to confirm

The Spec page's "Assumptions to confirm" lists eight; the macOS spec
recorded the owner's first answers (2026-10-26), and the design synced on
2026-11-05 settles the rest:

1. *The suggestion is gross; switching Net/Gross converts an untouched
   suggestion and keeps a typed number.* The suggestion is gross by default
   (Settings decide). **Overridden:** switching converts the number whether
   typed or not (see "Owner decisions").
2. *After New the spool is simply added at its label weight ("Added to
   Bambuddy"), with no weight prompt; tap or click the row to weigh it
   anyway.* Adopted (2026-11-05). It replaces the 2026-10-27 new spool card
   (Save as new, Save with weight…, Set weight…, a revealed entry with
   Cancel), which is gone.
3. *Saved rows reopen on tap for a re-weigh; last save wins; the status
   says who saved.* Adopted; an added row reopens the same way.
4. *The watch creates Bambu Lab tags in one tap but cannot fill the form;
   for other tags it says "Create it on the iPhone".* Adopted, see the
   watch spec.
5. *Settings apply on change on every platform; there is no Save button.*
   Adopted on the iPhone and iPad: no Save / Revert row. The Mac still has
   one (`macos-ui-spec.md`, "Implemented differently").
6. *Link to existing warns when the chosen spool carries a different tag and
   offers to move the link.* Adopted.
7. *Watch is dark only; every other surface light and dark.* Adopted: the
   iPhone follows the system appearance.
8. *Open icons stand in for SF Symbols; hover shows the real name.* The SF
   Symbol names below are the ones to use.

## Owner decisions (2026-10-26, 2026-10-27)

They apply to every platform and override the design where they conflict.

- **Rows strictly in scan order, newest on top.** A rescan of a tag already
  in the feed moves its row to the top and reopens it for a weight; no
  duplicate row. Several scans per session, every row stays editable.
- **Auto-save new lives on the Flipper** (protocol v2, `scan.autosave`,
  default Off). The host has no switch of its own.
  - On: an unknown Bambu tag (one with material) is added at once with the
    tag's defaults at its label weight; the row reads `Added to Bambuddy`
    and waits for nothing.
  - Off: the row reads `Not in Bambuddy` (`questionmark.circle`) with
    **New** and **Link to existing…**; no field, no focus, no number pad.
    New adds the spool from the tag at its label weight and the row reads
    `Added to Bambuddy`. Nothing reaches Bambuddy until New or Link.
  - An added row reopens on a tap like a saved one: `Enter weight`,
    pre-filled to label + core, focused; its Save records the weight.
  - A Flipper request makes the asked action the prominent one (New unless
    the Flipper asked to link). The Flipper's **Relink on host** on a row
    this session added offers **Link to existing…** alone, which moves the
    tag and deletes the spool just added.
- **Known spools:** a weight that comes in (from the Flipper) is saved at
  once. The Flipper's actions Mark as new (`full`), Mark as empty (`empty`,
  archives) and Delete (`delete`) show on the row. An archived spool
  scanned again shows as archived; a weight saved on it restores it.
- **Colour facts on new tags** (`tag_info`): Bambuddy's colour name (else
  Bambu's English name from `tools/data/bambu_color_codes.json`), Bambu's
  colour code (e.g. `11803`, stored as `material_number`), filament id,
  diameter, nozzle range. The row shows the colour name and the colour
  code; UID, filament id, diameter and nozzle range live in a details view,
  not in the row.
- **Net/Gross converts the number**, typed or not: gross → net subtracts
  the core, net → gross adds it.
- **The iPhone is portrait only in 1.0** (2026-11-05): landscape was never
  designed.

## Stance (from the Spec page)

- **One number per row.** The newest row that needs a weight takes focus
  with the suggestion pre-filled and selected; the first key replaces it. A
  correct suggestion costs one tap (Save).
- **Readable at a metre.** The weight value is the only custom-sized type:
  34 pt on iPhone, monospaced digits. Everything else is a system text style
  and follows Dynamic Type; the weight uses `@ScaledMetric(relativeTo:
  .largeTitle)` and caps at AX3 so it stays on one line.
- **Status is a word first.** Every state is a short sentence-case phrase
  plus an SF Symbol; colour never carries meaning alone. The swatch always
  sits next to the title and colour name.
- **Stock parts only.** `TabView`, `NavigationStack`, `List`, `Form`, sheets,
  `ContentUnavailableView`, `confirmationDialog`. Liquid Glass only where
  the system puts it, plus the connection banner.
- **One-handed at the bench.** The number pad rises on its own with the
  suggestion selected and Save stays above it.

## App structure

```swift
TabView {
  Tab("Feed", systemImage: "list.bullet") {
    NavigationStack {
      FeedList()                       // List { ForEach(feed) { Section { FeedRow($0) } } }
        .listSectionSpacing(.compact)
        .navigationTitle("Feed")       // large title
        .toolbar { Button("Clear", systemImage: "trash") }   // leading, icon only
        .safeAreaInset(edge: .top) { ConnectionBanner() }
        .overlay { ContentUnavailableView }                  // empty or blocking
        .sheet(item: $newRow)  { NewSpoolForm() }
        .sheet(item: $linkRow) { LinkPicker().searchable(…) }
        .sheet(item: $detailsRow) { TagDetails() }           // owner decision, not in the design
        .confirmationDialog("Clear the feed?")
    }
  }
  Tab("Spools", systemImage: "shippingbox") {               // read-only inventory
    NavigationStack {
      SpoolList()                                            // List, one card of rows
        .navigationTitle("Spools")
        .searchable(text: $query, prompt: "Material, colour, brand or note")
        .toolbar { Menu { sortPicker; materialPicker } }     // trailing
        .refreshable { await spools.refresh() }
        .sheet(item: $spool) { SpoolDetail() }
    }
  }
  Tab("Settings", systemImage: "gearshape") {
    NavigationStack { SettingsForm() }                       // applies on change
      .fullScreenCover { QRScanner() }
  }
}
// weight field
TextField("Weight", value: $grams, format: .number)
  .keyboardType(.numberPad)
  .focused($focus, equals: row.id)
  // select-all on focus via TextSelection (iOS 18+)
```

The app runs in portrait only. The tab bar hides while the number pad is
up and comes back when it lowers.
The Clear button sits at the trailing end of the navigation bar, icon only
(`trash`), accessibility label "Clear feed"; disabled while the feed is
empty (the iPhone page draws it top right).

## Connection banner (top safe-area inset, under the large title)

One compact capsule replaces the macOS status bar. When both sides are
healthy it reads `Flipper connected · Bambuddy ready` with
`checkmark.circle.fill` in green. Otherwise it shows each side that has a
problem, as that state's short phrase, symbol, tint and action: one line
when one side is down, two lines in the same capsule (Flipper first, then
Bambuddy, each with its own chip) when both are (owner decision
2026-10-27; the Spec page draws it since 2026-11-05, "Both sides down").

| Part | Spec |
|---|---|
| Shape | `Capsule()`, height 40, `.glassEffect()` on iOS 26+ (`.regularMaterial` below), 0.5 pt hairline, horizontal padding 14 leading / 6 trailing, 16 pt outer margin |
| Symbol | 16 pt, in the state's tint; a 12 pt spinner in its place while searching |
| Text | `.footnote.weight(.medium)` (13), `.primary`; a phrase that does not fit beside its chip wraps with the chip under it; text sizes above the second accessibility size are capped in the banner |
| Action | capsule chip height 28, padding 12, `.tint` fill, white `.footnote.weight(.semibold)` label |

Short phrases (core `FLIP` / `BAMBU`, field `short`):

| Side | State | Phrase | Symbol | Tint | Action |
|---|---|---|---|---|---|
| Flipper | none | `No Flipper · searching` | spinner (`antenna.radiowaves.left.and.right.slash` for VoiceOver) | gray | |
| Flipper | not started | `Flipper not connected` | `antenna.radiowaves.left.and.right` | orange | Settings |
| Flipper | ble | `Flipper connected` | `antenna.radiowaves.left.and.right` | green | |
| Flipper | nohello | `Open FliBuddy on the Flipper` | `app.dashed` | orange | |
| Flipper | other version | `Flipper speaks protocol 1, app speaks 2` (the side to update in the hint) | `exclamationmark.triangle` | orange | |
| Bambuddy | not set up | `Bambuddy not set up` | `server.rack` | orange | Settings |
| Bambuddy | unchecked | `Bambuddy not checked` | `server.rack` | gray | Check |
| Bambuddy | ready | `Bambuddy ready` | `server.rack` | green | |
| Bambuddy | unreachable | `Bambuddy unreachable` | `wifi.exclamationmark` | red | Retry |
| Bambuddy | key rejected | `Bambuddy rejected the API key` | `lock.trianglebadge.exclamationmark` | red | Settings |
| Bambuddy | noperm | `Key lacks inventory permission` | `lock.trianglebadge.exclamationmark` | red | Settings |
| Bambuddy | spoolman | `Bambuddy is in Spoolman mode` | `nosign` | red | Retry |
| Bambuddy | http address | `Bambuddy needs HTTPS` (hint `FliBuddy needs an https:// address.`) | `lock.slash` | red | Settings |
| Bambuddy | certificate | `Certificate not trusted` (hint: this device does not trust it) | `lock.trianglebadge.exclamationmark` | red | Retry |
| Bambuddy | nocolors | `Color names unavailable` | `paintpalette` | orange | |

USB is macOS only; the iPhone never shows `Flipper · USB`.

The `ble` and `ready` rows only appear inside the combined healthy text.
Spoolman mode is blocking anyway (the feed is replaced). The states the
Spec page does not draw keep their macOS phrases and tints: `Checking
Bambuddy…` with a spinner, a link problem such as `Bluetooth is off` orange
with the slash symbol. Tapping the banner outside the chip does nothing;
the detail the phrase leaves out is its accessibility hint. The chip reads
with its side: `Retry Bambuddy`, `Check Bambuddy`, `Flipper settings`,
`Bambuddy settings`.

While the empty feed shows a setup step (Connect Bambuddy, Bambuddy needs
HTTPS, Find your Flipper) the banner leaves out its setup lines (`Flipper
not connected` with no Flipper chosen yet, `Bambuddy not set up`, `Bambuddy
needs HTTPS`): the empty feed says the same with its button, so a first run
shows no banner (owner ruling 2026-11-05, Task 14). Real problems of a
set-up app stay (`No Flipper · searching`, `Bambuddy unreachable`, a
rejected key), and so does every line once the feed has rows. The iPad
sidebar keeps both rows at all times.
Settings switches to the Settings tab.

## Feed row (stacked card, one `Section` per row)

Card: `secondarySystemGroupedBackground`, inset-grouped corner (≈ 24),
padding 16, 12 between cards (`.listSectionSpacing(.compact)`).

Header, left to right: swatch · (title with the UID trailing on the same
line → colour line → status) . The owner decision moves the UID out of the
row: the title line holds the title alone (see "Tag details").

| Part | Spec |
|---|---|
| Swatch | 28 pt `Circle()` from the tag or spool RGBA, 0.5 pt `.separator` stroke; no colour: 1.5 pt dashed `.tertiary` outline. Hidden from VoiceOver. 12 pt to the text. |
| Title | material + subtype, `.headline` (17 sb). Blank tags: `NTAG 7-byte UID 04A2…`. |
| Colour line | `.subheadline` `.secondary` (15), 2 pt under the title: colour name, Bambu colour code, amount, joined by ` · `: `Bambu Green · 10501 · 590 g left of 1000`, `Bambu Green · 10501 · label 1000 g` (no spool yet), `Jade White · archived`. Parts that are unknown are left out; never the hex. While Bambuddy's colour names are unavailable the name is left out. The code comes from the spool's `material_number` or the tag's `tag_info.code`. |
| Status | symbol 17 pt in its tint + phrase, `.subheadline` (15), 6 pt under the colour line; `.secondary`, `.red` only for errors. |
| Timeout note | `exclamationmark.triangle.fill` 15 pt orange + `.footnote` (13) `.secondary`: `Flipper timed out after 10 s and showed an error; the save went through.` |

Entry block (12 pt under the header, 10 pt between its parts), present in
the entry states of the table below:

| Part | Spec |
|---|---|
| Weight field | full width, height 64, `tertiarySystemFill`, `.rect(cornerRadius: 14)`, padding 16; focus ring 2 pt `.tint`. Value `.system(.largeTitle, design: .rounded).bold().monospacedDigit()` scaled to 34 pt; the selected suggestion is highlighted (system selection); an empty field shows `0` in `.tertiary`. Unit `g` `.title2.weight(.semibold)` `.secondary` (22) after the value. The mode word (`gross` / `net`) right-aligned inside the field, `.footnote` `.secondary` (13). Accessibility label `Gross weight, grams` / `Net weight, grams`. |
| Net / Gross | segmented, full width, height 32, capsule track; labels 14 sb. |
| Readout | `.footnote.monospacedDigit()` `.secondary` (13): `Saves 590 g left · 832 − 242 g core` (gross), `Saves 590 g left` (net), `Type the scale reading` (empty), `Below the core weight (242 g)` in `.red`. |
| Save | full width, height 44, a tinted button (`.bordered`, capsule: the tint at about 14 % with `.tint` text), `.headline` (17 sb): `Save` / `Saving…`; disabled (45 % look) when empty, below the core or saving. Not filled: the weight is the focus. With Increase Contrast it is `.borderedProminent` (the tinted text reaches about 3.4:1). |
| Actions | rows with no spool (Not in Bambuddy, Flipper asks, Deleted): **New** (**New…** without material) and **Link to existing…** as two equal 44 pt capsules, 10 apart, 15 sb; the prominent one first, `.tint` with white text (New unless the Flipper asked to link), the other `tertiarySystemFill` with `.tint` text. |

A row without an entry block or actions has a minimum height of 76.

VoiceOver reads the header as one element: "PLA Basic, Bambu Green, 590
grams left of 1000, Enter weight" (the colour code is not read). Without
Bambuddy's colour name it reads Bambu's name for the tag's colour, else a
plain word for the swatch (`dark blue`, `light gray`), never the hex; the
Spools rows do the same. Grams go through `Grams.text` (`.formatted()`
without grouping: `1000`, as the design and the Flipper write it; owner
ruling); the weight field takes plain digits.
Title up to two lines, colour line and status up to three: wrapped, not
cut. Scrolling to the focused row does not animate with Reduce Motion. Rows that
can reopen get a "Weigh again" action ("Weigh" on an added spool that was
never weighed); every row gets a "Tag details" action. An error status is
a plain sentence (`Error: Bambuddy didn't answer`); the technical detail
(`Bambuddy HTTP 500: …`, the system reason) is its accessibility hint and
tooltip.

### Row states and copy

Same model and texts as the macOS table; only the entry column differs.

| State | Status text | Symbol | Tint | Entry block / actions |
|---|---|---|---|---|
| Looking up | `Looking up` | spinner | secondary | none |
| Enter weight | `Enter weight` | `scalemass` | blue | entry, focused when newest |
| Saving | `Saving…` | spinner | secondary | entry, disabled |
| Saved | `Saved 590 g left · by iPhone` / `· by Flipper` | `checkmark.circle.fill` | green | none; tap reopens |
| Timeout | `Saved 590 g left · by iPhone` + timeout note | `checkmark.circle.fill` | green | none; tap reopens |
| Not in Bambuddy, Bambu tag (Auto-save new off) | `Not in Bambuddy` | `questionmark.circle` | secondary | no entry, never focused, no number pad; **New** (prominent, adds the spool from the tag at its label weight) · **Link to existing…** |
| Added to Bambuddy (after New, or Auto-save new on) | `Added to Bambuddy` | `plus.circle.fill` | green | none, never focused; tap reopens it as Enter weight, pre-filled label + core |
| Not in Bambuddy, no material | `Not in Bambuddy; tag has no material` | `questionmark.circle` | secondary | **New…** (prominent, opens the form) · **Link to existing…** |
| Flipper asks to create | `Flipper asks to create it here` | `dot.radiowaves.left.and.right` | blue | **New** (or **New…** without material) prominent · **Link to existing…** |
| Flipper asks to link | `Flipper asks to link it here` | `dot.radiowaves.left.and.right` | blue | **Link to existing…** first and prominent · **New** / **New…** |
| Flipper asks to relink a spool added here | `Flipper asks to link it here` | `dot.radiowaves.left.and.right` | blue | **Link to existing…** alone (moves the tag, deletes the spool just added); no entry |
| Error | `Error: Bambuddy unreachable` (any message) | `exclamationmark.triangle.fill` | red text | on a spool the entry, value kept for retry; after a failed New or Link the row's actions stay as the retry |
| Updating (a Flipper action under way) | `Updating…` | spinner | secondary | entry disabled, if it was open |
| Marked as new | `Marked as new · 1000 g left · by Flipper` | `arrow.counterclockwise.circle.fill` | green | none; tap reopens |
| Marked as empty | `Marked as empty · archived · by Flipper`; colour line ends `· archived` | `archivebox` | secondary | none; tap reopens (a Save restores the spool) |
| Deleted | `Deleted from Bambuddy · by Flipper` | `trash` | secondary | **New** (tag with material) or **New…** (without) · **Link to existing…**, no entry |
| Archived spool scanned | `Enter weight`; colour line ends `· archived` | `scalemass` | blue | entry; Save restores the spool, then records the weight |

A weight saved from the Flipper on a row the iPhone shows as waiting turns
it into `Saved … · by Flipper` and drops the iPhone's draft. The Flipper's
**Add and weigh** on a tag not in Bambuddy creates the spool with its
weight; the row reads `Saving…` meanwhile, with no field and no buttons.

### Weight rules

- Suggestion = `base + (mode == gross ? core : 0)`; `base` is the spool's
  remaining grams, for a tag without a spool its label weight. `core` is the
  spool's core weight, for a tag without a spool the Settings value.
- **Switching Net/Gross converts the field's number, typed or not**: to net
  `n − core`, to gross `n + core`. An untouched suggestion stays untouched
  (it is still selected and the first key replaces it); a typed number
  stays typed. The readout follows. (The core's `setMode` keeps a touched
  number as is; the owner overrides that.) A conversion below 0 clamps to 0 (see
  "Open questions").
- Digits only, 0…9999. Save sends what the shipped `FeedViewModel.save`
  sends today (gross to Bambuddy).
- After a successful save the row shows `Saved n g left · by iPhone`, keeps
  the value as the new base, and focus moves to the newest row that still
  waits for a weight (Enter weight). With none, the pad lowers.
- Focus never steals from a field the user is typing in (`touched`): the
  new row only gets a 2 pt `.tint` outline and waits.

## Number pad and focus (from the iPhone flow)

"Correct suggestion: 1 tap. Otherwise: digits + 1 tap."

1. A row turns Enter weight (a scan of a known spool, or a tap on a saved
   or added row; a row not in Bambuddy raises nothing). The list scrolls it
   to the top (at the accessibility text sizes, where the card is taller
   than the space above the pad, its entry block goes just above the pad
   instead) and its field takes focus; the system number pad
   (`.numberPad`) rises with the suggestion selected.
2. The first key replaces the selection (a lone `0` is replaced too); ⌫
   (`delete.left`) deletes one digit; at most four digits (0…9999).
3. Net/Gross sits under the field and converts the number.
4. Tap Save, which always stays above the pad (the list scrolls the
   focused row's entry block, Save included, clear of the keyboard). Saving…
   → Saved, then a light success haptic (`.sensoryFeedback(.success, …)`);
   an error gives `.error`.
5. If another row waits, focus moves there and its suggestion is selected;
   otherwise the pad lowers. Tapping any field reopens it with its value
   selected; tapping a saved card reopens it. Tapping the list background
   lowers the pad.

The tab bar is hidden while the pad is up. While a sheet is open no row
takes focus.

## Tag details (owner decision; not in the design)

The design shows the UID in the row; the owner moved the technical facts
out of it. Long-press a card (`.contextMenu`) → **Tag details**
(`info.circle`), also a VoiceOver action, opens a sheet (medium detent) with
a grouped `Form` of `LabeledContent` rows, each present only when known:
UID (monospaced, as read), Bambuddy tag UID (with the `00000100` suffix),
tray UUID, colour name, colour code, filament id, diameter (`1.75 mm`),
nozzle (`190–230 °C`), label weight, core weight, spool (`#42`, archived or
not). Title: the row title. UID values are selectable for copying. Layout
beyond this is the builder's call.

## Empty and blocking states (`ContentUnavailableView`)

- Empty feed, the first thing missing first (`FeedEmptyState`):
  - no server (no URL or key): `server.rack`, `Connect Bambuddy`, `Scan the
    API-key QR code in Bambuddy, or type its address.`, **Open Settings**
    (the Spools tab's words);
  - no Flipper connected: `antenna.radiowaves.left.and.right`, `Find your
    Flipper`, `Open FliBuddy on the Flipper (Apps → NFC).`, **Scan for
    Flipper**, which opens Settings and starts the scan there;
  - otherwise `wave.3.right` (58 pt, `.secondary`), `No scans yet`,
    `Hold a spool's tag to the Flipper running FliBuddy.`
- First run asks for each permission where it is used: the camera on the
  first Scan QR code (denied: the footer reads `Camera access is off. Allow
  it in Settings.` with an **Open Settings** row and Scan dimmed), the local
  network on the first Bambuddy request (Test connection, or the test a QR
  read starts), Bluetooth on Scan for Flipper. Without a remembered Flipper
  the link does not start, so nothing creates the Bluetooth central before
  that scan; the banner then reads `Flipper not connected` with Settings.
- Spoolman mode (blocking, replaces the list even with rows): `nosign` in
  red, `Bambuddy is in Spoolman mode`, `FliBuddy works with Bambuddy's own
  inventory. Switch it back from Spoolman in Bambuddy's settings, then
  retry.`, button Retry.

## Clear

Toolbar trash → `confirmationDialog` titled `Clear the feed?`, message
`This clears the session log only. Nothing changes in Bambuddy.`, buttons
**Clear feed** (destructive) · Cancel.

## Sheets

**New spool** (tags without material only; Bambu tags create from the
tag): navigation bar with Cancel (`xmark`, leading) and Create (`checkmark`,
trailing, `.tint` when a material is chosen, disabled otherwise), title
`New spool`; under it `NTAG 7-byte` and the UID in monospaced
`.secondary`. Grouped `Form`:

- Section "Required": Material, menu `Choose` / PLA, PETG, ABS, ASA, TPU,
  PA, PC, PVA.
- Section "Optional": Subtype (placeholder `Basic, Matte, HF…`), Color
  (`ColorPicker`, the swatch RGBA), Color name (placeholder `Lilac`),
  Brand (placeholder `Elegoo`), Label weight (digits, default 1000, `g`).
- Footer: `FliBuddy adds the spool to Bambuddy at its label weight.`

**Link to existing**: navigation bar with Cancel (`xmark`) and Link
(`checkmark`, disabled without a pick), title `Link to existing`, the UID
under it. A `.searchable` field (`magnifyingglass`, placeholder `Material,
colour, brand or note`). Rows (core `invVM`): swatch,
title, `colour · brand · n g left`, the note on its own line, a `Tag <tag>`
badge when the spool carries a tag other than this one, a `checkmark` on
the pick. The filter matches material, subtype, colour, brand and note. Empty: `No spools match “<q>”.`;
no spools: `No spools in Bambuddy.`. When the pick carries a different tag,
Link asks first with an alert: title `<title> <colour> is linked to tag
<tag>`, message `Move the link to <uid>? The old tag will no longer identify
this spool.`, buttons **Move link** (`arrow.left.arrow.right`) · Cancel.

**Scan QR code** (`fullScreenCover`): camera preview edge to edge, close
(`xmark`) top leading, a corner-bracket frame in the middle, a capsule hint
under it: `Point at the QR code Bambuddy shows when you create an API key.`
A read closes the cover, fills URL and key, and starts Test connection.

## Settings (grouped `Form`, applies on change)

Large title `Settings`. Every change applies when it is made: pickers and
the segmented control at once, text fields when they are committed
(Return or leaving the field). There is no Save / Revert row. Each applied
change also goes to the watch (see "Settings sync to the watch").

```swift
Form {
  Section("Bambuddy") {
    Button("Scan QR code", systemImage: "qrcode.viewfinder")   // first row; .tertiary without a camera
    TextField("URL")                                           // https://bambuddy.example.com
    SecureField("API key")
    Button("Test connection") + trailing result
  } footer: QR footer
  Section("New items") { Picker("Empty spool"); LabeledContent("Core weight") { TextField + "g" } }
    footer: "Weight of everything that is not filament; 242 g = Bambu reusable spool 215 g + refill core 27 g."
  Section("Weighing") { Picker("Type weight as", Net | Gross).pickerStyle(.segmented) }
    footer: "Gross is the whole spool on the scale. Each row can still switch."
  Section("Flipper") {
    LabeledContent(name ?? "No Flipper paired") { symbol + state }   // "Workshop" · Bluetooth / Searching…
    Button("Scan for Flipper")
  } footer: "Bluetooth only on iPhone. The Flipper must be running FliBuddy."
  Section { Button("Forget This Flipper", role: .destructive) }      // centred
  Section { Link("Privacy Policy"); Link("Support"); ShareLink("Share Log") }
    footer: "FliBuddy 1.0 (42)"                                        // centred, from the bundle
}
```

QR footer: `Scan the QR code Bambuddy shows when you create an API key.`;
after a read `Filled from the QR code.` in green; a code with an http://
address fills the key only and reads `Filled in the API key. The QR code
has an http:// address; FliBuddy needs https://.` in red (no test runs);
without a camera `This device has no camera. Type the URL and API key
instead.` and the Scan row is dimmed; with camera access denied `Camera
access is off. Allow it in Settings.` with an **Open Settings** row. An
http:// URL shows `FliBuddy needs an https:// address.` under the field
and is not applied. Test results: `Not tested` · `Testing…` (spinner) ·
`Ready · inventory OK` (`checkmark.circle.fill`, green) · `Unreachable:
<reason>` · `Needs HTTPS` · `Certificate not trusted` (`xmark.circle.fill`,
red), one wording on every platform (the Spec page's; the mock-up's
failure shortens to `Unreachable`). Editing the URL or the
key resets the result to `Not tested`. The Flipper state reads `Bluetooth`
when connected, `Searching…` with a spinner in its place while scanning.
The discovered Flippers list under Scan (name, signal `Nearby · strong` /
`Nearby` as on the watch, a checkmark on the chosen one) stays as shipped;
the design does not show it. Rows are at least 52 pt high. The Empty spool
picker lists Bambuddy's spool catalog, loaded each time Settings opens;
there is no Reload (the design has none), a failed load shows in red under
the picker, and Test connection loads it again; without an https URL and a
key nothing loads (no error on a fresh install). The mock-up's four names
(`Bambu reusable + core`, `Bambu cardboard`, `Generic 1 kg plastic`,
`Custom`) are placeholders. Forget This Flipper sits in a section of its
own, centred, in red, disabled while no Flipper is remembered; under it
the app's version and build, centred (`FliBuddy 1.0 (42)`, from
`CFBundleShortVersionString` and `CFBundleVersion`).

## Settings sync to the watch

The iPhone sends the settings (URL, API key, core weight, empty spool, Net /
Gross) to the watch after every applied change, through the shipped
`WatchSettingsSender` (application context, only the latest counts). With
apply-on-change a text field sends once when it commits, not per keystroke.

## Spools (read-only inventory)

The second tab, between Feed and Settings: `shippingbox`, `Spools`. It
lists what Bambuddy holds, so the owner can look a spool up at the bench
without opening Bambuddy. Nothing is edited here in 1.0; the detail sends
the owner to Bambuddy for that. Design: the `Spools` frames of
`FliBuddy iPhone.dc.html`; the texts and the list logic are `spoolsVM`,
`spoolRowVM`, `spoolDetailVM` and `SPOOL_STATES` in `flibuddy-core.js`.

**Data.** One request per load: `GET /inventory/spools?include_archived=true`
(`listSpools(includeArchived: true)`, `inventory:read`); the scopes filter
on the device. `updated_at` is decoded for "updated … ago"; Bambuddy sends
it without a time zone and means UTC (its own web app appends `Z`). The
empty-spool catalog (`GET /inventory/catalog`) names the empty spool in the
detail; when it fails the detail shows the grams alone. The tab loads when
it first appears and again each time it comes on screen, on pull to
refresh, and after every write the feed makes (a save, New, Link, Mark as
new or empty, Delete, from the app or the Flipper), so the list follows the
bench. The last good list is kept as the cache; a list from other settings
(another URL or key) is never shown.

**List.** Under the large title: the search field (`.searchable`, prompt
`Material, color, brand or note`, the Link picker's filter: every word in
material, subtype, colour, brand or note, ignoring case and diacritics),
then the scope as a segmented control, `Active · Archived · All` (Active
first; a segmented control rather than `searchScopes`, which iOS shows only
while searching), then one inset-grouped card of rows. The toolbar's
trailing `line.3.horizontal.decrease.circle` menu (`.fill` and `.tint` while
a material is chosen) holds **Sort by** `Recently updated` (default) ·
`Least left` · `Material` · `Color`, and **Material** `All materials` plus
each material in the inventory. A footnote under the card counts the active
spools: `7 active spools. Pull down to refresh.`

Sort orders: Recently updated, newest `updated_at` first (spools without
one last); Least left, fewest grams first; Material, by title then color;
Color, by color name (spools without one last) then title. Ties fall back
to the spool id, so the order never jumps between loads.

**Row** (a button; tap opens the detail):

| Part | Spec |
|---|---|
| Swatch | 28 pt circle, the spool's RGBA, `.separator` hairline; dashed outline without a colour |
| Title | `PLA Matte` (material + subtype, as the feed), `.headline` |
| Sub line | `Charcoal · Bambu Lab` (colour name, brand; absent parts left out), `.subheadline` `.secondary` |
| Detail line | `410 g left of 1000 · updated 3 d ago`, `.footnote` `.secondary`, monospaced digits, one line; where it does not fit (or without `updated_at`) the remaining alone |
| Title line | after the title, at its trailing end: `Low` (orange, word in a tinted capsule) when less than 15 % of the label weight is left on an active spool; `Archived` (`.secondary`, word in a fill capsule) on an archived spool; `tag` glyph (`.secondary`) when a tag is linked |
| Trailing | the chevron alone; colour and brand stay on one line |
| VoiceOver | one sentence: `PLA Matte, Charcoal, Bambu Lab, 410 grams left of 1000, low, archived, tag linked, updated 3 days ago.` (the parts that apply) |

"Updated … ago": under a minute `just now`, then `5 min`, `3 h`, `3 d`,
`2 mo`, `1 y` ago (whole units, rounded down; a time ahead of the device's
clock reads `just now`). VoiceOver spells the unit out (`3 days ago`).

**States** (`ContentUnavailableView` in place of the list unless noted):

| State | Shows |
|---|---|
| First load (no list yet) | a spinner, `Loading spools…` |
| Pull to refresh | the system refresh control over the list |
| No server (no URL or key) | `server.rack`, `Connect Bambuddy`, `Scan the API-key QR code in Bambuddy, or type its address.`, **Open Settings** (the feed's no-server state of Task 13) |
| Unreachable, list cached | the list stays; a line above it: `exclamationmark.triangle.fill` (orange) `Couldn't refresh · showing 10:42` (the time of the cached list) with a **Retry** chip |
| Unreachable, nothing cached | `wifi.exclamationmark` (red), `Can't reach Bambuddy`, `Check that Bambuddy is running and on the same network.`, **Retry** |
| No permission | `lock.trianglebadge.exclamationmark` (red), `Can't read the inventory`, `The API key needs the inventory:read permission. Add it to the key in Bambuddy.`, **Open Settings** |
| Key rejected | same symbol, `Bambuddy rejected the API key`, `Check the key in Settings, or scan the QR code of a new one.`, **Open Settings** |
| Spoolman mode | the feed's blocking screen: `nosign` (red), `Bambuddy is in Spoolman mode`, the same text, **Retry** |
| Empty inventory | `shippingbox`, `No spools in Bambuddy`, `Spools added in Bambuddy, or with New in the feed, show here.` |
| No search results | `magnifyingglass`, `No spools match “<q>”`, `Search looks at material, color, brand and note.` |
| Material filter empties the scope | `line.3.horizontal.decrease.circle`, `No PETG spools`, `Choose All materials to see every spool.` |
| Scope empty | `shippingbox`, `No archived spools` / `No active spools`, `Bambuddy has spools in the other scope.` |

Retry checks Bambuddy again (the startup check, which also clears a
Spoolman refusal) and loads. A cache from a refresh that failed for
permission or Spoolman is dropped: those states replace the list.

**Detail** (a sheet, medium and large detents; Done `xmark` leading, the
title in the bar):

- swatch 64 pt, the title (`.title2.bold`), `Charcoal · Bambu Lab`;
- a linear `Gauge` of what is left of the label weight: `410 g left of
  1000 g` over the bar, `41%` trailing, `Low` in orange when low; the bar
  `.tint`, orange when low;
- **Today** (only when the feed has a row for the spool): the row's status
  symbol and phrase, e.g. `Saved 15 g left · by Flipper`;
- **Details**, `LabeledContent` rows: Material, Subtype, Brand, Label weight
  (`1000 g`), Empty spool (`Bambu Lab - Reusable spool + refill · 242 g`, the
  grams alone without a catalog entry), Last scale weight (`652 g`, or
  `Never weighed`), Tag (the UID in monospaced, or `No tag`), Note, Spool #
  (`21`), Archived on (`Oct 18, 2026`); Subtype, Brand and Note only when
  set, Archived on only when archived;
- **Open in Bambuddy** (`arrow.up.right.square`, `.tint`): opens
  `<Bambuddy URL>/inventory?spool=<id>` in the browser, which opens the
  spool's edit form in Bambuddy's inventory page; the footer under it:
  `Read-only. Edit spools in Bambuddy.`

The detail follows the list: a refresh updates it, and a spool gone from
Bambuddy closes it.

## Tokens

Spacing (pt): 4 symbol ↔ text in a status line · 8 between controls in a
group · 12 swatch ↔ text, header ↔ entry, between rows · 16 row padding and
list inset · 20 sheet and Form padding · 24 between Form sections · 32
empty-state stack. Inside a card: title → colour line 2, colour line →
status 6, inside the entry block 10.

Radii: `Circle()` swatch; row card insetGrouped Section (≈ 24); weight field
`.rect(cornerRadius: 14)`; buttons `.buttonBorderShape(.capsule)`; banner
`Capsule()`; sheets system.

Type roles (iOS sizes at the default content size): weight 34 bold rounded
monospaced digits · row title `.headline` 17 sb · colour line
`.subheadline` 15 · status `.subheadline` 15 · UID (details only)
`.caption.monospaced()` 12 · readout `.footnote.monospacedDigit()` 13 ·
banner `.footnote.weight(.medium)` 13 · Save `.headline` 17 sb on a tinted
`.bordered` capsule · unit `g`
`.title2.weight(.semibold)` 22.

Colours: `.primary` titles and the weight · `.secondary` colour line,
status, readout · `.tertiary` placeholders, empty value · `.tint`
(systemBlue) Save (its text, and its fill at about 14 %), focus, Flipper
requests · `.red` errors, Forget, Clear · `.green` saved, added, ready · `.orange` warnings (timeout, advertising,
no hello) · `.gray` searching, not checked · `systemGroupedBackground` the
feed and Form · `secondarySystemGroupedBackground` the row card ·
`tertiarySystemFill` the weight field and segmented track · `.separator`
the swatch stroke.

SF Symbols: `list.bullet`, `gearshape`, `trash`, `scalemass`,
`checkmark.circle.fill`, `plus.circle.fill`, `questionmark.circle`,
`exclamationmark.triangle`, `exclamationmark.triangle.fill`, `xmark.circle.fill`,
`dot.radiowaves.left.and.right`, `antenna.radiowaves.left.and.right`,
`antenna.radiowaves.left.and.right.slash`, `app.dashed`, `server.rack`,
`wifi.exclamationmark`, `lock.trianglebadge.exclamationmark`, `nosign`,
`paintpalette`, `wave.3.right`, `qrcode.viewfinder`, `link`,
`arrow.left.arrow.right`, `magnifyingglass`, `xmark`, `checkmark`,
`delete.left`, `arrow.counterclockwise.circle.fill`, `archivebox`,
`info.circle`, `shippingbox`, `tag`, `line.3.horizontal.decrease.circle`
(`.fill`), `arrow.up.right.square`, `chevron.right`.

## Differences from what ships

None remain. The fourteen differences found on 2026-10-27 were closed by
the plan `docs/plans/20261027-apple-ui-from-design.md`, and those found
after the design sync of 2026-11-05 by Tasks 10 and 11 of
`docs/plans/20261105-release-1.0.md`; where the app departs from this file,
the next section says how and why. What changed:

1. Connection banner: the top glass banner (`ConnectionBanner`,
   `ConnectionBannerView`) replaced the bottom macOS status bar on iOS,
   with the iPhone phrases, one line per side in trouble.
2. Row card: the stacked card, one `Section` per row.
3. Number pad and focus: number pad, select-all, scroll to the focused
   row, tab bar hidden while typing, pad lowered when no row waits.
4. Haptics: success after a host save, error on a failed host action.
5. Net/Gross converts the number on screen, typed or not (Mac too).
6. Colour line `<colour name> · <code>`; the UID left the row.
7. Tag details sheet from the row's context menu and VoiceOver action.
8. Settings apply on change, with the labels, rows and footers above.
9. QR scanner as a full-screen cover; a dimmed Scan row without a camera;
   a read applies and starts Test connection.
10. New spool sheet: colour picker, Required / Optional, toolbar buttons.
11. Link picker: `.searchable`, the move-link alert, toolbar buttons.
12. Clear: `trash` button, `Clear feed` in the dialog.
13. Watch sync after each applied change, debounced.
14. A tag not in Bambuddy (2026-11-05): `Not in Bambuddy` with New and
    Link to existing…; New adds the spool at its label weight (`Added to
    Bambuddy`), and a tap weighs it. The 2026-10-27 new spool card (Save as
    new, Save with weight…, Set weight…, Cancel) is gone.
15. Save (2026-11-05): a 44 pt tinted capsule, filled with Increase
    Contrast.
16. Settings (2026-11-05): Forget This Flipper in its own section, the
    version footer, no Reload catalog, `Ready · inventory OK`, the signal
    in words.
17. Portrait only (2026-11-05).

## Implemented differently (2026-10-27, updated 2026-11-05)

Where the app departs from this file or fills in what it leaves open, and
why. Items marked "by eye" still wait for the owner's check on screen.

Connection banner
- The banner is `ConnectionBanner` (`FliBuddyCore`), built from the same
  two `StatusItem`s as the macOS status bar; each item carries its iPhone
  phrase in `short` where it differs. A side counts as healthy when its
  tint is green (Flipper connected, Bambuddy ready), so the colour-names
  warning is a problem line of its own.
- The shape is a rounded rectangle with a 20 pt continuous corner rather
  than `Capsule()`: at one line (40 pt) it is the capsule, and at two
  lines the corners stay the same instead of rounding into the text. By
  eye.
- The chip fills with the app tint, not the state's tint. The healthy
  line's accessibility hint is the firmware version. By eye.
- A portrait iPad shows the same banner over its tiles; beside the iPad
  sidebar the banner gives way to the sidebar's status rows
  (`ipados-ui-spec.md`).

Feed row, number pad and focus
- The card is the row of its own inset-grouped `Section` with 16 pt row
  insets; the waiting-row hint is a 2 pt `.tint` stroke on the card's row
  background (24 pt continuous corner), not on the field. By eye.
- Save and the capsules are stock `.bordered` / `.borderedProminent`
  buttons with `.buttonBorderShape(.capsule)` at `.controlSize(.large)`;
  the label's minimum height (30) gives about 44 pt rather than an exact
  frame. Save is `.bordered` (`SaveEmphasis.tinted`), and
  `.borderedProminent` when `colorSchemeContrast` is `.increased`. The
  non-prominent capsule is the bordered style's tinted fill rather than
  `tertiarySystemFill`. By eye.
- While New or Link runs, the buttons dim; there is no spinner and the
  status keeps its phrase (the design's `Creating in Bambuddy…` row is the
  watch's only). A failed one keeps the actions for a retry.
- A reopened row (a re-weigh, or the first weight of an added spool) is
  focused before its field exists; the field takes the keyboard as it
  appears (`takesFeedFocusOnAppear`). By eye: the pad rises after a tap on
  an added row.
- Select-all on focus sets the field's `TextSelection` to the whole value
  one turn after the field takes the keyboard, so the caret the tap placed
  does not win; a Net/Gross switch reselects a converted suggestion. A tap
  anywhere in the 64 pt box focuses the field. By eye.
- Leading zeros drop as the user types (`FeedViewModel.sanitizeWeight`),
  which is how a lone `0` is replaced by the next key; the Mac shares it.
- "Tapping the list background lowers the pad" is a tap on a card's header
  (swatch, title, colour line, status) plus
  `.scrollDismissesKeyboard(.immediately)` on a drag: a list-wide tap
  gesture races the field's own tap and the buttons. A tap between cards
  does nothing. By eye.
- No row's field holds the keyboard while a sheet is open: the sheets
  count themselves in and out (`FeedViewModel.sheetOpened()` /
  `sheetClosed()`), and the focused row's field takes the pad back when the
  last one closes.
- The success haptic plays after a save from the host; the error haptic
  after any failed host action (save, New, Link), as `FeedViewModel.haptic`
  events. A save from the Flipper plays nothing on the iPhone.
- The timeout note's symbol and text are a `Label` 4 pt under the status.
- Tag details opens from the card's context menu and the VoiceOver action
  shared with the Mac, as a sheet with medium and large detents.

Settings, QR, sheets and Clear
- Apply on change lives in `SettingsViewModel` (`appliesOnChange`, which
  only the iPhone and iPad set). Each change applies on top of the settings
  applied last, one after another, so a picker change leaves a field that
  is still being typed out of it. A text field commits on Return, when
  another field takes the focus, and when the user leaves the Settings tab
  (`SettingsViewModel.left()`, which also stops a Flipper scan).
- A core weight out of range is not applied (its red line stays under the
  field) while a URL or key committed with it is. A failed apply shows a
  red line at the bottom of the form; the edit stays and the next commit
  tries again.
- URL and API key are labelled rows with the field trailing. By eye.
- The core weight field is text, not a number format: digits go into the
  setting as typed (`SettingsViewModel.coreWeightText`, at most four, an
  empty field reads 0 and is not applied), so the commit on focus loss
  never sees a stale number; the number pad has no Return.
- The Flipper row reads `Searching…` with the spinner while a scan runs
  (it wins over a connection), `Bluetooth` when connected, and `Not
  connected` with `antenna.radiowaves.left.and.right.slash` otherwise,
  which the spec does not name. A Flipper connected without a chosen one
  is titled `Flipper`. The scan button reads `Stop scanning` while it
  runs, as shipped.
- The catalog loads each time the Settings form appears
  (`SettingsViewModel.opened()`); after a committed URL or key fix it
  reloads only through Test connection or the next visit.
- The version footer reads the bundle as it is: `FliBuddy 0.1.0 (1)` until
  the release sets `MARKETING_VERSION` and the build number. The Flipper
  footer and the version are section footers, so they follow Dynamic Type
  and the form's margins. By eye.
- The watch gets the settings 0.5 s after the last applied change
  (`WatchSettingsSender.defaultDebounce`), so a quick run of changes sends
  once, the latest.
- The QR footer shows a failed read in red, in place of the hint. The
  close button is an `xmark` in a material circle; the bracket frame is
  240 pt, white. By eye.
- New spool: the colour picker has no opacity; with no colour on the tag
  it shows grey and stores nothing until a colour is picked. The title and
  `NTAG 7-byte · <uid>` sit in the navigation bar's centre. The material
  menu's empty entry reads `Choose` (the Mac keeps `Choose…`).
- Link to existing: the rows are buttons with the `checkmark` on the
  pick; the search field is always shown; no match is a
  `ContentUnavailableView` with `magnifyingglass`. Link with a pick that
  carries another tag asks with the alert; the iPad shows the inline
  warning instead (`ipados-ui-spec.md`).
- Clear keeps ⌘⌫ on a hardware keyboard, now a menu command of the app
  (`FeedCommands`, shared with the iPad) rather than the toolbar button's
  shortcut; ⌘S and ⌘↑ / ⌘↓ are menu commands too.

Spools (Task 12, 2026-11-05)
- "Updated … ago" is the app's own short formatter (`SpoolsViewModel.ago`):
  `RelativeDateTimeFormatter` writes `3d ago` and follows the device
  language inside an English row. Times and dates (`showing 10:42`,
  `Archived on`) follow the device's region.
- The Spoolman state reuses the feed's shipped sentence word for word
  (`Switch Bambuddy's inventory back from Spoolman in its settings, then
  retry.`), so both screens say the same.
- The stale line's Retry and the states' Retry check Bambuddy again before
  loading, which also lifts a Spoolman refusal once Bambuddy is fixed.
- The detail's Done is the `xmark` toolbar button; the inspector carries
  the same one. The selected row stays highlighted beside the inspector.
- The demo (`-FliBuddyDemo YES`) fills the tab from its in-memory
  inventory; `-FliBuddyDemoPane spools` opens on it for screenshots.

## Open questions for the owner

None. Decided 2026-10-27: Net/Gross below the core clamps to 0, as on the
Flipper; with both sides down the banner shows both. Decided 2026-11-05:
New adds the spool with no weight prompt; the iPhone stays in portrait.
