# Changelog

## v0.4.6

- Removed the 20-entry password vault cap. Saved accounts now grow with available Flipper memory and SD space.
- Existing encrypted vault.pv1 files keep the same format.

## v0.4.5

- Added editing for saved Name, Username, and Password fields with prefilled values and confirmation.
- Added Keep Existing, Generate Password, and Enter Custom choices while editing.
- Restores the original entry in memory if an encrypted save fails.
- Added explicit macOS ANSI keyboard setup using the standard Flipper HID identity.
- Keeps HID active after successful setup so repeated injections do not re-enumerate the keyboard.
- Restores the previous USB mode when PassVault exits or setup is cancelled.

## v0.4.3

- Added a clean-room FIDO 2.0 authenticator with CTAPHID, CTAP2, and ES256.
- Added non-resident WebAuthn credentials with physical Approve or Deny user presence.
- Added a separate AES-GCM encrypted FIDO credential store with persistence, allow-list matching, signature counters, and cancellation.
- Restores the previous USB configuration when FIDO2 mode exits.
- Improved new-entry navigation so saved entries open on Inject and return there after typing.
- Verified Chromium registration and authentication, restart persistence, ES256 signatures, and physical USB lifecycle behavior.

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
