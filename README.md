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
8. Save entry
9. Select saved account to view username/password
10. Press `Inject`, confirm, and the app HID-types the password only

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

If a legacy plaintext `vault.tsv` exists and no encrypted vault exists, first PIN setup imports it once, saves the encrypted vault, and removes the plaintext file after the encrypted save succeeds.

This is still a small Flipper utility, not a hardened audited password manager. Device compromise, weak PINs, shoulder surfing, debug access, or modified firmware can still expose vault contents.

Recommended hardening before stronger trust claims:

- edit/delete entries from the UI
- stronger/passphrase-based unlock UX
- clearer recovery/export story
- continued clear warnings in release notes

## Community release path

1. Publish the source in a public GitHub repo, e.g. `ck42x/flipper-ck42x-passvault`.
2. Include screenshots or a short demo GIF/video of add → generate → save → confirm HID type.
3. Attach a built `.fap` to a GitHub Release so users do not need a build chain.
4. Post to the Flipper Zero community with clear wording: password tool, PIN-gated encrypted storage, explicit HID typing, and no hardened-manager overclaim.
5. After feedback, keep the catalog copy accurate and avoid stronger trust claims until the UX/security model has been reviewed.
