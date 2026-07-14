<!-- Source: https://docs.flipper.net/zero/ibutton/write -->
<!-- Mirrored: 2026-07-14 -->

# Writing Data to iButton Keys

## Overview

After reading and saving Dallas iButton keys, you can write data to physical blanks or iButton touch memory keys of the same type.

## Compatible Blanks and Keys

iButton keys have a permanent UID that cannot be changed, but their memory can be rewritten. Blanks allow both UID and memory rewriting, though "Flipper Zero doesn't support rewriting a blank's memory yet."

**Compatible blanks for writing:**

| Blank Type | Compatible With | Notes |
|-----------|-----------------|-------|
| RW1990 | DS1990 | Writes only the UID |
| TM1990 | DS1990 | Writes only the UID |
| TM08V2 | DS1990 | Writes only the UID |
| RW2004 | DS1990, DS1992 | Writes only the UID |

**Supported iButton keys for memory writing:** DS1992, DS1996, and DS1971 keys can have memory data written to another key of the same type.

## How to Write Data

1. Navigate to **Main Menu > iButton > Saved**
2. Select the Dallas iButton key to write
3. Choose either:
   - **Write ID** (to write the UID to a blank)
   - **Full Write on Same Type** (to write memory data to a matching key)
4. Connect your Flipper Zero's pins to the blank or key as shown in documentation

## Troubleshooting

If writing fails, verify that pins are touching the key correctly, the blank is supported, or the key isn't damaged.
