# FliBuddy Apple Watch: UI specification from the design

Source: the Claude Design project "FliBuddy mobile app design"
(`docs/design/source/FliBuddy Watch.dc.html`, the shared prototype model
`docs/design/source/flibuddy-core.js`, and the Spec page
`docs/design/source/FliBuddy Spec.dc.html`), imported on 2026-10-27 and
synced again on 2026-11-05. The
watch page is drawn at 46 mm (208 × 248 pt); sizes below are in points at
that size.

This file covers the watch app (`host/Apple/Watch`, views in
`host/Sources/FliBuddyUI/Watch`). Copy and the row states follow
`ios-ui-spec.md` (and through it the core's `vm`) unless stated here.

Precedence: as in the iPhone spec. Where this file and the shipped code
disagree, this file wins; where it is silent, keep the shipped behaviour.
The owner decisions outrank the design; `protocol/PROTOCOL.md`,
`docs/design/flipper-ui-spec.md` and the approved design document outrank
this file on behaviour. Where the watch page and the Spec page differ, the
watch page wins (it is the drawn, interactive mock-up).

## Assumptions and owner decisions, as they apply to the watch

The eight assumptions and their answers are in `ios-ui-spec.md`. For the
watch:

- **Assumption 4:** the watch creates Bambu tags in one tap and cannot fill
  the form. The page: one **New** button only for a tag with prefill
  (material); a tag without material reads `Create it on the iPhone.`;
  a Flipper request to link reads `Link it on the iPhone.`. There is no
  Link to existing on the watch.
- **Assumption 2 (2026-11-05):** New adds the spool at its label weight
  and asks for no weight: the row reads `Added to Bambuddy` and shows as a
  result with **Weigh again**. A row not in Bambuddy and an added row do
  not wait for a weight, so neither opens on its own. The 2026-10-27 new
  spool card (Save as new, With weight, Set weight, Cancel) is gone.
- **Assumption 7:** dark only.
- **Net/Gross (owner):** switching converts the number, crowned or not: to
  net `n − core`, to gross `n + core`, clamped at 0. The core's `setMode`
  keeps a touched number; the owner overrides that.
- **Order (owner):** newest on top; a rescan moves the row to the top and
  reopens it.
- **Colour facts (owner):** the watch page shows no colour name or tag
  facts; the colour name, colour code, UID, filament id, diameter and
  nozzle range go in a Details section at the bottom of the row detail,
  never in the list row.
- **Known spools (owner):** a save from the Flipper shows `Saved … · by
  Flipper`; Mark as new / Mark as empty / Delete from the Flipper show with
  the macOS phrases; an archived spool shows `archived`.

## App structure (watch page)

```swift
NavigationStack {
  List(feed) { WatchFeedRow($0) }
    .navigationTitle("FliBuddy")
    .toolbar { ToolbarItem(placement: .topBarLeading) { ConnectionGlyph() } }  // tap → Settings
    .navigationDestination(for:) { RowDetail($0) }
}
// RowDetail
Text(grams, format: .number)
  .font(.system(size: 50, weight: .semibold, design: .rounded))
  .monospacedDigit()
  .focusable()
  .digitalCrownRotation($grams, from: 0, through: 5000, by: 1,
                        sensitivity: .low, isHapticFeedbackEnabled: true)
// 5 g per detent above ~6 detents/s
// save: WKInterfaceDevice.play(.success)   error: .play(.failure)
// AOD: @Environment(\.isLuminanceReduced)
```

Screens: Feed → Row detail; Feed → (glyph) Settings → Scan for Flipper.
Every screen but the feed has the system back button (`chevron.left`) top
leading; the time is top trailing.

## Connection glyph (feed only, top leading)

A 30 pt circular button (white 16 % fill) holding one 16 pt symbol in its
tint; a 13 pt spinner instead while searching. Tap opens Settings. Its
accessibility label is the phrase. One state shows (core `statusVM.watch`):
the Flipper's problem if the Flipper is not connected, else Bambuddy's
problem, else `antenna.radiowaves.left.and.right` in green labelled
`Flipper connected`.

| State | Symbol | Tint | Label |
|---|---|---|---|
| No Flipper | spinner | gray `#8E8E93` | `No Flipper · searching` |
| Flipper not connected (link not started; the watch starts it at launch, so only before that) | `antenna.radiowaves.left.and.right` | orange `#FF9F0A` | `Flipper not connected` |
| Connected, no hello | `app.dashed` | orange | `Open FliBuddy on the Flipper` |
| Other version | `exclamationmark.triangle` | orange | `Flipper speaks protocol 1, app speaks 2` |
| Bambuddy not set up | `server.rack` | orange | `Bambuddy not set up` |
| Bambuddy not checked | `server.rack` | gray | `Bambuddy not checked` |
| Bambuddy unreachable | `wifi.exclamationmark` | red `#FF453A` | `Bambuddy unreachable` |
| API key rejected | `lock.trianglebadge.exclamationmark` | red | `Bambuddy rejected the API key` |
| API key lacks permission | `lock.trianglebadge.exclamationmark` | red | `Key lacks inventory permission` |
| Spoolman mode | `nosign` | red | `Bambuddy is in Spoolman mode` |
| Color names unavailable | `paintpalette` | orange | `Color names unavailable` |
| All healthy | `antenna.radiowaves.left.and.right` | green `#30D158` | `Flipper connected` |

The page draws no blocking view for Spoolman mode on the watch; the red
glyph says it and saves fail with the error.

## Feed list

Title `FliBuddy` (17 sb). Rows are platters (`#222224`, corner 20, minimum
height 52, padding 9 × 12, 5 apart), a grid of swatch 16 · text · glyph 22:

| Part | Spec |
|---|---|
| Swatch | 16 pt `Circle()`, 0.5 pt light stroke; dashed outline without colour |
| Title | 15 sb, one line, tail truncation: `PLA Basic`; blank tags shortened to `NTAG 04A2…` |
| Sub line | 13 `.secondary`: `590 g left` (known), `label 1000 g` (tag with prefill), `Unknown tag` (no material); `archived` for an archived spool (owner) |
| Status glyph | 19 pt, centred in its 22 pt column: `scalemass` blue `#0A84FF` (Enter weight), `checkmark.circle.fill` green (Saved, Timeout), `plus.circle.fill` green (Added to Bambuddy), `questionmark.circle` gray (Not in Bambuddy, with or without material), `exclamationmark.triangle.fill` red (Error), `dot.radiowaves.left.and.right` blue (Flipper asks); a 14 pt spinner for Looking up, Saving, Creating and Updating. The marked and deleted states use their iPhone symbols. Accessibility label: the status phrase. |

Empty feed: only a centred line, 14 `.secondary`: `Set up FliBuddy on
your iPhone first.` without settings; `No Flipper found. It may be
connected to another device.` after 30 s of
searching (a Flipper serving another central stops advertising, so the
watch cannot tell it from one out of range); else `Hold a spool’s tag to
the Flipper running FliBuddy.` Unlike the iPhone, the watch connects to
the first Flipper found without one being chosen (owner ruling); Scan for
Flipper in its Settings remembers one.

**Auto-open (core `onFocus`):** when a row starts waiting for a weight
(scan or rescan of a known spool, a reopen) and the feed list is the
screen showing, its detail opens on its own. Rows not in Bambuddy and
added rows do not wait for a weight, so they never open on their own. If a detail,
Settings or the scan screen is showing, nothing opens; the row only moves
to the top.

## Row detail

A header line: swatch 12 · title 15 sb (one line). Then one of four bodies,
by state:

**Entry** (Enter weight, Saving, Error on a known spool):

| Part | Spec |
|---|---|
| Weight | centred, 54 high: value 50 sb rounded tabular digits, `g` 20 sb `.secondary` (the Spec table's 24 is superseded by the page) |
| Line | 13, centred, one line: `Saves 590 g left`; `Turn the crown` when empty; red `Below the core weight (242 g)`; red `Error: <message>` in the error state |
| Net / Gross | two pills side by side, 30 high, 4 apart, 13 sb; the selected one white with black text, the other white 16 % with white text |
| Save | full width, 40 high capsule, tinted: `#0A84FF` at 26 % with `#4DA2FF` text, 16 sb: `Save` / `Saving…`; 45 % when it cannot save. Not filled: the number is the focus. |
| Crown indicator | the page's 4 × 40 green-thumb bar on the right edge stands in for the system's crown indicator; use the system one |

**Saved** (Saved, Timeout, Added to Bambuddy; also Marked as new / Marked
as empty): the state
symbol 34 pt in its tint, the remaining `590 g` at 34 sb rounded tabular,
the status phrase 13 `.secondary` centred (`Saved 590 g left · by iPhone`),
and at the bottom **Weigh again** (40 high, white 16 % fill, 16 sb), which
reopens the entry and focuses it.

**Unknown** (Not in Bambuddy with or without material, Flipper asks to
create / link it, Deleted): the sub line (13 `.secondary`), the status
symbol 17 pt + phrase 15, a hint 13 `.secondary`: `Create it on the
iPhone.` for a tag without material, `Link it on the iPhone.` for a link
request, none otherwise; a red `Error: <message>` after a failed New; at
the bottom one **New** (40 high, filled `#0A84FF`, white) when the tag has
prefill, which adds the spool from the tag at its label weight in one tap
(`Creating in Bambuddy…`, then `Added to Bambuddy`).

**Busy** (Looking up, Creating): a 26 pt spinner and the phrase 15
`.secondary`, centred.

**Details** (owner; not on the page): below the body, reached by scrolling
(the crown scrolls when no weight is focused): colour name, colour code,
UID, filament id, diameter, nozzle range, spool id, each when known, as
`LabeledContent` rows.

## Digital Crown and haptics

- The crown moves the value 1 g per detent with the detent haptic; turned
  faster than about 6 detents per second it moves 5 g per detent.
  `sensitivity: .low`. Range: the shared 0…9999 (the page's snippet says
  `through: 5000`; no spool reaches either limit).
- A crown turn makes the value touched; Net/Gross still converts it
  (owner).
- Save succeeds: `WKInterfaceDevice.current().play(.success)`, the status
  reads `Saved 590 g left · by iPhone`, and one second later the detail pops
  back to the feed (when it was opened from the feed). The page's green
  ring and "Haptic · success" toast only simulate the haptic; do not build
  them.
- Save fails: `.play(.failure)`, the red error line, the value stays for a
  retry.
- New pops back to the feed one second later too (the page keeps the
  detail open on the added result; see "Implemented differently").
- After a save, focus moves to the newest waiting row; with the feed
  showing again, that row's detail auto-opens.

## Always-On (`isLuminanceReduced`)

The page draws the feed list: no swatches, no status glyphs, no platters,
no connection glyph; title at 55 % white, sub line at 35 %, time at 50 %;
rows do not respond to taps. Other screens are not drawn in AOD: keep their
layout and let the system dim them.

## Settings (read-only, from the iPhone)

Title `Settings`. Platters as in the feed (`#222224`, corner 20, padding
8 × 12), each a 15 pt label over a 13 pt `.secondary` value:

- Header `From iPhone` (12 `.secondary`): `Server` (`bambuddy.local:8000`,
  host and port of the URL), `Core weight` (`242 g`), `Weight as`
  (`Gross` / `Net`). Footer (12 `.secondary`): `Received 2 min ago. Change
  these on iPhone.` (the relative time of the last received settings).
- Header `Flipper`: a platter with the Flipper's name (`Workshop`) over the
  glyph's symbol and phrase in its tint (`Connected`, `nearby`, …), then a
  **Scan for Flipper** button (44 high capsule, `#222224`, `#0A84FF` 15 sb).

**Scan for Flipper** (its own screen): title `Scan for Flipper`, a 12 pt
spinner with `Searching…` (13 `.secondary`), then one platter per Flipper
found: `antenna.radiowaves.left.and.right` 18 pt blue, the name 15, the
signal 13 `.secondary` (`Nearby · strong`, `Nearby`). Tapping one connects
to it and returns to Settings.

Settings sync: the iPhone applies its settings on change and sends each
applied change through WatchConnectivity (`updateApplicationContext`,
latest wins); the watch applies what arrives at once and keeps its own
Flipper choice. The iPad sends nothing.

## Tokens (watch page)

The app uses text styles, so the sizes follow Dynamic Type: 12–13 →
`.footnote`, 14 → `.caption2`, 15 → `.body`, 15–16 sb → `.headline`,
17–19 symbols → `.title3`, the unit 20 sb → `.title3` semibold, 34 →
`.title`; the 50 pt weight scales with `.largeTitle` (`@ScaledMetric`).
Feed rows take two lines at the accessibility sizes.

Weight 50 sb rounded tabular · unit `g` 20 sb · screen title 17 sb · row
and detail title 15 sb · sub lines, readout line, hints 13 · section
headers and footers 12 · buttons 40 high (Scan 44), 16 sb (Scan 15 sb),
capsules; Save tinted (`#0A84FF` 26 %, `#4DA2FF` text), New filled · pills 30 high 13 sb · platter `#222224` corner 20 · background
black. Colours: blue `#0A84FF`, red `#FF453A`, green `#30D158`, orange
`#FF9F0A`, gray `#8E8E93`, secondary `rgba(235,235,245,0.6)`, fills white
16 %.

## Differences from what ships

None remain. The thirteen differences found on 2026-10-27 were closed by
the plan `docs/plans/20261027-apple-ui-from-design.md`, and those after the
design sync of 2026-11-05 by Tasks 10 and 11 of
`docs/plans/20261105-release-1.0.md`; where the app departs from this file,
the next section says how and why. What changed:

1. One `NavigationStack` (`WatchNavigation`) with Settings behind the
   leading glyph.
2. The single connection glyph (`WatchGlyph`) from the merged status.
3. Feed rows: 16 pt swatch, short sub line, trailing glyph; the empty text.
4. Auto-open of a new row that waits, pop one second after a save.
5. Entry: 50 pt value, readout line, Net / Gross pills, 40 pt Save.
6. Crown: `.low` sensitivity, 5 g per detent from 6 detents per second.
7. Net / Gross converts the crowned value.
8. Haptics `.success` / `.failure`.
9. Saved screen with `Weigh again`; unknown screen with one-tap New for
   tags with material and the two hints.
10. Details (colour code, UID, filament id, diameter, nozzle range) in the
    row detail.
11. Always-On: title and remaining only, no taps.
12. Read-only Settings (`WatchSettingsTexts`) with the Flipper platter and a
    separate Scan for Flipper screen.
13. Settings arrive after each change applied on the iPhone.
14. New adds the spool, `Added to Bambuddy` shows as a result with Weigh
    again (2026-11-05); the 2026-10-27 card is gone.
15. Save is tinted (2026-11-05).

## Implemented differently (2026-10-27, updated 2026-11-05)

Where the app departs from this file or fills in what it leaves open, and
why. Items marked "by eye" still wait for the owner's check on screen.

Navigation and glyph
- A Flipper link request on a tag with material shows the unknown body
  with the hint `Link it on the iPhone.` and New, filled as always;
  the asked action (Link) is not on the watch. A relink request on a row
  added here shows the hint without New.
- `WatchRootView` (`host/Sources/FliBuddyUI/Watch/WatchFeedView.swift`)
  holds the one `NavigationStack`; its path is `WatchNavigation`
  (`FliBuddyCore`), whose screens are a row's detail, Settings and Scan
  for Flipper (over Settings).
- The glyph comes from `WatchGlyph` (`FliBuddyCore`), built from the same
  two `StatusItem`s as the macOS status bar and the iPhone banner, in the
  iPhone's short phrase. States the table leaves out keep their status-bar
  phrase and look: `Checking Bambuddy…` (spinner), a Bluetooth problem.

Auto-open and pop
- Auto-open follows the rows rather than the core's focus: the newest row
  that started waiting since the last snapshot opens while the feed shows.
  On the watch no field is touched on the feed, so the iPhone's "never
  steal from a touched field" rule cannot hold it back. A row that started
  waiting while a detail or Settings showed does not open later on its
  own.
- The pop runs one second after a save Bambuddy took
  (`FeedViewModel.save` now reports it). It leaves any other screen alone;
  if the user went back first, the newest waiting row still opens. A save
  from the Flipper while a detail shows does not pop it.

Feed list
- A row that is busy on the host (Creating) shows the 14 pt spinner too.
- The row platter is the list row background; its 12 pt side padding is
  the list's own inset. By eye.
- Secondary glyph tints (Not in Bambuddy, marked empty, deleted) use the
  page's gray `#8E8E93`.
- Always-On: rows are plain (no button), with no swatch, glyph or platter;
  the toolbar glyph is hidden.

Row detail
- `WatchRowScreen` (`FliBuddyCore`) picks the body. While New from the
  watch runs, the busy body reads `Creating in Bambuddy…`; Looking up
  shows its own phrase. The header line stands in for a navigation title.
- After New the detail pops back to the feed one second later, as after a
  save; the page keeps it open on the `Added to Bambuddy` result with
  Weigh again (deferred, owner call).
- The added result's big number is the spool's remaining (its label
  weight); its button reads **Weigh** (the spool was never weighed; the
  page says Weigh again) and opens the crown entry pre-filled to label +
  core. Saved and marked results keep **Weigh again**.
- Unknown hints: a Flipper link request wins (`Link it on the
  iPhone.`); a tag without material otherwise reads `Create it on the
  iPhone.`, also under a Flipper create request; a deleted spool has no hint
  and offers New when the tag has material. A failed New shows `Error:
  <message>` in red under the phrase.
- Save's tinted fill is the watch blue at 26 % and its text `#4DA2FF`
  (`WatchCapsuleButtonStyle.tintedFill` / `tintedText`). By eye.
- Entry: with no value the number reads a dimmed `0`, the line `Turn the
  crown`, and Save stays dimmed; the crown starts at 0. The line drops the
  gross sum (`Saves 590 g left`, as the iPad's short readout) to stay on
  one line. A host error outranks the row's own error. The weight takes
  focus when the entry appears, so Weigh again lands on it. The pills are
  Net then Gross.
- The saved body's big number is what is left on a known spool; without
  one it is left out.
- Details sit under a 12 pt `Details` caption; ids and UIDs are monospaced.

Crown and haptics
- `CrownWeight.fastVelocity` is 6 crown units per second; with one unit
  per detent that is the page's 6 detents per second.
- The haptics follow `FeedViewModel.haptic` on the root view
  (`WKInterfaceDevice.current().play`), so any failed host action (also a
  failed New) plays `.failure`, on any screen.

Settings and Scan for Flipper
- Texts come from `WatchSettingsTexts` (`FliBuddyCore`). Settings that
  have not arrived since launch drop the `Received …` sentence; the age
  reads `just now`, `N min ago`, `N h ago` or `N d ago`. Before any
  settings arrive the section reads `Open FliBuddy on the iPhone to send
  its settings.`; a rejected payload shows its error in red there.
- The Flipper platter shows the Flipper half of the status only, not the
  merged glyph (the page's `replace('Flipper ', '')` on the glyph text
  would put Bambuddy's problems under the Flipper's name); a nameless
  Flipper reads `Flipper`. Bambuddy's status leaves Settings: the glyph
  has it.
- The scan runs while its screen shows and stops on leaving it. After the
  15 s scan ends the spinner gives way to `Scan again`; a Bluetooth
  problem shows in red. `Nearby · strong` from −60 dBm up, else `Nearby`.

## Open questions for the owner

- New pops the detail back to the feed after adding; the page keeps it open
  on the added result with Weigh again. Keep the pop, or stay?
