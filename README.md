# CK42X PassVault for Flipper Zero

A CK42X-branded external Flipper Zero app (`.fap`) that stores, generates, and types passwords from the Flipper after explicit confirmation.

Website: <https://ck42x.com>

## Flow

1. `+ Add New Password`
2. Enter account name
3. Enter username
4. Choose `Generate Password` or `Enter Custom`
5. For generated passwords, choose a preset:
   - Memorable 16+ mix
   - Strict 16+ A/a/0/!
   - Long 20+ passphrase
   - No special char
6. Save entry
7. Select saved account to view username/password
8. Press `Inject`, confirm, and the app HID-types the password only

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

The vault file is stored in app data on the Flipper SD storage as TSV/plaintext for reliability and simplicity. That means anyone with access to the SD card or app data can read the saved passwords. Use it for passwords you are comfortable storing on the device in plaintext.

Recommended hardening before broader trust claims:

- master unlock / PIN gate
- encrypted vault storage
- delete/edit entries from the UI
- clear warning in the README and release notes

## Community release path

1. Publish the source in a public GitHub repo, e.g. `ck42x/flipper-ck42x-passvault`.
2. Include screenshots or a short demo GIF/video of add → generate → save → confirm HID type.
3. Attach a built `.fap` to a GitHub Release so users do not need a build chain.
4. Post to the Flipper Zero community as a transparent v0 prototype: useful field vault, opt-in HID typing, plaintext-storage caveat.
5. After feedback, submit to community app catalogs only if their rules allow credential/password-manager tools and the security caveat is prominent.
