# GhostBook Technical Manifest

## Application Info

| Property | Value |
|----------|-------|
| App ID | ghostbook |
| Version | 0.6.0 |
| Author | Digi |
| License | MIT |
| Category | NFC |
| Repository | github.com/digitard/ghostbook |

## Requirements

| Requirement | Value |
|-------------|-------|
| Firmware | 0.89.0+ |
| SD Card | 1 MB free |
| Stack Size | 8 KB |

## Source Files

| File | Purpose |
|------|---------|
| ghostbook.c | Main app, crypto, auth |
| ghostbook.h | Types and declarations |
| ghostbook_scenes.c | UI scenes |
| ghostbook_passcode.c | Custom passcode view |
| ghostbook_nfc.c | NFC share/receive |
| application.fam | Build configuration |

## Security Specifications

### Passcode

| Parameter | Value |
|-----------|-------|
| Length | 6-10 buttons |
| Buttons | UP, DOWN, LEFT, RIGHT, OK, BACK |
| Combinations | 46,656 to 60,466,176 |
| Wipe Threshold | 3, 5, 7, or 10 attempts |

### Encryption

| Parameter | Value |
|-----------|-------|
| Key Size | 256 bits |
| Salt Size | 128 bits |
| IV Size | 128 bits |
| Iterations | 10,000 |
| Algorithm | FNV-1a based stream cipher |

### Key Derivation

```
Input: passcode + salt
Step 1: H0 = FNV-1a-256(salt || passcode)
Step 2: For i = 1 to 10000:
          Hi = FNV-1a-256(Hi-1 || salt || i)
Output: 32-byte derived key
```

### File Encryption

```
Input: plaintext + key
Step 1: Generate random 16-byte IV
Step 2: For each 32-byte block:
          keystream = FNV-1a-256(key || IV || block_index)
          ciphertext = plaintext XOR keystream
Output: IV || ciphertext
```

## Data Structures

### GhostAuth
```c
typedef struct {
    uint8_t salt[16];
    uint8_t passcode_hash[32];
    uint8_t failed_attempts;
    uint8_t passcode_length;
    uint8_t max_attempts;
    bool initialized;
} GhostAuth;
```

### GhostCard
```c
typedef struct {
    char handle[32];
    char name[48];
    char email[64];
    char discord[40];
    char signal[20];
    char telegram[40];
    char notes[128];
    GhostFlair flair;
} GhostCard;
```

## NFC Protocol

### Technology
- NTAG215 emulation (MfUltralight)
- 504 bytes user data (pages 4-129)
- Compatible with standard NFC readers

### Data Format
```
Bytes 0-3:   "GHST" (magic)
Byte 4:      Version (1)
Bytes 5-6:   Payload length (big endian)
Byte 7:      Reserved
Bytes 8+:    Card data (compact format)
```

### Compact Card Format
```
h=handle
n=name
e=email
d=discord
s=signal
g=telegram
t=notes
f=flair
```

## File Locations

| File | Path | Encrypted |
|------|------|-----------|
| Auth | apps_data/ghostbook/.auth | No (hash only) |
| Profile | apps_data/ghostbook/profile.enc | Yes |
| Contacts | apps_data/ghostbook/contacts/*.enc | Yes |
| Exports | /ext/*.vcf, *.csv | No |

## Scene Flow

```
App Start
    │
    ├─[Auth exists]─→ Lock Screen ─→ Main Menu
    │
    └─[No auth]─→ Security Setup ─→ Wipe Setup ─→ Passcode Setup ─→ Confirm ─→ Main Menu

Main Menu
    ├── View Card
    ├── Edit Profile ─→ Field Editors
    ├── Tap Share ─→ NFC Listener
    ├── Tap Receive ─→ NFC Poller
    ├── Contacts ─→ Contact View
    ├── Export
    └── About

Failed Auth
    └─[Attempts >= Max]─→ Wipe Animation ─→ Exit
```

## API Functions

### Crypto
```c
void ghost_derive_key(passcode, length, salt, key_out);
bool ghost_encrypt_data(key, plain, plain_len, cipher, cipher_len);
bool ghost_decrypt_data(key, cipher, cipher_len, plain, plain_len);
```

### Auth
```c
bool ghost_auth_load(app);
bool ghost_auth_save(app);
bool ghost_auth_verify(app, passcode);
void ghost_auth_set(app, passcode, length, max_attempts);
void ghost_wipe_all_data(app);
```

### NFC
```c
void ghost_nfc_share_start(app);
void ghost_nfc_share_stop(app);
void ghost_nfc_receive_start(app);
void ghost_nfc_receive_stop(app);
```

## Changelog

### 0.6.0
- NFC tap-to-share (NTAG215 emulation)
- NFC tap-to-receive
- Configurable wipe threshold (3/5/7/10)
- Telegram field added
- Melting ghost animation on wipe
- UI cleanup (cleaner headers, ghost branding)
- Clean documentation

### 0.5.1
- Variable passcode length (6-10)
- Security level selection
- 10,000 iteration key stretching
- SDK compatibility fixes

### 0.5.0
- Initial release
- Encrypted storage
- Fixed 6-button passcode
- 5-attempt auto-wipe
- vCard/CSV export

## Known Limitations

1. No passcode change without data wipe
2. Old contacts unreadable after reset
3. Export files are plaintext
4. Single profile per installation

## Security Notes

**Strengths:**
- Passcode never stored
- Auto-wipe prevents on-device brute force
- Unique salt per installation
- Random IV per encryption

**Limitations:**
- Custom crypto (not audited)
- Offline attacks possible with SD card access
- Passcode length visible in auth file size
- No perfect forward secrecy
