# Hard Hat Brigade Flipper Zero App
Flipper Zero app. This application sends infrared messages to the hardhat that will be available at DEF CON 33.

### Running the application

Launch app on Flipper Zero, type your message, point the device at the receiver, and press Save. After a brief pause and a vibration, the message is transmitted. Once you see "Message sent" and hear the success tones, the hardhat should display your message.

### Installing the application

Copy the hhb_ir_app.fap file to your Flipper Zero's SD Card, in the Apps/Infrared folder.

On the Fliper Zero, choose Apps, Infrared, HHB IR App.

### Building the app (optional)

This application uses documented APIs, so you can use uFBT tool to build and launch the application. See https://github.com/jamisonderek/flipper-zero-tutorials/wiki/UFBT

1. Install uFBT tool.
2. If using custom firmware, switch to target firmware.
3. Change to the proper channel/branch.
4. Connect Flipper Zero to your computer and exit any application on the Flipper Zero.
5. Make sure qFlipper and lab.flipper.net are not running on your computer.
6. run "ufbt launch"

### Technical details

Infrared signals are sent from the Flipper Zero to the hardhat.

- The protocol used is the NECext protocol: 
  - 38 kHz carrier, 33% duty cycle.
  - Preamble: 9.0ms lead/4.5ms space.
  - Data 0: 560uS burst/560uS space. Data 1: 560uS burst/1690uS space.
  - Data is LSB (Least Significant bit first).
  - 16-bit address (sent after preamble).
  - 16-bit command (send after address).
  - End of tx: 560uS burst/110ms space.
  
- All messages have an address of 0x1337.
- Each ASCII character of the message is sent as the command.
- Additional 50ms delay between messages (transmitted characters).
- The message is terminated with command 0x4 (EOT).

- Example: Sending a message of "Q" (ASCII 0x51, followed by 0x4 EOT). 
  - Positive number represents microseconds of modulated signals.
  - Negative number is no modulated signal present.
  - 9000 -4500                                   (Preamble)
  -  560 -1690  560 -1690  560 -1690  560  -560  (1110 -> 0111b = 7)
  -  560 -1690  560 -1690  560  -560  560  -560  (1100 -> 0011b = 3)
  -  560 -1690  560 -1690  560  -560  560  -560  (1100 -> 0011b = 3)
  -  560 -1690  560  -560  560  -560  560  -560  (1000 -> 0001b = 1)
  -  560 -1690  560  -560  560  -560  560  -560  (1000 -> 0001b = 1)
  -  560 -1690  560  -560  560 -1690  560  -560  (1010 -> 0101b = 5)
  -  560  -560  560  -560  560  -560  560  -560  (0000 -> 0000b = 0)
  -  560  -560  560  -560  560  -560  560  -560  (0000 -> 0000b = 0)
  -  560 -110000                                 (End of sending "Q", 0x51)
  - 9000 -4500                                   (Preamble)
  -  560 -1690  560 -1690  560 -1690  560  -560  (1110 -> 0111b = 7)
  -  560 -1690  560 -1690  560  -560  560  -560  (1100 -> 0011b = 3)
  -  560 -1690  560 -1690  560  -560  560  -560  (1100 -> 0011b = 3)
  -  560 -1690  560  -560  560  -560  560  -560  (1000 -> 0001b = 1)
  -  560  -560  560  -560  560 -1690  560  -560  (0010 -> 0100b = 4)
  -  560  -560  560  -560  560  -560  560  -560  (0000 -> 0000b = 0)
  -  560  -560  560  -560  560  -560  560  -560  (0000 -> 0000b = 0)
  -  560  -560  560  -560  560  -560  560  -560  (0000 -> 0000b = 0)
  -  560 -110000                                 (End of sending "EOT", 0x4)

### Credits

Code credit goes to @jamisonderek, the majority of the code is taken from his Basic Scenes tutorial, https://github.com/jamisonderek/flipper-zero-tutorials/tree/main/ui/basic_scenes. 
Check out his YouTube channel here: https://www.youtube.com/@MrDerekJamison
