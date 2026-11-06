# Catalog screenshots

The Flipper Apps Catalog shows these screenshots; the first is the
preview, a screen the Flipper shows without the phone app. They must be qFlipper exports exactly as qFlipper saves them:
512×256 PNG in its two colours, orange (254, 138, 44) and black (0, 0, 0),
never resized, recoloured, cropped or converted.
`tools/tests/test_catalog.py` checks every PNG here.

## Taking them

1. On the Flipper, open FliBuddy → Settings and set Connection to
   Bluetooth, so that the app leaves the USB port to qFlipper.
2. Connect the Flipper to qFlipper with the USB cable and open the screen
   streaming view.
3. With the FliBuddy iPhone app not connected, bring up the screens that
   need no app: hold a Bambu Lab spool tag to the Flipper for
   `1_no_app.png`, then Back and OK (Get app) for `6_get_app.png`.
4. Connect the FliBuddy iPhone app over Bluetooth and bring up the other
   screens.
5. Take each with qFlipper's screenshot button and save it here under
   the name below.

## Screens

| File | Screen |
|---|---|
| `1_no_app.png` | Result for a Bambu Lab tag with no app connected: the filament as title, the color swatch, Bambu's color name, `10100 - 1.75 mm - 1000 g`, inverted `No app` (preview) |
| `2_spool.png` | Result for a spool in Bambuddy: material and color, `In Bambuddy #...`, the remaining bar |
| `3_weight.png` | Weight entry (OK on a known spool) |
| `4_saved.png` | `Saved` after the weight entry |
| `5_new_spool.png` | Result for a Bambu Lab tag Bambuddy does not know: inverted `Not in Bambuddy` (Auto-save new off) |
| `6_get_app.png` | Get app: where the iPhone app comes from, with the QR code of the project page (iPhone app not connected) |

The list is the `screenshots` of `../catalog/manifest.yml`; add or rename
a file in both.
