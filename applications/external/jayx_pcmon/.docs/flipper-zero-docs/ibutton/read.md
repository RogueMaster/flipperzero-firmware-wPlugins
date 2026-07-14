<!-- Source: https://docs.flipper.net/zero/ibutton/read -->
<!-- Mirrored: 2026-07-14 -->

# Reading iButton Keys

## Overview

Flipper Zero allows you to read, save, and emulate iButton access control keys using Dallas, Cyfral, and Metakom protocols. You can also modify the unique IDs (UIDs) of saved keys.

## Supported iButton Key Types

The device can read the following iButton keys:

- **Dallas DS1990**: UID only (no memory)
- **Dallas DS1992**: UID and memory data
- **Dallas DS1996**: UID and memory data
- **Dallas DS1971**: UID and memory data
- **Dallas (unsupported keys)**: UID only
- **Metakom**: UID only (no memory)
- **Cyfral**: UID only (no memory)

## How to Read iButton Keys

1. Go to Main Menu > iButton > Read
2. Connect the key to the data and ground pins as shown in the documentation
3. Once reading finishes, view the key's data
4. To save: go to More > Save, name the key, then press Save

## Troubleshooting Reading Issues

- **Weak connection**: "Make sure you connect the key as shown" in the guide
- **Unsupported protocol**: The key may use a protocol the device doesn't support
- **Damaged key**: Physical damage may prevent reading
- **CRC errors on Dallas keys**: Try reading again and emulating; if emulation fails, the error cannot be fixed

## Editing Saved Keys

Navigate to Main Menu > iButton > Saved, select a key, choose Edit, enter the required hex value, and save with a new name.

## Emulating Keys

1. Go to Main Menu > iButton > Saved
2. Select the key to emulate
3. Place Flipper Zero against the iButton reader, ensuring pins make proper contact

### Emulation Troubleshooting

- Verify proper pin contact with the reader
- Use wires connected to GPIO pins 17 (data) and 8, 11, or 18 (ground) if direct contact fails
- Some readers have anti-emulation protection that cannot be bypassed
