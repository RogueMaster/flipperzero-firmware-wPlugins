# Changelog

## v0.4

- Added master PIN setup/unlock gate before vault access.
- Replaced plaintext active vault storage with AES-GCM encrypted vault.pv1.
- Migrates legacy plaintext vault.tsv once after PIN setup, then removes it after encrypted save succeeds.

## v0.3

- Generated passwords are checked against saved vault entries before saving, so the app avoids duplicate generated passwords already in the vault.
- Updated wording to describe PassVault plainly as a password storage, generation, and USB HID typing tool.

## v0.2

- Added About menu with ck42x.com link
- Added CK42X.com branding and Flipper-compatible crowned bee icon
- Added first app catalog screenshot
- Prepared the app for Flipper Apps catalog review

## v0.1

- First public build
- Local plaintext TSV vault in app data
- Password preset generation
- Opt-in USB HID password typing after confirmation
