<!-- Source: https://docs.flipper.net/zero/mobile-app/bug-report -->
<!-- Mirrored: 2026-07-14 -->

# Reporting Mobile App bugs

## Submitting a bug report via Flipper Mobile App

If you experience issues with the Flipper Mobile App, you can submit a bug report directly from the application by following these steps:

1. In the main menu tab, go to **options**
2. Tap **report bug**
3. In the title field, enter a short description of the issue
4. In the description field, enter the steps that led to the issue, as well as expected and actual results
5. Make sure that Flipper Mobile App logs are attached to the report
6. Tap **submit** to share the bug report with developers
7. (Optional) Copy the issue ID to submit it on the forum

## Submitting a bug report via the forum

If you want developers to address your issue sooner, or if you believe the bug affects your Flipper Zero, you can share information on the forum at https://forum.flipperzero.one/

Follow these steps:

1. Go to the mobile app section
2. Read the latest topics to ensure this issue hasn't been submitted before
3. If it hasn't, create a new topic by clicking the new topic button
4. Enter a short description of the issue in the topic name field
5. Click the + icon and select your mobile operating system tag
6. Describe the steps that led to this issue, as well as expected and actual results
7. Enter information about your Flipper Zero firmware version
8. Enter the issue ID from Flipper Mobile App (optional)

### Collecting Flipper Zero logs and device info

If the bug affects your device:

1. On your Flipper Zero, go to **Main Menu > Settings > System**
2. Set log level to **debug**
3. Connect your Flipper Zero to your computer via USB cable
4. On your computer, run Google Chrome or another Chromium-based browser and go to https://googlechromelabs.github.io/serial-terminal/
5. In the online terminal, set the baud rate parameter to **230400** and click connect
6. Select your Flipper Zero from the list of devices and click connect
7. In the opened Flipper Zero CLI, enter the **info device** command and press return
8. After getting the device info, enter the **log** command and press return
9. In Flipper Mobile App, repeat the steps leading to the bug
10. Download the file with device data and logs by clicking the download output button
11. Open the file in a text editor and delete data in the hardware UID and hardware name lines for privacy reasons
12. Attach the edited file to the topic by clicking the upload icon
13. Click the create topic button to publish the information on the forum
