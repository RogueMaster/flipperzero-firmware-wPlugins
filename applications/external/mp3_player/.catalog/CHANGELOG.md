## 3.4

* Added a third output, PAM8403, driving a 500 kHz pulse-density stream on pin 3 (PA6) through an external RC filter.
* Volume control now applies to every output, with a custom PAM8403 curve so its fixed noise floor no longer overwhelms quiet settings.
* Rebuilt the About screen so it scrolls with Up and Down and lists the full MAX98357A and PAM8403 pinout, one pin per line.
* README documents the pin 3 (PA6) pulse-density output and the required RC filter.

## 2.12

* Added a "Created by Coolshrimp" credit to the About screen.
* Renamed the Settings output option from MAX98357A to External for clarity.
* Documented that any MP3 bitrate from 8 to 320 kbps plays, constant or variable.
* Documented the Now Playing controls: Up and Down change volume, Left and Right change track, holding Left and Right seeks, OK plays and pauses, Back returns to the song list.
* Clarified that internal playback is quiet and low fidelity because of a hardware limitation, and that the external MAX98357A module is strongly recommended and keeps full volume control.
* Added screenshots and an MIT license.
