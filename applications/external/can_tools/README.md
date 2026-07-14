# CAN Tools

CAN Tools is a Flipper Zero external app for creating simple DBC-style signal definitions and decoding CAN frames with them.

## Features
- Create and save DBC signal entries (name, CAN ID, bit layout, scaling, unit, min/max).
- View saved signals and inspect their generated DBC string.
- Edit or delete saved signals.
- Manually decode a CAN frame against saved signals.

## How to use
1. Open CAN Tools from the Tools menu.
2. DBC Maker:
   - Select each field and enter values (defaults are provided).
   - Choose Generate & Save to store the signal.
3. View DBC:
   - Browse saved signals and select one to view details.
   - Use the actions menu to edit or delete a signal.
4. Decode Data:
   - Choose Manual frame.
   - Enter CAN ID and DLC, then input the data bytes.
   - The app will display decoded values for matching signals.

## Notes
- CAN ID accepts decimal or 0x-prefixed hex.
- DLC controls how many data bytes are entered.
- Only signals with matching CAN ID are decoded.
