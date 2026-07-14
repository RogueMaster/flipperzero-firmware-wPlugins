# How to flash

Go to ESP-TOOLS

Enter esp32 into bootloader mode

Go to "Manual flash"

Place the Bootloader binary into bootloader (0x1000)

Place the partitions binary into Part Table (0x8000)

Place the firmware into firmwareA (0x10000)

Flash it in slow mode or fast mode both works for me (you need to reboot the board for start using the firmware)

Sidenote: Now Gemini_AI.bin, Gemini_AI_bootloader.bin and Gemini_AI_partitions.bin should work with all ESP32 and ESP32 S series (please let my know if doesn't work with S series)

Sidenote 2: merged-flash.bin is under revision I plan to made a 1 file to flash everything at once but I can't figure out why is not working

# Screenshots

![image](https://github.com/user-attachments/assets/879b8621-29d0-4c55-85ff-f44fa94e1d4a)

![image](https://github.com/user-attachments/assets/3ee4cfea-37d1-413d-9bfe-25451e251d07)
