# T5577 Raw Writer App
An easy to use T5577 raw writer app for Flipper Zero. [Discord project page.](https://discord.com/channels/1211622338198765599/1267190551783018659)
## Instruction
Configure the modulation, RF Clock, number of blocks, and block data in the 'Config' menu. 

Or, you can load a .t5577 file into the app and write it. An example file can be found [here](https://github.com/zinongli/T5577_Raw_Writer/blob/main/examples/Tag_1.t5577). The configuration will be automatically loaded from block 0 data. 

The texts like:

'Modulation: ASK/MC'

'RF Clock: 64'

'Number of User Blocks: 7'

in the .t5577 files are derived from the block 0 data when saved. So if you want to adjust the configuration, you can simply edit block 0 data. Or, you can load the data directly and adjust the configuration in the app before writing tags. Editing the texts mentioned above wouldn't work. 

You can also save the data you've just loaded and/or configured. 

## Future goals
- [ ] Writing light blink
- [ ] Write page 1
- [ ] Write with password
- [ ] Load and automatically parse PM3 .json dumps
- [ ] Emulation

## Special Thanks
Thank [@jamisonderek](https://github.com/jamisonderek) for his [Flipper Zero Tutorial repository](https://github.com/jamisonderek/flipper-zero-tutorials) and [YouTube channel](https://github.com/jamisonderek/flipper-zero-tutorials#:~:text=YouTube%3A%20%40MrDerekJamison)! This app is built with his Skeleton App and GPIO Wiegand app as references. 
