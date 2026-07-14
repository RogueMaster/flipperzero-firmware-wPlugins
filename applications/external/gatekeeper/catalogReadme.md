# Gatekeeper

## Gatekeeper is a password manager for Flipper Zero that uses BadUSB (HID emulation).
### It allows you to store frequently used credentials and automatically enter them on any computer or terminal with a single click.

Features:
- **BadUSB Integration:** Gatekeeper can emulate a USB keyboard and automatically enter saved strings (passwords, tokens, commands).
- **Master Combo Protection:** Each time you launch the device, you are required to enter a secret combination of the 4-way D-pad (Up/Down/Left/Right).
- **Password Storage:** You can save up to 30 entries.

Password storage contains:
- Label — the entry name
- Payload — the string to be entered via BadUSB
- Icon — a visual icon for easy navigation


> note: While Gatekeeper uses Master Combo to protect access, Flipper Zero does not have a hardware Secure Element for application encryption.
Therefore, we recommend:
 not storing root passwords,
 not storing crypto wallet seed phrases,
 not storing financial recovery keys.
 The app is designed for convenient and quick entry of frequently used data.
