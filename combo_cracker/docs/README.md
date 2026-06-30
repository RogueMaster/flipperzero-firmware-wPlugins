# Combo Cracker
**Combo Cracker** is an on-the-go combination lock cracking tool for the **Flipper Zero**, inspired by security researcher [Samy Kamkar](https://github.com/samyk)’s work on the mechanical vulnerabilities in *Master Lock* combination padlocks.

Using a clever approach/exploit and feedback from the lock’s dial resistance, you can determine the combination in **just 8 attempts or less** — instead of the known issues which deduce such to 100 or so brute-force attempts.

# How it works
This Flipper Zero app allows you to input physical resistance value(s) and "lock positions" observed from turning the lock dial. The app uses that data to run Kamkar’s approach to output a short list of combinations. You can find information about how such works by watching Samy Kamkar's wonderful [video(s)](https://www.youtube.com/watch?v=qkolWO6pAL8)!

# Usage
UP/DOWN - Select the Lock/Resistance position(s)

LEFT/RIGHT - Increment/Decrease the position

ABOUT -> RIGHT - Brief description


# Credits & Acknowledgement:
Inspired by: [Samy Kamkar’s](https://github.com/samyk) lock cracking research


Built for: [Flipper Zero](https://github.com/flipperdevices/flipperzero-firmware)
