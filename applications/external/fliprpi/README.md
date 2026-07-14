Use your Flipper Zero to control your Raspberry Pi.

## Requirements
- Raspberry Pi
- MicroSD Card (32GB or larger)
- MicroUSB Cable (data-capable)
- Computer with Windows, macOS, or Linux and internet access

## Installation
1. Insert the MicroSD card into your computer using the card reader.
2. Download the Raspberry Pi Imager from the official website: https://www.raspberrypi.com/software/. Then, install the software by following the on-screen instructions for your operating system.
3. Open Raspberry Pi Imager after installation.
4. Click **CHOOSE OS**. Then go to **Raspberry Pi OS (other)** and select **Raspberry Pi OS Lite (64-bit)**.
5. Click **CHOOSE STORAGE** and select your MicroSD card.
6. Click the **gear icon** or **EDIT SETTINGS**. Then:
- Check **Configure wireless LAN**.
- Enter your **WiFi SSID** and **Password**.
7. Go to the **SERVICES** tab. Then:
- Enable **SSH**.
- Choose **Use password authentication**.
8. Click **SAVE** and confirm by clicking **YES**. Then wait for the process to finish.
9. Once finished, your SD card will be removed. Disconnect and reconnect it.
10. Open the **bootfs** drive that appears and double-click on the **config.txt** file.
11. At the bottom, under **all**, add the following lines:
- **enable_uart=1**
- **dtoverlay=disable-bt**
- **dtparam=uart0_console=on**
12. Save the file, then safely eject your SD card.
13. Insert your SD card into your Raspberry Pi, then power it on. Shortly afterward, connect your Pi to your Flipper Zero.
14. Open the FlipRPI app and click on **View** to see the installation process. Once prompted to log in, return to the main menu and click on **Send**.
15. Click **CUSTOM**, enter your username, then click **Save**. Repeat these steps to enter your password.

Now you are all set! The app is straightforward. You can either send a custom command to your Raspberry Pi or choose one of the pre-saved commands.

All contributions are welcome! Fork the repository and submit your edits. If you're having any issues, please create a new issue above.

Known Bugs:
- Sometimes not all of the data isn't sent over the UART (yes/no lines mainly but sometimes the terminal user-tag)

Wiring: (Raspberry Pi Zero 2 W -> Flipper)
- TX (GPIO 14 - Pin 8) -> Pin 14 (RX)
- RX (GPIO 15 - Pin 10) -> Pin 13 (TX)
- GND (Pin 6) -> Pin 11 (GND)
- Do NOT connect 3v3 or 5v (power via USB cable or battery pack instead)
