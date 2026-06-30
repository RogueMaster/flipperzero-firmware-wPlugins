# ComboCracker-FZ

**Combo Cracker** is an on-the-go combination lock cracking tool for the **Flipper Zero**, inspired by security researcher [Samy Kamkar](https://github.com/samyk)’s work on the mechanical vulnerabilities in *Master Lock* combination padlocks.

Using a clever approach/exploit and feedback from the lock’s dial resistance, you can determine the combination in **just 8 attempts or less** — instead of the known issues which deduce such to 100 or so brute-force attempts.

# Flipper Lab 🧪 
https://lab.flipper.net/apps/combo_cracker

## 📚 Background: Samy Kamkar's Research
Samy Kamkar discovered a weakness in many standard **Master Lock** combination padlocks that allows their 3-digit codes to be deduced using subtle physical feedback from the lock's mechanism.

## 🕵️ Side-Channel Attack

This method is a type of **mechanical side-channel attack**, as described by Samy Kamkar.
Instead of attacking the lock by brute force, it extracts hidden information by:

- Applying tension to the shackle,
- Observing how the dial behaves at certain positions,
- Measuring subtle differences in movement (i.e. which "gate" feels freer),
- Exploiting the predictable mechanics of the lock.

By analyzing this side-channel data, we can infer the internal state of the lock and reduce the combination space from thousands to just a handful of options — all without damaging or opening the lock first.


Kamkar's technique reduces the problem space dramatically — from over **60,000 combinations down to just 8 or fewer**.

🔗 **Learn more in Samy's video:**
[Cracking Master Locks with Samy Kamkar](https://www.youtube.com/watch?v=qkolWO6pAL8)

🔬 **Original write-up & Web Tool**  
[Samy Kamkar's Page](https://samy.pl/masterlock/)

## 🧠 How It Works
This Flipper Zero app allows you to input physical resistance value(s) and "lock positions" observed from turning the lock dial. The app uses that data to run Kamkar’s approach to output a short list of combinations. You can find information about how such works by watching Samy Kamkar's wonderful [video(s)](https://www.youtube.com/watch?v=qkolWO6pAL8)!

# Usage 🔧 
```
UP/DOWN - Select the Lock/Resistance position(s)
LEFT/RIGHT - Increment/Decrease the position
ABOUT -> RIGHT - Brief description
```


# Main Menu 📺 
![Main-Menu](https://github.com/user-attachments/assets/5483bbae-05ba-465c-a3a4-bd6809954f8a)


# Combination Output 🔒 
![Combo Output](https://github.com/user-attachments/assets/0af467b1-27f7-45b5-971a-efd6bf1d58be)


# 🙏 Credits & Acknowledgement:
Inspired by: [Samy Kamkar’s](https://github.com/samyk) lock cracking research


Built for: [Flipper Zero](https://github.com/flipperdevices/flipperzero-firmware)