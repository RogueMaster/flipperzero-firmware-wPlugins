v1.3:
Amiibo Playlist is a new main-menu picture after Amiibo Blast. It lists one built-in preset, MK8 - All Mii Costumes. That preset walks 21 dumps under the Amiibo Database folder to unlock every amiibo-gated Mii costume in Mario Kart 8 Deluxe. A short description shows before the run. Each dump stays on air until you press Next (Center); Back exits. There is no auto-advance and no playlist click. Missing or bad dumps show as Missing dump on that slot (numbers stay 1–21); Next continues. UIDs are left alone. Paths assume a Full Set layout under apps_data/amiibo_emulator/db. Rosalina also tries Super Mario Amiibo/Rosalina.bin if the Smash name is missing. Splatoon prefers Smash Inkling.bin, then Splatoon Amiibo/Inkling Girl.bin (and color / series alts).

Settings has Click volume: Off, Low, Med, High, or Max (equal steps 0.25–1.0). It applies to the transmit click for Amiibo Blast (not Playlist). Med is the default. Off plays no click. Changing the setting plays a test click at the new level (Off stays silent). The choice is saved with the other settings.

Key files are found under apps_data/amiibo_emulator or the Amiibo database folder even when they sit in a subfolder. key_retail.bin works, and so do unfixed-info.bin plus locked-secret.bin in the same folder. Existing nfc/amiibo_keys.bin paths still work.

v1.2:
Settings can delete scanned Amiibos or Favorites. Each one names what will be removed and asks you to hold Up for 2 seconds. The downloaded library on the SD card stays. Deleting one Amiibo you have open still asks first.

Sort lists is Unsorted or By Series. It applies to Favorites and to Scanned Amiibos. Unsorted is the flat list, and it is the default the first time you open the app. By Series groups them from the figure ID and shows a count for each series that has Amiibos. A series with nothing in it is hidden. One this app cannot name goes in Other. The choice stays after you close the app.

Lists keep Amiibo-shaped files: .bin files of 520, 540, or 572 bytes, and .nfc files that look like NTAG215. Other NFC dumps are skipped. One database folder shows at most 200 names, then a short note that the rest were left off. Favorites and Scanned Amiibos show 40 and say so when more are there.

About, on the main menu, says Amiibo is a Nintendo trademark and that the characters are copyright Nintendo. It also says the app is open source and written by Soap-n-Cartridge. The app does not own Amiibo or Nintendo content.

On launch the app plays the splash over 1.2 seconds, holds the last frame for 2 seconds, then opens the main menu.

Amiibo Blast lists the same Amiibos as Favorites. You cannot favorite or delete from that list. Blast makes a new ID, sends it for 1 second, pauses 2 seconds, and repeats until you press any key. Those IDs are not saved, and the favorite file stays as it was. This needs the key file. If the key is missing, Blast says so.

v1.1:
Folder names show how many Amiibos are inside, including folders within them. A folder with none shows no number. Every number ends on the same right edge. The main menu has an About picture for the Nintendo trademark and copyright notice. About also says the app is open source and written by Soap-n-Cartridge. After a scan works, the app returns to the Scan menu so the next Amiibo can be scanned.

v1.0:
The main menu is a row of pictures: Amiibo Database, Favorites, Scan Amiibos, Settings, and Help. You can scan a real Amiibo, save a favorite, and make a new ID. On a favorite, New UID asks first and then saves the new ID on that favorite. Settings is the only screen that says whether the key file is ready.

v0.1:
First version. Browse dump files on the SD card and emulate them.
