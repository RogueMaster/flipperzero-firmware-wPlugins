# USB Game Controller for Flipper Zero
This is an app for the Flipper Zero that emulates a game controller (specifically the Xbox 360 controller) and lets the user interact with the emulated joystick, A and B buttons.
It also contains a full implementation of XInput for the Flipper Zero that supports every input that a regular Xbox controller does, which you may use in other applications.

## How to use it?
Connect your Flipper Zero to a computer, or any other device that supports XInput. **This app likely doesn't work on Xbox consoles.**

From there, open the app and it should connect automatically. The D-Pad acts as your left joystick, the center button is A and the back button is B. Holding the back button by itself will close the app, but using other inputs with the back button won't. **Closing the app will reboot your Flipper.**

## Special Thanks

[Dave Madison's "Understanding the Xbox 360 Wired Controller’s USB Data"](https://www.partsnotincluded.com/understanding-the-xbox-360-wired-controllers-usb-data/) - Detailed documentation of the protocol

[Willy-JL](https://github.com/Willy-JL) - Help with bugs

Microsoft - Creating the Xbox itself

[The Flipper Zero Team](https://github.com/flipperdevices) - Creating the Flipper Zero itself
