# CK42X PassVault for Flipper Zero

A CK42X-branded external Flipper Zero app (`.fap`) that stores, generates, and types passwords from the Flipper after explicit confirmation.

Website: <https://ck42x.com>

## Flow

1. First launch: set a master PIN. Existing legacy `vault.tsv` data is migrated into encrypted storage after setup.
2. Later launches: unlock with the master PIN.
3. `+ Add New Password`
4. Enter account name
5. Enter username
6. Choose `Generate Password` or `Enter Custom`
7. For generated passwords, choose a preset:
   - Memorable 16+ mix
   - Strict 16+ A/a/0/!
   - Long 20+ passphrase
   - No special char
8. Confirm the entry with `Enter`
9. The saved entry opens directly on its Inject screen
10. Press `Inject`, confirm, and the app HID-types the password only
11. After typing, PassVault returns to the same Inject screen so the password can be injected again
12. `FIDO2 Security Key` starts the experimental USB runtime; Back or Stop
    restores the previous USB mode. Credential creation and assertion require
    an explicit on-device Approve/Deny decision.

## FIDO2 security key

The experimental v0.4.3 runtime supports FIDO 2.0 WebAuthn with ES256,
non-resident credentials, allow lists, and physical user presence.

1. Unlock PassVault with the master PIN.
2. Select `FIDO2 Security Key` and wait for `Ready / Waiting`.
3. On the website, choose its security-key or passkey registration/sign-in flow.
4. Approve or deny the ceremony with the Flipper controls.
5. Exit FIDO2 mode with Back when finished. PassVault restores the previous USB
   configuration.

FIDO credentials are limited to 20 records and remain separate from the
password vault. Client PIN, resident credentials, and user verification are not
implemented. Keep another authenticator and account recovery method for every
account.

Browser installer, release notes, and operating guide:
<https://www.ck42x.com/tools/ck42x-passvault/uploader>

## macOS keyboard setup

The development `v0.4.4-dev` candidate adds an explicit one-time setup action for macOS. It keeps the standard Flipper USB HID identity instead of impersonating an Apple keyboard.

1. Connect Flipper Zero directly to the Mac over USB.
2. Unlock PassVault and choose `macOS Keyboard Setup`.
3. If Keyboard Setup Assistant is not already open, launch it from macOS Keyboard settings or open `/System/Library/CoreServices/KeyboardSetupAssistant.app`.
4. Click Continue on the Mac.
5. Press `Send` on Flipper. PassVault sends the ANSI-position keys `Z`, then `/`.
6. Choose `ANSI` on the Mac when prompted.
7. Press Back on Flipper. PassVault keeps HID active for this app session, preventing another keyboard-enumeration cycle before each injection.
8. Focus a password field and use the normal Inject action as many times as needed.
9. Exit PassVault when finished. The original USB mode is restored on app exit.

This setup is intended for a US ANSI keyboard layout. Password symbols can be wrong when the Mac input source uses a different physical layout because USB HID sends key positions, not Unicode characters. Disconnecting USB, exiting PassVault, or switching away from HID creates a new enumeration event; reopen Mac mode before the next injection session if necessary.

## Branding

The app icon is a Flipper-compatible 10x10 monochrome simplification of the CK42X crowned bee mark from `ck42x.com`. The full source logo reference is preserved in `ck42x_website_bee_crown.png` for provenance.

The app also includes an `About / ck42x.com` menu item so users can find CK42X after installing the `.fap`.

## Build

From this directory:

```bash
/home/x3y5x/.local/share/venvs/ufbt/bin/ufbt
```

Output:

```text
dist/ck42x_passvault.fap
```

## Install / launch when Flipper is reachable over USB

From WSL if the Flipper is visible there:

```bash
/home/x3y5x/.local/share/venvs/ufbt/bin/ufbt launch
```

From Windows HERM when the Flipper is physically connected to HERM:

```powershell
C:\Users\lordb\.hermes\venvs\ufbt\Scripts\ufbt.exe launch FLIP_PORT=COM9
```

Adjust `COM9` if Windows assigns a different Flipper CDC port.

If USB automation is unavailable, copy `dist/ck42x_passvault.fap` to the Flipper SD card under `/ext/apps/Tools/` with qFlipper or another mounted SD path.

## Security note

Generated passwords use the Flipper RNG and the app checks generated passwords against saved entries before saving, so it will not intentionally create a duplicate generated password already in the vault.

v0.4 stores the active vault in app data as AES-GCM encrypted `vault.pv1` and gates vault access behind a master PIN. The key is derived in-app from the PIN and a per-vault random salt using a compact SHA-256 KDF. A fresh random AES-GCM nonce is used on each save.

FIDO2 credentials are kept separately in AES-GCM encrypted `fido2.pv1`, using
distinct file magic/AAD and the unlocked vault key. The experimental store is
bounded to 20 non-resident credentials and does not change `vault.pv1`.

If a legacy plaintext `vault.tsv` exists and no encrypted vault exists, first PIN setup imports it once, saves the encrypted vault, and removes the plaintext file after the encrypted save succeeds.

This is still a small Flipper utility, not a hardened audited password manager. Device compromise, weak PINs, shoulder surfing, debug access, or modified firmware can still expose vault contents.

The `v0.4.3` FIDO2 runtime is experimental. Physical testing on an Oaspote
Flipper Zero proved CTAP2 GetInfo, browser WebAuthn registration and
authentication in Chromium, credential persistence across a FIDO-mode restart,
ES256 signature verification, signature-counter advancement, and restoration of
the prior USB serial mode after exit. This is not a FIDO conformance or security
certification.

Flipper Zero has no secure element for these credentials. This build is not
equivalent to a hardened YubiKey, Titan, or other certified authenticator. Do not
use it as your only authenticator or recovery method; retain a backup security
key and account recovery path. Client PIN, resident credentials, and user
verification are intentionally unsupported.

