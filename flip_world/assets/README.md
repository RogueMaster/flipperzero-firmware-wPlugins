The first open-world multiplayer game for the Flipper Zero, best played with the VGM. Here's a video tutorial: https://www.youtube.com/watch?v=Qp7qmYMfdUA

## Connect Online
- Discord: https://discord.gg/5aN9qwkEc6
- YouTube: https://www.youtube.com/@jblanked
- Instagram: https://www.instagram.com/jblanked
- Other: https://www.jblanked.com/social/

## Requirements
- WiFi Developer Board, Raspberry Pi, or ESP32 device flashed with FlipperHTTP v1.8.2 or higher: https://github.com/jblanked/FlipperHTTP
- 2.4 GHz WiFi access point

## How It Works

FlipWorld and FlipSocial are connected. Your login information is the same in both apps; if you register an account in either app, you can log in to both using that information. This also means that your friends, messages, and achievements are synced between apps. You only need a username and password to start, which are set in the User Settings. Keep in mind your username will be displayed to others, so choose wisely.

**Settings**

- **WiFi**: Enter your SSID and password to connect to your 2.4 GHz network.
- **User**: Add or update your username and password (this is the same login information as your FlipSocial account).
- **Game**: Install the Official World Pack, choose your weapon, set your FPS (30, 60, 120, or 240), and select whether you want the screen backlight to always be on, the sound to be on, and the vibration to be on.

**Controls**

- **Press/Hold LEFT**: Turn left if not already facing left, then walk left if the button is still pressed.
- **Press/Hold RIGHT**: Turn right if not already facing right, then walk right if the button is still pressed.
- **Press/Hold UP**: Walk up.
- **Press/Hold DOWN**: Walk down.
- **Press OK**: Interact, attack, or teleport. Attacks enemies when colliding with them until all enemies are defeated. Interacts with NPCs when colliding with them.
- **HOLD OK**: In-Game Menu.
- **Press BACK**: Leave the menu.
- **HOLD BACK**: Exit the game.

**Player Attributes**

- **Health**: The amount of life points the player has.
- **XP**: The amount of experience points the player has.
- **Level**: The rank/level of the player.
- **Strength**: The attack power of the player's attacks.
- **Health Regeneration**: The amount of health a player gains per second.
- **Attack Timer**: The duration the player must wait between attacks.

As a new player, you have 100 health, 0 XP, 10 strength, 1 health regeneration, an attack timer of 1, and are level 1. Each level, the player gains an extra 1 strength and 10 health. Additionally, the amount of XP needed to level up increases exponentially by 1.5. For example, to reach level 2, you need 100 XP; for level 3, 150 XP; for level 4, 225 XP; and so on.

**Enemies**

Enemies have similar attributes to players but do not have XP or health regeneration. For example, level 1 enemies have 100 health and 10 strength, just like a level 1 player.

**Attacks**

If an enemy attacks you, your health decreases by the enemy's strength (attack power). Additionally, if an enemy defeats you, your XP decreases by the enemy's strength. Conversely, when you successfully attack an enemy, you gain health equal to 10% of the enemy's strength and increase your XP by the enemy's full strength.

An enemy attack registers if the enemy is facing you and collides with you. However, to attack an enemy successfully, the enemy must be facing away from you, and you must collide with them while pressing "OK".

**NPCs**

NPCs are friendly characters that players can interact with. Currently, you can interact with them by clicking "OK" while colliding with them.

## Short Tutorial

1. Ensure your WiFi Developer Board and Video Game Module are flashed with FlipperHTTP.
2. Install the app.
3. Restart your Flipper Zero, then open FlipWorld.
4. Click "Settings -> WiFi", then input your WiFi SSID and password.
5. Hit the "BACK" button, click "User". If your username is not present, click "Username" and add one. Do the same for the password field.
6. Go back to the main menu and hit "Play", followed by "Tutorial". It will register an account if necessary and fetch data from our API that's used to render our graphics.
