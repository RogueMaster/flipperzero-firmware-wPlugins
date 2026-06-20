# SLI Writer — Magic ISO15693 UID Writer

I made a simple Flipper app to write **magic ISO15693 tags with changeable UID** using `.nfc` files.
I also developed an **Android version** of the app, available as an `.apk`.
Sadly it is impossible to read a tag in privacy mode due to Android limitations.

---

## 🏷️ Supported Tags

### SLIX2 / ISO15693 (Magic UID)

[rfidfriend.com](http://rfidfriend.com) in **normal** mode or [AliExpress — PiSwords](https://aliexpress.com/item/1005006949791537.html) in **special** mode.

These are weird tags that need to be configured back to their original UID to unlock data write.

The old rectangulars tags were working fine with the **normal** mode - always try it first.

Save the original UID (usually the same if you order several tags from the same batch) and use **Write Special**.

> ⚠️ **Back up the original UID — if you lose it, you will never be able to rewrite the tags.**

---

### SLIX-L Tags

[rfidfriend.com](http://rfidfriend.com) in **normal** mode.

---

## ✍️ Write Sequence

### Normal mode

#### 1. Write data blocks
- Command: `WRITE_SINGLE_BLOCK`
- Mode: **non-addressed**
- Flags: `0x02`

#### 2. Write UID (Gen2 vendor commands)
```
02 E0 09 40 <uid_high>   → sets bytes 0–3
02 E0 09 41 <uid_low>    → sets bytes 4–7
```
Equivalent to:
```
proxmark hf 15 csetuid -u <uid> --v2
```

---

### Special mode (TAG-it TI2048 / AliExpress batch)

These tags require the original factory UID to be present before data blocks can be written.

**Step 1 — Restore factory UID** *(skipped automatically if card already has it)*
```
02 E0 09 40 <factory_uid_high>
02 E0 09 41 <factory_uid_low>
```

**Step 2 — Write data blocks (addressed + option flag)**
- Command: `WRITE_SINGLE_BLOCK`
- Mode: **addressed**
- Flags: `0x62` (high data rate `0x02` + addressed `0x20` + option `0x40`)
- Frame format:
```
62 21 <UID[8] LSB-first> <block_num> <data[4]>
```
Example for UID `E0 07 81 2B 4F 10 4B 15`, block 0, data `11 11 11 11`:
```
62 21 15 4B 10 4F 2B 81 07 E0 00 11 11 11 11
```

> **Note:** With Option flag (`0x62`), NXP cards use a 2-phase response. The Flipper will log timeouts after each block write — this is **normal** and does not indicate a failure.

**Step 3 — Write target UID**
```
02 E0 09 40 <target_uid_high>
02 E0 09 41 <target_uid_low>
```

---

### ⚠️ Important

The Gen2 layout command (`0x47`) is **intentionally NOT sent**.
- Not required for most readers
- **Will brick SLIX-L magic cards**

---

## 🔨 Build from source

```bash
# Install ufbt
pip3 install ufbt

# Pull Unleashed SDK
ufbt update --index-url=https://up.unleashedflip.com/directory.json

# Build
cd sli_writer
ufbt build

# Copy .fap to your Flipper SD card
cp /home/$USER/.ufbt/build/sli_writer.fap /path/to/SD/apps/NFC/
```

---

## 🛠️ Debugging

If writing fails:
1. Connect your Flipper via USB
2. Open a serial console (Putty, screen, etc.)
3. Set baud rate to `230400`

### Enable debug logs:
```
> log debug
```
Then look for `[SLI_Writer]` lines.

### Key debug messages
```
iso_send_raw: err=0 rxbytes=2 resp=[XX YY]
```
Command received — card returned an error. `YY` = ISO15693 error code.

```
iso_send_raw: err=6 rxbytes=0
```
No response — likely timeout or CRC issue.

```
block N: err=6 rxbytes=0
```
Normal behavior with Option flag (`0x62`) — not an error.

```
Write block N failed
```
Block write failed after all retries.

---

## Related

- [Unleashed Firmware](https://github.com/DarkFlippers/unleashed-firmware)
- [iso15693_nfc_writer (Unleashed non-catalog)](https://github.com/xMasterX/all-the-plugins/tree/dev/non_catalog_apps/iso15693_nfc_writer)
- [Proxmark3 hf 15 commands](https://github.com/RfidResearchGroup/proxmark3)
- [Toniebox community forum](https://forum.revvox.de)
