# Fiscal drive tester

A fiscal drive (FN) is a device that Russian law requires in every cash register. It stores receipts and sends them to the tax authorities. FN Tester connects the Flipper Zero to a fiscal drive over UART and shows the state of the drive in plain language, so you can check a drive before installing it into a cash register or diagnose a faulty one.

The interface is available in English and Russian. On the first launch the app asks for the language and saves the choice on the SD card; the language can be changed later with the Language menu item. The screenshots show the English interface.

# What you need

A fiscal drive (FN or FN-M) connected to the Flipper Zero:

- drive TX to pin 13, drive RX to pin 14
- GND and +3.3 V from the Flipper
- USART at 115200 baud, 8 data bits, no parity

The app occupies the GPIO pins while it runs and disables the expansion module detection. The USB console stays available for logs. The only thing written to the SD card is the language setting in apps_data/fn_test/settings.txt.

# Information about the drive

- Serial number
- Life stage
- Shift state and the document waiting for the OFD
- Current and maximum supported FFD version
- Software version and type: MGM (test drive) or production drive
- Warning flags of the drive

# Life cycle information

- End date of the drive and the remaining days
- Number of registrations and re-registrations, used and remaining
- Memory resource: 5-year and 30-day storage
- Marking notification area, when the drive reports it

# Reset MGM

A test drive (MGM) can be reset with command 60 16. The app checks that the drive is an MGM by its 33h data and asks for an explicit confirmation; the command is sent exactly once and cannot be cancelled while it runs (up to 30 seconds). A production drive is never reset.

# Controls

- Menu: Up and Down to choose, OK to open, Back to exit
- Reading: Back cancels the operation
- Info and result screens: Up and Down scroll lines, Left and Right switch pages, Back returns to the menu
