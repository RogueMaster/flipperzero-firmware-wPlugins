# NFC PC Login

A Flipper Zero app that uses NFC cards to automatically type passwords on your computer via USB HID or BLE HID. Perfect for quick desktop logins using NFC cards, tags or implants!

## Special Thanks

Special thanks to [**Equip**](https://github.com/equipter), [**tac0s**](https://github.com/dangerous-tac0s) and [**WillyJL**](https://github.com/willyjl/) for the help and suggestions while creating this app! 
And [**pr3**](https://github.com/the1anonlypr3/) for the app icon!!

## Features

- Store multiple NFC cards with passwords
- Automatically type passwords via USB HID or BLE HID
- Switch between USB and BLE modes
- Encrypted storage using Flipper's secure enclave
- Optional passcode protection for additional security
- Select specific cards or auto-match on scan
- Last selected card persists across restarts
- BadUSB keyboard layout support for international keyboards
- Import cards from .nfc files
- Configurable input delays, enter key behavior, and keyboard layouts

## Security

All card data is encrypted using AES-256-CBC with the Flipper Zero's secure enclave. The encryption is device-specific - your encrypted data can't be read on other Flipper Zeros.

You can optionally set a passcode for additional protection. When enabled, you'll need to enter the passcode when opening the app to decrypt your cards.

## Installation

1. Copy the NFC Login folder to your Flipper Zero's applications_user directory
2. Build using FBT: ./fbt launch APPSRC=nfc_login

## Usage

## First Launch

On first launch, you'll be prompted to set a passcode (unless you disable it in settings). The passcode encrypts your card data and is required each time you open the app.

## Adding a Card

1. Open the app from Applications
2. Enter your passcode if prompted
3. Select **"Add NFC Card"**
4. Enter a name for the card
5. Hold your NFC card/tag to the Flipper when prompted
6. Enter the password to associate with this card

## Using a Card for Login

1. Select **"Start Scan"** from the main menu
2. Hold your NFC card/tag to the Flipper
3. The password will be automatically typed via USB or BLE (depending on your selected mode)
4. Press Back to stop scanning

**Note**: If using BLE mode, pair your device to the Flipper first. It will appear as **Control (Flipper Name)** in Bluetooth settings.

## Selecting a Specific Card

1. Select **"List Cards"** from the main menu
2. Navigate to the card you want (Up/Down to navigate)
3. Press **OK** to select it (you'll see a star marker)
4. When scanning, only the selected card will trigger password entry
5. The selected card is remembered across app restarts

## Editing Cards

1. Go to **"List Cards"**
2. Navigate to the card you want to edit
3. **Hold OK** to open the edit menu
4. Choose what to edit: Name, Password, UID, or Delete

## Importing Cards

1. Go to **"List Cards"**
2. Press **Right** to open the file browser
3. Navigate to and select a .nfc file
4. Enter the password for the imported card

## Settings

- **HID Mode**: Switch between USB and BLE HID modes (use Left/Right to toggle)
- **Append Enter**: Toggle whether to press Enter after typing the password (use Left/Right or OK to toggle)
- **Input Delay**: Adjust delay between key presses (10ms, 50ms, 100ms, or 200ms)
- **Keyboard Layout**: Select a BadUSB keyboard layout file (.kl format)
  - Press **OK** to browse and select a layout
  - Use **Left/Right** to cycle through available layouts
  - Layouts are loaded from /ext/badusb/assets/layouts/
- **Reset Passcode**: Set a new passcode
- **Disable Passcode**: Toggle passcode requirement (use Left/Right or OK to toggle)
- **Credits**: View credits (use Left/Right to navigate pages)

## Technical Details

- **Encryption**: AES-256-CBC using Flipper's secure enclave (device unique key)
- **Keyboard Layouts**: Supports BadUSB .kl format files from /ext/badusb/assets/layouts/
- **Requirements**: Flipper Zero with USB HID and BLE support, ISO14443-3A compatible NFC cards

## Building

Requires Flipper Build Tool (FBT) and the Flipper Zero firmware source. From applications_user/NFC Login run: ../../fbt launch APPSRC=nfc_login

## License

This project is licensed under the GNU General Public License v3.0 (GPL-3.0).

See the [LICENSE](https://github.com/Play2BReal/NFC-Login/blob/master/LICENSE) file for details.
