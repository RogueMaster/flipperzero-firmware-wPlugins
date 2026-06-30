# FlipperZero_NFC_Playlist:
The idea behind this app is to allow for you to test multiple copies of NFC's at once as a bulk test
## How it works:
When starting the app you are greeted by a select file option where you choose the playlist you wanna run.

All playlists should be placed in `ext/apps_data/playlists/nfc_playlist`, but they can also be stored in other file locations. An example of the data format is shown below, and you can find an example file in the repository: [file](./playlist.txt). You are able to create your own playlists using the playlist editor in the app.
```txt
/ext/nfc/link.nfc
/ext/nfc/link2.nfc
```
## Feedback:
Any feedback is welcome and would be very much appreciated as it helps me to improve and build upon the project
- <a href="https://github.com/acegoal07/FlipperZero_NFC_Playlist/issues/new?assignees=acegoal07&labels=enhancement&projects=&template=feature_request.md&title=%5BFEATURE%7D">Feature request</a>
- <a href="https://github.com/acegoal07/FlipperZero_NFC_Playlist/issues/new?assignees=acegoal07&labels=bug&projects=&template=bug_report.md&title=%5BBUG%5D">Bug report</a>
## Supported Firmwares
As i know these firmwares are supported and working if you know any more please let me know
- <a href="https://github.com/Next-Flip/Momentum-Firmware" target="_blank">Momentum</a>
- <a href="https://github.com/RogueMaster/flipperzero-firmware-wPlugins" target="_blank">RogueMaster</a>
- <a href="https://github.com/DarkFlippers/unleashed-firmware" target="_blank">Unleashed</a>
## Settings:
- Emulate time (How long the NFC card will be emulated for)
- Delay time (How long the gap between the cards will be)
- LED indicator (Whether or not the LED's will be on)
- Skip errors (Makes it so you can make the emulation screen hide errors and skip delays between errors and emulation)
- Loop (Makes it so the playlist will loop back to the start when it reaches the end)
- Time controls (Allows you to disable the time controls)
- User controls (Allows you to control the position of the playlist using the buttons on the flipper skipping and rewinding the playlist)
- Back to defaults (Resets all settings to their default values)
- Save settings (Saves the current settings so they persist after reopening the app)
- Reload settings (Reloads the settings from the saved configuration)
- Delete settings (Deletes the saved settings)
## Playlist editor:
- Create playlist (Creates a new playlist with the given name)
- Delete playlist (Deletes the selected playlist)
- Rename playlist (Renames the selected playlist to the new name provided)
- Add NFC Item (Adds the selected nfc item to the currently selected playlist)
- Remove NFC Item (Opens a menu allowing you to select a line to remove from the playlist)
- Move NFC Item (Allows you to change the order of the NFC items in the playlist)
- View playlist content (Allows you to view the contents of the playlist)
## Feature ideas:
- A function to allow you to add multiple nfc items to a playlist at once
- A view playlist function which only shows the name of the playlist items excluding the file path