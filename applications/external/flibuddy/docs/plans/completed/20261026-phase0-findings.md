# Apply the Phase 0 findings: tray UUID, AMS tag UID form, core weight, colour names

## Overview
Phase 0 was run on 2026-10-26 with the owner's Flipper Zero (firmware 1.5.1),
real Bambu spools and the Bambuddy on the NAS. The results are recorded in
`docs/superpowers/specs/2026-10-25-flipper-spoolbuddy-design.md`, section
"Phase 0 findings (2026-10-26)". This plan brings the code in line with them:

- the tray UUID is block 9 of a Bambu tag; the two-candidate scheme goes;
- Bambu tag UIDs are sent to Bambuddy in the AMS form (`UID + "00000100"`);
- the core weight for new items defaults to 325 g and is an editable number
  in the host settings, with the catalog entry kept only as the
  `core_weight_catalog_id` reference;
- new items get `color_name` from Bambuddy's colour map and a subtype without
  the material word, matching AMS-added spools;
- a real tag dump (`protocol/fixtures/tags/bambu_pla_pure_real.json`, already
  committed, captured by the bridge from spool "PLA Pure", UID `7EF1AB04`)
  joins the fixtures and every parser is tested against it;
- the READMEs record the Phase 0 results.

No protocol change: `scan` still carries the raw dump, and `lookup`/`result`
are unchanged. The Flipper app is unaffected except where a test or a README
mentions tray UUID candidates.

## Context (from discovery)
- Tray UUID candidates: `host/Sources/SpoolBuddyCore/Tags/TagParser.swift`
  (`trayUuidCandidates`, `daemonTrayUuid`), `host/Sources/SpoolBuddyCore/Tags/ParsedTag.swift`,
  `host/Sources/SpoolBuddyCore/Session/LookupBuilder.swift`,
  `tools/bridge/bambu_tag.py` (`tray_uuid_candidates`), their tests, and the
  wording in `protocol/PROTOCOL.md`, `host/README.md`, `tools/README.md`.
  The Flipper already shows block 9 (`flipper/src/nfc_classify.c`, `BLOCK_TRAY_UUID`).
- Bambuddy calls: `host/Sources/SpoolBuddyCore/Bambuddy/BambuddyClient.swift`
  (`spoolByTag`, `createSpool`, `linkTag`), `tools/bridge/bambuddy.py`.
- Core weight: `host/Sources/SpoolBuddyCore/Session/Settings.swift`
  (`defaultCoreWeight = 250`, `useCatalogEntry`),
  `host/Sources/SpoolBuddyCore/ViewModels/SettingsViewModel.swift`,
  `host/Sources/SpoolBuddyMac/SettingsView.swift` (core weight is read-only
  `LabeledContent`), `tools/bridge/session.py` (`DEFAULT_CORE = 250`),
  `tools/bridge/__main__.py` (`--core-weight` default).
- New-item fields: `host/Sources/SpoolBuddyCore/ViewModels/NewSpoolForm.swift`,
  `host/Sources/SpoolBuddyCore/Session/IntentHandler.swift`,
  `host/Sources/SpoolBuddyCore/Bambuddy/Models.swift` (`NewSpool`),
  `tools/bridge/session.py` (`_perform`, action `new`).
- Fixtures and their tests: `protocol/fixtures/tags/`, `tools/fixtures/`,
  `tools/tests/test_fixtures.py` (checks committed files against generators;
  the real dump has no generator), `tools/tests/test_tag_vectors.py` and
  `tools/fixtures/make_tag_vectors.py` (C vectors from tag fixtures),
  `flipper/tests/vectors/tag_dumps.h`, `flipper/tests/test_nfc_classify.c`,
  `host/Tests/SpoolBuddyCoreTests/TagParserTests.swift`, `tools/tests/test_bambu_tag.py`.
- Bambuddy facts (verified live on 2026-10-26):
  - `GET /api/v1/inventory/colors/map` → `{"colors": {"ff9016": "Pumpkin Orange", ...},
    "by_material": {"pla matte|ffffff": "Ivory White", ...}}`; keys are
    lower-case hex without `#` and without alpha; needs inventory read.
  - `tag_uid` is normalised to its last 16 hex characters, never padded;
    AMS writes `ECA8FEDE00000100` for MIFARE UID `ECA8FEDE`.
  - `by-tag` tries `tray_uuid` first, then `tag_uid`.
  - AMS-added spools have `subtype` "Basic" for tray_sub_brands "PLA Basic".

## Development Approach
- **Testing approach**: TDD. Write the failing test, then the code.
- Complete each task fully before moving to the next
- Make small, focused changes
- **CRITICAL: every task MUST include new/updated tests** for code changes in that task
  - tests are not optional - they are a required part of the checklist
  - write unit tests for new functions/methods
  - write unit tests for modified functions/methods
  - add new test cases for new code paths
  - update existing test cases if behavior changes
  - tests cover both success and error scenarios
- **CRITICAL: all tests must pass before starting next task** - no exceptions
- **CRITICAL: update this plan file when scope changes during implementation**
- Run tests after each change
- Keep the wire protocol unchanged (`protocol/schema/message.schema.json`
  and the message fixtures must not change)
- Follow `AGENTS.md`: English everywhere in the repository, commit per task

## Testing Strategy
- **Unit tests**: required for every task (see Development Approach above)
- Three suites: `make -C flipper/tests test`, `cd host && swift test`,
  `cd tools && uv run pytest`; `make test` runs all, `make lint` and
  `make coverage` gate the end.
- No e2e suite; hardware checks are Post-Completion.

## Progress Tracking
- Mark completed items with `[x]` immediately when done
- Add newly discovered tasks with ➕ prefix
- Document issues/blockers with ⚠️ prefix
- Update plan if implementation deviates from original scope
- Keep plan in sync with actual work done

## What Goes Where
- **Implementation Steps** (`[ ]` checkboxes): tasks achievable within this codebase - code changes, tests, documentation updates
- **Post-Completion** (no checkboxes): items requiring external action - manual testing, changes in consuming projects, deployment configs, third-party verifications
- **Checkbox placement**: Checkboxes belong only in Task sections (`### Task N:` or `### Iteration N:`). Do not put checkboxes in Success criteria, Overview, or Context — they cause extra loop iterations.

## Implementation Steps

### Task 1: Wire the real tag dump into the fixture suites
- [x] in `tools/tests/test_fixtures.py`, make the "committed file equals generator output" check apply only to fixtures with `"synthetic": true`; real dumps (`"synthetic": false`) are checked for shape only (64 blocks, UID, SAK, ATQA, no `missing`)
- [x] extend `tools/fixtures/make_tag_vectors.py` so `flipper/tests/vectors/tag_dumps.h` also carries the real dump, regenerate the header, and keep `tools/tests/test_tag_vectors.py` failing when the header is stale
- [x] add a C test in `flipper/tests/test_nfc_classify.c` (and `test_tag_dump.c` where the loader is exercised) that classifies the real dump: Bambu, UID `7EF1AB04`, 16/16 sectors, tray line from block 9 starting `F1C8FDF0`
- [x] add Swift and Python parser tests over the real dump: UID `7EF1AB04`, tray UUID `F1C8FDF0BC1643C0B37189ADB0687992`, material `PLA`, detailed type `PLA Pure`, variant `A19-B00`, material id `GFA19`, RGBA `A4DBE8FF`, label 1000, nozzle 190–230, has prefill
- [x] describe the real fixture in `protocol/README.md` and the root `README.md` ("Fixtures"), keeping the synthetic one for the key vectors
- [x] run all three suites - must pass before task 2
- ⚠️ the real dump was captured with sectors 3-15 missing (`"missing": [3..15]`),
  not 16/16 as assumed above: the real-dump shape check allows `missing`
  (sorted, sector 0 present, missing sectors zero-filled, read trailers carry
  Key A) and the C classifier test expects "Sectors read 3/16". Blocks 1-10
  are present, so every parser field above is covered. A full-sector real
  dump can replace it after a re-capture.

### Task 2: Single tray UUID from block 9
- [x] Swift: replace `trayUuidCandidates: [String]` with `trayUuid: String?` in `ParsedTag`; `TagParser` reads block 9 only, `nil` when the block is missing or all zero; delete `daemonTrayUuid`; update `LookupBuilder`, `NewSpoolForm`, `IntentHandler`, `FeedItem` and any display text that listed candidates
- [x] Swift tests: real dump → the block 9 value; synthetic dump → its block 9 value; missing block 9 → `nil`; all-zero block 9 → `nil`; lookup, create and link use the single value
- [x] Python: same change in `tools/bridge/bambu_tag.py` (`tray_uuid: str | None`) and `tools/bridge/session.py`; drop the candidate wording from the module docstring; update `tools/tests/test_bambu_tag.py` and `tools/tests/test_bridge_protocol.py`
- [x] docs: `protocol/PROTOCOL.md`, `host/README.md`, `tools/README.md`, `flipper/README.md` no longer mention candidates or blocks 4+5 as a tray UUID source; state block 9 and why
- [x] run all three suites - must pass before task 3
- ➕ `findSpool(trayCandidates:uid:)` / `find_by_tag(candidates, uid)` became
  `findSpool(trayUuid:uid:)` / `find_by_tag(tray_uuid, uid)`: one `tray_uuid`
  query when present, then `tag_uid`. `LookupBuilder` and `FeedItem` never
  referenced the candidates and needed no change. The root README Phase 0
  checklist item about candidates is left for task 7.

### Task 3: Send Bambu tag UIDs in the AMS form
- [x] Swift: add `ParsedTag.bambuddyTagUid` — for `.bambu` tags the upper-case UID followed by `00000100`, for NTAG the upper-case UID as read; use it in `LookupBuilder` (by-tag query), `NewSpoolForm`/`NewSpool` (create) and the link call
- [x] Swift tests: `7EF1AB04` → `7EF1AB0400000100`; NTAG 7-byte UID unchanged; `BambuddyClientTests` stub asserts the query and body values
- [x] Python: same helper in `tools/bridge/bambu_tag.py`, used by `tools/bridge/session.py` for lookup, create and link; tests in `tools/tests/test_bambu_tag.py` and `tools/tests/test_bridge_protocol.py`
- [x] document the form in `protocol/PROTOCOL.md` ("what the host sends to Bambuddy") and `host/README.md`
- [x] run all three suites - must pass before task 4
- ➕ the by-tag query is built in `IntentHandler.find` (via
  `SpoolInventory.findSpool`), not in `LookupBuilder`; the bridge has no link
  call (`link_on_host` is only flagged), so in Python the AMS form covers
  lookup and create. Display lines, error texts and `SessionError.spoolNotFound`
  keep the UID as read. Spools stored earlier with the bare UID (e.g. #63,
  `7EF1AB04`) are no longer found by UID; they still match by tray UUID.

### Task 4: Core weight 325 g, editable in settings
- [x] Swift: `Settings.defaultCoreWeight = 325`; `useCatalogEntry` still copies the entry weight; add validation (1…5000 g) in `SettingsViewModel`; a user-edited core weight is saved even when a catalog entry is selected, and selecting an entry overwrites the number once
- [x] `SettingsView.swift`: replace the read-only `LabeledContent("Core weight")` with a numeric `TextField` bound to the draft, with the explanation "weight of everything that is not filament; 325 g = Bambu reusable spool 293 g + refill core 32 g"
- [x] Swift tests: default 325; picking an entry copies its weight; editing after picking keeps the edit on save; out-of-range rejected; persisted and reloaded through `Settings` defaults
- [x] Python: `DEFAULT_CORE = 325` in `tools/bridge/session.py` and the `--core-weight` default/help in `tools/bridge/__main__.py`; tests updated
- [x] docs: root `README.md` (Mac host and bridge sections), `host/README.md`, `tools/README.md` say 325 g and why; mention that AMS-added spools take their core weight from the first "Bambu Lab" catalog entry in Bambuddy, so that entry should be 325 g too
- [x] run all three suites - must pass before task 5
- ➕ `lookup_unknown.json` (a message fixture, unchanged) still carries
  `"core": 250`; the tests that compare against it pass a 250 g core
  explicitly (`testSettings` in Swift, `core_weight=250` in Python), and new
  tests check the 325 g default. The picker's fallback label for an entry
  missing from the catalog no longer shows a weight, since the core weight
  may now differ from the entry's.

### Task 5: Colour names from Bambuddy's colour map
- [x] Swift: `BambuddyClient.colorMap()` → `GET /inventory/colors/map` decoded into `colors: [String: String]` and `byMaterial: [String: String]`; `ColorNameResolver.name(rgba:, detailedType:)` tries `byMaterial["<detailed type lower-cased>|<hex6 lower-cased>"]`, then `colors[hex6]`, else `nil`; the session loads the map once after the startup check and keeps it; a failed load leaves names empty and logs to the feed status, never blocks a scan
- [x] Swift: `NewSpoolForm`/`IntentHandler` set `color_name` from the resolver and omit it when unresolved (no hex string); the feed row shows the resolved name when present
- [x] Swift tests: `FF9016` + "PLA Basic" → "Pumpkin Orange" (by material or colors); `A4DBE8` → `nil`; alpha stripped; case-insensitive; map fetch failure → create still succeeds with no `color_name`
- [x] Python: same in `tools/bridge/bambuddy.py` and `tools/bridge/session.py` with tests in `tools/tests/test_bambuddy.py` and `tools/tests/test_bridge_protocol.py`
- [x] run all three suites - must pass before task 6
- ➕ the map failure is shown in the status bar (`SessionSnapshot.colorNamesProblem`,
  `FeedViewModel.colorNamesProblem`), and the next startup check retries the
  load; settings changes reload it. Rows scanned before the map arrives are
  named when it does. The feed and the host's New form show the resolved
  name; the feed still falls back to the hex for display only. The
  `colorMap` call is counted apart from `FakeInventory.calls`, so the
  call-order tests are unchanged. The bridge loads the map in `main` right
  after the inventory-mode check (`Bridge.load_color_map`).

### Task 6: Subtype without the material word
- [x] Swift and Python: derive `subtype` for new items from the detailed type by dropping a leading word equal to the material, case-insensitive ("PLA Pure" → "Pure", "PLA Basic" → "Basic", "PETG-HF" with material "PETG" → "PETG-HF" unchanged, empty → `nil`)
- [x] tests for the cases above in `TagParserTests`/`NewSpoolForm` tests and `tools/tests/test_bambu_tag.py`
- [x] the feed row and the Flipper `lookup` lines keep showing the full detailed type; only the Bambuddy `subtype` field changes
- [x] run all three suites - must pass before task 7
- ➕ the Flipper's found `lookup` line and the feed title join material and
  subtype (`FeedItem.title` in Swift, `session.title` in Python), so a spool
  created with subtype "Basic" still reads "PLA Basic"; before, the found
  line showed the bare subtype. The host's New form now starts with "Basic"
  and its placeholder reads "e.g. Basic". `PROTOCOL.md` ("New items")
  describes the rule.

### Task 7: Record the Phase 0 results in the documentation
- [x] root `README.md`: turn the "Phase 0 checklist" into "Phase 0 results (2026-10-26)" with what was verified (handshake, scans with and without host, both tags of a spool, reading through the box, weigh/new end to end, block 9, AMS tag UID form, core weight 325) and what was not (NTAG scan, link of a hand-added spool, Back during "Reading tag"); keep the unverified items as a short remaining-checks list
- [x] `flipper/README.md`: note that Back on the Idle screen leaves the app and hands USB back to the CLI, and that a tag moved away early yields an all-missing dump
- [x] `host/README.md`: settings section updated for the editable core weight and the colour map
- [x] `tools/README.md`: `--core-weight` default 325 and the real fixture capture note
- [x] run all three suites - must pass before task 8
- ➕ the host shows an all-missing dump as an unknown tag with only the UID
  (no tray UUID, no prefill), not with an "unreadable" text; the READMEs say
  so. The remaining Phase 0 checks stay as unticked items in the root README.

### Task 8: Verify acceptance criteria
- [x] verify every bullet of the Overview is implemented
- [x] `protocol/schema/message.schema.json` and `protocol/fixtures/messages/*` unchanged (`git diff --stat` on them is empty)
- [x] `make test` passes
- [x] `make lint` passes
- [x] `make coverage` meets the 80 % gate
- [x] `cd flipper && ufbt` builds the FAP

## Technical Details

### Tray UUID
Block 9 of a Bambu MIFARE Classic 1K tag, 16 bytes, upper-case hex. A block
of 16 zero bytes means "no tray UUID". Blocks 4+5 are the detailed type in
ASCII (NUL-padded) and are used only for `detailed_type`.

### AMS tag UID form
`bambuddy_tag_uid = uid_hex_upper + "00000100"` for Bambu tags (8 bytes,
16 hex characters). Bambuddy keeps the last 16 hex characters of whatever
it receives, so the value round-trips unchanged. NTAG UIDs (7 bytes) are
sent as read.

### Core weight
`core_weight` in Bambuddy is everything on the scale that is not filament.
Measured by the owner: Bambu reusable spool 293 g, refill core 32 g; the
sum, 325 g, applies to sealed spools and mounted refills alike. `net =
gross − core_weight`; `gross = net + core_weight`. The catalog entry is
recorded as `core_weight_catalog_id` for Bambuddy's bookkeeping only.

### Colour map
```
GET /api/v1/inventory/colors/map
{"colors": {"ff9016": "Pumpkin Orange", ...},
 "by_material": {"pla matte|ffffff": "Ivory White", ...}}
```
Lookup order: `by_material["<detailed_type.lower()>|<hex6.lower()>"]`, then
`colors[hex6.lower()]`. `hex6` is the RGBA without its alpha byte.

### Real fixture
`protocol/fixtures/tags/bambu_pla_pure_real.json`: UID `7EF1AB04`, SAK 8,
ATQA 0400, 64 blocks, sector trailers rebuilt from the derived Key A by the
bridge, `"synthetic": false`, sectors 3-15 missing (zero-filled). Expected parse: PLA / "PLA Pure" / A19-B00 /
GFA19 / A4DBE8FF / 1000 g / 1.75 mm / 190–230 °C / drying 55 °C 8 h / tray
UUID `F1C8FDF0BC1643C0B37189ADB0687992`.

## Post-Completion
*Items requiring manual intervention or external systems - no checkboxes, informational only*

**Manual verification** (owner, with the Flipper and the NAS):
- Run the Mac host with the new settings screen: core weight shows 325 g
  (a core weight saved before, e.g. 250 g, is kept: enter 325 once),
  "Bambu Lab - Plastic Low Temp" selected; scan the orange spool (Bambuddy
  #30) and weigh it in gross mode; check `last_scale_weight` and
  `weight_used` in Bambuddy.
- Create one more new item from the Flipper and check in Bambuddy that
  `tag_uid` has the `00000100` tail, `subtype` has no material word and
  `color_name` is a name or empty.
- Remaining Phase 0 items: scan an NTAG, link a hand-added spool through
  the host picker, press Back during "Reading tag".

**External systems**:
- Spool #63 ("PLA Pure") was created before this plan with `tag_uid`
  `7EF1AB04` and `subtype` "PLA Pure"; fix by hand in Bambuddy if desired.
- Upstream: the SpoolBuddy daemon in maziggy/bambuddy reads the tray UUID
  from blocks 4+5; the owner decides whether to file an issue.
- Phase 2 (BLE, iPad/iPhone, QR code for the API key) is a separate plan.
