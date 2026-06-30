This is an implementation of the classic Asteroids game for the [Flipper Zero](https://flipperzero.one/). Inside you will find a simple 2D engine that can be reused to implement other games. Note: This one is SimplyMinimal's fork of Antirez's version with several modifications.

# What's New
* Auto rapid fire (less wear on the buttons this way too)
* Up button applies thrusters
* Haptic feedback and LED effects
* Power up system
* High Score system
* Automatic save and load of high score
* Ability to Pause
* Some modifications to certain game play elements

## What's coming next
* Settings screen
* Enabling sound effects (configurable on/off option)
* ~~Power Ups~~ Improved power up management

---

This is a screenshot, but the game looks a lot better in the device itself:

![Asteroids for Flipper Zero screenshot](/images/Asteroids-PowerUps.png "In game screenshot")

![Pause Screen](images/PauseScreen.png "Pause screen")

# Controls:
* Left/Right: rotate ship in the two directions.
* Ok, short press: Short burst bullets
* Ok, long press: Auto-fire bullets
* Up: Accelerate
* Down: Decelerate
* Back (Short Press): Pause game
* Back (Long Press): Exit game. It will automatically save the high scoore too.

Your high scores will automatically be saved. Go forth and compete!

---
# Power Ups
* ![](assets/firepower_shifted_9x10.png "Ammunition") - Machine gun fire. Press and hold OK button to fire more than the default 5 bullets
* ![](assets/heart_10x10.png "Lives") - Et tu, Brute? Gives a bonus life up to a maximum of 5 lives
* ![](assets/nuke_10x10.png "Nuke") - Nuke (work in progress). Destroys everything in sight (but keeps the power ups)
* ![](assets/split_shield_10x10.png "Shield") - Use the force! Spins up a shield that can be used as a battering ram. Take zero damage while in use.
* D - A mini drone assist that has your back!

## More power ups coming soon...

---
## Installing the binary file (no build needed)
### Option 1 (Recommended)
Download from the Flipper Zero App catalog store  
https://lab.flipper.net/apps/asteroids


### Option 2)
Go to the releases and drop the `asteroids.fap` file into the
following Flipper Zero location:

    /ext/apps/Games

The `ext` part means that we are in the SD card. So if you don't want
to use the Android (or other) application to upload the file,
you can just take out the SD card, insert it in your computer,
copy the fine into `apps/Games`, and that's it.

## Installing the app from source
Step 1) Install `ufbt` cli tool
- **Linux & macOS**: `python3 -m pip install --upgrade ufbt`
- **Windows**: `py -m pip install --upgrade ufbt`

Step 2) Clone this repo with `git`
```
git clone https://github.com/SimplyMinimal/FlipperZero-Asteroids/
```

Step 3) Launch Asteroids  
* `cd FlipperZero-Asteroids`
* Connect your Flipper via USB.
* Build and install with: `ufbt launch`.

## License

BSD licensed. Enjoy.
