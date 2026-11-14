# FliBuddy iPad: UI specification from the design

Source: the Claude Design project "FliBuddy mobile app design"
(`docs/design/source/FliBuddy Spec.dc.html` and
`docs/design/source/FliBuddy iPad.dc.html`), imported on 2026-10-27 and
synced again on 2026-11-05. Like the iPhone page, the iPad page imports the
shared prototype model `docs/design/source/flibuddy-core.js` (row
view-model `vm`, connection tables `FLIP` / `BAMBU` and `statusVM`,
`Session`).

This file covers the iPad app (`host/Apple/iOS`, the same target as the
iPhone, views in `host/Sources/FliBuddyUI`). **Everything not stated here is
as in `ios-ui-spec.md`**: the owner decisions, the assumptions and their
answers, the stance, the row-states table and its copy, the weight rules,
the sheets, Tag details, the empty and blocking states, Settings (applies on
change) and the tokens.

Precedence: as in the iPhone spec. Where this file and the shipped code
disagree, this file wins; where it is silent, the iPhone spec, then the
shipped behaviour. The owner decisions outrank the design pages;
`protocol/PROTOCOL.md`, `docs/design/flipper-ui-spec.md` and the approved
design document outrank this file on behaviour.

## What the iPad adds to the iPhone answers

- Assumption 5 (settings apply on change) holds on the iPad as on the
  iPhone. The iPad has no WatchConnectivity, so nothing is sent to a watch.
- The iPad shows both connection states in the sidebar in landscape, and
  the iPhone banner in portrait.
- The iPad turns every way; the iPhone is portrait only.

## Layouts

"Landscape on a bench stand, usually with a keyboard." The design draws
landscape (1366 × 1024) as a `NavigationSplitView` and portrait
(1024 × 1366) as a `TabView` whose tab bar iPadOS draws at the top; the
detail is the feed as tiles in both.

- **Landscape (regular width, the window wider than tall):** sidebar +
  tiles, below.
- **Portrait (regular width, the window taller than wide):** the `TabView`
  (Feed / Spools / Settings) as iPadOS's top tab bar, the iPhone connection banner
  under the title, Clear as the `trash` icon button in the navigation bar,
  the same tiles. The tab bar stays while the keyboard is up.
- **Compact width** (a narrow Split View or Stage Manager window): the
  iPhone layout exactly, stacked cards and all.

The window decides, measured at the root, not the device's orientation:
`FeedLayout.forWindow(width:height:isCompact:)`.

## Landscape structure

```swift
NavigationSplitView {
  List(selection: $pane) {
    Section("Connections") {
      FlipperStatusRow()
      BambuddyStatusRow() }
    NavigationLink(value: .feed) {
      Label("Feed", systemImage: "list.bullet") }.badge(feed.count)
    NavigationLink(value: .spools) {
      Label("Spools", systemImage: "shippingbox") }.badge(activeSpools)
    NavigationLink(value: .settings) {
      Label("Settings", systemImage: "gearshape") }   // shows "⌘," trailing
  }
  .navigationTitle("FliBuddy")
  .safeAreaInset(edge: .bottom) {
    Button("Clear feed", systemImage: "trash", role: .destructive)
      .keyboardShortcut(.delete) }                    // shows "⌘⌫" trailing
} detail: {
  ScrollView {
    LazyVGrid(columns: Array(repeating: GridItem(.flexible()),
                             count: FeedLayout.tileColumns(width: w))) {
      ForEach(feed) { FeedTile($0) } } }              // min 300 pt, at most 3 columns
}
// portrait: TabView (top tab bar), banner, same tiles
// compact width: the iPhone TabView and cards
// Spools pane: SpoolList().inspector(isPresented:) { SpoolDetail() }
//   .inspectorColumnWidth(320); portrait and compact: .sheet instead
```

Sidebar (system sidebar, ≈ 300 wide, glass on iPadOS 26):

| Part | Spec |
|---|---|
| Title | `FliBuddy`, large title |
| Section header | `Connections`, `.subheadline.weight(.semibold)` `.secondary` |
| Status row | symbol 22 pt in the state's tint · name (`Flipper` / `Bambuddy`, `.body` 17) over a sub line (`.footnote` 13, `.secondary`, `.red` when the Bambuddy state is red) · the state's action as a 28 pt `.tint` capsule chip (Connect, Check, Retry, Settings) |
| Flipper sub line | `Workshop · Bluetooth` when connected (the Flipper's name), else the status bar phrase without the `Flipper · ` prefix: `No Flipper · searching` (with a 10 pt spinner), `Flipper not connected`, `Open FliBuddy on the Flipper` |
| Bambuddy sub line | the core's `BAMBU` phrase without the `Bambuddy` prefix, capitalised: `Ready` with the URL's host (`bambuddy.local`) on a line of its own (it wrapped as `Ready · bambuddy.example.com`), `Not checked`, `Not set up`, `Unreachable`, `Rejected the API key`, `API key lacks inventory permission`, `In Spoolman mode`, `Ready · color names unavailable` (the prototype strips the prefix with string replaces, which leaves `· not checked` for one state; the capitalised forms are the intent) |
| Feed | `list.bullet` · `Feed` · the row count as a badge |
| Spools | `shippingbox` · `Spools` · the number of active spools as a badge (none until the first load) |
| Settings | `gearshape` · `Settings` · `⌘,` in `.footnote` `.tertiary` |
| Clear feed | bottom of the sidebar, `trash` · `Clear feed` in red · `⌘⌫`; disabled while the feed is empty |

Symbols and tints for the status rows are the macOS status-bar table
(`macos-ui-spec.md`, "Status bar"), the USB row excepted.

## Feed tiles (both orientations)

The tiles sit in a grid of `.flexible()` columns (`FeedLayout.tileColumns`) on `systemGroupedBackground`: at least
300 pt wide, 12 apart across and down, 28 pt side margins, top-aligned (a
tile is as tall as its content), **at most three to a row**. That makes
three on a 13" iPad (1366 / 1376 pt landscape beside the sidebar, 1024 /
1032 pt portrait) and two on an 11" (1180–1210 pt landscape, 820–834 pt
portrait); a wider window widens the three tiles.

A tile is the iPhone card made compact: `secondarySystemGroupedBackground`,
corner 22, padding 14, 10 between the header and the entry block.

| Part | Spec |
|---|---|
| Header | swatch 24 pt, 10 to the text; title `.headline` (17 sb); colour line `.subheadline` `.secondary`; status symbol + phrase `.subheadline`; timeout note `.footnote`. Same texts as the iPhone card. |
| Weight field | full width, height 52, `tertiarySystemFill`, `.rect(cornerRadius: 12)`, padding 14; value `.system(.largeTitle, design: .rounded).bold().monospacedDigit()` scaled to 30 pt, placeholder `0`; unit `g` `.title3.weight(.semibold)` `.secondary`; focus ring 2 pt `.tint`. The short readout sits right-aligned inside the field, `.caption.monospacedDigit()` `.secondary`: `Saves 590 g`, red `Below core`, nothing while the field is empty (VoiceOver reads the full readout). Accessibility label `Gross weight, grams` / `Net weight, grams`. |
| Net / Gross and Save | one row, 8 apart: the segmented control takes the rest of the width; Save 36 high, at least 80 wide, tinted capsule (`.bordered`, `.borderedProminent` with Increase Contrast), `.subheadline.weight(.semibold)` (15): `Save` / `Saving…`; pointer label `Save (⌘S)` |
| Actions | **New** / **New…** and **Link to existing…** as two equal 36 pt capsules, 8 apart, the prominent one first (as on the iPhone); stacked full width where both labels do not fit whole at the shared width (a 3-column tile), never truncated |

The focused row is marked by its field's focus ring; a row waiting behind a
touched field gets the 2 pt `.tint` outline on the tile. A tap on a saved
or added tile reopens it.

## Spools

As on the iPhone (`ios-ui-spec.md`, "Spools"): the same data, search,
scopes, menu, states and detail, but the spools are **tiles, not
full-width rows**, in the feed's visual language. Design: the `Spools ·
landscape` and `Spools · portrait` frames of `FliBuddy iPad.dc.html`.

- **Tiles (top tabs and sidebar):** a `LazyVGrid` on
  `systemGroupedBackground` with the feed's metrics: at least 300 pt wide,
  12 apart, 28 pt side margins, at most three to a row, the column count
  from the grid's own width (`FeedLayout.tileColumns`). A tile is the feed
  tile's card (`secondarySystemGroupedBackground`, 22 pt corners, 14 pt
  padding): swatch 28, the title with `Low`, `Archived` and the `tag` glyph
  at its trailing end (top-trailing corner), colour and brand, and
  `410 g left of 1000 · updated 3 d ago` on one line (what is left alone
  where the whole line does not fit). The active count sits under the grid.
- The scope control sits above the grid, at most 300 pt wide; iPadOS puts
  the search field in the navigation bar, beside the sort and material
  menu. Pull to refresh works on the grid.
- **Landscape (sidebar):** a tapped tile gets the feed's 2 pt tint outline
  and its detail opens in an `.inspector` on the trailing edge, 320 pt wide
  (`inspectorColumnWidth(320)`), with a close button; the inspector takes
  width from the grid, so a 13" iPad shows two columns beside it. Picking
  another tile swaps the inspector's spool.
- **Portrait (top tabs):** the detail opens in a sheet, as on the iPhone.
- **Compact width** (a narrow Split View or Stage Manager window): the
  iPhone's list of rows.
- The sidebar's Spools badge counts the active spools of the last list; the
  iPad loads the list when the sidebar first shows so the count is there.
- The connection banner stays on the feed; on Spools a Bambuddy problem
  shows as the Spools state instead.

## Keyboard and touch

"Hardware keyboard: same as macOS. Touch: same as iPhone."

1. The newest Enter weight row focuses its field with the suggestion
   selected (the same focus model as the Mac and iPhone; it never steals
   from a touched field). A row not in Bambuddy or an added row takes no
   focus.
2. With a hardware keyboard: type, ⇥ to Net/Gross, ⌘S or Return saves.
   Holding ⌘ shows the shortcut overlay (the commands below are declared as
   menu commands so they appear there).
3. Without a keyboard the system number keyboard appears; the grid scrolls
   the focused tile into view above it.
4. Portrait keeps the same tiles; a top tab bar replaces the sidebar.
5. Clear lives in the sidebar (landscape), the toolbar (portrait) and the ⌘
   menu; it asks for confirmation.

| Action | Keys |
|---|---|
| Save focused row | ⌘S or Return in the field |
| Clear feed (with confirmation) | ⌘⌫ |
| Settings | ⌘, |
| Field → mode → Save | ⇥ (⇧⇥ back) |
| Next / previous row | ⌘↓ / ⌘↑ (macOS parity; not on the iPad page) |

A successful save gives the iPhone's success haptic only where the device
has one; the iPad has none, so nothing extra.

## Sheets and dialogs

As on the iPhone (New spool, Link to existing, Tag details, Scan QR code),
presented as form sheets. Two differences on the iPad page:

- **Link to existing**: the move warning is inline under the list, not an
  alert: `exclamationmark.triangle.fill` orange + `<title> <colour> is linked
  to tag <tag>. Saving moves the link to <uid>.`; the ✓ button then moves the
  link. Use the inline form on the iPad (it has the room) and the alert on
  the iPhone.
- **Clear the feed?** is a centred alert-style dialog with **Clear feed**
  (destructive) and Cancel, same copy as the iPhone, in both orientations.

## Settings

The iPhone form in the detail column (landscape) or the Settings tab
(portrait), title `Settings`, applies on change. The page shows the QR
footer `Scan the QR code Bambuddy shows when you create an API key.`, the
test result `Ready · inventory OK`, the Flipper rows (`Workshop`, state,
`Scan for Flipper`) with the footer `The Flipper must be running
FliBuddy.` (no `Bluetooth only on iPhone`), then **Forget This Flipper** in
a section of its own, centred, and the version footer `FliBuddy 1.0 (42)`.
Without a camera the iPhone's no-camera footer applies.

## Differences from what ships

None remain. The seven differences found on 2026-10-27 were closed by the
plan `docs/plans/20261027-apple-ui-from-design.md`, and those after the
design sync of 2026-11-05 by Task 11 of
`docs/plans/20261105-release-1.0.md` (the iPhone list covers everything
shared); where the app departs from this file, the next section says how
and why. What changed:

1. Layout by the window (`FeedLayout`): sidebar in landscape, top tabs in
   portrait, the iPhone layout at a compact width. The 2026-10-27 rule (a
   one-line table when the window leaves 900 pt beside the sidebar, else
   the iPhone cards) is gone: it gave an 11" iPad in landscape the cards
   without a sidebar.
2. Sidebar (`Sidebar`, `PadSidebar`): Connections with the two status rows
   and their chips, Feed with the count badge, Settings with `⌘,`, Clear
   feed at the bottom.
3. No status bar in the detail column: the sidebar (landscape) or the
   banner (portrait) shows both states.
4. The feed as tiles in both orientations (2026-11-05), replacing the
   one-line table and the portrait cards at most 760 wide.
5. ⌘S, ⌘↑ / ⌘↓, ⌘⌫ and ⌘, as menu commands (`FeedCommands`), listed in
   the ⌘ overlay.
6. Clear feed in the sidebar.
7. Link warning `Saving moves the link to <uid>.`
8. Settings (2026-11-05): the Flipper footer, Forget in its own section,
   the version footer.

## Implemented differently (2026-10-27, updated 2026-11-05)

Where the app departs from this file or fills in what it leaves open, and
why. Items marked "by eye" still wait for the owner's check on screen.

Layout
- `FeedLayout.forWindow(width:height:isCompact:)` (`FliBuddyCore`): a
  compact size class gets `.cards`; otherwise a window wider than tall
  gets `.sidebar`, else `.tabs`. Within 20 pt of square either way
  (`squareDeadBand`, 40) the layout keeps what it had, so a Stage Manager
  resize across square does not flip it at every point. The window is
  measured at the root with `onGeometryChange` as the view's size plus its
  safe-area insets (`FeedLayout.windowSize`), because the keyboard is a
  bottom inset: a 13" iPad in portrait reports 1032 × 941 with the
  keyboard up, which alone read as landscape. `ignoresSafeArea` on the
  measuring view does not change what the geometry reports. The first frame
  (size zero) is the portrait layout until the size arrives.
- The grid has as many flexible columns as `FeedLayout.tileColumns` allows
  (`.adaptive` cannot cap the count at three: min 300 pt, max 3), measured from the grid's
  own width. By eye: three on a 13", two on an 11", in both orientations.
- Rotating swaps `NavigationSplitView` and `TabView`, so the feed and the
  Settings form are built again. The Settings model lives in `RootView`,
  so a field being typed keeps its draft; the focused row takes the keyboard back from the feed. Unlike
  before, leaving the Settings pane keeps the model too, as the iPhone's
  tab always did. The QR cover hangs off the root (`settingsSession`) and
  its scan lives in the model (`QRScanCover`), so a rotation keeps both;
  however the cover goes, an open scan ends as cancelled. Only leaving
  Settings (another tab or pane) commits the field and stops a Flipper
  scan (`SettingsViewModel.left()`); a rebuild does neither. By eye: rotate while typing a weight, while Settings is
  open, and with the QR cover up.

Sidebar
- The rows come from `Sidebar` (`FliBuddyCore`), built from the same two
  `StatusItem`s as the macOS status bar and the iPhone banner. A connected
  Flipper without a saved name reads `Flipper · Bluetooth`. Sub lines for
  states the table above leaves out: `Checking…` (with the spinner); a
  Flipper problem (`Bluetooth is off`, a version mismatch) shows as it is.
- A state that spins (searching, checking) keeps its symbol
  (`antenna.radiowaves.left.and.right.slash`, `server.rack`) and shows the
  10 pt spinner before the sub line. The chip fills with the app tint, as
  on the iPhone banner. By eye.
- Clear feed is a plain red button in the sidebar's bottom inset with
  `⌘⌫` trailing; disabled it dims. By eye.

Tiles
- A tile's header is the iPhone card's header with a 24 pt swatch; the
  UID stays in Tag details (owner decision). The tile draws its own
  background in a `ScrollView`, so the context-menu preview is the tile's
  rounded shape. By eye.
- The design's selected-row tint is not drawn (the iPad page computes it
  but no tile uses it): the field's focus ring marks the focused row.
- Save and the actions are stock `.bordered` / `.borderedProminent`
  capsules at the regular control size with a 22 pt label minimum height,
  about 36 pt, not an exact frame. The segmented control keeps its system
  height. By eye.
- The tile's field is the iPhone field (`PhoneWeightField`) in its tile
  size; Return in it saves with a hardware keyboard, on the iPhone too.

Keyboard
- The commands are `FeedCommands` on the app's scene: a `Feed` menu with
  Save Focused Row (⌘S), Previous Row (⌘↑), Next Row (⌘↓) and Clear Feed…
  (⌘⌫), and Settings… (⌘,) in the app menu. ⌘⌫ shows the feed and asks;
  ⌘, opens Settings in any layout. The iPhone gets the same commands.

Dialogs
- Clear the feed? is an alert in both iPad layouts; a compact window keeps
  the iPhone's confirmation dialog.
- The inline Link warning goes by device (an iPad shows it in every
  layout, never the alert): `<title> <colour> is linked to tag <tag>.
  Saving moves the link to <uid>.` The ✓ button keeps its title; the Mac
  keeps `… Linking moves it to <uid>.` and `Move link`.

Settings
- The form is the iPhone's, at most 640 wide and centred in the detail
  column (owner decision: no full-width form on the iPad;
  `FeedLayout.settingsMaximumWidth`).

## Open questions for the owner

None. Decided 2026-11-05: tiles in both orientations, the sidebar in
landscape, the top tab bar in portrait.
