# Nah2-Nah3: Flipper Zero Mini-Game Suite

**Nah2-Nah3** is a fun and engaging mini-game application for the Flipper Zero, featuring rhythm and action-based gameplay optimized for the device's 128x64 display and input system.

## Features
- **Multiple Game Modes**: Choose from six games (Zero Hero and Flip Zip fully implemented; Line Car, Flip IQ, Tectone Sim, and Space Flight planned [Drop Per is a placeholder for Flip IQ]).
- **Zero Hero**: A rhythm game where you press buttons to hit falling notes, with dynamic difficulty and streak tracking.
- **Flip Zip**: An running game where you navigate a mascot through lanes, jumping over obstacles with a speed bar and tap-based boosts.
- **Dynamic Menu**: Navigate a visually appealing menu with animations to select games.
- **Orientation Support**: Toggle between left-handed and right-handed modes (menus & paused); supports flippable horizontal and vertical orientations without leaving the application.
- **Visual Feedback**: Scrolling notifications, day/night mode, and a credits screen.
- **Optimized Performance**: Uses faux multithreading and fixed-point arithmetic for smooth gameplay.

## Game Modes
1. **Zero Hero**: Hit notes in five lanes (Up, Left, OK, Right, Down) to build streaks and scores.
2. **Flip Zip**: Move a mascot between lanes, jump over obstacles, and boost speed with precise taps.
3. **Line Car**: Planned (Drift or Nah).
4. **Flip IQ**: Planned (Flip Your IQ) **WIP**.
5. **Tectone Sim**: Planned (Based Hits).
6. **Space Flight**: Planned (Star Chase).

## States
- **Loading**: Initial 1.5-second loading screen.
- **Title**: Main menu for game selection.
- **Rotate**: Prompts device rotation with animation.
- **Zero Hero**: Rhythm gameplay state.
- **Flip Zip**: Action gameplay state.
- **Pause**: Pause screen for active games.
- **Credits**: Scrolling credits, exits the app.

## Controls
### Title Menu
- **Left/Right**: Switch between left/right game options.
- **Up/Down**: Navigate between rows.
- **OK**: Start selected game.
- **Back (x3)**: Enter credits screen.
- **Back (Hold 1.5s)**: Toggle left-handed mode.

### Rotate Screen Animation
- **Any Button**: Skip animation and start game.

### Zero Hero
- **Up/Left/OK/Right/Down**: Hit notes in corresponding lanes.
- **Back**: Pause game.
- **Back (Hold 1.5s)**: Toggle left-handed mode.

### Flip Zip
- **Left/Right**: Move mascot between lanes.
- **Up/Down**: Adjust vertical position.
- **OK (Press)**: Jump.
- **OK (Release)**: End jump hold.
- **Back**: Pause game.
- **Back (Hold 1.5s)**: Toggle left-handed mode.

### Pause
- **OK**: Resume game.
- **Back (x2)**: Return to title menu.
- **Back (Hold 1.5s)**: Toggle left-handed mode.

### Credits
- **Back**: Exit application early.

## Building and Running
1. Clone this repository to your Flipper Zero development environment.
2. Compile `nah2nah3.c` using the Flipper Zero firmware SDK.
3. Deploy to your Flipper Zero and launch from the apps menu.

## Screenshots
Below are screenshots showcasing **Nah2-Nah3** in action on the Flipper Zero:

![Nah2-Nah3 Intro Crawler part 1](https://raw.githubusercontent.com/DigiMancer3D/nah2nah3/refs/heads/main/screenshots/Screenshot-20250718-121106.png)
![Nah2-Nah3 Intro Crawler part 2](https://raw.githubusercontent.com/DigiMancer3D/nah2nah3/refs/heads/main/screenshots/Screenshot-20250718-121110.png)
</br>
![Nah2-Nah3 Title Screen](https://raw.githubusercontent.com/DigiMancer3D/nah2nah3/refs/heads/main/screenshots/Screenshot-20250718-121119.png)
</br>
![Nah2-Nah3 Game-Select Screen (Zero Hero Selected)](https://raw.githubusercontent.com/DigiMancer3D/nah2nah3/refs/heads/main/screenshots/Screenshot-20250718-121122.png)
![Nah2-Nah3 Game-Select Screen (Flip Zip Selected)](https://raw.githubusercontent.com/DigiMancer3D/nah2nah3/refs/heads/main/screenshots/Screenshot-20250718-121128.png)
</br>
![Nah2-Nah3 Rotate Animation scene 1](https://raw.githubusercontent.com/DigiMancer3D/nah2nah3/refs/heads/main/screenshots/Screenshot-20250718-121157.png)
![Nah2-Nah3 Rotate Animation scene 4](https://raw.githubusercontent.com/DigiMancer3D/nah2nah3/refs/heads/main/screenshots/Screenshot-20250718-121202.png)
</br>
![Zero Hero Gameplay](https://raw.githubusercontent.com/DigiMancer3D/nah2nah3/refs/heads/main/screenshots/Screenshot-20250718-121215.png)
![Zero Hero Gameplay (with notification on screen)](https://raw.githubusercontent.com/DigiMancer3D/nah2nah3/refs/heads/main/screenshots/Screenshot-20250718-121222.png)
</br>
![Flip Zip Gameplay 1](https://raw.githubusercontent.com/DigiMancer3D/nah2nah3/refs/heads/main/screenshots/Screenshot-20250718-121308.png)
![Flip Zip Gameplay 2](https://raw.githubusercontent.com/DigiMancer3D/nah2nah3/refs/heads/main/screenshots/Screenshot-20250718-121327.png)

## Notes
- Only Zero Hero and Flip Zip are fully implemented; other games are placeholders.
- Optimized for Flipper Zero’s hardware constraints (limited stdlib, 128x64 display).
- Uses Furi APIs for GUI, input, and timers.

## Contributing
Feel free to submit pull requests to implement the remaining game modes or enhance existing features!

#### NOTICE:  The application although were completely designed by Z0M8I3D aka DigiMancer3D, Grok 3 (xAI) wrote the inital code, edited code while guided by the designer.

###### PERSONAL NOTE FROM DESIGNER:  I don't know C specifically, and the last time I wrote C++ (non-strict code style) was in like 2012 but last time I wrote serious C++ code was 2007/2008. So, Grok was used to transmute JS -> C (strict) then coached for changes there after.  I am personally using this project to learn C and have helped with visuals and that is probably why the UI score still overflow the wrong way and other little messed up details in this game suite.
