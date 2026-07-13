<!-- Source: https://docs.flipper.net/zero/infrared/read -->
<!-- Mirrored: 2026-07-14 -->

# Reading Infrared Signals

## Capturing Infrared Signals

The Flipper Zero can capture and save infrared (IR) signals from remotes used to control devices like TVs, air conditioners, projectors, and audio systems. The device uses a built-in IR receiver that captures signals with a carrier frequency of 38 kHz.

For remotes with known protocols, the Flipper Zero automatically decodes IR signals. Unknown protocols are recorded in raw format.

**To capture and save a signal:**

1. Go to Main Menu > Infrared > Learn New Remote
2. Position your IR remote in line of sight with the Flipper Zero's IR receiver
3. Press the button you want recorded
4. Once captured, the remote protocol name appears on screen
5. Press Save, name the button, and press Save again

After saving the first button, Flipper Zero creates a virtual remote. You can add additional buttons by selecting +, or customize/delete the remote by selecting Edit. Saved remotes can also be customized through Main Menu > Infrared > Saved Remotes.

## Emulating Infrared Signals

The Flipper Zero features a built-in IR transmitter that can send saved signals to control compatible devices.

**To send a signal:**

1. Go to Main Menu > Infrared > Saved Remotes
2. Select the saved remote from the list
3. Point your Flipper Zero at the target device (ensure the IR transmitter faces the device)
4. Select the button you want to emulate and press OK
