# TV Off

One-tap TV power-off.

Open the app and it immediately sends every TV power code it knows, one after another. Point your Flipper at the TV and wait until it turns off.

## Controls

- **OK**: pause or resume. After a full pass, OK starts again.
- **Back**: exit.

## Code list

The app uses the TV universal remote dictionary already on your SD card (infrared/assets/tv.ir), so it stays current whenever you update the firmware databases.

To use your own list, save an IR file as apps_data/tv_off/codes.ir on the SD card. Every signal named Power in that file is sent, in file order.

## Notes

- Many TVs use the same code for power on and power off, so a TV that is already off may turn on.
- Aim directly at the TV for best range.
