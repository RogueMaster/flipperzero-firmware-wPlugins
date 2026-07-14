# Flipper Zero Game Boy Printer Emulator
<p align='center'>
  <img src='https://github.com/kbembedded/flipper-gb-printer/blob/main/.assets/GCIM_0011-bw-4x-color.png' width=288>
</p>
A Game Boy Printer Emulator for Flipper Zero that saves images as PNG on the microSD card.

## Hardware Interface
The Game Boy is connected to the Flipper Zero's GPIO pins via a GBC style Game Link Cable. The [Flipper GB Link module](https://www.tindie.com/products/kbembedded/game-link-gpio-module-for-flipper-zero-game-boy/) is an easy way to connect a Game Boy via a Game Link Cable to the Flipper Zero.

<p align='center'>
  <a href="https://www.tindie.com/stores/kbembedded/?ref=offsite_badges&utm_source=sellers_kbembedded&utm_medium=badges&utm_campaign=badge_large">
<img src="https://i.imgur.com/WQIJK8G.png" alt="Flipper GB Link module" width="480">
  </a>
</p>
<p align='center'>
<a href="https://www.tindie.com/stores/kbembedded/?ref=offsite_badges&utm_source=sellers_kbembedded&utm_medium=badges&utm_campaign=badge_large"><img src="https://d2ss6ovg47m0r5.cloudfront.net/badges/tindie-larges.png" alt="I sell on Tindie" width="200" height="104"></a>
</p>

Details on making your own interface adapter can be found here [https://github.com/kbembedded/Flipper-Zero-Game-Boy-Pokemon-Trading](https://github.com/kbembedded/Flipper-Zero-Game-Boy-Pokemon-Trading?tab=readme-ov-file#how-does-it-work)


## Use
Connect a Game Boy to the Flipper through the link cable interface, open the `Flipper GB Printer` app on the Flipper Zero, configure save files, and the PNG palette the file will be saved with. Press `OK` on the `Receive!` command to put the Flipper Zero in receive mode. As files are printed from the Game Boy to the Flipper, the screen will display a count of number of files received, the number of files saved as a PNG (or errors in saving them).


#### Pinout Selection
The `Select Pinout` option from the main menu allows a user to configure the pin interface on the Flipper Zero for the link cable. This defaults to `Original` and is likely what you want to use in most cases. Most of the adapters that exist as well as instructions on making custom cables, use this pinout. The `MLVK2.5` pinout is used for MALVEKE boards that are hardware revision 2.5 or lower. The 2.5.1 version of the MALVEKE uses the `Original` pinout. Any custom pinouts can also be defined by selecting `Custom` and setting each individual pin[^1].

The pinout is saved automatically and will be re-used when running the Flipper GB Printer at a later date.

[^1]: There are limitations on which pins can be used for which purpose. There is no logic to control this, some custom configurations may simply not work.


#### Configuration Options
**Save bin**: If yes, saves a raw binary of the image data received from the Game Boy as a file named `GCIM_YYYY-MM-DD_XXXX.bin`.

**Save bin+hdr**: If yes, saves an additional file named `GCIM_YYYY-MM-DD_XXXX-hdr.bin` that can be directly imported to [https://herrzatacke.github.io/gb-printer-web/#/import](https://herrzatacke.github.io/gb-printer-web/#/import).

**Save PNG**: If yes, save a converted copy of the image as a PNG to a file named `GCIM_YYYY-MM-DD_XXXX-zzz.png` (where `zzz` is used to indicate the palette it was saved with).

**PNG Palette**: Select one of 18 palettes to render the PNGs in. The default, `B&W`, is grayscale; and all other palettes are all approximations of 2-bit palettes used on real Game Boy devices or emulators.


All files are saved to the Flipper's microSD card, in the `apps_dir/flipper_gb_printer/` directory. They are organized in to subfolders dated `YYYY-MM-DD/` of the date the photos were printed to the Flipper Zero, and numbered in the order they were printed on each date.


## Palettes
TODO


## Game Compatibility
There is no reason this shouldn't work with every Game Boy game that prints to the Game Boy Printer. However, this project was aimed at the Game Boy Camera and most of the testing has been done there. There are a handful of other games that have been tested and reported to have worked. If there are any games that have issues, please open up an [Issue](https://github.com/kbembedded/flipper-gb-printer/issues) and provide some detail.


## Future Plans
This is still in development and does not yet have a lot of features I want to implement. Near term is to support more transfer modes; [Photo!](https://github.com/untoxa/gb-photo)'s fast clock and Transfer modes, and Transfer from the Game Boy Camera. A little further out is supporting managing/manipulating frames and Game Boy Camera photos of images printed to the Flipper. And long-term be able to print from the Flipper Zero to a Game Boy Printer. And somewhere in between those, support remote controls for Photo! via IR and link cable. There is also some fantastic animation in the works.
