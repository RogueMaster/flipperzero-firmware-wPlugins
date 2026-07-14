# 👻 GhostBook

**Encrypted NFC Contact Sharing for Flipper Zero**

Share your contact info with a tap. Protected by passcode. Auto-wipes on failed attempts.

## Features

- **NFC Tap-to-Share** — Emulates NTAG215, works with other Flippers
- **Variable Passcode** — 6-10 button combinations (46K to 60M possibilities)
- **Configurable Auto-Wipe** — 3, 5, 7, or 10 attempts before destruction
- **AES-class Encryption** — 256-bit keys with 10,000 iteration stretching
- **Animated Wipe Screen** — Dramatic melting ghost sequence

## Installation

Download `ghostbook.fap` from Releases and copy to:
```
SD Card/apps/NFC/ghostbook.fap
```

Or build from source:
```bash
cd ghostbook
ufbt
ufbt launch
```

## First-Time Setup

1. **Security Level** — Choose passcode length (6-10 buttons)
2. **Wipe Threshold** — Choose attempts before auto-wipe (3/5/7/10)
3. **Create Passcode** — Enter button sequence (UP/DOWN/LEFT/RIGHT/OK/BACK)
4. **Confirm Passcode** — Enter again to verify

## Usage

### Sharing Your Card
1. Main Menu → **Tap Share**
2. Hold Flipper near another device
3. Cyan LED blinks while broadcasting
4. Press BACK to stop

### Receiving Cards
1. Main Menu → **Tap Receive**
2. Hold near a sharing Flipper
3. Card auto-saves on successful read
4. Press BACK to cancel

### Managing Contacts
- **View Card** — See your profile
- **Edit Profile** — Modify your info
- **Contacts** — Browse received cards
- **Export** — vCard or CSV format

## Security

| Setting | Options | Notes |
|---------|---------|-------|
| Passcode Length | 6-10 buttons | More = harder to crack |
| Wipe Threshold | 3/5/7/10 attempts | Fewer = more secure |
| Key Stretching | 10,000 iterations | Slows offline attacks |
| Encryption | 256-bit derived key | Per-file random IV |

### What Happens on Wipe

After exceeding failed attempts:
1. Animated melting ghost sequence
2. All data permanently deleted
3. Auth, profile, and contacts destroyed
4. No recovery possible

## Card Fields

| Field | Max Length |
|-------|------------|
| @Handle | 32 chars |
| Name | 48 chars |
| Email | 64 chars |
| Discord | 40 chars |
| Signal | 20 chars |
| Telegram | 40 chars |
| Notes | 128 chars |
| Flair | 8 options |

## File Locations

```
apps_data/ghostbook/
├── .auth              # Passcode hash, salt, settings
├── profile.enc        # Your encrypted card
└── contacts/          # Received cards (encrypted)
    └── *.enc
```

## Version

**0.6.0** — NFC sharing, configurable wipe, Telegram field, UI cleanup

## Author

**Digi** — [github.com/digitard](https://github.com/digitard)

## License

MIT
