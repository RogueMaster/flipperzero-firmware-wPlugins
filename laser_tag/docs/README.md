## **Laser Tag**
**Turn your Flipper Zero into a Laser Tag Device!**

## Real World - Team based Laser Tag game
Use Flipper Zero as your laser blaster, RFID scan for power-ups, and automatic detection of add-on weapons to GPIO such as the Rabbit Labs Masta-Blasta for arena style play.

## Key Features:
- **Team Battles**: Choose your team and face off in epic Red vs. Blue laser battles.
- **Real-Time Gameplay**: Smooth and responsive laser firing and hit detection.
- **Immersive Sound**: Laser firing and game-over sounds to enhance your battlefield experience.
- **Dynamic Health and Ammo Bars**: Keep track of your health and ammo with clean, dynamic UI elements.
- **Vibration Feedback**: Feel every hit with integrated vibration feedback.
- **RFID Powerups**: Specific tags can be written to any T5577 or EM4100 for adding ammo.
- **External IR Boards**: Add or remove an external infrared blaster anytime during gameplay to switch between internal/external IR gun or swap weapons.

## How to Play
- **Select Your Team**: Use the Left or Right button to choose between Red or Blue team.
- **Fire Your Laser**: Press the OK button to shoot your laser at your opponents.
- **Reload**: When your ammo runs out, press 'Down' to reload and get back into action.
- **Survive**: Track your health, and make sure to avoid getting hit by your opponents' lasers. If your health reaches zero, it's game over!
- **RFID Powerups**: Press the UP button during gameplay to scan a Powerup Tag.

## Current Powerups for RFID Tags (T5577/EM4100):
- **Universal Ammo Refill**: 13 37 00 FD 0A – Increases ammo by 0x0A for any player.
- **Red Team Ammo Refill**: 13 37 A1 FD 0A – Increases ammo by 0x0A for the Red player.
- **Blue Team Ammo Refill**: 13 37 B2 FD 0A – Increases ammo by 0x0A for the Blue player.

*Tip*: You can modify the last byte (e.g., 0A) to change the amount of ammo refilled. Stay tuned for future updates and new powerups!

## Developers:
- **RocketGod** (@RocketGod-git)
- **codeallnight** (@jamisonderek)
