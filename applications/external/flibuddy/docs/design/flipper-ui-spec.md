# Flipper UI spec

The screens of the FliBuddy Flipper app, after the Claude Design page
"FliBuddy Flipper" (`docs/design/source/flipper-ui.js`, the prototype's
renderer and state machine). The design's pixel positions are the target;
this document is what the app does. The flow is the pure reducer in
`flipper/src/app_state.{h,c}`, all wording is built by
`flipper/src/ui_frame.c` and both are tested on the host
(`flipper/tests/test_app_state.c`, `test_ui_frame.c`); `ui_draw.c` only
places the frame on the canvas. Keep this file current when either changes.

## Conventions

Screen 128×64, 1 bit. OK always does the screen's main action, ◀ opens the
menu or cancels, ▶ is the quick second action, Back goes back. The bottom
row holds up to three button chips (◀ label left, OK label centre, label ▶
right; the firmware's `elements_button_left/center/right`). An OK chip too
wide for the gap beside a ▶ (or ◀) chip moves aside to keep 2 px clear of
it, drawn as the firmware draws it (Waiting's `Get app` beside `Settings`).
A key without a chip does nothing, except where a screen says so below.

Titles use FontPrimary with a dotted rule under them, body text
FontSecondary, the weight digits FontBigNumbers. The transport icon (USB or
Bluetooth) sits top right on every screen with a header: the connected
host's transport, else USB for Connection = USB and Bluetooth otherwise;
the Bluetooth icon blinks while the Flipper advertises without a host. Icons are XBM files in
`flipper/images/`, generated from the design's bitmaps by
`tools/fixtures/make_flipper_icons.py` (a pytest fails when they are stale);
the Get app QR code is generated from its URL by the same script, which
also writes it into the design's table.

The firmware fonts are ASCII only: `...` stands for the ellipsis and ` - `
for the design's middle dot (`PLA Basic - 590 g left`). A spool is titled
`material subtype` (`PLA Basic`), or the subtype alone when it already
starts with the material (`PLA Silk+`).

## Screens

Keys: OK / ◀ / ▶ / ▲▼ / Back.

1. **Waiting for app** (no host): title "FliBuddy", the version, a USB
   line and a Bluetooth line, each with its icon, as in the design, then
   "Waiting for app" with animated dots. The USB line reads `USB: no app`,
   `USB unavailable` (the port did not open) or `USB off` (Connection =
   Bluetooth); the Bluetooth line `Visible as <name>` (advertising; the
   icon blinks), `Bluetooth: no app` (a central subscribed without a
   `hello`) or `Bluetooth off` (Connection = USB, not advertising, or the
   profile did not start). A host app whose `hello` speaks another
   protocol version replaces its transport's line with `App protocol N`
   until a host connects or that port closes. The design's "Cable OK, no
   app" (a cable without the app) is not told apart from no cable: the
   Flipper only sees the port opened. OK Get app (screen 24), ▶ Settings,
   Back exits the app. A tag read here shows Result · no host.
2. **Ready** (host connected): spool picture with animated waves, "Ready",
   "Hold spool tag to the back". After an action an inverted bar with a
   check names the last outcome (`Saved PLA Basic 590 g`, `Added PLA
   Silk+`, `New PLA Basic 1000 g`, `Empty PLA Basic`, `Deleted PLA Basic`)
   and the text reads "Next spool". ◀ Recent, ▶ Settings, Back exits the
   app.
3. **Reading tag**: spool and waves, "Keep still". Back goes home.
4. **Looking up**: while the `lookup` is pending (no timeout): the tag's own
   detailed type or material when it has one, "Asking the app". Back goes
   home. Not in the design.
5. **Result · known**: colour swatch (Bayer-dithered by the luminance of the
   spool's RGBA) and the spool as title; the colour name (or
   `Color #RRGGBB`); a check and `In Bambuddy #67` (the UID is in Tag
   details); a remaining bar against the label with `590 / 1000 g`, or
   inverted `Unused - 1000 g` when nothing was printed from it yet
   (remaining = label), or inverted `Archived` for a spool marked empty.
   OK Weigh, ▶ New (Marked as new, screen 16), ◀ Menu, Back home. Without the host's spool fields: `Tag 04B7...3C80` (a 4-byte
   UID is shown whole) and the host's lines.
6. **Result · unknown Bambu tag** (Auto-save new off): swatch and the tag's
   material as title; the colour name (Bambuddy's, else Bambu's own);
   `11803 - 1.75 mm - 1000 g` (Bambu's colour code, diameter, label weight;
   the UID when the tag has none of them); inverted `Not in Bambuddy`. OK Add
   (adds this spool to Bambuddy as new and full), ▶ Link, ◀ Menu (Add and
   weigh: Weight, whose Save adds it with that weight, for a started
   spool), Back home (nothing is added). A tag with data but no material is
   titled `Spool tag`, inverted `Not in Bambuddy`, OK Create (asks the host
   to create it), ▶ Link.
7. **Result · blank tag**: `Blank tag`; the tag type; UID; inverted
   `No spool data`. OK Link (asks the host), ◀ Menu, Back home.
8. **Result · no host**: what the tag itself says, read on the Flipper
   (`flipper/src/tag_facts.h`, the block layout of the host's TagParser).
   A Bambu Lab tag: its detailed type or material as title, the swatch of
   its RGBA, the colour name (Bambu's English name for the tag's filament
   and colour ids, from `flipper/src/bambu_color_table.h`, generated from
   `tools/data/bambu_color_codes.json` as the host's table is; else
   `Color #RRGGBB`) and `10100 - 1.75 mm - 1000 g` (Bambu's colour code,
   diameter, label weight), as Result · unknown shows them from the host.
   Any other tag, or one without those blocks, shows its type and
   `Tag 04B7...3C80`. Inverted `No app`. ◀ Menu (Tag details only), Back
   home. Result · no host (no app, or the app went away) and its Menu and
   Tag details show only the tag's own facts; every other result only what
   the host sent.
9. **Menu** (▲▼ wrap, OK chooses, Back to the result; four rows visible):
   known: Weigh, Mark as new, Mark as empty, Relink in app, Tag details,
   Delete spool. Unknown Bambu: Add new, Add and weigh, Link in
   app, Tag details. No material: Create in app, Link in app, Tag details. Blank:
   Link in app, Tag details. No app: Tag details.
10. **Tag details**: Type, UID with colons, Data (`15/16 sectors` or
    `135 pages`), then what is known: Code (`11803`), Fila (filament id
    `GFA01`), RGBA, Label (`1000 g`), Diam (`1.75 mm`), Nozzle (`190-230 C`),
    from the host, or without the app from the tag, which then also gives
    Dry (`55 C 8 h`, drying temperature and hours) and Bed (`35 C`). Five
    rows show; ▲▼ scroll, with arrows at the right when there is more.
    OK / Back to the menu.
11. **Delete spool?**: warning icon, the spool and colour, "Removes it from
    Bambuddy for good." ◀ Cancel and Back to the menu, ▶ Delete (far from
    OK); OK does nothing.
12. **Weight**: title "Weight", a net/gross chip top right, four big digits
    with dimmed leading zeros and a cursor bar under the active digit; the
    cursor starts on the hundreds. ◀▶ move (wrapping); the position left of
    the first digit is the mode chip, where ▲▼ toggle net / gross and
    convert the number on screen, typed or not (gross - core, floored at 0;
    net + core). ▲▼ on a digit change it (wrapping;
    held, they repeat). OK Save, Back to the result. The line under the
    digits: gross `Net 590 g - core 242`, gross without a known core
    `Core weight unknown`, net `Saves 590 g left`; gross below the core
    shows inverted `Below core weight`, and OK flashes red without sending.
    Pre-fill: gross = remaining + core, net = remaining; without a spool
    the lookup defaults (gross: the last gross, else label + core; net:
    gross - core, else label). Weight opens in the Weigh as setting's mode
    every time; the chip changes it for that entry only. After Add and
    weigh on a new spool, Save creates the spool and records the weight.
    With After scan = Weigh a scan of a spool in Bambuddy opens Weight at
    once (not one Auto-save new just added, not a fresh lookup after a
    link); Back shows its result.
13. **Saving / Adding / Deleting / Updating** (title per action): the spool
    (`- 590 g` net when saving), an indeterminate bar, "Waiting for
    Bambuddy". No keys; Error · No app reply after 10 s.
14. **Saved**: big check, "Saved", `PLA Basic - 590 g left`; after 1.3 s
    (or OK / Back) Ready with the Saved bar.
15. **Added to Bambuddy**: the spool and colour, `Spool #31 - 1000 g`. OK
    Weigh, ▶ Next (Ready with the `Added ...` bar), Back Ready. No time
    limit. Shown after Add on a new spool, and at once after a scan when
    Auto-save new is on and the host created the spool.
16. **Marked as new** (Mark as new, in the menu or ▶ on a known result):
    `1000 g left - full`, a 3 s progress bar and "Next spool in 3 s". ◀ Undo, OK Weigh, Back Ready.
17. **Marked as empty**: `0 g left - archived`, the same bar. ◀ Undo; OK and
    Back go to Ready.
18. **Deleted**: "Tag is free to reuse", the same bar, no Undo. When the
    bar runs out, Ready with the Deleted bar; Back goes to Ready at once,
    without the bar; OK does nothing.
19. **Link tag / Create in app**: "Pick the spool in" / "Finish the spool
    in", "the FliBuddy app", bouncing dots. ◀ Cancel and Back to the result.
    The host's `result` only stops the 10 s timeout; when the host links or
    creates the spool it pushes a fresh `lookup` for the same scan and the
    screen becomes Result · known. Such a lookup also replaces Result, Menu
    and Tag details for that scan.
20. **Error**: a title per case, warning icon, up to three lines (the
    host's `lines` and `error` when present). Titles follow the failed
    action (`Not saved`, `Not added`, `Not deleted`, `Not updated`,
    `Not linked`) or the case (`Lookup failed`, `No app reply`, `App
    lost`, `Not sent`, `Tag not read`, `Unsupported tag`). `Tag not read`
    has no chips: it says "Hold the tag still against the back. Reading
    again...", runs a 1.5 s countdown bar and returns to Ready by itself
    (Back sooner). `Unsupported tag` (an unsupported SAK, a
    password-protected NTAG) is the same screen with "Not a spool tag
    FliBuddy can read.", no error cue (no red LED, vibro or sound) and no
    retry text; the tag stays remembered until it leaves the field. Back
    from `Lookup failed`, and from a result shown without the host, makes
    the reader forget the tag, so a spool still on the Flipper is read
    again. After a failed intent, while the
    host is there, ◀ Back and Back return to the result and ▶ Retry
    resends the same intent under its id; otherwise there is no Retry chip
    and ◀, ▶ and Back go home.
21. **Settings** (▲▼ select, ◀▶ change the value in place, Back saves and
    returns home; four rows show): Connection (Auto, the default / USB / Bluetooth; Auto
    runs both and the first host to say hello wins), Auto-save
    new (Off / On), After scan (Show / Weigh), Weigh as (Gross / Net),
    Feedback (Vibro+LED / LED / Off), Sound (Off / On), Version (OK opens
    About). Leaving with another Connection closes the transports it no
    longer uses and opens the new ones; a transport in use that failed to
    open is opened again. A host on a transport that keeps running stays.
22. **About**: app icon, "FliBuddy", "for Bambuddy", `Version 1.0`,
    `Protocol 2 - fw 1.5.1`, `USB app: <app>` / `BLE app: <app>` (the
    transport first, so a long app name is cut, not the transport) or
    `App: not connected`. OK / Back to Settings.
23. **Recent** (Ready's ◀; ▲▼ select, wrapping, four rows show; OK opens;
    Back to Ready): the spools whose weight was saved this session, newest
    first, one row per tag, six at most (the oldest goes). A row has a
    check, the spool's name and, right-aligned, what is left (`PLA Basic`
    … `484 g`), so a long name is cut, not the grams; the design writes
    `PLA Matte 410 g` as one label. With none: "No scans yet". OK shows
    the row's Result · known from the scan and spool it keeps, so the spool
    can be weighed again (or marked, deleted, ...) without a scan; its
    intents carry that scan's id and tag. A later lookup or result for a
    listed tag updates its row in place, a delete removes it. The list
    lives in RAM and is gone when the app exits. Losing the host goes to
    Waiting; the list stays. The reader is off here.
24. **Get app** (Waiting's OK): no header; on the left "No app yet?"
    (FontPrimary), "Scan for the", "iPhone app", "App Store:" and
    **FliBuddy** (FontPrimary); on the right a QR code of
    `https://sokogen.github.io/flipperzero-flibuddy/` (version 3, 29
    modules at 2 px: 58×58 at x 67, y 3, so 3 px stay clear to the edges),
    generated into `flipper/images/qr_get_app.xbm`. ◀ Back and Back return
    to Waiting; OK and ▶ do nothing. A host that connects moves on to
    Ready. A tag read here shows Result · no host, as from Waiting. It
    never opens by itself: the app starts on Waiting, also the first time.

## Mark as new, Mark as empty and Undo

Undo is local. The Mark toasts send nothing when they open; ◀ Undo returns
to the result and nothing leaves the Flipper. The mark is sent (`full` /
`empty`) when the bar runs out, on OK, on Back, or when another tag comes
near; it then goes through Updating to Ready with the New / Empty bar
(no second feedback). OK on Marked as new sends `full` first and opens
Weight once it is done. Losing the host on a pending toast drops the mark
and shows Result · no host. The design commits at once and offers Undo
afterwards; this app holds the change for the 3 s instead.

## Reading tags

The reader runs on Waiting, Get app, Ready, Reading, Result, Saved and the
toasts. A
tag that was read is not read again until it has left the field (three
150 ms polls in a row without an answer) or another tag answers, so a spool
left on the Flipper does not rescan in a loop. Only one intent is ever in
flight.

## Feedback

Per the Feedback and Sound settings (`flipper/src/feedback.h`):

| Event | LED | Vibro | Sound = On |
|---|---|---|---|
| host connected, tag read | blue | short | - |
| saved, added, marked, deleted, a link's fresh lookup | green | short | success beeps |
| error, Save refused below the core | red | long | error beeps |

Feedback = LED drops the vibro; Off drops the LED and the vibro. Sound = On
adds the beeps whatever Feedback says. Every event wakes the backlight.

## Wording

The Flipper's own texts say "app" for the FliBuddy app on the phone or
watch (`Waiting for app`, `No app`, `Create in app`, `Pick the spool in the
FliBuddy app`, `No app reply`: "No reply in 10 s. Open the FliBuddy app.");
the design page still says "host", and the prose here keeps "host" for the
protocol peer.
