# Changelog

## v0.1

- Initial release.
- Added Bluetooth HID password typing.
- Added generated normal and hidden passwords.
- Added configurable character classes and password length.
- Added configurable daily usage limits for hidden passwords.
- Added device-bound authenticated password scrambling.
- Added filesystem import for regular passwords.
- Added a password naming step.
- Added Tab and Enter actions to the repeat-entry screen.
- Renamed the user-facing app to Password Keyboard and the BLE device to PassKey.
- Renamed the internal app ID to `password_keyboard`.
- Changed hidden-password usage limits from global to per-password settings.
- Added Manual mode using the selected normal or hidden type.
- Added long-press Rename and Delete actions for saved passwords.
- Added delete confirmation and migration from the previous vault format.
- Added a persisted default password name in Settings.
- Added the credit "Idea by: Evgeniy Raev".
- Excluded creation-flow test entries from hidden-password daily usage limits.
