# VaultFlipper user guide

*Deutsche Fassung: [GUIDE.de.md](GUIDE.de.md)*

This guide takes you from zero to typing your first password with the Flipper, and covers everyday use, several computers, updates and troubleshooting.

**Contents**

1. [What you need](#1-what-you-need)
2. [Install the Flipper app](#2-install-the-flipper-app)
3. [Install the companion](#3-install-the-companion)
4. [First setup](#4-first-setup)
5. [Using VaultFlipper](#5-using-vaultflipper)
6. [Typing on different computers](#6-typing-on-different-computers)
7. [Keeping the Flipper up to date](#7-keeping-the-flipper-up-to-date)
8. [Accounts, organizations and filters](#8-accounts-organizations-and-filters)
9. [Security in short](#9-security-in-short)
10. [Troubleshooting](#10-troubleshooting)
11. [Removing VaultFlipper](#11-removing-vaultflipper)

---

## 1. What you need

| | |
| --- | --- |
| Flipper Zero | official firmware 1.5 or newer ([update](https://flipperzero.one/update)) |
| Computer | macOS, Linux or Windows with a USB cable to the Flipper |
| Python | 3.11 or newer, plus [uv](https://docs.astral.sh/uv/getting-started/installation/) |
| Bitwarden CLI | `bw` – `brew install bitwarden-cli` or `npm install -g @bitwarden/cli` |
| A vault | bitwarden.com, bitwarden.eu or a self-hosted Bitwarden / Vaultwarden server |

## 2. Install the Flipper app

**From a release (recommended).** Download `vaultflipper.fap` from the [releases page](https://github.com/its-sven/vaultflipper/releases). Open qFlipper, go to the file manager and copy it to `SD Card/apps/Tools/`.

**From source.**

```bash
git clone https://github.com/its-sven/vaultflipper.git
cd vaultflipper
make launch        # builds with ufbt, installs and starts the app
```

Open **Apps ▸ Tools ▸ VaultFlipper** once. Without a vault it shows a **Device ID** such as `C74D-75C4` – you will compare it in step 4.

## 3. Install the companion

```bash
uv tool install "git+https://github.com/its-sven/vaultflipper#subdirectory=companion"
vaultflipper --version
```

`pipx install "git+https://github.com/its-sven/vaultflipper#subdirectory=companion"` works too.

## 4. First setup

Close qFlipper and the VaultFlipper app – only one program can use the Flipper's USB port – then run:

```bash
vaultflipper setup
```

The wizard has five steps:

| Step | What happens |
| --- | --- |
| 1 · Bitwarden CLI | checks that `bw` is installed |
| 2 · Accounts | asks for a name (shown on the Flipper), where the vault is hosted and how you sign in, then signs in with `bw` |
| 3 · Pairing | reads the Flipper's device key over USB and shows its Device ID – it must match the one on the Flipper |
| 4 · Keyboard layout | detects the layout of this computer, e.g. `de-DE-mac` |
| 5 · Sync | unlocks every account, asks you for a **PIN**, encrypts and sends the vault |

**Signing in.** Pick *Email and master password*, *Single sign-on (SSO)* – your browser opens the company sign-in – or *API key*. You always type secrets straight into `bw` or your browser; VaultFlipper never sees your master password.

**Choosing the PIN.** Press the arrow keys on your computer (or type `U D L R`), at least 8 times, then Enter. The same sequence unlocks the Flipper. Use **12 or more** presses – see [Security](#9-security-in-short).

## 5. Using VaultFlipper

### Unlock

Open **Apps ▸ Tools ▸ VaultFlipper** and press your arrow sequence, then **OK**. **Back** deletes the last press. Unlocking takes about a second.

### Find a login

| Screen | What you see |
| --- | --- |
| Home | your accounts with their number of logins and folders, then *2FA codes*, *Search*, *Settings*, *Lock* |
| Account | its folders and collections; large folders are split, e.g. *Mail* · `Am – Ko` |
| Folder | the logins, each with its username (or the website when there is none) |
| Search | type part of a name, username or URL; up to 50 matches across all accounts, with usernames |

![Folder with usernames](../flipper/vaultflipper/docs/screenshots/4-entries.png)

In menus the selected row opens and shows details; the next row peeks in at the bottom when there is more.

### The entry card

![Entry card](../flipper/vaultflipper/docs/screenshots/5-card.png)

| Key | Action |
| --- | --- |
| **OK** | type username, Tab, password, Enter |
| **◀ Left** | type the username |
| **▶ Right** | type the password |
| **▲ Up** | show or hide the password |
| **▼ Down** | more: login without Enter, single fields, all details |
| **Back** | back to the list |

Click into the login field on your computer first; the Flipper types wherever the cursor is. The LED blinks green when typing is done.

### 2FA codes

When logins in your vault have a 2FA secret (TOTP), the home screen shows **2FA codes**. The list holds every such login from all accounts, sorted by name, with its current code and username. The line under the title shrinks and the seconds on the right count down until the codes change.

![2FA codes](../flipper/vaultflipper/docs/screenshots/8-codes.png)

Open one for a big code with a countdown – easy to read off when you type it on a phone yourself.

![2FA code](../flipper/vaultflipper/docs/screenshots/9-code.png)

| Key | Action |
| --- | --- |
| **OK** | type the code (no Enter) |
| **◀ ▶** | previous or next login |
| **Back** | back to the list |

From an entry card the same view is under **▼ ▸ Show 2FA code**. SHA-1, SHA-256, SHA-512 and Steam Guard codes work; counter-based (HOTP) secrets are left out at sync.

**Clock.** Codes depend on the time. The Flipper keeps local time, set by qFlipper or the Flipper mobile app; `vaultflipper sync` measures how far it is from UTC and stores that on the Flipper. After a daylight saving change, sync again or pick the right zone under *Settings ▸ Time zone*.

### Lock

Hold **Back** anywhere, choose **Lock** on the home screen, or wait two minutes. Leaving the app locks it as well. After locking, the Flipper is a normal USB device again.

## 6. Typing on different computers

While unlocked, the Flipper is a USB keyboard called **VaultFlipper Keyboard** – like a security key. A keyboard sends key *positions*, so the Flipper must use the layout of the computer it types into.

**Pick a layout:** *Settings ▸ Keyboard layout*. All BadUSB layouts are available, plus `de-DE-mac` for German Macs. The choice is remembered.

**Check it:** run `vaultflipper typetest`, keep the terminal in front and choose *Settings ▸ Typing test* on the Flipper. It reports every character that arrives wrong.

**Mac – first time only:** macOS opens the *Keyboard Setup Assistant* when the Flipper connects. Click *Continue*, then choose *Settings ▸ Mac keyboard setup* on the Flipper and press **Send key** twice. Confirm the type macOS suggests and click *Done*. macOS remembers it.

**Phones:** connect with a USB-C/OTG adapter. On Android set the physical keyboard layout in the system settings; iPhones with USB-C accept the Flipper as a keyboard.

**Characters that cannot be typed:** umlauts, `€` and other non-ASCII characters have no fixed key position. The companion warns about logins that contain them; the Flipper leaves them out and then shows *Not all typed* with the number of missing characters.

## 7. Keeping the Flipper up to date

After you change logins in Bitwarden, lock the Flipper app and run:

```bash
vaultflipper sync
```

`vaultflipper status` shows whether `bw` works, which accounts are signed in and whether the connected Flipper is the paired one. If VaultFlipper ever shows a different Device ID (new Flipper, enclave reset), run `vaultflipper pair` again.

## 8. Accounts, organizations and filters

```bash
vaultflipper account list
vaultflipper account add Work --server https://vault.example.com --login sso
vaultflipper account add Family --server https://vault.bitwarden.eu --collection "Family/Streaming"
vaultflipper login Work --sso
vaultflipper account remove Old
```

| Option | Effect |
| --- | --- |
| `--folder NAME` | only these personal folders (repeatable) |
| `--collection Org/Collection` | only these organization collections (repeatable) |
| `--favorites-only` | only favorites |
| `--no-orgs` | skip shared organization items |
| `--no-totp` | leave out 2FA secrets – no codes on the Flipper |

With only `--folder`, no organization items are synced; with only `--collection`, no personal items. Give both to combine them. Items shared with several of your accounts appear only once. A vault holds roughly 20,000 logins in a few folders, or about 350 folders; the folder directory (24 KB) is the limit.

**Single sign-on:** `vaultflipper login NAME --sso` opens your company sign-in; `bw` then asks for the master password to decrypt the vault. On Vaultwarden the SSO identifier is filled in automatically. Accounts without any master password (Bitwarden *trusted devices* only) cannot be unlocked by the Bitwarden CLI.

## 9. Security in short

- The vault file only opens on **your** Flipper and with **your** PIN: the key combines a secret from the Flipper's crypto enclave with your PIN (Argon2id).
- Only the folder you open is decrypted, and only in RAM. Locking wipes it.
- From the 3rd wrong PIN the app slows down; after 10 it deletes the vault.
- The Flipper has no secure boot: someone with your device can flash other firmware and guess PINs on the device at about one per second. A 12-press PIN makes that take months. If you lose the Flipper, change the passwords that matter most.
- Only type into computers you trust – a keyboard can be read by the computer it is plugged into.
- 2FA codes put the second factor on the same device as the password, as Bitwarden itself does when it stores TOTP. If you want them apart, add the account with `--no-totp`.

Details: [SECURITY.md](../SECURITY.md) and [FORMAT.md](FORMAT.md).

## 10. Troubleshooting

| Problem | Solution |
| --- | --- |
| *No Flipper found* | close qFlipper and lock or quit VaultFlipper, then try again |
| *Account … is not logged in* | `vaultflipper login NAME` (add `--sso` for company sign-in) |
| Typing goes to the wrong window | click into the target field first; VMs (VMware, Parallels) may grab new USB keyboards |
| Wrong characters | pick another layout and run `vaultflipper typetest` |
| *No USB host* on the Flipper | connect the USB cable to a computer or phone, not a charger |
| *Vault invalid – sync again* | the file is from an older version or damaged: `vaultflipper sync` |
| *Out of memory* | close other apps; restart the Flipper |
| 2FA codes are rejected | check the zone under *Settings ▸ Time zone*, or run `vaultflipper sync` to measure the clock again |
| *Clock not set* | connect the Flipper to qFlipper once (it sets the clock), then `vaultflipper sync` |
| Forgot the PIN | sync again from your computer and choose a new PIN |

## 11. Removing VaultFlipper

- Flipper: delete `apps/Tools/vaultflipper.fap` and the folder `apps_data/vaultflipper/`.
- Computer: `uv tool uninstall vaultflipper` and delete `~/.config/vaultflipper/` (signs out of all `bw` sessions stored there).
