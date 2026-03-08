# CAN Commander: CAN Bus Reverse Engineering Tool

## Introduction
CAN Commander is a comprehensive tool designed for the reverse engineering of CAN (Controller Area Network) bus systems. This project aims to provide a robust platform for automotive enthusiasts, engineers, and security researchers to interact with and analyze CAN networks, facilitating a deeper understanding and manipulation of vehicular communication systems.

## Features
- **Message Injection**: Send custom CAN messages to test responses from different modules.
- **Message Logging**: Record and log CAN traffic for analysis.
- **Network Sniffing**: Monitor the CAN network to observe communication between different components.
- **Message Decoding**: Decode CAN messages and understand the underlying data structures.
- **Man-in-the-Middle Capabilities**: Use as a set and forget MITM device to do in-place packet swapping.
- **User-friendly Interface**: An intuitive interface for interacting with the CAN network. -App Version
- **Real-time Data Visualization**: Graphical representation of the CAN traffic for easier analysis.
- **DTC and Diagnostics**: Get all the features of a standard OBDII PID scanner
- **Wireless Options**: Communicate via wire tap, WiFi, or Bluetooth Low-Energy (BLE)

## Getting Started
To get started with CAN Commander, follow these steps:

1. **Clone the Repository**:
   - Next install autowp's mcp2515 library: https://github.com/autowp/arduino-mcp2515

1.5 **Using the CAN Commander Official Board**:
   - All CAN Commander Official Boards ship pre-flashed :)
   - To update use the update tool or download the latest code from this repo
   - New instructions for the CAN Commander FlipperZero app will be posted in the coming days.
   - All functionality will be accessible via a complete GUI interface with easy to use buttons/toggles
   - This includes access to the on board GPS, the external CC1101 module, and the user programmable Neopixels
     
2. **Setup and Installation**:
   1. **Flashing the Main Application**:
      - The core of CAN Commander is the `CAN_Commander.ino` script.
      - This script must be flashed onto a compatible microcontroller like Arduino Uno, Mega, or Nano using the Arduino IDE.

   2. **MCP2515 CAN Controller**:
      - The MCP2515 is a CAN (Controller Area Network) controller that interfaces with microcontrollers over SPI (Serial Peripheral Interface).
      - It typically comes as a breakout board which you need to connect to your microcontroller.
    
       - Connect the MCP2515's MOSI, MISO, SCK, and CS pins to the corresponding SPI pins on the Arduino. **Please Verify for your Arduino**
       - For Arduino Uno, Mega, and Nano, the SPI pins are usually located as follows:
          - **MOSI**: Pin 11
          - **MISO**: Pin 12
          - **SCK**: Pin 13
          - **CS**: Pin 10 (You can use other digital pins, but Pin 10 is standard for most libraries)

**Using 3.3V Logic Microcontrollers**
- If you're using a microcontroller like the ESP32 or Raspberry Pi Pico, which operate at 3.3V logic, you need a level shifter.
- The level shifter will convert the 5V signals from the MCP2515 to 3.3V, making it safe to connect to these microcontrollers.

**Optional: Mobile App**
    In addition to the main `CAN_Commander.ino` application, there is also a mobile app available. This is not the main application and is optional. If you choose to use it, you can find the source code in the `AndroidEnvironment/CANCommanderApp_BLE_Edition` and `AndroidEnvironment/CANCommanderApp_WiFi_Edition` directories.
    1. **Running the Tool**:
    - Flash CAN_WiFi to an ESP32 or WiFi enabled Arduino

## Usage
- **Basic Operations**:
  
  - **Reading ALL CAN data:** Input `1` into the terminal to start reading all CAN traffic. You can input `a` to convert the received message data into *ASCII*.
    
  - **Filter and Read:** Input `4` into the terminal, then input a **Filter Mask** in *HEX*, finally input the **Frame ID** you want to filter and hit enter. The default Filter Mask is 7FF and can be used by just pressing enter at that step.
    
  - **Sending CAN messages:** Input `2` into the terminal, then input the **Frame ID** you want for your message, next input the data of the message in **HEX** format. Then specify the send/message frequency of your frame in messages per second. (Input `5` to send 5 messages per second)
    
- **Advanced Features**:
  
  - **Tracking Data Changes:** Input `5` into the terminal to enter the data tracker. Then input a **Filter Mask**, then the **Frame ID** you want to track and hit enter. The program will now only show you when the data of that CAN ID changes and what the changes are.
  
  - **Tracking Min/Max Values:** Follow the **Tracking Data Changes**, then type `max` at any time to display the max value for that **Frame ID**. Likewise, type `min` at any point to display the minimum value. This resets upon restarting the program/function.
    
  - **View Diagnostics Data:** Input `6` into the terminal to enter diagnostics mode/PID manager. Then enter the PID you wish to view in hex format. These are universal codes which can be found online, or by enter the code `0x0` as the PID. Several PIDs are listed when you enter diagnostics mode to get you started. This will be greatly expanded soon with support for all available PIDs. A DTC (Diagnostic Trouble Code) viewer and clearer are in the works and will be released soon including the ability to clear 'permanent' DTC and turn off the check engine light. Stay tuned!

## Hardware Requirements
- Microcontroller (Arduino Uno, Mega, Nano, Pico, ESP8266, ESP32)
- MCP2515 (Make sure you verify the crystal 8/16/etc)

![20240113_010817](https://github.com/MatthewKuKanich/CAN_Commander/assets/113921492/410fce00-e54a-4fb6-ad24-4d3d278d5a5b)

## Software Dependencies
- Arduino IDE
- USB, WiFi, or Bluetooth Serial Terminal

## Contributing
I welcome contributions to the CAN Commander project! I am currently working on the contributing guide and related documents. I'll moderate PRs regularly until this is finished so you can still contribute.

## Acknowledgments
- Thanks to autowp for the amazing mcp2515 library: https://github.com/autowp/arduino-mcp2515
