# Bar code scanner emulator
### Scanner work at COM-port emulation. 
### Characteristics
* Speed: 19200 (Bit Rate)
* VID: 1155 | PID: 22336
* Name: Flipper {Your Flipper name}
* Manufacturer Name: Flipper Devices Inc.
* SerialNumber=flip_{Your Flipper name}

## Installation
[Flipper APP Catalog](https://lab.flipper.net/apps/bc_scanner)

1. Copy bc_scanner.fap file in Flipper SDCard (apps folder)
2. Open bc_scanner app, for creating bcscanner folder, or create a folder manually via qFlipper 
3. Add bar code files (.bc) in "bcscanner" folder

## GUI
### App icon and name
![AppIcon](https://github.com/polarikus/flipper-zero_bc_scanner_emulator/assets/59930161/6800cb98-0cf7-4e97-ba0e-2c870c7b5d9b)
### File Select
After open app - select bar code to be emulated

![BcFilemanage](https://github.com/polarikus/flipper-zero_bc_scanner_emulator/assets/59930161/93f5289f-1847-4b45-9684-ac2f99a27a88)
### Emulation
1. Click "Run" Button

![Run](https://user-images.githubusercontent.com/59930161/214501072-33a32970-7f1c-4f3b-b6f0-39bb4f2201ce.png)
![Work](https://user-images.githubusercontent.com/59930161/214501068-ad11e74f-b501-408b-939b-8d2d7748e542.png)

When sending completed You will see 100% on the display:

![End](https://user-images.githubusercontent.com/59930161/214501074-f567af0c-16d3-4287-81e6-a96ebb6a57f2.png)

2. If you need to resend, click "Run" again 
3. To exit - press the "Back" button on the flipper

## Create Bar Code File
1. Open the .txt file in an editor that recognizes GS special characters, such as sublime text
2. Add a barcode in any format, such as GS1 format
3. Save file and change extension on .bc
4. Copy file in Flipper SDCard, to "/bcscanner" folder

File example (change the extension to .bc) [test_evotor.txt](https://github.com/polarikus/flipper-zero_bc_scanner_emulator/files/14998160/test_evotor.txt)
