# Gemini-Flipper

Currently in development (Beta phase), anyone who wants to join and help improve it can make their PR and also C developers who want to help improve the Flipper native app are welcome.

You can find the first version in the releases section or at https://lab.flipper.net/apps/gemini_ia you can find the latest stable version.

Special thanks to Derek Jamison who created a large part and I would dare say the entire application in C thanks to his work and effort (I only made some small changes that I would never have achieved without his help and tutorials) he deserves all the recognition for this 👏👏 https://github.com/jamisonderek

Important: Due to changes in the Flipper Zero firmware regarding the implementation of mJs and the changes it had, the .js application no longer works with the new firmware versions, but do not worry the native C application is now ready for everyone to enjoy

## How it works

Currently the code takes the api key in plain text stored in:

"/ext/apps_data/gemini_ia/key.txt" <-- You need to create it manually

You can get the Gemini Api Key using this link: https://ai.google.dev/gemini-api/docs/api-key You only need a Gmail account

it reads the .txt file and sends it via serial command to the esp32 as soon as the app is initialized (it would be nice to implement some kind of encryption to the api key but it is important that it is sent in plain text and unencrypted to the esp32 because that is how the firmware can read and implement it)

If you previously connected to a Wi-Fi network it will automatically reconnect and you can continue chatting without having to reconnect to the Wi-Fi because it stores known networks and connects if you are near one

I added the source code for the new firmware for the esp32 and also the precompiled binary for the ESP-Flasher app (you can find the binary in the releases section)

## How to get started

You can watch a video by Derek Jamison who did an excellent job explaining how to get started with the app https://www.youtube.com/watch?v=j92i2CGUWC4 and you can also support his channel by liking and subscribing. It's really worth it, he's a great teacher for those of us who are starting to create our own apps.

## How to flash the firmware using ESP-Flasher

Check the folder "Gemini IA Precompiled binaries" there is a tutorial on how to do it there

## TO DO

Make the board automatically reboot when closing the app (the implementation is already applied in the esp32 firmware but it still needs to be implemented in the Flipper app)

## Native App

The native app is now 100% working, the only problem is that I still can't get the board to reboot when closing the app (so you don't have any issues when reopening the app) <-- every time you want to enter the app you'll have to press the reset button on your esp32 board first (a tutorial coming soon)

## Contributors

https://github.com/d4rks1d33

https://github.com/jamisonderek