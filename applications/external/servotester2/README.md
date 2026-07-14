# Servo tester app for Flipper Zero

https://user-images.githubusercontent.com/8887221/228034391-aa9df44c-dcf7-4999-9d22-590cc7ac0392.mp4

This app aims to replicate the behavior of a standard [RC servo](https://en.wikipedia.org/wiki/Servo_(radio_control)) tester, by replicating a [PWM RC servo signal](https://en.wikipedia.org/wiki/Servo_control). You can read more about it in [this blog](https://mhasbini.com/blog/servo-tester-flipper-zero.html)!

## Usage

- Connect the Servo PWM input to `A7` Flipper Zero in.
- Enable 5V output in the GPIO menu or plug-in USB-C charging cable. (If you're supplying power from an external power source, make sure it has common ground with the flipper GND)
- Start the ServoTester app
- Test different servo positions

## Controls

- Left/Right: change angle in manual mode.
- Up/Down: change mode

The app has three modes:

- Manual: left increase 10°, right decrease 10°.
- Center: move to 90°.
- Sweep: move between 0° & 180° every 1 second.

The indicated angle range is: 0° to 180°, but the actual servo angle may differ, because not every servo has a 180° range. 


## Building

Install [uFBT - micro Flipper Build Tool](https://github.com/flipperdevices/flipperzero-ufbt) to get a toolkit for building flipper zero applications. 

- Clone this git repository
- cd to `ServoTesterApp` content.
- run `ufbt && ufbt launch` with flipper connected. It automatically installs the compiled application into Flipper Zero.

## Installing

Drop the compiled `servotesterapp.fap` from `./dist` into `/ext/apps/GPIO/` on the flipper. 

