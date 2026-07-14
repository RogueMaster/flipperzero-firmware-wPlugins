# Sig Diary

<div align="center">

<img src="images/sigdiary.png" style="width: 20%; display: flex;"></img>

[![GitHub release](https://img.shields.io/github/release/C0d3-5t3w/flipper-sigdiary.svg)](https://github.com/C0d3-5t3w/flipper-sigdiary/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

</div>

> A passive background scanning application for Flipper Zero that logs and annotates signals (IR/RF/NFC) based on fingerprinting. Perfect for hobbyist recon or daily security audits.

## 📋 Features

- **Multi-protocol Scanning**: Simultaneously monitors IR, RF, and NFC signals
- **Automatic Annotation**: Identifies common signals such as garage doors, remote controls, and access cards
- **Signal Logging**: Maintains a timestamped log of all detected signals
- **Interactive UI**: Browse through captured signals with detailed information
- **Persistent Storage**: Saves all signal data to the SD card for later analysis

### Starting the App

1. On your Flipper Zero, navigate to **Apps** → **Tools**
2. Find and select **Sig Diary**
3. The app will start scanning immediately

### Controls

- **Center Button**: Toggle between scanning and log view
- **Left/Right Buttons**: Navigate through logged signals (in log view)
- **Back Button**: Return to scanning mode or exit app

### Understanding the Display

#### Scanning Mode
- Shows active scanners (IR, RF, NFC)
- Displays total number of signals detected
- Notifies when new signals are detected with LED flash

#### Log View
- Shows timestamp of the signal
- Displays signal type (IR, RF, or NFC)
- Shows annotation of the signal when available
- Navigation controls at the bottom

### Signal Log File

All detected signals are logged to: `/ext/sigdiary/log.txt`

## 🔍 How It Works

Sig Diary runs three concurrent background scanners:

1. **IR Scanner**: Detects infrared signals from remote controls
2. **RF Scanner**: Monitors radio frequencies (primarily 433.92MHz, 315MHz, 868MHz)
3. **NFC Scanner**: Detects nearby NFC cards and tags

When a signal is detected, the app:
1. Records the signal's characteristics
2. Timestamps the detection
3. Attempts to identify the signal type based on fingerprinting
4. Logs the information to the persistent log file
5. Notifies the user with a brief flash

### Signal Database

Help improve signal identification by submitting information about common RF, IR, and NFC signals to our database.

## 📜 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## ⚠️ Disclaimer

Sig Diary is intended for educational and security audit purposes only. Always respect privacy and property rights when scanning for signals. The authors are not responsible for any misuse of this application.

## Credits

- Developed by [C0d3-5t3w](https://github.com/C0d3-5t3w)

---

<p align="center">
  Made with ❤️ for the Flipper Zero community
</p>
