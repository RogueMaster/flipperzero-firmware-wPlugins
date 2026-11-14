# Release acceptance

Unchecked means not verified, not optional. Attach evidence to a release issue.

## Engineering

- [ ] Swift unit tests and independent v4 engine/wire contract tests pass in CI.
- [x] Local Swift tests and independent C sanitizer/fuzz/cross-language wire fixtures pass.
- [ ] Fresh 0.5.2 sandboxed UI tests: create/edit/review, file import and disconnected run disabled.
- [ ] Fresh 0.5.2 unsigned universal Store archive validated.
- [ ] Sandboxed release checks: published/unpublished/offline states, correct pinned links and network-client entitlement.
- [ ] Signed Store archive validated.
- [ ] Sandbox entitlements, Bluetooth description, icon and privacy resources checked.
- [ ] JSON import/export and backup recovery tested with real file pickers.
- [ ] Light/dark, narrow window, long names, large text and VoiceOver checked.
- [ ] Clean install and upgrade preserve definitions and leave corrupt files intact.

## Physical device matrix

- [ ] Fresh 0.5.2 device run: exact small and SD-backed captures; completion and retained connection.
- [ ] Fresh 0.5.2 parameters, field navigation, Stop during delay and reconnect without replay.
- [ ] Flipper Info navigation, patch version, physical QR scan and busy-state Stop verified on device.
- [ ] Movement never drags after another HID app or Stop/restart; keyboard and mouse buttons release on every interruption.
- [ ] Verify Turkish QWERTY PC punctuation/dead-key correction with a live full-ASCII capture (local translation/host checks pass).
- [ ] Verify native Turkish letters with the updated Mac/FAP pair; local checks do not close the stopped live test.
- [ ] First pairing shows expected two-device confirmation; wrong/unpaired Mac denied.
- [ ] Reconnect works, with no replay and no stale queued output.
- [ ] macOS, Windows and Linux targets: supported shortcuts and US output verified.
- [ ] Turkish Q asset present/missing/wrong layout cases verified.
- [ ] Literal brackets, all printable ASCII, Turkish letters, tabs, LF/CRLF and rejection of unsupported Unicode.
- [ ] Small, 4096-byte and maximum/near-maximum typed programs (SD path).
- [ ] Stop during layout, transfer, USB wait, key combination and long delay.
- [ ] USB unplug, BLE loss, Bluetooth off, Mac sleep/quit and Flipper power cycle.
- [ ] Hours-long standalone mouse run with visible timer; physical Stop while offline.
- [ ] Save without output; SD snapshots survive restart and require confirmed local Run.
- [ ] USB loss stops standalone execution without automatic restart; reconnect restores remote Stop.
- [ ] SD full/corruption/interrupted replacement, 20-slot limit and confirmed Delete.
- [ ] No stuck modifiers after interruptions; target inspected before retries.
- [ ] App remains connected after successful runs; completion is HID submission only.
- [ ] Other Flipper app/official companion active: useful error, no accidental execution.

## Distribution

- [ ] Rights/licensing review recorded and recipient/source obligations satisfied.
- [ ] Owner confirms seller/team, bundle ID, app name, SKU and price/territories.
- [ ] Account Holder confirms paid agreements, tax/banking and required declarations.
- [ ] Permanent privacy/support/source URLs and immutable source release available.
- [ ] Encryption, age rating, privacy and accessibility declarations audited.
- [ ] Correct Store signing/profile/build number; validation and upload successful.
- [ ] Internal beta feedback recorded; external beta review completed if applicable.
- [ ] Physical review video and reviewer instructions verified accessible.
- [ ] App Review approved; owner approves manual release.
- [ ] Store-installed app tested for receipt/install, Bluetooth permission and update.

No beta feedback, hardware result, legal clearance or Apple approval can be
generated as a substitute for completing these checks.
