# Amiibo Emulator

Version 1.2.

This app is similar to the devices that are selling online for $30-50, except this is even better! Whether you want to backup your Amiibo collection and use them on the go, or download a complete set of Amiibos (use Google to find them), this app will ocme in handy! You can then generate random identifiers, so games like Ocarina of Time will allow unlimited loot drops. The "Amiibo Blast" option gives you a new Amiibo identifier sent to your game every couple seconds until you stop it. Then on your game controller, just activate the Amiibo option over and over, and watch the bombs, hearts, arrows, etc rain down around you!

This app saves Amiibos you already own, so you can use them again later.

You do not need a downloaded list of Amiibos. You do not need a key file to save and play your own.

1. Scan your real Amiibo.
2. Save a copy in Favorites.
3. Press Emulate and hold the Flipper on the game reader. The game sees that same Amiibo.

A downloaded library is optional. The app cannot include Nintendo's key file. You add that file yourself. A new ID needs that key. Without it, New UID and Auto UID cannot change the ID. Emulate still works. It uses the ID from your Amiibo.

Open Help in the app for the short steps on the Flipper.

## Main menu

The main menu is a row of pictures. Left and Right move. OK opens one. Back returns to the pictures.

1. **Amiibo Database** is for Amiibo files you add yourself. You do not need this to use your own scans.
2. **Favorites** keeps copies of Amiibos you want to use again.
3. **Scan Amiibos** reads a real Amiibo.
4. **Amiibo Blast** lists the same Amiibos as Favorites. Blast sends a new ID for 1 second, pauses 2 seconds, and repeats. It does not save that ID. Any key stops.
5. **Settings** shows the app version, Auto UID, whether the key file is ready, and whether Favorites and Scanned Amiibos are unsorted or grouped by series.
6. **Help** explains the steps on the Flipper.
7. **About** says Amiibo is a Nintendo trademark and that the characters are copyright Nintendo. It also says the app is open source and written by Soap-n-Cartridge. The app does not own Amiibo or Nintendo content.

From one of these screens, Left and Right jump to the next one.

## Scan Amiibos

Hold your real Amiibo on the back of the Flipper. The name is already on it. You do not type it.

The scan stays in Scanned Amiibos. Fav saves a copy in Favorites and leaves you on that Amiibo. The ID stays the same.

If a scan does not work, press OK to try again. Back returns to the Scan menu.

## Favorites

Open a saved Amiibo. The buttons are New UID, Emulate, and Del.

- **Emulate** plays that Amiibo. Hold the Flipper on the game reader.
- **New UID** asks first. Press it again to make a new ID. Back cancels. The new ID is saved on that favorite.
- **Del** asks before it removes the Amiibo. Cancel or Back keeps it.

## Amiibo Database

This list is optional. It is for Amiibo files you put on the SD card yourself. You do not need it to use a scan or a favorite. Folders come first, then files, A to Z.

A .bin file is listed only at 520, 540, or 572 bytes. An .nfc file is listed only when its header looks like an NTAG215 Amiibo. Other NFC dumps are skipped. One folder shows at most 200 names and then says the list was shortened. Favorites and Scanned Amiibos use the same file check, show 40 names, and say so when more are there.

Open a file. The buttons are Fav, Emulate, and New UID.

- **Fav** saves a copy in Favorites and stays on this Amiibo.
- **Emulate** plays it.
- **New UID** makes a new ID for this visit only. It does not change the file. This needs the key file.

A folder named ZE always tries to make a new ID. That needs the key file. A folder named SSB keeps the ID saved in the file.

## Settings

- **Auto UID**, when on, makes a new ID when you emulate a file from the database. Next ID makes another one on that screen. Both need the key file. A scan or a favorite keeps its own ID.
- **Keys** says if the key file is ready. That line is only on this screen.
- **Sort lists** is Unsorted or By Series. It applies to Favorites and to Scanned Amiibos. Unsorted is the flat list, and it is the default the first time you open the app. By Series makes a folder for each series that has Amiibos, with a count, using the series byte in the figure ID. A series with nothing in it is hidden. Anything this app cannot name goes in Other. The choice stays after you close the app.
- **Delete Scanned Amiibos** removes only the scans. The screen names what will be removed. Hold Up for 2 seconds. The downloaded library stays.
- **Delete Favorites** removes only Favorites. The screen names what will be removed. Hold Up for 2 seconds. The downloaded library stays.

## The key file

The app cannot ship Nintendo's key file. Emulate of a scanned Amiibo works without it.

New UID and Auto UID cannot change the ID unless that key file is on the SD card. If you already have the file, put key_retail.bin in the app folder apps_data/amiibo_emulator. If amiibo_keys.bin is already in the nfc folder, you can leave it there. Open Settings to see if the key is ready.
