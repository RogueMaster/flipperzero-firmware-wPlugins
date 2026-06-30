Changelog:

v1.0 - 2024-05-29:
- Initial release of Simultaneous UHF RFID Flipper Zero App.

v1.1 - 2024-08-29:
- Support for the YRM100 added 
- Visual changes to read and write screens to show CRC and PC
- Added new configuration options like region, default access password to use for reading, writing, locking
- Can lock individual memory banks
- Can permanently inactivate a tag using the kill password 
- This version doesn't fully support these features for the M6E and M7E, however, these will be added in the next release!

v1.1.1 - 2025-03-14:
- Added ability to pause EPC values scrolling on read screen by holding down up arrow
- Changed label for connection setting in configuration menu to display the current reader connection status
- Performed some debugging and found that using a baud rate of 384000 for the YRM100 series readers results in the optimal performance 
- In the next release, I will be fixing reading for the other baud rates 9600 and 115200

v1.1.2 - 2025-05-03:
- Added bug fixes to make reading more consistent with all baud rates
- Added error handling and UI enhancements to prevent users from changing settings before connecting to the reader.