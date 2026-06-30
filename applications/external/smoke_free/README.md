# 🚭 Smoke Free for Flipper Zero

This is an application for the **Flipper Zero** device that tracks the time elapsed since your last smoke, helps you stay motivated, and automatically logs your progress and milestones directly to the SD card.

---
## 📸 Screenshots

| Counter Screen | Menu Settings | Confirmation Screen |
| :---: | :---: | :---: |
| ![Main screen showing elapsed time](screenshots/1.png) | ![Menu screen with options](screenshots/2.png) | ![Double confirmation screen](screenshots/3.png) |

---
## ✨ Features

* **Real-time Counter:** Continuously updates and displays the exact time passed since you quit smoking (formatted dynamically into days, hours, minutes, and seconds).
* **Milestone System:** Automatically detects and alerts you when you hit significant goals (**1 hour**, **12 hours**, **1 day**, **7 days**).
* **Persistent Storage:** Saves your session start timestamp to the SD card so your progress is never lost, even if your Flipper reboots or runs out of battery.
* **Progress Logging:** Maintains a text-based log file on the SD card, recording the exact date and time of your milestones or accidental resets.
* **Anti-Accidental Reset:** Features a strict double-confirmation system ("Are you sure?" → "Are you 100% sure?!") to prevent resetting your counter by mistake.
* **Dynamic Motivation:** Shows your next target goal based on your current progress, changing to a motivational message once long-term milestones are achieved.

---
## 🛠️ Hardware Requirements

* **Flipper Zero** device (utilizes the internal RTC clock and SD card storage, no external sensors required).

---
## 🚀 Usage

1. Launch the application. On the first run, it will automatically initialize a new session and save it to the SD card. On subsequent launches, it will seamlessly reload your active session.
2. The main screen displays the real-time counter, the current date/time, and your next milestone target.
3. To open the menu, press the **OK** or **Back** button on the main screen. 
4. Navigate through the menu options using the **Up/Down/Left/Right** directional buttons.
5. If you happen to smoke, select **"Reset counter (Puff)"** and press **OK**. You will be prompted with a double-confirmation screen to ensure it wasn't an accidental press. Confirming twice will clear your log history and restart the timer.
6. To return from the menu back to the counter without changing anything, select **"Continue smoke-free"** or press **Back**.
7. To close the app, open the menu, navigate to **"Exit"**, and press **OK**. A final motivational reminder will flash on the screen before closing.

---
## 💻 Code Structure

* **nie_pale.c**: The complete application source file.
* **nie_pale()**: The main entry point that initializes the storage records, sets up the GUI viewport, and runs the primary application loop.
* **draw()**: The rendering callback responsible for drawing the layout of the counter screen, the menu, the multi-stage confirmation pop-ups, and the exit message.
* **input_callback()**: Processes all button presses, managing the states between the live counter, menu switching, and confirmation handling.
* **check_milestone()**: Monitored during the loop to check if the elapsed time has passed a threshold, triggering a visual status update and appending it to the log.
* **reset_session()**: Handles resetting the internal timer states, overwriting the timestamp configuration, and logging the reset event to the SD card.
* **load_start_timestamp()** / **save_start_timestamp()**: Low-level functions utilizing the Storage API to read and write the binary state file under **/ext/apps/Health/nie_pale_start.bin**.
* **append_log()** / **clear_log()**: Manages the text-based event tracking system saved under **/ext/apps/Health/nie_pale_log.txt**.
