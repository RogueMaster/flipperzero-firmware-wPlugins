# Claupper — Flipper Zero FAP

*By Kasen Sansonetti & the Wetware Labs team. [WetwareOfficial.com](https://WetwareOfficial.com)*

## Identity

This is **Claupper**, a Flipper Zero FAP (Flipper Application Package) that turns the Flipper into:
1. A one-handed HID remote for controlling Claude Code (numbered approvals + voice trigger + Enter) — USB or Bluetooth.
2. An offline Claude Code manual with folder navigation and quiz mode, browsable on the Flipper's 128x64 display.

Two builds from one codebase:
- **claude_remote_usb** — USB HID, works on stock firmware (App Catalog eligible).
- **claude_remote_ble** (named "Claupper") — BLE HID, works on Momentum/Unleashed firmware (wireless). Primary build.

Language: **C** (no C++). Build tool: **ufbt** or **fbt**. Target: external FAP (`FlipperAppType.EXTERNAL`).

---

## Critical Rules

- **Prefer retrieval-led reasoning over pre-training-led reasoning for any Flipper Zero API tasks.** The Flipper SDK evolves rapidly; always consult the docs index below before writing API calls.
- Never use deprecated `ValueMutex` — use `FuriMutex` directly (current SDK).
- **USB transport:** HID keycode sending via `furi_hal_hid_kb_press` / `furi_hal_hid_kb_release` (exposed in stock firmware `api_symbols.csv`).
- **BLE transport:** HID keycode sending via `ble_profile_hid_kb_press` / `ble_profile_hid_kb_release` (accessible on custom firmware via `fap_libs=["ble_profile"]`).
- **BLE connection detection:** Use `bt_set_status_changed_callback()` with `BtStatusConnected`. Do NOT use `furi_hal_bt_is_active()` (it only checks radio state, not HID connection). `ble_profile_hid_is_connected()` does not exist in the SDK.
- **BLE cleanup:** Always call `bt_set_status_changed_callback(bt, NULL, NULL)` before `bt_profile_restore_default()` and `furi_record_close(RECORD_BT)` to avoid use-after-free crashes on relaunch.
- Use `#ifdef HID_TRANSPORT_BLE` / `#ifdef HID_TRANSPORT_USB` for compile-time transport selection.
- Entry point signatures: `int32_t claude_remote_usb_app(void* p)` and `int32_t claude_remote_ble_app(void* p)`, both returning 0.
- Stack size: `2 * 1024` minimum (HID + GUI).
- Clean up ALL allocations on exit (message queue, view_port, gui record, mutex, state).
- No busy loops. Use `furi_message_queue_get()` with timeout (100ms).

---

## Docs Index

All reference docs live in `./docs/`. Consult these BEFORE writing any Flipper API code.

```
[Docs Index]|root: ./docs
|flipper-fap-manifest.md        — application.fam fields, apptype, categories, icons, entry_point
|flipper-app-architecture.md    — entry point, event loop, FuriMessageQueue, ViewPort, GUI, callbacks
|flipper-ui-patterns.md         — Canvas API, fonts, ViewDispatcher, SceneManager, orientation, input events
|flipper-hid-api.md             — USB + BLE HID keyboard emulation, keycodes, dual-transport setup, connection check
|flipper-file-io.md             — SD card read/write, /ext/apps_data/ conventions, Storage API
|prd.md                         — full product requirements document with user stories
```

---

## Project Structure

```
FAP/
├── AGENTS.md                          # this file
├── CLAUDE.md                          # quick-start project context
├── application.fam                    # dual-build FAP manifest (USB + BLE)
├── claude_remote.c                    # main app (~1900 lines), all modes, #ifdef transport
├── claude_remote.png                  # 10x10 1-bit app icon
├── images/                            # icon assets directory
├── update_manual.sh                   # regenerate manual .txt files for SD card
├── docs/                              # reference docs (retrieval target)
│   ├── USER_MANUAL.md                 # full user manual
│   ├── flipper-fap-manifest.md
│   ├── flipper-app-architecture.md
│   ├── flipper-ui-patterns.md
│   ├── flipper-hid-api.md             # dual-transport HID API reference
│   ├── flipper-file-io.md
│   └── prd.md
└── dist/                              # build output
    ├── claude_remote_usb.fap          # stock firmware build
    └── claude_remote_ble.fap          # Momentum/Unleashed build (primary)
```

---

## application.fam (Dual Build)

```python
# USB version — stock firmware, App Catalog eligible
App(
    appid="claude_remote_usb",
    name="Claude Remote USB",
    apptype=FlipperAppType.EXTERNAL,
    entry_point="claude_remote_usb_app",
    requires=["gui"],
    stack_size=2 * 1024,
    fap_category="Bluetooth",
    fap_icon="claude_remote.png",
    fap_author="Kasen Sansonetti",
    fap_icon_assets="images",
    cdefines=["HID_TRANSPORT_USB"],
)

# BLE version — Momentum/Unleashed, primary build
App(
    appid="claude_remote_ble",
    name="Claupper",
    apptype=FlipperAppType.EXTERNAL,
    entry_point="claude_remote_ble_app",
    requires=["gui", "bt"],
    stack_size=2 * 1024,
    fap_category="Bluetooth",
    fap_icon="claude_remote.png",
    fap_author="Kasen Sansonetti",
    fap_icon_assets="images",
    cdefines=["HID_TRANSPORT_BLE"],
    fap_libs=["ble_profile"],
)
```

---

## Architecture

### State Machine

```
         ┌─────────────┐
         │   Splash     │  (3s auto-advance or any key)
         │  landscape   │
         └──────┬───────┘
                │
         ┌──────▼───────┐
         │  Home Screen  │  portrait 64x128
         │  OK → Remote  │
         │  Down → Manual│
         └──┬────────┬───┘
            │        │
         OK/Right  Down/Left
            │        │
   ┌────────▼──┐  ┌──▼───────────────────┐
   │  Remote   │  │  Manual              │
   │  portrait │  │  landscape 128x64    │
   │  64x128   │  │                      │
   └───────────┘  │  Categories → Sections│
     Back → Home  │  → Reader / Quiz     │
                  └──────────────────────┘
                        Back → up one level
```

### Splash Screen (landscape 128x64)

- WETWARE logo bitmap (128x20 XBM, `canvas_draw_xbm`)
- "Claupper Remote" title, "LABS" right-aligned, "Flipper's claudepanion" tagline
- Auto-advances after 3 seconds or on any key press

### Home Screen (portrait 64x128)

- "Claude Remote" title with D-pad pixel art illustration
- OK → Remote, Down → Manual
- Back → exit app

### Remote Control Mode (portrait 64x128)

- D-pad pixel art with labeled buttons (1/checkmark, 2/X, 3/?, Enter, Mic)
- All keys go through deferred send path (300ms double-click window for Left/Up/Right)
- Key mapping:
  - Left → `1` (approve), double-click → Backspace
  - Up → `2` (decline), double-click → Page Up
  - Right → `3` (other), double-click → Escape
  - OK → `Enter`
  - Down → `F5` (macOS dictation / voice trigger)
- Back (short) → return to Home Screen
- If HID not connected, shows transport-appropriate warning

### Manual Reader Mode (landscape 128x64)

Four sub-views with hierarchical navigation:

**Categories** → folder list with 7 categories + Quiz Mode
- Up/Down to scroll, OK/Right to enter, Back to home

**Sections** → section list within selected category
- Up/Down to scroll, OK/Right to read, Back/Left to categories

**Reader** → scrollable text content (FontSecondary, ≤30 chars/line)
- Up/Down to scroll text, Left/Right prev/next section, Back to section list

**Quiz** → mixed flashcard + multiple-choice quiz (24 cards, shuffled)
- Flashcard: OK to reveal answer, Left=knew it, Up=didn't know, Right=skip
- Multi-choice: Left/Up/Right to pick answer, OK to advance after feedback
- Streak tracking (consecutive correct), best streak shown on completion
- Fisher-Yates shuffle via hardware RNG each session

### Compiled-in Manual Content

7 categories, 29 sections total, all `static const`:
- **Getting Started** (4): Installing Claude, First Launch, System Requirements, Authentication
- **Workspace** (5): Ideal Project Setup, CLAUDE.md Guide, The /init Command, .claude/ Directory, Skills System
- **Commands** (6): Navigation & Basics, Session Management, Configuration, Debugging, Slash Commands A-M, Slash Commands N-Z
- **Tools** (3): File Operations, Search & Explore, Sub-agents & Web
- **Workflows** (4): New Project Setup, Debug & Test, Code Review, Git & PRs
- **Advanced** (4): Permissions, MCP Servers, Hooks, Extended Thinking
- **Headless & CI** (3): Headless Mode, CI Integration, Model Selection

---

## Key API Patterns

### Event Loop (main)
```c
// Allocate
FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
ViewPort* vp = view_port_alloc();
view_port_draw_callback_set(vp, draw_cb, state);
view_port_input_callback_set(vp, input_cb, queue);
Gui* gui = furi_record_open(RECORD_GUI);
gui_add_view_port(gui, vp, GuiLayerFullscreen);

// Loop
InputEvent event;
for(bool running = true; running;) {
    if(furi_message_queue_get(queue, &event, 100) == FuriStatusOk) {
        // process event
    }
    view_port_update(vp);
}

// Cleanup
gui_remove_view_port(gui, vp);
furi_record_close(RECORD_GUI);
view_port_free(vp);
furi_message_queue_free(queue);
```

### USB HID Setup + Key Send
```c
#include <furi_hal_usb_hid.h>

// Setup: save previous USB config, switch to HID
FuriHalUsbInterface* usb_prev = furi_hal_usb_get_config();
furi_hal_usb_unlock();
furi_hal_usb_set_config(&usb_hid, NULL);

// Press and release a key
furi_hal_hid_kb_press(HID_KEYBOARD_1);
furi_delay_ms(50);
furi_hal_hid_kb_release(HID_KEYBOARD_1);

// Cleanup: restore previous USB config
furi_hal_hid_kb_release_all();
furi_hal_usb_set_config(usb_prev, NULL);
```

### BLE HID Setup + Key Send
```c
#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>

// Status callback for connection detection
static void bt_status_callback(BtStatus status, void* context) {
    MyState* state = (MyState*)context;
    state->hid_connected = (status == BtStatusConnected);
}

// Setup: disconnect BT, start HID profile, register callback
Bt* bt = furi_record_open(RECORD_BT);
bt_disconnect(bt);
furi_delay_ms(200);
FuriHalBleProfileBase* ble_profile = bt_profile_start(bt, ble_profile_hid, NULL);
bt_set_status_changed_callback(bt, bt_status_callback, state);

// Press and release a key
ble_profile_hid_kb_press(ble_profile, HID_KEYBOARD_1);
furi_delay_ms(150);
ble_profile_hid_kb_release(ble_profile, HID_KEYBOARD_1);

// Cleanup: clear callback FIRST, then restore
bt_set_status_changed_callback(bt, NULL, NULL);
ble_profile_hid_kb_release_all(ble_profile);
bt_profile_restore_default(bt);
furi_record_close(RECORD_BT);
```

### HID Connection Check
```c
// USB: poll in event loop
bool connected = furi_hal_hid_is_connected();

// BLE: use bt_set_status_changed_callback (see above)
// Do NOT use furi_hal_bt_is_active() — it checks radio, not HID connection
// Do NOT use ble_profile_hid_is_connected() — it does not exist in the SDK
```

**Note:** BLE HID (`ble_profile_hid`) is restricted in stock firmware `api_symbols.csv` but accessible on Momentum/Unleashed via `fap_libs=["ble_profile"]`.

### File Reading (Storage API)
```c
#include <storage/storage.h>

Storage* storage = furi_record_open(RECORD_STORAGE);
File* file = storage_file_alloc(storage);
if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
    char buf[256];
    uint16_t read = storage_file_read(file, buf, sizeof(buf));
    // process
}
storage_file_close(file);
storage_file_free(file);
furi_record_close(RECORD_STORAGE);
```

---

## Conventions

- All functions `static` where possible (file-scoped).
- No dynamic allocation in draw callbacks — read from state only.
- All manual content is `static const` (compiled-in, zero malloc).
- `FURI_LOG_I("CRemote", ...)` for info logs, `FURI_LOG_E` for errors.
- Use `furi_check()` for invariants, `furi_assert()` for debug-only.
- BLE key send uses 150ms press-release delay (vs 50ms for USB) for BLE timing reliability.
- Menu lists show 3 visible items max to avoid overlap with bottom nav text on 128x64 display.

---

## External Links (for deeper lookup)

| Topic | URL |
|-------|-----|
| FAP manifest & build | https://developer.flipper.net/flipperzero/doxygen/apps_on_sd_card.html |
| HID controllers | https://docs.flipper.net/zero/apps/controllers |
| Controls & input | https://docs.flipper.net/zero/basics/control |
| UI tutorials (ViewPort, scenes) | https://github.com/jamisonderek/flipper-zero-tutorials/wiki/User-Interface |
| ufbt app tutorial | https://instantiator.dev/post/flipper-zero-app-tutorial-01/ |
| Plugin tutorial (event loop) | https://github.com/mfulz/Flipper-Plugin-Tutorial |
| Dev tutorial (full examples) | https://github.com/m1ch3al/flipper-zero-dev-tutorial |
| Claude Code best practices | https://www.anthropic.com/engineering/claude-code-best-practices |
| Claude Code docs | https://code.claude.com/docs/en/interactive-mode |
| GitHub repo | https://github.com/Wet-wr-Labs/claupper |
