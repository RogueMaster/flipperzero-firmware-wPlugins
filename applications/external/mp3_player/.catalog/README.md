# MP3 Player

A real MP3 player for the Flipper Zero. Copy ordinary MP3 files to your microSD card and play them. No desktop conversion, no special encoding.

Created by Coolshrimp.

## Two outputs

Pick one under Settings, Output:

* **Internal** plays through the Flipper's built-in sounder using a custom 10-bit PWM carrier.
* **External** sends I2S audio to a MAX98357A amplifier module wired to the GPIO header.

## Internal output is not ideal

Internal playback works, but the Flipper's internal sounder is a piezo buzzer, not a music speaker. Expect buzzy, low-fidelity sound, and expect it to be **very quiet** even at 100% volume. That is a hardware limitation of driving a piezo element from a GPIO PWM carrier. It is not something the app can fix.

**An external MAX98357A module is strongly recommended.** It is dramatically louder and cleaner, and volume control still works: the app scales the audio in software before it reaches the amplifier, so the volume keys behave identically on both outputs.

## Any MP3 bitrate plays

The decoder reads the bitrate from each frame header, so every MPEG-1, MPEG-2 and MPEG-2.5 Layer III bitrate from 8 kbps to 320 kbps plays. Constant and variable bitrate files are both supported, including mixed-bitrate files.

Sample rates of 8, 11.025, 12, 16, 22.05, 24, 32, 44.1 and 48 kHz are all resampled automatically for the selected output. Mono and stereo, CRC-protected frames and free-format streams are supported. ID3v2 tags and embedded album art are skipped without loading artwork into RAM.

Files that are not really MP3 audio will show an error. That includes AAC, M4A, WMA or FLAC renamed to .mp3, DRM-protected content, and severely damaged files.

## Controls

Song List:

* **Up and Down** move through the list.
* **OK** opens Now Playing and starts that song.
* **Back** returns to the main menu.

Now Playing:

* **Up and Down** change the volume.
* **Left and Right**, short press, skip to the previous or next song.
* **Left and Right**, held down, seek backward or forward in five-second steps.
* **OK** plays and pauses.
* **Back** returns to the song list.

Seeking resumes only if the song was playing before the hold. A paused song stays paused.

## Music folder

The default library is /ext/music. To use a different folder, edit the single line in this file on the SD card:

/ext/apps_data/mp3_player/music_path.txt

Then choose Settings, Rescan library. Restarting the app is not required. Invalid paths fall back to /ext/music.

Only MP3 files directly inside that folder are read, and subfolders are ignored. The scan stops after five seconds, 1024 directory entries, or 100 songs. Keeping the library flat leaves enough RAM for the decoder.

Volume, repeat mode and output selection persist between launches.

## External module wiring

Wire a MAX98357A breakout to the Flipper GPIO header as follows:

* BCLK to pin 5, which is PB3.
* DIN to pin 2, which is PA7.
* LRC or LRCLK to pin 4, which is PA4.
* VIN to pin 1, which is +5 V.
* GND to any GND pin.

Connect the module while the Flipper is powered off. Do not connect either speaker lead to ground. On battery power the app enables the 5 V OTG rail during external playback and disables it afterward. GPIO signals stay at 3.3 V logic even though the amplifier runs from 5 V. The module needs no MCLK or I2C setup, so leave the SD and GAIN pads at their board defaults.

## Limitations

* Internal output is quiet and buzzy by nature. Use the external module if you can.
* Folder recursion is disabled, so keep MP3 files directly in the selected folder.
* Output is mono, resampled to 15.625 kHz for Internal or 16 kHz for External.
* MP3 decoding is CPU and memory intensive on the STM32WB55. Very high bitrate or unusual files may underrun.
* Native 44.1 and 48 kHz output remains future SAI work.

## Credits

Uses minimp3 by lieff, released under CC0-1.0.
