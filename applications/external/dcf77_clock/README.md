# Flipper-Zero DCF77 Clock Spoof

Spoofs a [DCF77](https://en.wikipedia.org/wiki/DCF77) time signal of a selectable time on the RFID antenna and on GPIO A4 pin.

Uses PWM with frequency of 77.5 kHz on the GPIO pin to simulate the signal.

## Roadmap

(I will probably working on this very soon)

- Add the option to also change the date. Currently the date can only implicitable be changed by overflow (constantly move hour up until in the next day and vice versa)
- Add the option to also send "fake" signals that the DCF77 protocol does allow to mess with clocks who don't check that. For example the DCF77 protocol allows to send 25:63 as a "valid" time

## Usage

Normally a nearby clock listening to the DCF77 signal gets synchronized in two to five minutes depending on the signal strength.

Default mode controls:
- Press OK to toggle the LED blinking
- Press up/down to toggle between CEST (dst) and CET time
- Press right/left to enter edit mode 

Edit mode controls:
- Press right/left to select hours, minutes or seconds to edit
- Press up/down to edit the selected part of the time (there us overfolow, so e.g. going below 12AM will lead to being at 11AM and subtracting one day from the date)
- Press OK to save the changes and exit edit mode
- Press back to discard the changes and exit edit mode

## Antenna

(Disclaimer: For research purposes only. Sending DCF77 is probably illegal in your country, so use this app responsible and only in a small range (therefore you will probably not need an antenna))

The RFID antenna works best at distances of up to 50cm. The signal gets recognized in few seconds.

When using the GPIO, best results are achieved if you connect a ferrite antenna over 330 ohm resistor and a capactior to ground.

It also works with analog beeper or small in-ear headphone connected to the GPIO pin.

## Note

This is a fork of https://github.com/mdaskalov/dcf77-clock-sync. Thanks to @mdaskalov for doing the initial work!
