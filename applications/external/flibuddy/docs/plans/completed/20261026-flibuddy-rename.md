# Rename the project from SpoolBuddy to FliBuddy

## Overview
- "SpoolBuddy" is the name of Bambuddy's own kiosk device (its API lives under
  `/api/v1/spoolbuddy`, its UI says "SpoolBuddy devices"). Our Flipper + Apple
  tool was named after it by mistake. The app is now called **FliBuddy**
  (chosen 2026-10-26: "FlipBuddy" is taken in the App Store, "FlipperBuddy"
  carries a FLIPPER trademark risk). `docs/design/ui-brief.md` and
  `docs/design/macos-ui-spec.md` already use the new name.
- This plan renames every identifier, file, target, display string and
  document that refers to *our* app, and leaves every reference to
  *Bambuddy's* SpoolBuddy alone. Nothing has shipped, so there is no
  compatibility layer: old ids are simply replaced.
- It also fixes the test litter found while merging Phase 2: `swift test`
  leaves one empty `spoolbuddy-tests-<UUID>.plist` per test in
  `~/Library/Preferences` (1502 after the Phase 2 run) because `cfprefsd`
  rewrites the plist after `ScratchDefaults.deinit` deletes it.

## Context (from discovery)
- 110 tracked files mention SpoolBuddy (`git grep -i -l spoolbuddy`).
- Files and directories named after it: `flipper/src/spoolbuddy_app.{c,h}`,
  `flipper/src/ble_spoolbuddy_svc.{c,h}`, `flipper/icons/spoolbuddy_10px.png`,
  `host/Sources/SpoolBuddyCore/`, `host/Sources/SpoolBuddyUI/`,
  `host/Sources/SpoolBuddyMac/`, `host/Tests/SpoolBuddyCoreTests/`,
  `host/Sources/SpoolBuddyCore/SpoolBuddyCore.swift`,
  `host/Sources/SpoolBuddyCore/Transport/BLE/SpoolBuddyGATT.swift`.
- Identifiers: FAP `appid="spoolbuddy"`, `name="SpoolBuddy"`,
  `entry_point="spoolbuddy_app"` (`flipper/application.fam`); C macros and
  symbols prefixed `SPOOLBUDDY_` / `spoolbuddy_`; log tags `"SpoolBuddy"`,
  `"SpoolBuddyBle"`, thread names `"SpoolBuddyNfc"`, `"SpoolBuddyBle"`;
  settings dir `/ext/apps_data/spoolbuddy` (`flipper/src/app_settings.h`);
  hello ids `spoolbuddy-fap/0.1.0` (`flipper/src/spoolbuddy_app.h`),
  `spoolbuddy-mac` (`host/Sources/SpoolBuddyCore/SpoolBuddyCore.swift`),
  `spoolbuddy-bridge` (`tools/`, `protocol/PROTOCOL.md`,
  `protocol/fixtures/messages/hello_*.json`); Swift package `SpoolBuddy`
  with products `SpoolBuddyCore`, `SpoolBuddyUI`, `SpoolBuddyMac`
  (`host/Package.swift`); XcodeGen project `SpoolBuddy` with targets
  `SpoolBuddyiOS`, `SpoolBuddyWatch`, `PRODUCT_NAME`, display names and
  usage descriptions, bundle ids `cool.bambuddy.spoolbuddy` and
  `cool.bambuddy.spoolbuddy.watchkitapp` (`host/Apple/project.yml`);
  Keychain service `"SpoolBuddy"` (`KeychainSecretStore.defaultService`);
  DispatchQueue labels `"SpoolBuddy.*"`; CI artifact `spoolbuddy-fap`
  (`.github/workflows/flipper.yml`); coverage step name (`host.yml`);
  `tools/pyproject.toml` name `spoolbuddy-tools`;
  `tools/swift_coverage.py` `DEFAULT_PREFIX = "Sources/SpoolBuddyCore/"`;
  `Makefile` targets `apple-build` (project and scheme names) and
  `coverage`; tests `tools/tests/test_apple_project.py`,
  `test_mac_info_plist.py`, `test_swift_coverage.py`, `test_ci.py`.
- User-visible strings: Flipper Idle title `"SpoolBuddy"`
  (`flipper/src/scene_idle.c`); Mac window title and the macOS-only message
  (`host/Sources/SpoolBuddyMac/App.swift`); iOS and watch
  `navigationTitle("SpoolBuddy")`; empty-feed text (`FeedView.swift`);
  Bluetooth hint (`SettingsForm.swift`); watch settings hint
  (`WatchSettingsView.swift`); error texts in `APIKeyQRPayload.swift`,
  `Session.swift`, `SettingsSync.swift`, `Transport.swift`,
  `FlipperLink.swift`.
- Docs: `README.md` (title `# flipper-spoolbuddy`, layout, commands),
  `AGENTS.md`, `flipper/README.md`, `host/README.md`,
  `.github/workflows/README.md`, `protocol/PROTOCOL.md`,
  `docs/superpowers/specs/2026-10-25-flipper-spoolbuddy-design.md`,
  `docs/plans/20261026-macos-ui-from-design.md` (target paths).
- **Must stay as they are** (they name Bambuddy's device, not ours):
  `POST /spoolbuddy/scale/update-spool-weight` in
  `host/Sources/SpoolBuddyCore/Bambuddy/BambuddyClient.swift` and
  `Models.swift`; "SpoolBuddy daemon (`pn5180.py`)" in `tools/bambu_keys.py`;
  the design spec's API table and its Phase 0 findings about the daemon;
  the README sentence "instead of SpoolBuddy hardware"; everything under
  `docs/plans/completed/` (history) and `.ralphex/progress/`; (➕ found in
  Task 6) "the SpoolBuddy daemon" in `flipper/src/bambu_keys.h` and the
  daemon comments in `TagParserTests.swift` and `tools/tests/test_bambu_tag.py`;
  the `/spoolbuddy/scale/update-spool-weight` route in `protocol/PROTOCOL.md`,
  `tools/bridge/bambuddy.py`, `BambuddyClientTests.swift` and
  `tools/tests/test_bambuddy.py`; `docs/design/ui-brief.md` line 170, which
  explains the name; this plan file.
- **Unchanged by design**: the GATT UUIDs (`SpoolBuddyGATT.swift`,
  `flipper/src/ble_wire.h`); the BLE advertised name (it is the Flipper's own
  name); `UserDefaults` keys (`bambuddy.*`, `weight.*`, `flipper.*`); the
  protocol version and message shapes; the `.ufbt` SDK directory.
- Tests: Swift (`swift test`, 439 tests), C (`make -C flipper/tests test`,
  234 tests), Python (`cd tools && uv run pytest`, 319 tests), the
  simulator builds (`make apple-project apple-build`), the FAP build
  (`cd flipper && ufbt`).

## Development Approach
- **Testing approach**: Regular for the rename (existing tests follow the
  rename and must stay green); TDD for the new key-value store in Task 5.
- Complete each task fully before moving to the next.
- Rename with `git mv` so history follows the files.
- Do a case-aware search after every task: `git grep -n -i spoolbuddy`
  must show only the "must stay" references listed above (check each hit).
- **CRITICAL: every task MUST include new/updated tests** for code changes
  in that task; **all tests must pass before starting the next task**.
- **CRITICAL: update this plan file when scope changes during implementation.**

## Testing Strategy
- **Unit tests**: the existing suites are renamed with their targets and run
  after every task; Task 5 adds tests for the in-memory store and the
  no-litter guarantee.
- **E2E tests**: none in the project; the simulator and FAP builds are the
  integration check (Task 6).

## Progress Tracking
- Mark completed items with `[x]` immediately when done
- Add newly discovered tasks with ➕ prefix
- Document issues/blockers with ⚠️ prefix
- Update plan if implementation deviates from original scope

## What Goes Where
- **Implementation Steps** (`[ ]` checkboxes): changes inside this repository.
- **Post-Completion** (no checkboxes): the directory rename on the owner's
  Mac, migration of the owner's saved settings and the Flipper's SD card.

## Implementation Steps

### Task 1: Rename the Flipper app
- [x] `git mv flipper/src/spoolbuddy_app.c flipper/src/flibuddy_app.c` and
      the header; `git mv flipper/src/ble_spoolbuddy_svc.c
      flipper/src/ble_flibuddy_svc.c` and the header;
      `git mv flipper/icons/spoolbuddy_10px.png flipper/icons/flibuddy_10px.png`
- [x] `flipper/application.fam`: `appid="flibuddy"`, `name="FliBuddy"`,
      `entry_point="flibuddy_app"`, `fap_icon="icons/flibuddy_10px.png"`;
      the description keeps "Bambuddy"
- [x] rename every `SPOOLBUDDY_*` macro, `spoolbuddy_*` symbol, include
      guard and `#include` to `FLIBUDDY_*` / `flibuddy_*` across
      `flipper/src` and `flipper/tests`; log tags and thread names become
      `"FliBuddy"`, `"FliBuddyBle"`, `"FliBuddyNfc"`
- [x] `flipper/src/app_settings.h`: `APP_SETTINGS_DIR` is
      `/ext/apps_data/flibuddy` (no migration of the old directory: the only
      setting is the transport, which defaults to USB)
- [x] `flipper/src/scene_idle.c`: the title reads `FliBuddy`
- [x] `flipper/src/flibuddy_app.h`: hello id `flibuddy-fap/0.1.0`
- [x] update `flipper/tests/*.c` expectations (hello id, settings path, any
      symbol names) and `flipper/tests/Makefile` if it names files
      (➕ added `test_settings_live_in_the_flibuddy_dir`; the Makefile names
      no files. ⚠️ `test_proto_decode.c` `test_hello_fixtures` and
      `test_tag_dump.c` `test_hello_matches_fixture` compare against
      `protocol/fixtures/messages/hello_*.json`, so their
      `spoolbuddy-fap`/`spoolbuddy-mac` strings move with the fixtures in
      Task 2; `src/bambu_keys.h` keeps "the SpoolBuddy daemon", Bambuddy's)
- [x] `flipper/README.md`: FAP name `dist/flibuddy.fap`, install path
      `/ext/apps/Tools/flibuddy.fap`, settings path, app name (➕ also the
      two FAP facts in the root `README.md`: `dist/flibuddy.fap` and the
      settings path)
- [x] run `make -C flipper/tests test` and `cd flipper && ufbt` - both must
      pass before task 2

### Task 2: Rename the protocol ids and the Python tools
- [x] `protocol/PROTOCOL.md`: `flibuddy-fap/0.1.0`, `flibuddy-mac/0.1.0`,
      `flibuddy-bridge/0.1.0` in the `app` row and the examples (➕ also the
      title and the schema's `$id`/`title`: `flipper-flibuddy`)
- [x] regenerate `protocol/fixtures/messages/hello_flipper.json` and
      `hello_host.json` through `tools/fixtures/make_message_fixtures.py`
      (change the generator, not the files by hand) (⚠️ no generator writes
      the hello fixtures: like `lookup_*`, `result_*` and `invalid_*` they
      are hand-written, so the `app` values were edited in place)
- [x] ➕ the C fixture tests follow: `flipper/tests/test_proto_decode.c`
      (`test_hello_fixtures`) and `flipper/tests/test_tag_dump.c`
      (`test_hello_matches_fixture`) expect `flibuddy-mac/0.1.0` and
      `flibuddy-fap/0.1.0`; the Swift tests that read the hello fixtures
      follow as well; run `make -C flipper/tests test` (➕ the synthetic
      `spoolbuddy-fap`/`spoolbuddy-mac` hellos in the Swift tests changed too;
      `HostModelTests` line 250 checks `FliBuddyCore.appName` and moves with
      it in Task 3)
- [x] `tools/bridge/__init__.py`: `APP = f"flibuddy-bridge/{VERSION}"`; update
      `tools/tests/test_bridge_protocol.py`
- [x] `tools/pyproject.toml`: name `flibuddy-tools`, description mentions
      flipper-flibuddy; `tools/swift_coverage.py` `DEFAULT_PREFIX =
      "Sources/FliBuddyCore/"` and its docstring; `tools/README.md` (➕ and
      `tools/uv.lock`; `tools/README.md` named neither. ⚠️ `make coverage`
      finds no files under the new prefix and fails until Task 3 renames
      the directory)
- [x] keep `tools/bambu_keys.py` line 3 ("SpoolBuddy daemon (`pn5180.py`)"):
      it names Bambuddy's daemon
- [x] update `tools/tests/test_fixtures.py`, `test_swift_coverage.py`,
      `test_ci.py`, `test_apple_project.py`, `test_mac_info_plist.py`
      expectations for the new names and paths (the latter two will pass
      fully after task 3 and 4; note it with a TODO and remove it then)
      (`test_swift_coverage.py` updated; `test_fixtures.py` and `test_ci.py`
      name nothing to rename. To keep the suite green the other two are not
      touched here: `test_mac_info_plist.py` moves with `Package.swift` in
      Task 3, `test_apple_project.py` with `project.yml` in Task 4)
- [x] run `cd tools && uv run pytest` - must pass (except the two files
      waiting on tasks 3 and 4, if any) before task 3 (319 passed; C 234
      and Swift 439 also pass)

### Task 3: Rename the Swift package, targets and sources
- [x] `git mv host/Sources/SpoolBuddyCore host/Sources/FliBuddyCore`,
      `SpoolBuddyUI` → `FliBuddyUI`, `SpoolBuddyMac` → `FliBuddyMac`,
      `host/Tests/SpoolBuddyCoreTests` → `host/Tests/FliBuddyCoreTests`;
      inside: `SpoolBuddyCore.swift` → `FliBuddyCore.swift`,
      `SpoolBuddyGATT.swift` → `FliBuddyGATT.swift` (UUID values unchanged)
- [x] `host/Package.swift`: package `FliBuddy`, products and targets
      `FliBuddyCore`, `FliBuddyUI`, `FliBuddyMac`, `FliBuddyCoreTests`,
      the Info.plist path; keep the `Fixtures` symlink working (it is
      relative, so it moved with the directory unchanged)
- [x] every `import SpoolBuddyCore` / `import SpoolBuddyUI`,
      `@testable import`, `enum SpoolBuddyCore` (→ `enum FliBuddyCore`,
      `appName = "flibuddy-mac"`), `SpoolBuddyGATT` type name, DispatchQueue
      labels `"FliBuddy.*"`, `KeychainSecretStore.defaultService = "FliBuddy"`,
      test Keychain service prefix `FliBuddyTests-` (➕ the iOS and watch
      shells in `host/Apple` still import `SpoolBuddyCore`/`SpoolBuddyUI`:
      they build only through `project.yml`, so they move with it in Task 4)
- [x] user-visible strings: Mac window title `FliBuddy`, the macOS-only
      message, iOS and watch `navigationTitle("FliBuddy")`, empty-feed text,
      Bluetooth hint, watch settings hint, the error texts listed in Context;
      the Spoolman-mode error keeps "Bambuddy" and says "FliBuddy supports
      internal inventory only"
- [x] `host/Sources/FliBuddyMac/Info.plist`: bundle name / identifier if
      present follow Task 4's bundle id scheme (none present: the plist
      keeps no `CFBundleIdentifier` on purpose; its Bluetooth usage text
      now names FliBuddy)
- [x] `host/README.md` and `AGENTS.md`: target names, paths, commands, the
      `@ViewState` rule's paths (➕ `host/README.md` still names
      `SpoolBuddyiOS`/`SpoolBuddyWatch` in its Apple section until Task 4
      renames those targets)
- [x] `docs/plans/20261026-macos-ui-from-design.md`: update the file paths in
      its Context section to the new target names (the plan is not started)
- [x] ➕ `tools/tests/test_mac_info_plist.py`: the `FliBuddyMac` plist path,
      the manifest's `"/Sources/FliBuddyMac/Info.plist"` and the docstring
      (moved here from Task 2 so the Python suite stays green); run
      `cd tools && uv run pytest`
- [x] ➕ the remaining test strings: `HostModelTests` expects
      `flibuddy-mac/0.1.0`, `SessionTests` hello app `flibuddy`, the
      rejected-scheme case `spoolbuddy://config` in `APIKeyQRPayloadTests`
      becomes `flibuddy://config` (still rejected: the QR scheme is
      `bambuddy://`), the error-text expectations in `BLETransportTests`,
      `FlipperLinkTests`, `SettingsViewModelTests` (➕ new expectations pin
      `KeychainSecretStore.defaultService == "FliBuddy"`, the Spoolman-mode
      text "FliBuddy supports" and the "update FliBuddy" endings of the QR
      and settings-sync version errors)
- [x] run `cd host && swift test` - must pass before task 4 (440 passed;
      `swift build`, pytest 319, `make coverage` with FliBuddyCore at
      88.2 % and `make lint` pass)

### Task 4: Rename the Xcode project, bundle ids and CI
- [x] `host/Apple/project.yml`: project `FliBuddy`, package reference
      `FliBuddy`, targets `FliBuddyiOS` and `FliBuddyWatch`, `PRODUCT_NAME`
      `FliBuddy` / `FliBuddyWatch`, display names `FliBuddy`, usage
      descriptions start with "FliBuddy …", bundle ids
      `com.sokolachko.flibuddy` and `com.sokolachko.flibuddy.watchkitapp`,
      `INFOPLIST_KEY_WKCompanionAppBundleIdentifier` to match (the owner's
      domain replaces the `cool.bambuddy` placeholder, which is Bambuddy's)
- [x] `host/Apple/iOS/App.swift`, `host/Apple/Watch/App.swift`: imports and
      titles; comments in `Base.xcconfig` / `Signing.example.xcconfig` if
      they name the project (➕ the imports become `FliBuddyCore`/`FliBuddyUI`,
      the app structs `FliBuddyiOSApp`/`FliBuddyWatchApp`; `host/README.md`'s
      Apple section names the new targets)
- [x] `Makefile`: `apple-build` uses `FliBuddy.xcodeproj` and the new
      schemes; the `coverage` comment; `.gitignore` entries for the generated
      project (the entry is the glob `host/Apple/*.xcodeproj`, so it needed
      no change)
- [x] `.github/workflows/flipper.yml`: artifact `flibuddy-fap`;
      `host.yml`: coverage step name; `.github/workflows/README.md`
- [x] `tools/tests/test_apple_project.py` asserts the new project, target,
      package and scheme names and bundle ids (`test_mac_info_plist.py`
      follows in Task 3; Task 2 added no TODO) (➕ a new test pins the project
      name, product and display names, the watch bundle id, the "FliBuddy …"
      usage descriptions and `-project FliBuddy.xcodeproj` in `apple-build`)
- [x] run `cd tools && uv run pytest` and `make apple-project apple-build`
      - must pass before task 5 (pytest 321 passed; both simulator builds
      succeed, the iOS app's Info.plist has `com.sokolachko.flibuddy` and
      display name FliBuddy; `make lint` passes)

### Task 5: Stop the tests from littering ~/Library/Preferences
- [x] write the failing test first in `host/Tests/FliBuddyCoreTests/
      SettingsTests.swift`: `SettingsStore` backed by an in-memory key-value
      store saves and loads a full `Settings` round trip without touching
      `UserDefaults` (➕ it also pins the eight stored keys; a second test
      covers `InMemoryKeyValueStore` itself)
- [x] add `public protocol KeyValueStore` in
      `host/Sources/FliBuddyCore/Session/Settings.swift` with the handful of
      accessors `SettingsStore` uses (`string(forKey:)`, `integer`/`object`,
      `set(_:forKey:)`, `removeObject(forKey:)`), make `UserDefaults` conform
      in an extension, and change `SettingsStore.init(defaults:)` to take
      `any KeyValueStore` (default `UserDefaults.standard`); keep the stored
      keys exactly as they are (`string`, `object`, `set(_: Any?)`,
      `removeObject`: `SettingsStore` reads integers through `object`, so
      `integer` was not needed)
- [x] add `InMemoryKeyValueStore` next to `InMemorySecretStore` (test
      support or the library, matching where `InMemorySecretStore` lives)
      (in the library, with `dictionaryRepresentation()` for tests)
- [x] replace `ScratchDefaults` and every `UserDefaults(suiteName:)` in the
      tests with the in-memory store; delete the plist-removal workaround
- [x] keep one test that `UserDefaults` satisfies the protocol, using
      `UserDefaults.standard` with a unique key that the test removes
      afterwards (no suite, so no plist)
- [x] verify: `ls ~/Library/Preferences | grep -c -E
      '(spoolbuddy|flibuddy)-tests-'` prints 0 after `cd host && swift test`
      (the 130 empty plists left by earlier runs were deleted first; 0 after
      `swift test` and again after `make coverage`)
- [x] run `cd host && swift test` - must pass before task 6 (443 passed;
      `make lint` and `make coverage`, FliBuddyCore 88.2 %, pass)

### Task 6: Verify acceptance criteria
- [x] `git grep -n -i spoolbuddy` lists only the "must stay" references from
      Context (Bambuddy route, Bambuddy daemon, README "instead of SpoolBuddy
      hardware", the design spec's API table and Phase 0 findings about the
      daemon, `docs/plans/completed/`, `.ralphex/`); paste the list into the
      plan under a ➕ note
      (➕ hits outside `docs/plans/` and `.ralphex/`, all Bambuddy's:
      `flipper/src/bambu_keys.h:1` (daemon),
      `host/Sources/FliBuddyCore/Bambuddy/BambuddyClient.swift:81` and
      `Models.swift:75` (route),
      `host/Tests/FliBuddyCoreTests/BambuddyClientTests.swift:99,423` (route),
      `host/Tests/FliBuddyCoreTests/TagParserTests.swift:179` (daemon),
      `protocol/PROTOCOL.md:177` (route), `tools/bambu_keys.py:3` (daemon),
      `tools/bridge/bambuddy.py:106` (route),
      `tools/tests/test_bambu_tag.py:128` (daemon),
      `tools/tests/test_bambuddy.py:93` (route),
      `docs/design/ui-brief.md:170` (explains the name), `README.md:4`
      ("instead of SpoolBuddy hardware"), and in the design spec lines 15,
      25, 27, 32, 69, 98, 113-116, 159, 458, 496 (the kiosk, the daemon,
      the API table). ⚠️ still ours, left for Task 7: `README.md` lines 1, 25-27,
      79, 116-117, 121, 133-139, 158, 228 (line 11 is the spec's file name,
      kept); the design spec lines 1, 186, 394, 400, 419, 423, 521, 552
      (line 5 links a completed plan's file name, kept))
- [x] `git grep -n -i -E 'cool\.bambuddy'` finds nothing (⚠️ outside history
      in `docs/plans/completed/` and this plan's own Context it finds
      `README.md:139` and the design spec line 552, both fixed in Task 7)
- [x] run `cd host && swift test`, `make -C flipper/tests test`,
      `cd tools && uv run pytest`, `cd flipper && ufbt`,
      `make apple-project apple-build`, `make coverage` (Swift 443 passed,
      C 235 with 0 failures, pytest 321 passed, `ufbt` builds and APPCHKs
      `flibuddy.fap`, `FliBuddy.xcodeproj` generated and both simulator
      builds succeed, `make coverage` passes; 0 test plists in
      `~/Library/Preferences` afterwards)
- [x] run `make lint` (flipper, host, tools) - all issues must be fixed
      (exit 0; only the Command Line Tools' ld search-path warnings, which
      are not lint findings)
- [x] verify test coverage still meets the 80 % gate (FliBuddyCore 88.3 %,
      tools 96.51 %)

### Task 7: Update documentation
- [x] `README.md`: title `# flipper-flibuddy`, first paragraph introduces
      FliBuddy and keeps the sentence that it replaces Bambuddy's SpoolBuddy
      hardware; add one sentence on the name ("FliBuddy, not SpoolBuddy:
      SpoolBuddy is Bambuddy's own kiosk device"); commands and paths
- [x] `docs/superpowers/specs/2026-10-25-flipper-spoolbuddy-design.md`:
      add a dated note at the top that the project was renamed FliBuddy on
      2026-10-26, and change the references to *our* app (file names,
      targets, hello ids, FAP name, Keychain service); leave the Bambuddy
      kiosk references untouched; keep the file name (it is linked from
      history)
- [x] `AGENTS.md`, `flipper/README.md`, `host/README.md`,
      `.github/workflows/README.md`, `tools/README.md`: final pass for
      leftovers (none found)
- [x] update project knowledge docs if new patterns discovered (AGENTS.md:
      host tests use `InMemoryKeyValueStore`, never `UserDefaults(suiteName:)`)
- [x] ➕ re-run `git grep -n -i spoolbuddy -- ':!docs/plans' ':!.ralphex'`
      and `git grep -n -i -E 'cool\.bambuddy' -- ':!docs/plans'` after the
      doc edits: only the "must stay" hits listed in Task 6 remain (README:
      `SPOOLBUDDY_BLE_REQUIRE_ENCRYPTION` becomes
      `FLIBUDDY_BLE_REQUIRE_ENCRYPTION`, the bundle ids become
      `com.sokolachko.flibuddy`; spec: the tree root, the BLE profile symbol,
      the settings dir, the UI target, the macro, the bundle id)
      (`cool.bambuddy` finds nothing; the spoolbuddy hits are the Task 6
      "must stay" list plus the new name notes: `README.md:13` and the
      spec's rename note, lines 4-5; `README.md:11` and spec line 9 are
      file names, kept. `make test` (Swift 443, C 0 failures, pytest 321)
      and `make lint` pass)

## Technical Details
- Name scheme: `FliBuddy` in prose, titles and Swift type/target names;
  `flibuddy` in file names, the FAP appid, settings dir, hello ids, CI
  artifact, bundle ids; `FLIBUDDY_` for C macros.
- Hello ids are informational on both sides today (neither end validates the
  peer's prefix), so changing them breaks nothing.
- The Mac app's `UserDefaults` domain follows the executable name
  (`FliBuddyMac` after the rename) and the Keychain service string changes;
  both migrations happen on the owner's Mac after the merge, see
  Post-Completion. Keys inside the domain do not change.
- Bundle ids: `com.sokolachko.flibuddy` is a placeholder in the owner's own
  domain; nothing is signed or published yet, so it can still change.
- `KeyValueStore` is deliberately minimal: `SettingsStore` reads strings,
  integers and optional objects and writes or removes single keys.

## Post-Completion
*Items requiring manual intervention or external systems - no checkboxes,
informational only*

**Owner's Mac** (done by the assistant after the merge):
- rename the checkout: `mv ~/src/flipper-spoolbuddy ~/src/flipper-flibuddy`
  (after ralphex has exited; it runs inside the directory)
- migrate the Mac app's settings: `defaults export SpoolBuddyMac - |
  defaults import FliBuddyMac -`, then `defaults delete SpoolBuddyMac`
- migrate the API key: add a generic password with service `FliBuddy`,
  account `bambuddy-api-key` and the same secret; delete the `SpoolBuddy`
  item
- delete any remaining `~/Library/Preferences/spoolbuddy-tests-*.plist`

**Flipper SD card**:
- remove `/ext/apps/Tools/spoolbuddy.fap` and `/ext/apps_data/spoolbuddy/`,
  install `dist/flibuddy.fap` (`cd flipper && ufbt launch`)

**Manual verification**:
- the Flipper lists the app as "FliBuddy" under Tools and its Idle screen
  shows the new title
- the Mac app starts with the migrated URL, catalog entry, core weight and
  API key and connects over USB
- the iPhone app installs with the new display name; the watch app appears
  as its companion
