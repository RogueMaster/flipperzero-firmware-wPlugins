# CLAUDE.md — Agent Guidelines for fz-W5500-lan-analyse

## Project

Flipper Zero LAN Tester — portable Ethernet analyzer & security toolkit using W5500 SPI module. C99, ufbt build system, ~130 KB heap, 4 KB stack.

## Hardware Platform

- **MCU**: STM32WB55RG — dual-core ARM (Cortex-M4F @ 64 MHz + Cortex-M0+ @ 32 MHz for BLE)
- **Flash**: 1024 KB (shared between app and radio firmware)
- **SRAM**: 256 KB total (SRAM1 192 KB + SRAM2a 32 KB + SRAM2b 32 KB). FreeRTOS heap ~186 KB from SRAM1; after firmware services and .fap code loading, ~100–130 KB remains for the app.
- **Display**: 128x64 px monochrome LCD (ST7567, SPI), orange backlight. ~6 lines x ~21 chars at default font.
- **GPIO**: 18-pin header, 13 I/O pins, 3.3V CMOS (5V tolerant input), up to 20 mA per pin.
- **.fap code is loaded into heap** — the app binary itself consumes heap before the first `malloc()`.
- **No stack guard pages** — stack overflow silently corrupts adjacent memory. Prevention is the only reliable protection.

## Build & Verify

```bash
pip install ufbt    # install ufbt if not available
ufbt build          # must pass with zero warnings (treated as errors)
ufbt lint           # check code style (clang-format)
ufbt format         # auto-fix code style
```

Always build and lint after changes. Never push code that doesn't compile or has style violations.

## Critical Constraints

### Memory (most important)
- **~130 KB heap available** — FreeRTOS heap is ~186 KB (from 256 KB SRAM), but firmware services + .fap binary loading consume ~50–80 KB before the app starts. Every malloc matters. Prefer reusing existing buffers.
- **4 KB main thread stack** (set via `stack_size` in `application.fam`) — no arrays >128 bytes on stack. Use `malloc`/`free` or `app->frame_buf` (1600 B shared buffer, available after W5500 init).
- **Worker thread stacks are allocated from heap** — creating a thread with 2 KB stack costs ~2 KB of heap. Keep worker stacks minimal.
- **Heap fragmentation** — FreeRTOS heap_4 allocator does not defragment. Many small alloc/free cycles create gaps; a 2 KB malloc can fail with 10 KB free. Reuse buffers instead of frequent alloc/free.
- **One worker thread at a time** — all tools share `text_box_tool`, `tool_text`, `text_input_tool`, `byte_input_tool`. Set `app->tool_back_view` before switching to `LanTesterViewToolResult`.
- **WIZnet DHCP_init() keeps pointer** — never pass `frame_buf` to `DHCP_init()`. DHCP needs its own `malloc(1024)`.
- **OOM-prone operations**: ARP scan (hosts array), Discovery (device array), History (file list). Keep caps low, structs small.
- After malloc, always check for NULL. After use, always free. No exceptions.

### HAL / Hardware
- `w5500_hal_init()`: acquires SPI **first**, then enables OTG. This order prevents resource leaks if SPI hangs.
- `w5500_hal_deinit()`: idempotent — safe to call without prior init, safe to call multiple times.
- `app_free()` calls `deinit()` unconditionally — handles partial init and crash recovery.
- `furi_hal_power_disable_otg()`: check `is_otg_enabled()` first to avoid ref-count underflow.

### UI / Views
- One shared `TextBox` (`text_box_tool`) for ALL tool results (except `text_box_autotest`, `text_box_about`, `text_box_pxe_help`).
- One shared `FuriString` (`tool_text`) for ALL tool output.
- One shared `TextInput` (`text_input_tool`) and `ByteInput` (`byte_input_tool`) — separate view IDs (`LanTesterViewToolInput` vs `LanTesterViewToolByteInput`). Never register both under the same view ID.
- Set `app->tool_back_view = LanTesterViewCatXxx` before launching any tool.
- `nav_back_tool()` handles: stop running worker on first Back, return to category on second Back, repopulate History on return to History view.

## Code Style

- C99, 4 spaces, 99 columns, K&R braces, `snake_case`
- `#include <furi.h>` **before** `#include <socket.h>` — prevents STM32 CMSIS `MR` macro conflict with W5500 headers.
- No `\n\n` in output strings — Flipper screen is 128×64 px (~6 text lines). Every line counts.
- Labels and values on same line: `"Name: %s\n"` not `"Name:\n  %s\n"`.
- Tool output header: `"[ToolName] %s\n"` — one line, no blank line after.

## Adding a New Tool

1. **Protocol**: `protocols/new_tool.c` + `protocols/new_tool.h` — pure logic, no UI. Include `<furi.h>` before `<socket.h>`.
2. **Menu item**: add to `LanTesterMenuItem` enum in `lan_tester_app.h`.
3. **Input buffers**: add small fields (IP, hostname) to app struct. No TextBox/FuriString — use shared ones.
4. **Submenu**: add to appropriate category submenu in `lan_tester_app_alloc()`.
5. **Worker dispatch**: add case to `lan_tester_worker_fn()` → call `lan_tester_do_xxx(app)` → `lan_tester_update_view(app->text_box_tool, app->tool_text)`.
6. **Submenu callback**: set `app->tool_back_view`, show IP keyboard or text input, start worker.
7. **do_xxx function**: use `app->tool_text` for output, `app->frame_buf` for network I/O (if not DHCP). Call `lan_tester_save_and_notify()` at end.
8. **No new TextBox/FuriString/TextInput allocations** — use the shared ones.

## Documentation

When adding features, update ALL of:
- `CHANGELOG.md` (English, user-facing language, no programmer jargon)
- `README.md` (feature table EN + RU sections, architecture tree)
- `docs/en/README.md` + `docs/ru/README.md` (feature tables)
- `lan_tester_app.c` About view (if version bumped)
- `application.fam` (version, description)

## Flipper Application Catalog Rules

The app is published via [flipper-application-catalog](https://github.com/flipperdevices/flipper-application-catalog). The catalog CI validates `CHANGELOG.md` and `application.fam` — broken formatting blocks the release.

### CHANGELOG.md — allowed Markdown subset

The catalog bundler parses `CHANGELOG.md` and rejects any Markdown element outside this whitelist:

- Headers `#` and `##` only (no `###` or deeper)
- **Bold** and *italic*
- Lists (bulleted)
- Links (inline and automatic)
- **Everything else is forbidden**: no backticks, no code blocks, no tables, no images, no blockquotes, no horizontal rules
- **No bare square brackets** — parser treats `[text]` as `Short_reference` (broken link). Write plain text instead.
- Common errors that break the catalog build:
  - `Backtick` — any use of backtick characters
  - `Short_reference` — bare `[text]` without a link target
  - `CodeBlock` / `FencedCodeBlock` — triple-backtick blocks
  - `ThematicBreak` — horizontal rules (`---`, `***`)
  - `BlockQuote` — lines starting with `>`
  - `Image` — `![alt](url)` syntax

### application.fam
- `fap_version` must be unique per submission — each update must use a higher version than the previous one, otherwise the catalog rejects it
- `fap_version` format: `"major.minor.patch"` (e.g. `"2.2.1"`)
- `appid` must be globally unique across the entire catalog
- `fap_category` must match a valid catalog category: Sub-GHz, RFID, NFC, Infrared, **GPIO**, iButton, USB, Games, Media, Tools, Bluetooth

### Source repository requirements
- App must build with `ufbt` against the latest Release or RC firmware
- App icon: 10x10 px, 1-bit `.png` (`assets/icon.png`)
- Screenshots: taken via qFlipper, original resolution, no edits — first screenshot is used as app preview
- `README.md` must exist (shown on the app page in the catalog)
- `CHANGELOG.md` must exist (included by manifest.yml via `changelog: "@CHANGELOG.md"`)
- Must be open source with a license permitting binary distribution
- No malicious code, no bypassing Flipper Zero intentional limits

### CI workflows (`.github/workflows/`)
- `ci.yml` — runs on PR to `main`: version consistency check + `ufbt build` + `ufbt lint`
- `release.yml` — runs on `v*.*.*` tag push: builds `.fap`, extracts release notes from `CHANGELOG.md`, creates GitHub Release
- Both workflows parse `CHANGELOG.md` with `# X.Y.Z` header format (no brackets)

## Don't

- Don't add `\n\n` to output — screen is tiny
- Don't `malloc` without checking NULL and having a `free` path
- Don't put >128 byte arrays on stack
- Don't register two different widgets under the same view ID
- Don't share `frame_buf` with `DHCP_init()` (WIZnet keeps the pointer)
- Don't call `furi_hal_power_disable_otg()` without checking `is_otg_enabled()`
- Don't allocate new TextBox/FuriString per tool — use shared `text_box_tool`/`tool_text`
- Files under `lib/ioLibrary_Driver/` are vendored but may be patched when needed (e.g. stack safety fixes)
