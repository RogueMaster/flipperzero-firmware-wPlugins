Install from the Flipper App Catalog: https://lab.flipper.net/apps/anki_remote

# What does Anki-Remote do?

It lets you assign any keyboard key to each button on your Flipper Zero and use it as a BLE (Bluetooth Low Energy) remote. Key mappings are saved to the SD card inside the apps_data folder.

## How to Use

To use the app, simply open it and select **Manage Presets** to create, edit, switch between, rename, or delete key-mapping presets. All presets and settings will be saved to the SD card.

Then, go back to the main menu, press **Start**, and connect to Bluetooth. Once you're successfully connected, you'll see the controller screen.

## Purpose

The purpose I originally had in mind for this app was to be used as a remote for [Anki](https://apps.ankiweb.net/), a popular spaced repetition flashcard app for med school students. However, this app's usability is not limited to Anki and it **can be used for any other purpose or program**.

The current HID keyboard in the BT remote app is limited for this use case because it takes a long time to navigate to a different button, and you have to be looking at the Flipper's screen at all times to see which key you have selected.

## Credits & References

The following projects were used or referenced to help me make my app:
- [Flipper Zero's Official HID](https://github.com/flipperdevices/flipperzero-firmware/tree/dev/applications/system/hid_app) keyboard logic, core functionality, most images, and the remote screen layout.
- [BT Trigger](https://github.com/xMasterX/all-the-plugins/tree/dev/apps_source_code/bluetooth-trigger) bluetooth cross-compatibility, the awaiting bluetooth screen layout, and general example code.
- [Instantiator's Flipper Zero App Tutorial](https://instantiator.dev/post/flipper-zero-app-tutorial-01/) installing ufbt and setting up the development environment in VSCode.
