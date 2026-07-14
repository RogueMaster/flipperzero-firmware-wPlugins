# Polk MagnifiMini Remote — Flipper Zero FAP

IR remote for the **Polk MagnifiMini soundbar**. Runs on Flipper Zero as an external app (FAP).

## Build

```sh
cd /Users/varun/Workspace/Hardware/Flipper/Apps/PolkRemote
ufbt          # build
ufbt launch   # build + deploy via USB
```

`ufbt` auto-generates `polk_magnifimini_remote_icons.h` from `images/*.png` — do not create this file manually. The header name is derived from `appid` in `application.fam`; if `appid` ever changes, update the `#include` in `scene_main.c` to match.

## Display

Portrait orientation (`ViewOrientationVertical`): **64 px wide × 128 px tall**.  
The canvas origin is top-left. Both views (`main_view`, `submenu_view`) use portrait.

## File map

| File | Purpose |
|---|---|
| `polk_remote.h` | Structs, enums, `ROWS_COUNT`, declarations |
| `polk_remote.c` | IR constants, `ROWS[]` array, alloc/free/entry |
| `scene_main.c` | Main grid view — draw + input |
| `scene_submenu.c` | Submenu view — draw + input |
| `scenes.h` | Scene IDs, custom event, scene callback declarations |
| `application.fam` | FAP manifest (`appid="polk_magnifimini_remote"`) |
| `images/` | 16×16 PNG icons (power, mute, night, vol_dn, vol_up, prev, play, next, minus, plus); `remote.png` (24×24 FAP menu icon) |

## Layout — main view

```
y=0..15   Title bar: "Polk Magnifi" centred, bottom separator line
y=16..33  Row 0
y=34..51  Row 1
...        (ROW_H = 18, VISIBLE_ROWS = 6)
```

**Why ROW_H=18 (not 16):** Icons are 16×16. If ROW_H=16, `canvas_draw_frame`'s top/bottom lines coincide with icon pixel rows and get overwritten (invisible border). 18 px gives a 1 px margin; icons are drawn at `row_y + 1`.

**Why icon cx=10 (left) and cx=54 (right):** `canvas_draw_icon(cx - 8, ...)` → icon starts at x=2. cx=8 would start icon at x=0, overwriting the frame's left border line. This applies to both `RowLOR` and `RowLR`.

Only 6 of 7 rows are visible at once. `scroll_offset` in `MainViewModel` tracks the first visible row index. Navigation updates it atomically with `selected_row` inside `with_view_model`.

## Row types

```c
RowSingle   // OK fires cmd_ok; no left/right (unused)
RowLR       // Left=cmd_left, Right=cmd_right; centre shows label text only (not clickable)
RowLOR      // Left/OK/Right each fire a command
RowSubmenu  // OK opens submenu; no direct IR commands on the row itself
```

Current rows (ROWS_COUNT = 7):

| # | Label | Type | Left | Centre | Right |
|---|---|---|---|---|---|
| 0 | POWER | RowLOR | Night | Power | Mute |
| 1 | VOL | RowLOR | Vol Down | *(label, not clickable)* | Vol Up |
| 2 | PLAYBACK | RowLOR | Prev | Play | Next |
| 3 | VOICE | RowLR | Voice Down | — | Voice Up |
| 4 | BASS | RowLR | Bass Down | — | Bass Up |
| 5 | SOURCE | RowSubmenu | — | → submenu (AUX / Optical / HDMI ARC) | — |
| 6 | EQ PRESET | RowSubmenu | — | → submenu (Cinema / Sports / Music) | — |

**VOL centre special case:** `cmd_ok=NULL` and type is not `RowSubmenu`, so OK press on VOL centre is suppressed in the input callback — no highlight, no IR send.

## Press feedback

Three equal-width cells per row: x=0–20 (left), x=21–42 (centre), x=43–63 (right).  
On `InputTypePress`, the pressed cell is inverted (black fill + white icon/text).  
On `InputTypeRelease`, inversion clears. State lives in `MainViewModel.is_pressed / pressed_row / pressed_cell`.

Selection highlight: hollow `canvas_draw_frame` around the full row (all 4 sides).

## Submenu view

Header (16 px) + up to N items (16 px each). Title is centred with a bottom separator line.  
Title wraps to two lines **only if** `canvas_string_width(canvas, title) > SCREEN_W` — prevents "EQ PRESET" (fits in ~54 px) from splitting unnecessarily.  
Selected item: hollow border. Pressed: full inversion (same pattern as main view).

## IR protocol

All commands use `InfraredProtocolNECext`, address `0x000091C8`.  
IR bytes in the `.ir` capture file are little-endian (e.g. "C8 91 00 00" → `0x000091C8`).  
`polk_send()` calls `infrared_send(msg, 2)` (sends twice for reliability).

## Key non-obvious decisions

- **Custom `View` instead of built-in `Submenu` widget:** needed for full control over header rendering and press-inversion feedback. The built-in widget doesn't support per-item colour inversion on press.
- **`scroll_offset` zero-initialised safely:** `view_allocate_model` zeroes the model, so `scroll_offset=0` on first paint is correct without explicit init.
- **`fap_icon_assets` and `fap_icon` are independent:** `fap_icon_assets="images"` compiles all PNGs in `images/` into the icon header (for use inside the app). `fap_icon="images/remote.png"` is the 10×10 (or 24×24, SDK resizes) icon shown in the Flipper device menu.
