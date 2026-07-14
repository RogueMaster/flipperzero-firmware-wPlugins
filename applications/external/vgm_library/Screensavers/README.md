## Screensavers
The current screensavers are compiled examples from the [PicoDVI-Adafruit Fork](https://learn.adafruit.com/picodvi-arduino-library-video-out-for-rp2040-boards/screensavers), edited and adapted for the Video Game Module.

### Installation
There are two installation methods; choose the one that is most convenient for you:

**Method 1**
1. Install the Video Game Module Tool app on your Flipper Zero from the Apps catalog: [Video Game Module Tool](https://lab.flipper.net/apps/video_game_module_tool).
2. Download all screensavers.
3. Disconnect your Video Game Module from your Flipper and connect your Flipper to your computer.
4. Open qFlipper.
5. Navigate to the `SD Card/apps_data/vgm/` directory. If the folder doesn’t exist, create it. 
6. Drag all screensavers you downloaded earlier into the `vgm` directory.
7. Once completed, disconnect your Flipper from your computer, then turn it off.
8. Connect your Video Game Module to your Flipper, then turn your Flipper on.
9. Open the Video Game Module Tool app on your Flipper. It should be located in the `Apps -> Tools` folder from the main menu.
10. In the Video Game Module Tool app, select `Install Firmware from File`, then choose `apps_data`.
11. Scroll down and click on the `vgm` folder and then select the `.uf2` file.
12. The app will begin flashing the firmware to your Video Game Module. Wait until the process is complete. Afterwards, connect your HDMI display and power on your Video Game Module to view the screensaver.

**Method 2**
1. Download your preferred screensaver.
2. Press and hold the `BOOT` button on your Video Game Module for 2 seconds.
3. While holding the `BOOT` button, connect the Video Game Module to your computer via USB. A new device named `RPI-RP2` should appear in your device list.
4. Drag and drop the downloaded file onto the device. The device will automatically disconnect after the file transfer completes. Connect your HDMI display and power on your Video Game Module to view the screensaver.
