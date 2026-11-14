# Flipper SpoolBuddy: Phase 0 spike and Phase 1 (Flipper + macOS over USB)

## Overview

Build the first working slice of the project described in
`docs/superpowers/specs/2026-10-25-flipper-spoolbuddy-design.md`:

- A Flipper Zero external app (FAP, stock SDK, firmware release 1.5.1) that
  reads a filament spool's NFC tag (Bambu MIFARE Classic with HKDF-derived
  keys, or NTAG), streams the raw dump to a host as one JSON line over USB
  CDC, shows the host's reply, and lets the user send "weigh" and "new"
  intents from the Flipper's own screen.
- A macOS host (SwiftPM package, SwiftUI executable, no Xcode project) that
  owns tag parsing and every Bambuddy call: lookup by tray UUID / UID,
  weight update (net or gross), create new full item, link to an existing
  spool. It shows a session feed where any weight can be entered or
  corrected.
- A Python dev bridge (`tools/bridge`) used for the Phase 0 spike and kept as
  a debugging tool.
- A shared protocol contract (`protocol/`) with a JSON schema and fixtures
  that both the C and Swift test suites validate against.

Problem solved: keep Bambuddy's filament inventory accurate using hardware
the owner already has (Flipper Zero, Mac), without SpoolBuddy hardware.

Out of scope for this plan: BLE transport, iPad/iPhone host (Phase 2),
offline queue on the Flipper, tag writing, Spoolman mode.

## Context (from discovery)

- New repository `~/src/flipper-spoolbuddy`; only the design doc exists.
- Toolchains on this Mac: Swift 6.4 via Command Line Tools (**no Xcode**, so
  no `xcodebuild`; `swift build` / `swift test` only), Python 3.13, `uv`,
  `ralphex`. `ufbt` must be installed (`python3 -m pip install --upgrade
  ufbt`, or via `uv tool install ufbt`).
- Flipper firmware latest release: **1.5.1** (2026-10-15). Verified exported
  in its `targets/f7/api_symbols.csv`: `nfc_alloc`, `nfc_scanner_*`,
  `iso14443_3a_poller_sync_read`, `mf_classic_poller_sync_detect_type`,
  `mf_classic_poller_sync_read` (whole card with `MfClassicDeviceKeys`),
  `mf_ultralight_poller_sync_read_card`, `furi_hal_usb_set_config` +
  `usb_cdc_single`, `furi_hal_cdc_send/receive/set_callbacks`,
  `number_input_*`, `submenu`, `dialog_ex`, `widget`. **Not exported:**
  `mbedtls_sha256_*`, `mbedtls_md_hmac_*` (ship our own SHA-256/HMAC/HKDF).
- Bambuddy deployed version 1.2.5.2 (verified against that tag):
  - header `X-API-Key: bb_…` (also accepts `Authorization: Bearer bb_…`)
  - `GET /api/v1/inventory/spools/by-tag?tray_uuid=&tag_uid=` → 404 when none
  - `POST /api/v1/spoolbuddy/scale/update-spool-weight` `{spool_id, weight_grams}` (gross)
  - `POST /api/v1/inventory/spools` (`material` required, min length 1)
  - `PATCH /api/v1/inventory/spools/{id}/link-tag` `{tag_uid, tray_uuid, tag_type, data_origin}` → 409 `tag_already_linked`
  - `GET /api/v1/inventory/spools`, `GET /api/v1/inventory/spools/{id}`
  - `GET /api/v1/inventory/catalog` → `[{id, name, weight}]`
  - `GET /api/v1/settings/spoolman` → `{spoolman_enabled: "true"|"false", …}` (needs settings read permission)
- Bambu key derivation (from SpoolBuddy daemon `pn5180.py`): HKDF-SHA256,
  salt = 16-byte master key `9A759CF2C4F7CAFF222CB9769B41BC96`, IKM = 4-byte
  UID, info = `"RFID-A\0"` (7 bytes), 96 bytes out = 16 × 6-byte Key A.
- Tray UUID location is unsettled: SpoolBuddy daemon uses blocks 4+5 as
  ASCII hex; the public tag guide says block 9 (16 raw bytes). The parser
  emits **candidates in order [block 9 hex, blocks 4+5 ASCII-hex]** and the
  lookup tries them in order; Phase 0 manual check settles it (Post-Completion).
- ⚠️ Xcode.app is now installed but its license is not accepted, so `git`,
  `swift`, `cc` from `/usr/bin` exit 69. Workaround used by all tasks:
  `DEVELOPER_DIR=/Library/Developer/CommandLineTools` (recorded in `AGENTS.md`).

## Development Approach

- **Testing approach**: TDD (write the failing test first, then the code).
- Complete each task fully before moving to the next.
- Make small, focused changes.
- **CRITICAL: every task MUST include new/updated tests** for code changes in that task
  - tests are not optional - they are a required part of the checklist
  - write unit tests for new functions/methods
  - write unit tests for modified functions/methods
  - add new test cases for new code paths
  - update existing test cases if behavior changes
  - tests cover both success and error scenarios
- **CRITICAL: all tests must pass before starting next task** - no exceptions
- **CRITICAL: update this plan file when scope changes during implementation**
- Run tests after each change.
- Repository content in English (code, comments, docs, commits); talk to the
  owner in Russian. Put this in `AGENTS.md`.
- No Flipper firmware forks; everything builds with stock `ufbt` pinned to
  release 1.5.1 (`ufbt update --branch=1.5.1`, dotenv mode so the SDK lives
  in `flipper/.ufbt`).
- Flipper hardware code (NFC, USB) is isolated behind small interfaces so
  that everything else in `flipper/src` compiles and is tested on the host
  with plain `cc` (`flipper/tests/Makefile`).

## Testing Strategy

- **Unit tests**: required for every task.
  - `flipper/tests`: host-compiled C tests (`make -C flipper/tests test`), a
    tiny assert-based runner, no Flipper needed.
  - `host/`: `swift test` (swift-testing; the Command Line Tools ship no XCTest) in the `SpoolBuddyCore` package.
  - `tools/`: `uv run pytest` for the Python bridge and the fixture/schema
    checks.
- **Fixtures**: `protocol/fixtures/` holds sample messages and tag dumps.
  Until Phase 0 captures real dumps, the Bambu fixture is synthetic, built
  from the documented block layout by `tools/fixtures/make_bambu_dump.py`,
  and marked `synthetic: true`.
- **No e2e UI tests** (no Xcode, no UI test runner); view models are tested
  directly.
- Hardware-in-the-loop checks are manual and live in Post-Completion.

## Progress Tracking

- Mark completed items with `[x]` immediately when done
- Add newly discovered tasks with ➕ prefix
- Document issues/blockers with ⚠️ prefix
- Update plan if implementation deviates from original scope
- Keep plan in sync with actual work done

## What Goes Where

- **Implementation Steps** (`[ ]` checkboxes): tasks achievable within this codebase - code changes, tests, documentation updates
- **Post-Completion** (no checkboxes): items requiring external action - manual testing, changes in consuming projects, deployment configs, third-party verifications
- **Checkbox placement**: Checkboxes belong only in Task sections. Do not put checkboxes in Success criteria, Overview, or Context — they cause extra loop iterations.

## Implementation Steps

### Task 1: Repository scaffolding and conventions
- [x] create `AGENTS.md` (English content, Russian communication, stock ufbt only, TDD, layout summary)
- [x] create `README.md` with the one-paragraph purpose, the directory layout from the design, and the three test commands
- [x] create `.gitignore` (`flipper/.ufbt/`, `flipper/dist/`, `host/.build/`, `.venv/`, `__pycache__/`, `.DS_Store`) and `.editorconfig`
- [x] create empty directories with placeholder README lines: `protocol/`, `flipper/`, `host/`, `tools/`, `.github/workflows/`
- [x] create `tools/pyproject.toml` (uv project, Python 3.13, deps: `pyserial`, `httpx`, `jsonschema`; dev: `pytest`) and `tools/tests/test_smoke.py`
- [x] run `cd tools && uv run pytest` - must pass before next task
- [x] make the initial commit

### Task 2: Protocol contract: PROTOCOL.md, JSON schema, fixtures
- [x] write `protocol/PROTOCOL.md`: NDJSON framing, `v: 1`, `hello` handshake, messages `scan`, `lookup`, `intent`, `result` with every field and its type, max line length 8192 bytes, examples
- [x] write `protocol/schema/message.schema.json` (one schema with a `t` discriminator covering all five messages; `intent.action` enum `weigh|new|create_on_host|link_on_host`; `mode` enum `net|gross`; `tag.type` enum `mfc|ntag`)
- [x] write `tools/fixtures/make_bambu_dump.py` that builds a synthetic MIFARE Classic 1K dump from the documented layout (block 1 variant/material ids, block 2 filament type, block 4 detailed type, block 5 RGBA + label weight uint16 LE + diameter float32, block 6 temps, block 9 tray UID 16 bytes, trailers with derived Key A) and writes `protocol/fixtures/tags/bambu_pla_basic_synthetic.json`
- [x] write `protocol/fixtures/tags/ntag215_blank.json` (UID + pages)
- [x] write `protocol/fixtures/messages/*.json`: one valid example per message type plus `invalid_missing_t.json`, `invalid_bad_action.json`
- [x] write `tools/tests/test_fixtures.py` (TDD: fails until fixtures exist): every `messages/*.json` validates or fails against the schema as its name says; every `tags/*.json` has `uid`, `type`, and either 64 `blocks` or `pages`
- [x] run `cd tools && uv run pytest` - must pass before next task
- [x] ➕ `tools/fixtures/make_ntag_dump.py` and `tools/fixtures/make_message_fixtures.py` generate the NTAG dump and the `scan_*`/`intent_*` messages (they embed whole dumps); `test_fixtures.py` fails when a committed fixture is stale
- [x] ➕ `tools/bambu_keys.py` (`hkdf_sha256`, `derive_keys`) created here because the synthetic dump needs derived Key A in its trailers; RFC 5869 case 1 tested in `test_fixtures.py`. Task 3 adds the remaining vectors, its own test module and the C header generator

### Task 3: Bambu key derivation in C (SHA-256, HMAC, HKDF)
- [x] write `tools/bambu_keys.py` (port of the daemon's `hkdf_derive_keys`) and `tools/tests/test_bambu_keys.py` with vectors: SHA-256 and HMAC RFC 4231 cases, HKDF RFC 5869 test case 1, and two UIDs → 16 keys each; emit `flipper/tests/vectors/bambu_keys.h` from a script `tools/fixtures/make_key_vectors.py`
- [x] write `flipper/tests/test_bambu_keys.c` first (TDD): SHA-256 known answers, HMAC RFC 4231, HKDF RFC 5869 case 1, UID → keys equal to the generated vectors
- [x] implement `flipper/src/crypto/sha256.{c,h}`, `hmac_sha256.{c,h}` (small, public-domain-style, no dynamic allocation)
- [x] implement `flipper/src/bambu_keys.{c,h}`: `bambu_derive_keys(const uint8_t uid[4], uint8_t out[96])` and `bambu_sector_key(const uint8_t keys[96], uint8_t sector, uint8_t out[6])`
- [x] write `flipper/tests/Makefile` (`cc -std=c11 -Wall -Wextra -Werror`, builds all `test_*.c` with `../src/**` that have no Flipper dependency, target `test`) and `flipper/tests/minitest.h` (assert macros, counts)
- [x] run `make -C flipper/tests test` - must pass before next task
- [x] ➕ HKDF lives in its own `flipper/src/crypto/hkdf_sha256.{c,h}`; crypto symbols carry an `sb_` prefix (`sb_sha256`, `sb_hmac_sha256`, `sb_hkdf_sha256`) to stay clear of any SDK names. Vectors also cover RFC 4231 cases 1-7, RFC 5869 cases 2-3, SHA-256 padding boundaries and one million `a`. The Makefile picks up every `../src/**/*.c` that includes no Flipper SDK header and builds with ASan/UBSan (`SANITIZE=` to disable); objects go to `flipper/tests/build/` (gitignored)

### Task 4: Tag dump model and `scan` JSON writer (C, host-testable)
- [x] write `flipper/tests/test_tag_dump.c` first: a `TagDump` with type `mfc`, UID, SAK, 64 blocks with sector 7 missing serializes to exactly the fixture `protocol/fixtures/messages/scan_mfc.json` (compare after whitespace-insensitive normalization); an `ntag` dump with 135 pages serializes to `scan_ntag.json`; a dump too large for the 8192-byte line buffer returns an error
- [x] implement `flipper/src/tag_dump.{c,h}`: `TagDump` struct (type, uid[10]+len, sak, atqa, blocks[64][16], block_present bitmap, pages[231][4], page_count)
- [x] implement `flipper/src/json_writer.{c,h}`: append-only writer into a fixed buffer (object/array open-close, string with escaping, int, hex bytes), returns false on overflow
- [x] implement `flipper/src/proto_encode.{c,h}`: `proto_encode_scan(const TagDump*, const char* scan_id, char* buf, size_t cap)` and `proto_encode_hello(...)`
- [x] add the new tests to `flipper/tests/Makefile`
- [x] run `make -C flipper/tests test` - must pass before next task
- [x] ➕ `tools/fixtures/make_tag_vectors.py` emits the tag fixtures as C arrays in `flipper/tests/vectors/tag_dumps.h` (`tools/tests/test_tag_vectors.py` checks it is current); `flipper/tests/fixture.h` reads and compacts protocol fixtures (`PROTOCOL_DIR` passed by the Makefile). `json_writer` also has `json_bool` and its own `test_json_writer.c`; `proto_encode` exposes `proto_write_tag`/`proto_write_header`/`proto_id_valid` for the intent encoder of Task 6. A valid dump can never exceed the line limit (a full NTAG216 is about 4 KB), so the overflow test uses a too-small buffer, and the 8192-byte line cap is tested through an oversized `hello` field; a sector counts as missing unless all three of its data blocks are present

### Task 5: Minimal JSON reader and `lookup`/`result` decoding (C, host-testable)
- [x] write `flipper/tests/test_proto_decode.c` first: decode the fixtures `lookup_found.json`, `lookup_unknown.json`, `lookup_error.json`, `result_ok.json`, `result_error.json` into structs (status, spool_id, up to 4 `lines` of ≤ 40 chars, defaults gross/label/core, error string); unknown `t` → ignored; malformed line → error, no crash; line longer than cap → error
- [x] implement `flipper/src/json_reader.{c,h}`: minimal non-allocating tokenizer good enough for flat objects with string/int/bool/array-of-string values (no nesting beyond `lines` and `defaults`)
- [x] implement `flipper/src/proto_decode.{c,h}`: `proto_decode_line(const char*, ProtoMessage*)` with `ProtoMessage` tagged union for `hello`, `lookup`, `result`
- [x] add to `flipper/tests/Makefile`
- [x] run `make -C flipper/tests test` - must pass before next task
- [x] ➕ `flipper/tests/test_json_reader.c` covers the reader on its own (escapes, surrogate pairs, integer overflow, depth limit, sticky errors, trailing content). The reader turns every control or non-ASCII character into one `?` so a 40-character line is 40 bytes (noted under Wording in `PROTOCOL.md`). `proto_decode_line` returns `ProtoDecodeOk`/`Ignored`/`Malformed`: unknown `t`, `scan`/`intent` and `v != 1` are ignored; schema rules (ids, enums, `spool_id` when found, `error` when status error / `ok:false`, grams 0-9999, exactly one of `fw`/`os`) make a line malformed; unknown fields are skipped; every truncated prefix of a fixture is tested to be malformed under ASan/UBSan. The Makefile picks up new `test_*.c` and sources by itself, so it needed no change

### Task 6: Intent model and app state machine (C, host-testable)
- [x] write `flipper/tests/test_intents.c` first: `intent_new_weigh(scan, grams, mode)` produces an id unique per call (monotonic counter + scan_id), encodes to `intent_weigh.json` fixture shape including the repeated `tag`; `intent_new(scan)`, `create_on_host`, `link_on_host` likewise; a `result` with a matching id resolves the in-flight intent, a mismatched id is ignored
- [x] write `flipper/tests/test_app_state.c` first: the state machine `Idle → Reading → WaitingLookup → Result{Found|UnknownPrefill|UnknownBare|NoHost} → EnterWeight → WaitingResult → Done|Error → Idle` driven by events (`tag_read`, `lookup_received`, `menu_weigh`, `weight_entered`, `result_received`, `host_lost`, `back`); no-host path goes `Reading → Result{NoHost}` when transport is down; net/gross mode persists across scans
- [x] implement `flipper/src/intents.{c,h}` with an `IntentStore` interface (`put`, `resolve`, `current`) and the in-memory single-slot implementation (the future SD-backed queue implements the same interface)
- [x] implement `flipper/src/app_state.{c,h}` as a pure reducer with an event enum and a state struct (no GUI calls)
- [x] add to `flipper/tests/Makefile` (no edit needed: the Makefile picks up every `test_*.c` and every SDK-free source)
- [x] run `make -C flipper/tests test` - must pass before next task
- ➕ ids come from `IdSeq` (`s-`/`i-` + zero-padded counter); `app_state_init` takes the first scan and intent counter values so the app (Task 7) can seed them per run, e.g. from the RTC, and not reuse ids the host still remembers from an earlier run
- ➕ `app_reduce` returns an effect (`SendScan`, `SendIntent`) instead of doing I/O; extra events beyond the listed ones: `host_connected`, `tag_detected` (Idle → Reading), `read_failed`, `menu_new`/`menu_create_on_host`/`menu_link_on_host`, `toggle_mode`, `timeout` (WaitingResult → Error; Task 9 wires the 10 s timer); `host_lost` on Result/EnterWeight/WaitingLookup → Result{NoHost}, mid-intent → Error
- ➕ moved `PROTO_GRAMS_MAX` to `proto_encode.h` and made `proto_line_cap()` public for the intent encoder; test dump loaders shared via `flipper/tests/dump_loaders.h`

### Task 7: FAP skeleton with ufbt, USB CDC transport, idle screen
- [x] create `flipper/application.fam` (appid `spoolbuddy`, category Tools, entry `spoolbuddy_app`, icon placeholder, `fap_version`, stack 4 KB), `flipper/.env` via `ufbt dotenv_create`, `ufbt update --branch=1.5.1`
- [x] implement `flipper/src/transport.h` interface (`open`, `close`, `send_line`, `set_rx_callback`, `is_connected`) and `flipper/src/transport_usb.c` using `furi_hal_usb_set_config(&usb_cdc_single)`, `furi_hal_cdc_set_callbacks`, a 8192-byte RX line accumulator, restore previous USB config on close
- [x] implement `flipper/src/line_buffer.{c,h}` (accumulate bytes, emit complete lines, drop over-long lines) with `flipper/tests/test_line_buffer.c` written first (split packets, two lines in one packet, CRLF, overflow)
- [x] implement `flipper/src/spoolbuddy_app.c`: view dispatcher, scene manager, `Idle` scene (widget: "Hold a spool to the back", status line `USB` / `no host` from transport state + received `hello`)
- [x] send `hello` on connect; treat a host `hello` within 2 s as connected
- [x] `ufbt` build succeeds (`cd flipper && ufbt`), produces `dist/spoolbuddy.fap`
- [x] run `make -C flipper/tests test` and `cd flipper && ufbt` - both must pass before next task
- ➕ `flipper/src/host_link.{c,h}`: the `hello` handshake as a pure state machine (Closed → Waiting → Up, or NoHost after 2 s without an answer; a later host `hello` connects and is answered; port close loses the host), with `flipper/tests/test_host_link.c`; the app polls the transport's DTR state every 250 ms
- ➕ `id_seed_from_time()` in `intents.{c,h}` seeds the scan and intent id counters from the RTC (`(s mod 10^6) * 1000`), tested in `test_intents.c`
- ➕ `AppState.draft` holds the intent being built: the ~2.2 KB `Intent` local in `app_reduce` was too big for the 4 KB app stack
- ➕ scenes live in `flipper/src/scenes.h` (X-macro list) and `scene_<name>.c`; every SDK-dependent `.c` includes an SDK header directly so the host test build skips it (noted in `flipper/README.md`)
- ➕ `transport_usb` disables the CLI VCP (`cli_vcp_disable`) while the app runs and re-enables it on close; packets go out in 64-byte chunks with a zero-length packet after a full last chunk
- ➕ `furi_hal_cdc_send` only queues a packet: `transport_usb` waits for each packet's `tx_ep_callback` and reports an unconfirmed one as maybe sent, so a line whose end was not confirmed is never reported as sent
- ⚠️ on-device behaviour (USB re-enumeration, DTR detection, hello with a real host) is not verified yet; see Post-Completion

### Task 8: NFC reading on the Flipper (Bambu MIFARE Classic and NTAG)
- [x] implement `flipper/src/nfc_read.{c,h}` behind an interface `tag_reader_read(TagDump*)` : `nfc_scanner` detects ISO14443-3A → `iso14443_3a_poller_sync_read` for UID/SAK/ATQA → SAK `0x08`/`0x18`: derive keys, fill `MfClassicDeviceKeys` (`key_a_mask = 0xFFFF`), `mf_classic_poller_sync_read`, copy readable blocks and mark missing sectors; SAK `0x00`: `mf_ultralight_poller_sync_read_card`, copy pages
- [x] `Reading` scene: start the reader in a worker thread, show progress, emit `tag_read` or `read_failed` events; stop the NFC on back
- [x] on `tag_read`: encode `scan`, send it when connected, else go to `Result{NoHost}` showing UID, type, and for Bambu tags the ASCII of blocks 2 and 4 (material / detailed type) and the block-9 hex
- [x] write `flipper/tests/test_nfc_classify.c` first for the pure helper `tag_type_from_sak()` and the "no host" display-string builder
- [x] run `make -C flipper/tests test` and `cd flipper && ufbt` - both must pass before next task
- ➕ the pure helpers live in `flipper/src/nfc_classify.{c,h}`: `tag_type_from_sak()` (0x08/0x18 → mfc, 0x00 → ntag, anything else unsupported) and `tag_summary_text()` (type, UID, block 2/4 ASCII sanitized like the JSON reader, block 9 hex on two lines, sectors read N/16 or page count)
- ➕ `nfc_read.h` is `TagReader` (`start`/`stop`/`is_started`) plus `tag_reader_read(TagReader*, TagDump*)`; one start reads at most one tag, and the app runs it while the screen is Idle or Reading (`sync_reader` in `spoolbuddy_app.c`), so it scans continuously on Idle. A MIFARE Classic with a non-4-byte UID, or one where no sector opens while the tag still answers, is sent with every sector missing so the host can still match the UID; an NTAG with no page read counts as failed (the schema needs one page)
- ➕ every state change goes through `spoolbuddy_reduce()`, which runs the effect, syncs the reader and switches to the scene of the new `AppScreen` (or redraws it via `SpoolBuddyEventRedraw`, which replaces `SpoolBuddyEventStatus`). Scenes added now: `reading` (Reading, WaitingLookup, WaitingResult), `result` (text only: the summary for NoHost, the host's lines otherwise; Back closes) and `done` (Done/Error text with an OK button). Task 9 adds the Result submenu and the EnterWeight/WaitingResult scenes on top
- ⚠️ on-device reading (sync poller behaviour with derived keys, partial reads, NTAG through a refill box) is not verified yet; see Post-Completion

### Task 9: Result, weight entry, intent and done/error screens on the Flipper
- [x] `Result` scene (widget + submenu): Found → lines from host + items **Enter weight**, **Close**; UnknownPrefill → items **New**, **Link on host**, **Close**; UnknownBare → **Create on host**, **Link on host**, **Close**
- [x] `EnterWeight` scene: `number_input` pre-filled from `defaults.gross` (fallback `label + core`), range 0–9999, header shows current mode; a submenu item **Mode: net/gross** toggles and persists for the session
- [x] `WaitingResult` scene with a 10 s timeout → Error
- [x] `Done`/`Error` scene: shows `result.lines` or the error; OK/back → Idle
- [x] wire the reducer from Task 6 to the scenes; all state transitions go through it
- [x] extend `flipper/tests/test_app_state.c` for the timeout and the mode toggle paths
- [x] run `make -C flipper/tests test` and `cd flipper && ufbt` - both must pass before next task
- ➕ the Result scene shows the text (host lines, or the tag summary without a host) with an **Actions** button (**Close** without a host) that opens the submenu; Back from the submenu returns to the text, the scene state remembers which is open and Idle resets it, so Back from EnterWeight lands on the menu again
- ➕ **Mode: net/gross** is an item of the Found menu (`AppEventToggleMode` now also applies on Result{Found}); the EnterWeight pre-fill follows the mode: gross = `defaults.gross` (fallback label + core), net = gross - core (fallback the label weight), capped to 0–9999
- ➕ the 10 s timeout lives in the reducer: every `AppEvent` carries `now_ms` (stamped by `spoolbuddy_reduce`), sending an intent records it, and `app_result_timed_out()` (wrap-safe, `APP_RESULT_TIMEOUT_MS`) tells the 250 ms tick when to send `AppEventTimeout`
- ➕ screen wording is pure and host-tested in `flipper/src/screen_text.{c,h}` (`flipper/tests/test_screen_text.c`): host lines, Done/Error text (host lines + error for a failed lookup or intent, local messages otherwise), the waiting screen's intent summary, the weight header and the mode item
- ➕ new scenes `enter_weight` (number input; a redraw does not reset the digits being typed) and `waiting` (WaitingResult; Back cannot cancel); `reading` now covers Reading and WaitingLookup only
- ⚠️ on-device behaviour of the menus, the number input and the timeout is not verified yet; see Post-Completion

### Task 10: Python dev bridge (Phase 0 spike tool)
- [x] write `tools/tests/test_bambu_tag.py` first: parse the synthetic fixture → material `PLA`, detailed `PLA Basic`, RGBA, label weight 1000, tray UUID candidates in order [block 9, blocks 4+5]; NTAG fixture → UID only
- [x] implement `tools/bridge/bambu_tag.py` (pure parser, mirrors the Swift one later)
- [x] write `tools/tests/test_bambuddy.py` first using `httpx.MockTransport`: `find_by_tag` tries candidates then UID, 404 → `None`; `update_weight` posts gross; `net_to_gross`; `spoolman_enabled` check
- [x] implement `tools/bridge/bambuddy.py` (httpx client with `X-API-Key`)
- [x] implement `tools/bridge/__main__.py`: `--port /dev/cu.usbmodem*` (auto-detect `flip_`), `--bambuddy URL`, `--api-key`, reads NDJSON, answers `hello`, on `scan` prints the parsed tag and the lookup, answers `lookup`; on `intent` performs it and answers `result`; `--dump-dir` saves every scan as a fixture file
- [x] write `tools/tests/test_bridge_protocol.py` first: feeding the fixture `scan_mfc.json` line through the handler yields a `lookup` line that validates against the schema; an `intent` with an already-seen id returns the cached `result`
- [x] run `cd tools && uv run pytest` - must pass before next task
- [x] ➕ protocol handling lives in `tools/bridge/session.py` (`Bridge`, `LineFramer`, `find_flipper_port`, `dump_fixture`); `__main__.py` is only the CLI and serial loop, tested in `tools/tests/test_bridge_main.py` with a fake port (hello first, Spoolman refusal, missing permission, no Flipper)
- [x] ➕ blocks 4+5 candidate follows the daemon exactly (`_extract_tray_uuid` in maziggy/bambuddy `spoolbuddy/daemon/nfc_reader.py`): first 32 ASCII hex chars of blocks 4+5, else block 4 raw hex; all-zero values skipped, duplicates collapsed. For the synthetic fixture (`PLA Basic` in block 4) that is block 4 raw hex
- [x] ➕ `new` refuses when the tag already matches a spool (no duplicates); `tag_type` is `bambulab` / `ntag` (Bambuddy model comment: tag vendor); only successful or definitive results are cached, transient Bambuddy errors are retried on a repeated id

### Task 11: Swift package skeleton and protocol models
- [x] create `host/Package.swift` (swift-tools 6.0, macOS 14+): library `SpoolBuddyCore`, executable `SpoolBuddyMac` (SwiftUI `@main`), test target `SpoolBuddyCoreTests`; no third-party dependencies
- [x] write `SpoolBuddyCoreTests/ProtocolTests.swift` first: every fixture in `protocol/fixtures/messages` round-trips through `Codable` (decode → encode → decode equal); invalid fixtures fail to decode; NDJSON framer splits partial packets and drops over-long lines
- [x] implement `Sources/SpoolBuddyCore/Protocol/Message.swift` (enum with associated values keyed by `t`), `TagDump.swift`, `Intent.swift`, `NDJSONFramer.swift`
- [x] make fixtures reachable from tests as package resources (symlink or copy step documented in `host/README.md`)
- [x] run `cd host && swift test` - must pass before next task
- ⚠️ the Command Line Tools have no XCTest, so the Swift suites use swift-testing (`import Testing`); the CLT macro plugin has a broken rpath, worked around with `host/Toolchain/clt-testing-plugin` symlinks and a `-plugin-path` added by `Package.swift` when `DEVELOPER_DIR` is the CLT (see `host/README.md`)

### Task 12: Swift tag parser
- [x] write `TagParserTests.swift` first: synthetic Bambu fixture → `ParsedTag(kind: .bambu, uid, trayUuidCandidates: [block9, blocks4+5], material: "PLA", detailedType: "PLA Basic", rgba: "RRGGBBAA", labelWeight: 1000, nozzleMin/Max, dryingTemp)`; missing sector 1 → material `nil` and `hasPrefill == false`; NTAG fixture → `.ntag` with UID and `hasPrefill == false`
- ⚠️ block 2 (material) is in sector 0, so a missing sector 1 drops detailed type, RGBA and label weight but not `material`; the Python test uses missing sector 0 for "no material, no prefill" — do the same in Swift
- [x] implement `Sources/SpoolBuddyCore/Tags/TagParser.swift` and `ParsedTag.swift` (pure functions, byte-exact per the layout in the design)
- [x] implement display helpers `ParsedTag.displayLines` (≤ 4 lines, ≤ 40 chars, used for the Flipper `lookup.lines`)
- [x] run `cd host && swift test` - must pass before next task

### Task 13: Swift Bambuddy client
- [x] write `BambuddyClientTests.swift` first with a `URLProtocol` stub: `findSpool(trayCandidates:uid:)` queries candidates in order then UID and maps 404 → `nil`; `updateWeight(spoolId:gross:)`; `createSpool(_:)` sends `material`, `label_weight`, `core_weight`, `core_weight_catalog_id`, `weight_used: 0`, tags, `data_origin: "flipper"`; `linkTag` maps 409 to `.tagAlreadyLinked(otherSpoolId)`; `listSpools`, `catalog`, `spoolmanEnabled` (403 → `.permissionMissing("settings:read")`); all send `X-API-Key`
- [x] implement `Sources/SpoolBuddyCore/Bambuddy/BambuddyClient.swift`, `Models.swift` (Spool, CatalogEntry), `BambuddyError.swift`
- [x] implement `WeightMath.swift`: `grossFromNet(net, core)`, `remaining(label, used)` with tests (negative clamp, zero core)
- [x] run `cd host && swift test` - must pass before next task
- ⚠️ Bambuddy 1.2.5.2 answers the `link-tag` 409 with a plain string detail ("Tag UID already linked to another active spool"); only later releases (#3110) name the holder in `detail.spool_id`. `BambuddyError.tagAlreadyLinked(otherSpoolId:)` is therefore optional and filled when present.
- ➕ `NewSpool(tag:coreWeight:coreWeightCatalogId:)` and `TagLink(tag:)` build the create/link bodies from a `ParsedTag` (same field rules as the Python bridge); `spool(id:)` reads one spool; 401 maps to `.unauthorized`, 404 by id to `.spoolNotFound`

### Task 14: Swift USB serial transport
- [x] write `SerialTransportTests.swift` first using an `openpty()` pair (the transport opens the slave path like a real device, exercising `open`/`termios`; the test plays the Flipper on the master): lines written to the Flipper end arrive as framed messages; `send` writes a newline-terminated line; closing the Flipper end reports disconnect; `discoverFlipperPorts()` filters `/dev/cu.usbmodemflip*` from a given directory listing (injectable)
- [x] implement `Sources/SpoolBuddyCore/Transport/Transport.swift` protocol (`connect`, `send(Message)`, `messages: AsyncStream<Message>`, `state`) and `SerialTransport.swift` (POSIX `open`/`termios` raw mode 115200 8N1; reads via a `DispatchSourceRead` on the non-blocking descriptor)
- [x] implement `hello` handshake and the 2 s connected timeout in `TransportSession.swift` (tests in `TransportSessionTests.swift` with an in-memory `FakeTransport` plus one pty round trip; a Flipper `hello` within 2 s of ours is not answered, any other is, mirroring `host_link.c`)
- [x] run `cd host && swift test` - must pass before next task

### Task 15: Session feed model and intent handling
- [x] write `SessionTests.swift` first: a `scan` creates a `FeedRow(status: .scanned)`, lookup found → row carries the spool and `.awaitingWeight`; unknown with prefill → `.unknown(prefill: true)`; `lookup` reply built for the Flipper has `status`, `spool_id`, ≤ 4 `lines`, `defaults` (gross = last_scale_weight or label + core); an `intent weigh` with `mode: net` calls `updateWeight` with `net + core`; `intent new` creates with the configured catalog entry and links the row; `create_on_host` / `link_on_host` only flag the row; a repeated intent id returns the cached `result` without a second API call; editing a weight on the host sends the same update and marks `savedBy: .host`
- [x] implement `Sources/SpoolBuddyCore/Session/FeedRow.swift`, `Session.swift` (actor; owns rows, intent cache, settings reference), `LookupBuilder.swift`, `IntentHandler.swift`
- [x] implement `Settings.swift` (base URL, API key via Keychain with an injectable store for tests, catalog entry id + weight, default mode) with `SettingsTests.swift` (in-memory store)
- [x] implement startup check: `spoolmanEnabled == true` → session refuses with a clear message; permission missing → message naming the missing permission
- [x] run `cd host && swift test` - must pass before next task
- ➕ the found `defaults.gross` falls back to `remaining + core` (what the spool should weigh now) instead of `label + core` when Bambuddy has no `last_scale_weight`; the two agree for an unused spool
- ➕ `SpoolInventory` protocol (`Bambuddy/SpoolInventory.swift`) is what `Session` calls; `BambuddyClient` conforms, tests use `FakeInventory`. `BambuddyState` (`unchecked`, `notConfigured`, `ready`, `refused(spoolmanMode | permissionMissing | unauthorized)`, `unreachable`) is checked on the first scan/intent; `refused` sticks until `checkBambuddy()`/`reconfigure`, `unreachable` is rechecked on the next scan
- ➕ host actions for Task 16 live on `Session`: `saveWeight(row:grams:mode:)`, `createSpool(row:_:)` (New form), `link(row:toSpool:)` (picker; a 409 marks the row and rethrows `tagAlreadyLinked`); `snapshots()` streams `SessionSnapshot(rows, bambuddy)` to the views, rows kept oldest first
- ➕ concurrent repeats of an intent id share one in-flight call; the result cache keeps the last 256 ids

### Task 16: macOS SwiftUI app
- [x] write `FeedViewModelTests.swift` first: rows render newest first; weight field accepts integers only; save enabled only with a value; row status text mapping; link picker filter matches material/colour/brand/note case-insensitively
- [x] implement `Sources/SpoolBuddyMac/App.swift`, `FeedView.swift` (list rows: colour swatch, material + subtype, colour name, remaining, weight field with net/gross toggle, Save, status, saved-by), `UnknownRowActions.swift` (**New** form prefilled; **Link to existing** picker), `SettingsView.swift` (URL, API key, catalog entry picker loaded from `/inventory/catalog`, default mode, port picker, Test connection), `StatusBar` (transport state, Bambuddy state), **Clear** action
- [x] implement `FeedViewModel.swift` and `SettingsViewModel.swift` in `SpoolBuddyCore` (platform-neutral, reused in Phase 2)
- [x] `swift build` produces a runnable `SpoolBuddyMac`; document `swift run SpoolBuddyMac` in `host/README.md`
- [x] run `cd host && swift test` - must pass before next task
- ➕ `FlipperLink` (`ViewModels/FlipperLink.swift`) pumps the transport into the session: opens the chosen port or the first Flipper found, runs the handshake, answers each Flipper message with `Session.handle`, and reconnects after 2 s when the link drops or the port will not open; `HostModel` wires settings store, session, link and feed for the app
- ➕ the Flipper port is part of `Settings` (`port`, `nil` = automatic) and saved with the rest; `Session.listSpools()` feeds the Link picker through the same readiness check as the other host actions
- ➕ ⚠️ the macOS 27 SDK declares `@State` as a macro whose `SwiftUIMacros` plugin ships only with Xcode, so with the Command Line Tools `SpoolBuddyMac` uses `@ViewState` (a typealias of the `State` property wrapper struct); documented in `host/README.md`

### Task 17: CI workflows
- [x] `.github/workflows/flipper.yml` (paths `flipper/**`, `protocol/**`): ubuntu, `pip install ufbt`, `ufbt update --branch=1.5.1`, `cd flipper && ufbt`, `make -C flipper/tests test`, upload `dist/*.fap` artifact
- [x] `.github/workflows/host.yml` (paths `host/**`, `protocol/**`): macos-latest, `swift test`
- [x] `.github/workflows/tools.yml` (paths `tools/**`, `protocol/**`): ubuntu, `uv sync`, `uv run pytest`
- [x] add a `Makefile` at the repo root with `test` running all three suites locally
- [x] run `make test` - must pass before next task
- [x] ➕ `tools/tests/test_ci.py` (pyyaml dev dep) checks the workflow triggers, commands and the root `Makefile`; `tools.yml` also watches `flipper/tests/vectors/**`, which the tools tests compare against their generators; workflows pass `actionlint`, and the flipper job was reproduced in Linux containers (gcc C tests, `ufbt` 1.5.1 build). `host.yml` (macos-latest) is not run locally

### Task 18: Verify acceptance criteria
- [x] verify all requirements from Overview are implemented (scan → lookup → weigh/new/link from Flipper; feed with editable weights on the host; dedupe via tray UUID candidates then UID; Spoolman refused)
- [x] verify edge cases: missing sectors, over-long lines, host disconnect mid-intent, 409 on link, 404 on lookup, net with zero core
- [x] run full test suite (`make test`)
- [x] run linters: `swift build -Xswiftc -warnings-as-errors`, C tests with `-Werror`, `uv run ruff check` (add ruff to dev deps)
- [x] verify test coverage: `swift test --enable-code-coverage` for `SpoolBuddyCore` ≥ 80 %, pytest `--cov` ≥ 80 %
- ➕ requirements and edge cases traced to tests in each suite: Flipper flow in `test_app_state.c` (weigh, New only with prefill, Link/Create on host, host lost mid-intent → Error, timeout), host in `SessionTests`/`FeedViewModelTests` (host-edited weights, Spoolman refused), lookup order in `findSpoolTriesTrayCandidatesInOrderThenUID` and `test_find_by_tag_tries_candidates_in_order_then_uid`, missing sectors in the C/Swift/Python parser tests, over-long lines in `test_line_buffer.c`, `NDJSONFramerTests`, `test_framer_drops_over_long_lines`, 409/404 in `BambuddyClientTests`/`test_bambuddy.py`. Added the missing Swift session test `weighNetWithAZeroCoreSendsTheNet`
- ➕ root `Makefile` gained `lint` (C tests with `-Werror`, `swift build -Xswiftc -warnings-as-errors`, `uv run ruff check`) and `coverage` (`swift test --enable-code-coverage` checked by the new `tools/swift_coverage.py`, `pytest --cov` with `fail_under = 80`); `tools.yml` runs ruff; ruff and pytest-cov are dev deps. Measured: SpoolBuddyCore 94.9 % lines, tools 96 %. The test target also builds cleanly with `-warnings-as-errors`; the only remaining output is `ld` search-path notices from the Command Line Tools

### Task 19: Update documentation
- [x] update `README.md` (install ufbt, build and `ufbt launch` the FAP, run the Mac host, run the Python bridge, where fixtures live, how to capture real dumps with `--dump-dir`)
- [x] finalize `protocol/PROTOCOL.md` against the implemented schema
- [x] add "Phase 0 checklist" section to `README.md` mirroring Post-Completion below
- [x] update the design doc status line and note the tray-UUID candidate approach
- ➕ ⚠️ finalizing PROTOCOL.md turned up a contract mismatch: for a MIFARE Classic whose sectors all fail (any non-Bambu MFC, or a 7-byte UID) the Flipper sends `"blocks":{}` with every sector in `missing`, so the host can match it by UID, but the schema had `minProperties: 1` on `blocks` and both hosts dropped the scan as malformed. Fixed in one commit: the schema allows an empty `blocks` (still required for `mfc`), new fixture `scan_mfc_unreadable.json` (generated, compared by the C encoder test), Swift `TagDump` accepts and always encodes `blocks` for `mfc`, tests in all three suites (C fixture compare, `scanMfcWithEverySectorMissingKeepsAnEmptyBlocksObject`, `anUnreadableMfcIsLookedUpByUIDWithoutPrefill`, `test_scan_of_an_unreadable_mfc_is_looked_up_by_uid`, `test_scan_mfc_unreadable_has_every_sector_missing_and_no_blocks`). PROTOCOL.md also now documents the late-hello handshake, the lookup wait without timeout, the retry rule for failed intents and the Spoolman refusal

## Technical Details

### Bambu key derivation
```
PRK = HMAC-SHA256(key = MASTER_KEY, msg = UID[4])
T1  = HMAC-SHA256(PRK, "RFID-A\0" || 0x01), T2 = HMAC-SHA256(PRK, T1 || "RFID-A\0" || 0x02), T3 = ...
OKM = T1 || T2 || T3 (96 bytes); key_a[sector] = OKM[sector*6 .. sector*6+6]
MASTER_KEY = 9A 75 9C F2 C4 F7 CA FF 22 2C B9 76 9B 41 BC 96
```

### Tag dump JSON (`scan`)
```json
{"t":"scan","v":1,"scan_id":"s-000012",
 "tag":{"type":"mfc","uid":"04A1B2C3","sak":8,"atqa":"0400",
        "blocks":{"0":"…32 hex…","1":"…"},"missing":[7]}}
{"t":"scan","v":1,"scan_id":"s-000013",
 "tag":{"type":"ntag","uid":"04112233445566","sak":0,"pages":{"0":"04112233","1":"…"}}}
```
Sector trailers (blocks 3, 7, …, 63) are omitted from `blocks`. Hex is
upper-case. `missing` lists sectors that failed authentication.

### Host → Flipper
```json
{"t":"lookup","v":1,"scan_id":"s-000012","status":"found","spool_id":42,
 "lines":["PLA Basic","Jade White","484 g left of 1000","core 250 g"],
 "defaults":{"gross":734,"label":1000,"core":250}}
{"t":"lookup","v":1,"scan_id":"s-000012","status":"unknown","prefill":true,
 "lines":["PLA Basic","#FFFFFFFF","label 1000 g"],"defaults":{"label":1000,"core":250}}
{"t":"result","v":1,"id":"i-000003","ok":true,"lines":["Saved","484 g left"]}
```

### Flipper → Host intent
```json
{"t":"intent","v":1,"id":"i-000003","scan_id":"s-000012","action":"weigh",
 "grams":734,"mode":"gross","tag":{…same object as in scan…}}
```

### Weight rules (host)
- `gross = net + core_weight` (core from the matched spool; for a new item, from the configured catalog entry).
- `POST /spoolbuddy/scale/update-spool-weight {spool_id, weight_grams: gross}`.
- New item: `label_weight` from tag (fallback 1000), `core_weight` + `core_weight_catalog_id` from settings, `weight_used: 0`, `material` from tag (required; if absent the Flipper is not offered **New**), `subtype`, `color_name` (hex string when no catalog name), `rgba`, `brand: "Bambu Lab"`, `tag_uid`, `tray_uuid` (first candidate), `tag_type`, `data_origin: "flipper"`.

### Flipper app structure
```
flipper/
  application.fam
  src/spoolbuddy_app.c        view dispatcher, scenes
  src/scenes/*.c              idle, reading, result, enter_weight, waiting, done
  src/app_state.{c,h}         pure reducer (host-tested)
  src/intents.{c,h}           intent store interface + in-memory impl (host-tested)
  src/tag_dump.{c,h}          dump model (host-tested)
  src/json_writer.{c,h} / json_reader.{c,h} / proto_encode.{c,h} / proto_decode.{c,h}
  src/line_buffer.{c,h}       (host-tested)
  src/line_sender.{c,h}       packets out, resync after a cut line (host-tested)
  src/crypto/sha256, hmac_sha256; src/bambu_keys.{c,h}   (host-tested)
  src/transport.h, transport_usb.c                        (Flipper only)
  src/nfc_read.{c,h}                                      (Flipper only)
  tests/Makefile, minitest.h, test_*.c, vectors/
```

### Host package structure
```
host/Package.swift
host/Sources/SpoolBuddyCore/{Protocol,Tags,Bambuddy,Transport,Session,ViewModels}
host/Sources/SpoolBuddyMac/{App,FeedView,SettingsView,...}.swift
host/Tests/SpoolBuddyCoreTests/*.swift
```

### Constraints to remember
- No Xcode on the build Mac: SwiftPM only; no `.xcodeproj`, no UI tests.
- ufbt pinned to firmware release 1.5.1; the Flipper must run 1.5.1.
- Flipper line buffers are static (8192 bytes RX, 8192 bytes TX); no heap growth per scan.
- The Flipper never creates an item without `material`.

## Post-Completion

**Phase 0 manual checks (owner with Flipper, spools and the NAS):**
- Install ufbt, run `cd flipper && ufbt launch` with the Flipper on USB; confirm firmware 1.5.1 on the device.
- With the app open, check the USB link: the Mac sees a new `/dev/cu.usbmodemflip_*`, opening it (e.g. `python -m serial.tools.miniterm`) yields the Flipper `hello`, typing a host `hello` line within 2 s turns the Idle status line to `USB`, closing it shows `no host`, and the CLI/qFlipper work again after leaving the app.
- With the Python bridge or the Mac host connected: scan a known spool → Actions → Enter weight sends a `weigh` intent and shows the host's result; toggle Mode: net and check the pre-fill drops by the core weight; an unknown tag offers New / Link on host (or Create on host / Link on host without prefill); stop the host while "Saving" and check the error after 10 s.
- Without a host connected, scan a Bambu spool and an NTAG: the Flipper shows the UID, the type and for the Bambu tag the material, detailed type, tray hex and "Sectors read 16/16"; Back while "Reading tag" returns to Idle and scanning resumes.
- Create a Bambuddy API key with inventory read + update and settings read; put the URL (`http://<BAMBUDDY_LAN_IP>:8000`) and key into the Mac host settings or the Python bridge flags.
- Scan a sealed Bambu spool: confirm all 16 sectors read (no `missing`), save the dump with `--dump-dir protocol/fixtures/tags/`, replace the synthetic fixture with the real one.
- Compare the tray UUID candidates with the `tray_uuid` Bambuddy already holds for a spool an AMS has reported (`GET /inventory/spools`): record which block wins, drop the losing candidate from both parsers, and file an upstream issue on maziggy/bambuddy if the daemon's blocks 4+5 reading is wrong.
- Scan a sealed refill through its box; note where the Flipper must be held and whether it reads at all.
- Weigh one open spool from the Flipper and one from the host; check `last_scale_weight`, `weight_used` and `last_weighed_at` in Bambuddy.
- Create one new item from the Flipper; confirm it appears with the right label weight, core weight and tag, and that a second scan of the same spool (other tag) matches it via the tray UUID.
- Link one hand-added spool via the host picker; confirm a repeat scan matches.

**External system updates:**
- None on the Synology side. The host reaches Bambuddy on its macvlan LAN address.

**Deferred to Phase 2 (separate plan):**
- BLE serial transport on the Flipper, CoreBluetooth host, iPad/iPhone UI, Xcode project, TestFlight once the paid developer account exists.
