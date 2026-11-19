Hobby servo tester with angle control, jitter-free hardware PWM and an animated servo on screen.

## Why ServoMotor
- **Hardware PWM:** pulses come from the TIM1 timer on PA7, not from a software delay loop, so screen redraws, buttons or Bluetooth cannot stretch them and the servo does not twitch. Measured: 1000.0 / 1500.0 / 2000.0 us with a standard deviation below 0.3 us.
- **Angles, not raw microseconds:** pick 0-180 degrees in 10 degree steps; the app sends the matching pulse.
- **Fixed 50 Hz frame:** 20 ms period, independent of the pulse width.
- **Visual:** animated top-view servo with a clamp horn and a marker for the commanded angle.
- **Built-in help:** wiring diagram, pulse vs. position explanation and safety tips.

## Controls
The bottom bar shows **OK mode** (OK cycles the modes) and **up/down pages** (the arrow keys switch windows).

- **OK:** change mode (Manual, Center, Sweep)
- **Left / Right:** -10 / +10 degrees (hold to repeat)
- **Down / Up:** next / previous page (Main, Wiring, Pulse info, Safety)
- **Back:** return to the main page / exit

## Wiring
- **Red (+):** pin 1, 5 V
- **Yellow / White / Orange (signal):** pin 2, PA7 (3.3 V logic)
- **Brown / Black (-):** pin 8, GND

The 5 V rail provides about 1.2 A at most. For larger servos use an external 5-6 V supply and connect its ground to Flipper GND.

## Good to know
The degrees are nominal: 0-180 maps to 1000-2000 us. Many micro servos (SG90, MG90S) only travel about 90 degrees over this range. If a servo buzzes at one end, it is pushing against its end stop: move it back right away.
