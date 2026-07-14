# RTL8720DN & Flipper Zero WiFi Attack Tool
Based on: https://github.com/tesa-klebeband/RTL8720dn-Deauther also the FAP was made based on UART example from https://github.com/jamisonderek/flipper-zero-tutorials
## Overview
This project utilizes the **RTL8720DN** WiFi module in combination with the **Flipper Zero** to perform various **WiFi penetration testing attacks** on **2.4GHz** and **5GHz** networks. The implemented attacks include:

- **Deauthentication Attacks** – Disconnect clients from a network by sending deauth packets.
- **Evil Portal** – Create a fake access point with a captive portal.
- **Beacon Flooding** – Generate multiple fake SSIDs to confuse or disrupt WiFi networks.

This project is intended **for educational and security research purposes only**. Ensure you have **explicit permission** before using these techniques on any network.

---

## Hardware Connections
To connect the **RTL8720DN** to the **Flipper Zero**, follow the wiring diagram below:

| RTL8720DN | Flipper Zero |
|-----------|--------------|
| 5V        | (1) 5V           |
| GND       | (8) GND          |
| RX1       | (13) TX           |
| TX1       | (14) RX           |

Be sure to connect the RTL8720DN on the TX1 and RX1 port instead of RX0 and TX0. I preffered use the second port because the first one had a lot of loggins messages.

Also you can connect the RTL8720DN with 3V but it isn't redommended since the 3v GPIO port is shared with de SD card and it could be corrupted.

---

## Requirements
- **Flipper Zero** with custom firmware
- **RTL8720DN** WiFi module
- Wires for connection

---

## Installation & Usage
1. Connect the **RTL8720DN** to the **Flipper Zero** using the above wiring setup.
2. Flash the necessary firmware onto the **RTL8720DN**.
3. Build and install the flipper zero FAP

---

## Future Improvements
- **Visual Indicator for Beacon Frame Attacks**: Implement an LED indicator on the **RTL8720DN** module to provide real-time feedback on beacon frame attacks.
- **Customizable WiFi Scanning Parameters**: Allow users to define specific parameters for scanning WiFi access points to improve efficiency and flexibility.

---

## Disclaimer
**This project is for educational and authorized security testing only.** Unauthorized use of WiFi attacks is illegal in many jurisdictions. The creators are not responsible for any misuse or legal consequences arising from this project.

---

## License
This project is licensed under the **MIT License**. Feel free to modify and contribute.

