# FliBuddy: UI design brief for macOS, iPhone, iPad and Apple Watch

Prompt for an external design model. Paste it whole, or trim the platform
sections you do not need. The "Deliverables" section can be shortened when the
tool only renders frames. Brought up to date on 2026-11-05 with the design it
produced (`docs/design/source/`) and the owner's answers, so a new round
starts from what is decided.

---

## Role and goal

You are designing the user interface of **FliBuddy**, a small production
tool for a 3D-printing workshop. Design native Apple apps for macOS, iPhone,
iPad and Apple Watch that follow the current Apple Human Interface Guidelines
(the iOS 27, iPadOS 27, macOS 27 and watchOS 27 idiom: system materials and
Liquid Glass through standard controls; deployment minimums are iOS, iPadOS
and watchOS 26 and macOS 14, so the macOS app must still look right there).
Optimise for speed and glanceability during repetitive physical work, not for
marketing polish. Use only native, standard SwiftUI components and system
styling; no custom chrome, no illustrations, no onboarding tour.

## Who uses it and how

One person in a workshop. In one hand a **Flipper Zero** (a handheld NFC
reader running our FliBuddy app), on the bench a **kitchen scale** and a filament
spool. The person:

1. holds the spool's NFC tag to the Flipper → a row appears in the app's feed
   within a second, already identified against the inventory server
   (**Bambuddy**, self-hosted on the LAN);
2. reads the number on the scale and types it (**gross**, the whole spool) or
   types the filament-only number (**net**); the app sends it to Bambuddy;
3. moves on to the next spool.

The weight entry is the critical path on every platform. Typing must need
the fewest taps possible, the suggested value must be pre-filled, and the
result must be readable from arm's length. The Flipper itself also lets the
user type the weight on its own buttons; the app and the Flipper are peers,
whichever side saves last wins, and the app shows which side saved.

Secondary flows:

- **New**: the tag is not in the inventory yet. For Bambu Lab tags the app
  already knows material, subtype, colour and label weight, so **New** is one
  tap: the spool is added at its label weight ("Added to Bambuddy") with no
  weight prompt, and a tap on the row weighs it anyway. For other tags a
  short form asks for material (required), subtype, colour, brand and label
  weight.
- **Link to existing**: the tag belongs to a spool that was added to Bambuddy
  by hand. A picker over the inventory with a text filter (material, colour,
  brand or note); if the tag is already linked to another spool the app says
  which one and offers to move it.
- **Spools** (iPhone and iPad): a read-only browser of what Bambuddy holds,
  to look a spool up at the bench: search (material, colour, brand or note),
  Active · Archived · All, a material filter and four sort orders; a row
  shows what is left and when it was last updated, `Low` and `Archived` in
  words; the detail lists every fact Bambuddy keeps and opens the spool in
  Bambuddy for edits. No editing in 1.0.
- **Settings**: Bambuddy URL and API key, the empty-spool weight used for new
  items, net/gross default, the Flipper connection (USB on Mac, Bluetooth
  everywhere), and on iPhone/iPad a **Scan QR code** button that fills URL
  and key from the QR code Bambuddy shows when a key is created. Changes
  apply as they are made, no Save button; Forget This Flipper sits apart,
  and the app's version and build close the form.

Everything is one session in memory: the feed is a log of what was scanned
since the app started, newest first, with a **Clear** action.

## Platform matrix

| Platform | Transport to the Flipper | Layout | Screens |
|---|---|---|---|
| macOS 14+ | USB serial or Bluetooth LE | one main window, Settings window via ⌘, | Feed, Settings |
| iPhone | Bluetooth LE | `TabView`, portrait only | Feed, Spools (read-only inventory), Settings |
| iPad | Bluetooth LE | `NavigationSplitView` in landscape, `TabView` (top tab bar) in portrait | Feed as tiles, Spools with the detail in an inspector (a sheet in portrait), sidebar with status, Spools count and settings entry |
| Apple Watch | Bluetooth LE | compact | Feed list, Row detail (weight entry), Settings (read-only, received from the iPhone), Scan for Flipper |

Out of scope: the Flipper's own 128×64 monochrome screens; the watch never
types settings (they arrive from the phone); USB exists only on macOS.

## The feed row

Each row shows, in this priority:

1. **Colour swatch** from the tag's RGBA, or a neutral placeholder when the
   tag has no colour.
2. **Title**: material and subtype, e.g. "PLA Basic", "PLA Pure", "PETG HF".
3. **Colour name** when the inventory knows it ("Pumpkin Orange"); nothing
   when it does not (never show the hex as a name).
4. **Remaining**: "15 g left of 1000" for known spools; "label 1000 g" for
   unknown tags with prefill; empty otherwise.
5. **Status** (secondary text, red for errors):
   - `Looking up` (just scanned, waiting for the inventory)
   - `Enter weight` (found; the weight field is active)
   - `Not in Bambuddy` (unknown tag with material; offers **New** and
     **Link to existing…**)
   - `Not in Bambuddy; tag has no material` (unknown, no prefill; **New…**
     opens the form)
   - `Saved 15 g left`, with `by host` or `by Flipper`
   - `Added to Bambuddy` (after New, or created on scan; no weight prompt)
   - `Marked as new`, `Marked as empty · archived`, `Deleted from Bambuddy`
     (actions taken on the Flipper)
   - `Error: …` (network, permission, conflict)
   - a request from the Flipper: `Flipper asks to create it here` / `Flipper asks to link it here`
6. **Weight entry**: a numeric field pre-filled with the suggested value
   (last scale weight, or remaining + core), unit "g", a Net/Gross segmented
   control, **Save**, a tinted button rather than a filled one (the weight
   is the focus; filled with Increase Contrast). On the watch this is the
   Digital Crown.
7. **UID** of the tag in tertiary text (e.g. `7EF1AB04`), useful when two
   spools look alike. The apps move it, with the other tag facts, to a
   Tag details view.

Example rows from a real session (use them in the mock-ups):

- Swatch #FF9016 · **PLA Basic** · Pumpkin Orange · 15 g left of 1000 ·
  Saved 15 g left · by host · suggested gross 257 g (core 242 g)
- Swatch #A4DBE8 · **PLA Pure** · (no colour name) · label 1000 g ·
  Not in Bambuddy → **New** / **Link to existing…**
- Swatch #00AE42 · **PLA Basic** · Bambu Green · 590 g left of 1000 ·
  Enter weight · suggested gross 832 g
- No swatch · **NTAG 7-byte UID 04A2…** · Not in Bambuddy; tag has no
  material → **New…** (form) / **Link to existing…**

## Connection and system states

Show these in a status bar (macOS), the sidebar (iPad landscape), a compact
banner (iPhone, iPad portrait) or a glyph (watch). Every state needs a
design, light and dark:

- Flipper: `No Flipper` (searching) · `USB` · `BLE advertising` (Flipper
  visible, not connected) · `BLE connected` · `Connected, no hello` (link
  open, app on the Flipper not running) · `other version` (the Flipper's
  FliBuddy speaks another protocol version).
- Bambuddy: `not set up` · `not checked yet` · `ready` · `unreachable` ·
  `API key rejected` · `API key lacks inventory permission` · `Bambuddy is
  in Spoolman mode` (the app refuses to work until the user changes it) ·
  `colour names unavailable` (non-blocking; rows show no colour name).
- Both sides down: the banner shows two lines, the Flipper first, each with
  its own action.
- Row-level: saving in progress; the Flipper's 10-second timeout fired while
  the host still saved (row shows saved, Flipper showed an error).
- Empty feed: "Hold a spool's tag to the Flipper running FliBuddy."
- Settings: connection test in progress / ok / failed; QR scan unavailable
  on this device.

## Platform specifics

**macOS.** Main window: toolbar with Clear and Settings; the feed as a
`List`; status bar at the bottom with the Flipper and Bambuddy states. The
weight field accepts typing immediately when a row becomes `Enter weight`
(focus follows the newest row that needs a weight). Keyboard: ⌘S save, ⌘⌫
clear, ⌘, settings, Tab between field and mode. Settings is a grouped `Form`
with four sections that apply on change: Bambuddy (URL, API key, Test
connection), New items (empty-spool catalog entry, core weight number with
the explanation "weight of everything that is not filament; 242 g = Bambu
reusable spool 215 g + refill core 27 g"), Weighing (Net/Gross default),
Flipper (transport: USB/Bluetooth, port picker or Scan for Flipper, Forget);
the version under them.

**iPhone.** Portrait only. Feed, Spools and Settings tabs. Rows are tall enough
for a thumb; the weight field opens the numeric keypad with the suggested
value selected; Net/Gross is a segmented control under the field; Save is a
44 pt tinted button that stays above the keypad. The New form and the Link
picker are sheets. Settings has the Scan QR code button in the Bambuddy
section, opening the camera full-screen; Forget This Flipper in a section
of its own; the version at the bottom.

**iPad.** Landscape on a bench: sidebar with the connection states, Settings
entry and Clear; the detail is the feed as compact tiles in an adaptive
grid, at least 300 pt wide and at most three to a row (three on a 13" iPad,
two on an 11"), with Net/Gross and Save on one row and a short readout
inside the field. Portrait keeps the tiles and swaps the sidebar for a top
tab bar and the iPhone banner; a narrow multitasking window gets the iPhone
layout. Spools sits in the sidebar between Feed and Settings with the
number of active spools; a picked spool opens in an inspector beside the
list (a sheet in portrait). Keyboard shortcuts as on macOS.

**Apple Watch.** Three screens. *Feed list*: swatch, title, remaining,
status glyph; newest first; tap opens the row. *Row detail*: the weight as a
large number driven by the Digital Crown (1 g per detent, faster turns move
5 g), Net/Gross toggle, a tinted Save; for unknown tags a single **New**
button. A
haptic on save and on error. *Settings*: read-only values received from the
iPhone (server, core weight, mode), a "Scan for Flipper" button, and the
Flipper connection state. Always-on display: the feed list dims to title
and remaining only.

## Constraints

- Native SwiftUI only: `List`, `Form` (grouped), `NavigationSplitView`,
  `TabView`, toolbars, sheets, `ContentUnavailableView`, SF Symbols, system
  semantic colours, system materials, Dynamic Type, VoiceOver labels on
  every control, keyboard shortcuts on macOS/iPad, haptics on watch.
- Large hit targets, minimal typing, numbers readable from one metre.
- Colour swatches must not be the only carrier of meaning; the title and
  colour name are always present.
- No account system, no multi-user, no charts or analytics, no offline
  queue, no Spoolman, no onboarding flow beyond Settings and the QR scan.
- Name the app "FliBuddy" and the server "Bambuddy" in copy. Do not use
  "SpoolBuddy": that is the name of Bambuddy's own kiosk device. Copy is
  English, short, sentence case.

## Deliverables

1. Screen-by-screen specification per platform: layout, component
   hierarchy in SwiftUI terms, every state listed above, light and dark,
   iPad landscape and portrait (13" and 11"), watch with and without
   always-on dimming.
2. The feed row as a component spec: anatomy, states, spacing, type roles.
3. The weight-entry interaction on each platform, step by step, including
   focus and keyboard behaviour on macOS and the crown behaviour on watch.
4. A token list: spacing scale, type roles (system text styles), semantic
   colours (system names), corner radii, SF Symbol names used.
5. Rendered frames for: macOS main window with four rows; macOS Settings;
   iPhone feed with a row in `Enter weight`; iPhone New form sheet; iPhone
   Spools list and spool detail; iPad landscape, also on Spools with the
   inspector; watch feed list and watch row detail.
