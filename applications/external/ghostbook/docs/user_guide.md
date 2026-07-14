# GhostBook User Guide

## Table of Contents

1. [Installation](#installation)
2. [First-Time Setup](#first-time-setup)
3. [Daily Use](#daily-use)
4. [NFC Sharing](#nfc-sharing)
5. [Managing Contacts](#managing-contacts)
6. [Exporting Data](#exporting-data)
7. [Security Settings](#security-settings)
8. [Troubleshooting](#troubleshooting)

---

## Installation

### From Release
1. Download `ghostbook.fap` from GitHub Releases
2. Connect Flipper Zero to computer
3. Copy file to: `SD Card/apps/NFC/ghostbook.fap`
4. Launch from: Apps → NFC → GhostBook

### From Source
```bash
git clone https://github.com/digitard/ghostbook
cd ghostbook
ufbt
ufbt launch
```

---

## First-Time Setup

### Step 1: Security Level

Choose how many buttons in your passcode:

| Level | Buttons | Combinations | Offline Crack Time |
|-------|---------|--------------|-------------------|
| Standard | 6 | 46,656 | Minutes |
| Enhanced | 7 | 279,936 | Hours |
| Enhanced+ | 8 | 1,679,616 | Days |
| Paranoid | 9 | 10,077,696 | Weeks |
| Paranoid+ | 10 | 60,466,176 | Months |

### Step 2: Wipe Threshold

Choose how many wrong attempts before auto-wipe:

| Setting | Attempts | Best For |
|---------|----------|----------|
| Maximum | 3 | High security, careful users |
| Recommended | 5 | Most users (default) |
| Relaxed | 7 | Longer passcodes, some typos OK |
| Lenient | 10 | Very long passcodes |

### Step 3: Create Passcode

Use any combination of buttons:
- UP, DOWN, LEFT, RIGHT, OK, BACK

Each press fills one slot. Example for 6-button:
```
_ _ _ _ _ _  →  * _ _ _ _ _  →  * * * * * *
```

### Step 4: Confirm Passcode

Enter the exact same sequence again. If they match, you're in.

**Important:** There is NO password recovery. Write it down somewhere safe.

---

## Daily Use

### Unlocking

1. Launch GhostBook
2. Enter your passcode
3. Main menu appears

### Wrong Passcode Warning

Each wrong attempt shows remaining tries:
```
WRONG CODE
3 attempts remaining
All data will be WIPED
```

### Auto-Wipe

Exceed your threshold and watch the ghost melt:
```
!! I'M MELTING !!
    (o_o)
    /| |\
```

All data permanently destroyed. No recovery.

---

## NFC Sharing

### Sharing Your Card

1. Select **Tap Share** from main menu
2. Screen shows "SHARING..." with cyan LED
3. Hold Flipper back near receiving device
4. LED blinks when scanned
5. Press BACK to stop sharing

Your Flipper emulates an NTAG215 tag containing your card data.

### Receiving Cards

1. Select **Tap Receive** from main menu
2. Screen shows "SCANNING..." with magenta LED
3. Hold near a sharing Flipper
4. Success: "CARD SAVED!" popup
5. Card added to contacts automatically

### Tips

- Hold Flippers back-to-back (NFC antennas are on the back)
- Keep steady for 1-2 seconds
- Sharing continues until you press BACK
- Receiving stops after one successful read

---

## Managing Contacts

### View Your Card

Main Menu → **View Card**

Shows your complete profile with flair icon.

### Edit Profile

Main Menu → **Edit Profile**

Fields:
- **@Handle** — Required, your username
- **Name** — Full name
- **Email** — Contact email
- **Discord** — Discord tag
- **Signal** — Phone number
- **Telegram** — Telegram username
- **Notes** — Anything else
- **Flair** — ASCII art icon

### Browse Contacts

Main Menu → **Contacts**

Lists all received cards. Select to view details.

### Flair Options

| Icon | Name |
|------|------|
| (none) | None |
| (x_x) | Skull |
| (o_o) | Ghost |
| [01] | Matrix |
| ~#!~ | Glitch |
| [=] | Lock |
| (O) | Eye |
| /!\\ | Bolt |
| (*) | Star |

---

## Exporting Data

Main Menu → **Export**

### Options

| Export | Output File | Contents |
|--------|-------------|----------|
| My Card → vCard | ghostbook_me.vcf | Your profile |
| Contacts → vCard | ghostbook_all.vcf | All contacts |
| All → CSV | ghostbook.csv | Everything |

### Security Warning

**Exported files are NOT encrypted!**

They're saved to SD card root in plaintext. Transfer them securely and delete from SD card afterward.

### vCard Format

Standard format, importable to phones and email clients:
```
BEGIN:VCARD
VERSION:3.0
FN:Your Name
NICKNAME:@handle
EMAIL:you@email.com
END:VCARD
```

---

## Security Settings

### What's Stored

| Data | Location | Protection |
|------|----------|------------|
| Passcode | .auth | Never stored (only hash) |
| Salt | .auth | Random 16 bytes |
| Settings | .auth | Passcode length, wipe threshold |
| Profile | profile.enc | Encrypted |
| Contacts | contacts/*.enc | Encrypted individually |

### Encryption Details

- **Key Derivation:** 10,000 iterations of FNV-1a hashing
- **Key Size:** 256 bits
- **IV:** Random 16 bytes per file
- **Algorithm:** Stream cipher with derived keystream

### Changing Settings

To change passcode or security settings:
1. Delete `apps_data/ghostbook/` folder
2. Restart app
3. Set up fresh

This wipes all data. There's no way to change settings without reset.

---

## Troubleshooting

### Can't unlock

- Double-check button sequence
- Remember: order matters, all 6 buttons are valid
- Each wrong attempt counts toward wipe

### Forgot passcode

No recovery exists. After max attempts:
1. Data wipes automatically
2. Or manually delete: `apps_data/ghostbook/`
3. Start fresh with new passcode

### NFC not working

- Make sure both Flippers are in correct mode (Share/Receive)
- Hold backs together (antenna location)
- Keep steady for 1-2 seconds
- Check that sharing Flipper shows "SHARING..."

### Contacts not showing

- May be encrypted with different key
- If you reset passcode, old contacts are unreadable
- This is expected security behavior

### Export files empty

- Ensure profile has data
- Check SD card has space
- Files go to SD card root

---

## Quick Reference

### Button Layout
```
       [UP]
[LEFT] [OK] [RIGHT]
      [DOWN]
      
      [BACK]
```

### Main Menu
```
> View Card
* Edit Profile
< > Tap Share
< < Tap Receive
# Contacts
^ Export
? About
```

### File Locations
```
SD Card/
├── apps/NFC/ghostbook.fap
├── apps_data/ghostbook/
│   ├── .auth
│   ├── profile.enc
│   └── contacts/*.enc
└── ghostbook_*.vcf/csv (exports)
```

---

*GhostBook v0.6.0 — Trust no one. Leave nothing.*
