## 1.2

Fixed the bug where the user block number is incorrectly handled throughout the app's saving and loading functions. Also updated the wording in configuration to be more accurate.

## 1.1

There's no functional update in this version. Only some tweaks.
1. The block number iterator was updated but then @Willy-JL noticed it causes more trouble so it was reverted. 
2. The writing success screen now uses up-to-date system icon. The changes was made by @Willy-JL in his app catalog. 
3. The functions that handle file format and directory are updated to use OFW's built-in functions instead of the old manual method. Thank @augustozanellato for introducing me to these functions. This wouldn't affect functionality. However, the .t5577 files in previous versions wouldn't be supported in this update. Please use the updated example 'examples/Tag_1.t5577' as reference. The main change is parsing the 8-digit hex block data into 4 bytes. 
4. Added Github workflow to check build each time a new commit is made. This was copied from @xtruan 's [PR](https://github.com/zinongli/KeyCopier/pull/5) to my other app.

What's Changed
* Update t5577_writer.c by @Moon-Byeong-heui in https://github.com/zinongli/T5577_Raw_Writer/pull/8
* Revert "Update t5577_writer.c" by @zinongli in https://github.com/zinongli/T5577_Raw_Writer/pull/9
* Use Stock Functions to Achieve Directory and Flipper Format functions by @zinongli in https://github.com/zinongli/T5577_Raw_Writer/pull/10

New Contributors
* @Moon-Byeong-heui made their first contribution in https://github.com/zinongli/T5577_Raw_Writer/pull/8

**Full Changelog**: https://github.com/zinongli/T5577_Raw_Writer/compare/v1.0...v1.1

## 1.0

The app is fully functional now. It supports writing and editing manually on the flipper along side all the previous features.
Note: this app is built with API 69.0 on OFW 0.104.0 release.

## 0.1

Basic functions are all ready.
Users can write to their T5577.
They can configure the modulation, RF clock, and block number using the configuration menu.
They can save the current file.
They can load an existing file.

The function to add block 1-7 data is still being developped.
For now, users would have to write their own raw data using the data file template.
Put it into apps_data/t5577_writer folder, load it inside the app, and write onto a T5577 fob.

To install, download the following .fap file, drag the .fap into your Flipper Zero's apps/RFID folder, and start writing!
