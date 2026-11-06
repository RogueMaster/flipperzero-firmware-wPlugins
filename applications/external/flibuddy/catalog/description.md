# FliBuddy

FliBuddy turns a Flipper Zero into a spool reader for 3D printing filament. Hold the NFC tag of a Bambu Lab spool to the back of the Flipper and it shows what the spool is: the filament, its color by name, the color code, diameter, label weight and temperatures, with no phone and no network. With the FliBuddy app on an iPhone, iPad or Apple Watch it also keeps a Bambuddy filament inventory up to date: which spool it is, how much filament is left, and new spools as they arrive.

FliBuddy is an independent open source project. It is not affiliated with Bambu Lab or Flipper Devices.

## On the Flipper alone

- Reads Bambu Lab spool tags (MIFARE Classic 1K, unlocked with the keys derived from each tag's UID) and shows what the tag carries: the filament (for example PLA Basic), a color swatch, Bambu's color name and the code printed on the box (for example Jade White, 10100), the diameter and the label weight.
- Tag details adds the filament id, the RGBA color, the nozzle temperature range, the drying temperature and time, and the bed temperature, each when the tag carries it, with the UID and how much of the tag was read.
- NTAG213, NTAG215 and NTAG216 tags show their type, UID and size.
- A tag left on the Flipper is not read again until it is taken away.
- The LED and vibration confirm each read (as the Feedback setting chooses, with optional sound), so the screen need not be watched.
- **Get app** (OK on the start screen) shows a QR code of the project page, where the phone app is.

## With the FliBuddy app and Bambuddy

The Flipper sends each tag over Bluetooth to the FliBuddy app, which looks it up in Bambuddy, a self-hosted filament inventory for Bambu Lab printers. The app does the network work; the Flipper never goes online.

- A spool in Bambuddy shows its material, color and remaining weight. OK opens a four-digit weight entry, prefilled, for the spool on a scale (gross) or the filament alone (net); OK again saves it to Bambuddy.
- A Bambu Lab spool Bambuddy does not know yet is saved as new with one press, or by itself after the scan with Auto-save new; a tag can also be linked to a spool in the app.
- The menu marks a spool as new (full) or empty, with a three second undo, relinks it, shows Tag details or deletes the spool from Bambuddy after a confirmation.
- Recent lists the spools weighed in this session, to weigh one again without a scan.
- With After scan set to Weigh, a known spool opens straight on the weight entry.

## Extra requirements

- The FliBuddy app for iPhone and iPad, with its Apple Watch app: see the [project page](https://sokogen.github.io/flipperzero-flibuddy/) for the App Store link.
- A [Bambuddy](https://github.com/maziggy/bambuddy) server in its internal inventory mode, reachable from the phone, and a Bambuddy API key for the app.
- Without them FliBuddy still reads tags as described above.

## Settings

- Connection: Auto, Bluetooth or USB. The FliBuddy apps connect over Bluetooth. While FliBuddy holds the USB port (Auto or USB), qFlipper cannot use the cable.
- Auto-save new, After scan (Show or Weigh), Weigh as (Gross or Net), Feedback (Vibro and LED, LED, Off) and Sound.
- The settings file is the only thing FliBuddy writes, in its own folder under apps data on the SD card.

## Source

FliBuddy is MIT licensed. Source code, the protocol and the issue tracker: [github.com/sokogen/flipperzero-flibuddy](https://github.com/sokogen/flipperzero-flibuddy).
