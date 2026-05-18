# FreeStyle Libre NFC Serial Number Decoding

## Background

FreeStyle Libre CGM sensors (Libre 1/2/3) use ISO 15693 NFC tags.
The 8-byte UID encodes the human-readable serial number printed on the sensor.

## UID Structure

| Byte | Value | Meaning |
|------|-------|---------|
| 0 | `E0` | Fixed — ISO 15693 tag identifier |
| 1 | `7A` | Abbott Diabetes Care manufacturer code |
| 2–7 | variable | Encoded serial number (6 bytes) |

## Decoding Algorithm

1. **Extract bytes 2–7** from the UID in their original order (do NOT reverse).
2. **Interpret as a 48-bit big-endian integer.**
3. **Extract 9 groups of 5 bits**, starting from bit 44 down to bit 0 (MSB-first).
   - Skip the top 3 bits (bits 47:45) — these are always 0 for Abbott sensors.
4. **Look up each 5-bit value** in the character table below.
5. **Append** each character left-to-right to build the 9-character serial.

## Character Lookup Table

```
Index: 0  1  2  3  4  5  6  7  8  9  10 11 12 13 14 15
Char:  0  1  2  3  4  5  6  7  8  9  A  C  D  E  F  G

Index: 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31
Char:  H  J  K  L  M  N  P  Q  R  T  U  V  W  X  Y  Z
```

Note: Letters B, I, O, S are omitted to avoid visual ambiguity.

## Reference Implementation (Python)

```python
LOOKUP = "0123456789ACDEFGHJKLMNPQRTUVWXYZ"

def decode_libre_serial(uid_hex: str) -> str:
    """
    Decode a FreeStyle Libre NFC UID to its ASCII serial number.

    Args:
        uid_hex: UID as colon-separated hex bytes, e.g. "E0:7A:00:C2:1C:C6:45:11"

    Returns:
        9-character serial number string, e.g. "0R8FDDJ8J"
    """
    bytes_ = bytes.fromhex(uid_hex.replace(":", ""))
    assert bytes_[0] == 0xE0, "Not an ISO 15693 tag"
    assert bytes_[1] == 0x7A, "Not an Abbott sensor"

    # bytes 2-7, big-endian
    value = int.from_bytes(bytes_[2:8], byteorder="big")

    serial = ""
    for i in range(8, -1, -1):          # groups 8 down to 0 (MSB first)
        shift = i * 5
        index = (value >> shift) & 0x1F
        serial += LOOKUP[index]

    return serial


# Verified examples
assert decode_libre_serial("E0:7A:00:C2:1C:C6:45:11") == "0R8FDDJ8J"
# E0:7A:00:B9:21:18:30:3D → "0Q4HJHD1X"  (unconfirmed, apply same algorithm)
```

## Verified Examples

| UID | Serial |
|-----|--------|
| `E0:7A:00:C2:1C:C6:45:11` | `0R8FDDJ8J` ✅ confirmed |
| `E0:7A:00:B9:21:18:30:3D` | `0Q4HJHD1X` (computed, unconfirmed) |

## Common Mistakes

| Mistake | Effect |
|---------|--------|
| Reversing bytes 2–7 before processing | Wrong serial |
| Extracting 10 characters (50 bits) instead of 9 (45 bits) | Wrong serial |
| Extracting LSB-first instead of MSB-first | Reversed serial |
