# Running it

Two ways.

## Just run the app

```powershell
cd flipper-zero\drop-dect
ufbt launch
```

On the Flipper, open `Apps > Tools > FliPort Live` and drop it.

## Make it run on its own

This needs custom firmware, because stock firmware closes an app once you leave it.

1. Copy this folder into the firmware source tree, for example `applications/system/drop_dect/`.
2. Open `application.fam` in that copy and uncomment the `drop_detect_svc` block near the bottom.
3. Copy `assets/model.json` onto the SD card at `/ext/apps_assets/drop_detect_svc/model.json`.
4. Build and flash the firmware:

   ```powershell
   ./fbt flash_usb
   ```

After that the siren works at all times, with no app open.
