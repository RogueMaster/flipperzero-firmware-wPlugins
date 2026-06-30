# Music to Sub-GHz Radio

## Overview

The `Music to Sub-GHz Radio` application converts Flipper Music Files (.FMF) into a RAW .SUB file format that can be transmitted over the Sub-GHz radio! The Flipper Zero can receive the music and play it back.

There are large collections of songs available for the Flipper Zero that can be converted, for example [UberGuidoZ collection](https://github.com/UberGuidoZ/Flipper/tree/main/Music_Player). The `Music to Sub-GHz Radio` application supports converting both .FMF (Flipper Music Format) files and .TXT files into the .SUB file format.

To listen to the music without using the Sub-GHz radio, you can use the [Flipper Zero Music Player](https://lab.flipper.net/apps/music_player) application.

## How to convert music

1. Download [Music to Sub-GHz Radio](https://lab.flipper.net/apps/fmf_to_sub) from lab.flipper.net.
2. Open the `Music to Sub-GHz Radio` application.
3. Select the `Configure` option.
4. Choose the `Frequency` you want to transmit on.
5. Choose the `Modulation` you want to use (AM650 is a good default choice).
6. Click the `Back` button.
7. Select the `Convert` option.  This will display the number of the sub file.
8. You can use `Left` and `Right` buttons to change the number of the sub file.
9. Click the `OK` button and then choose the music file you want to convert.

- You will see a status of "Converting..."
- After a few seconds you should see the message "Saved in Sub-GHz folder"
- A new file will be saved in `SD Card/subghz/` with a name like "Flip5.sub".

## Send music with Flipboard Signal

The easiest way to send music is to use the [Flipboard Signal application](https://lab.flipper.net/apps/flipboard_signal).

1. Download [Flipboard Signal](https://lab.flipper.net/apps/flipboard_signal) from lab.flipper.net.
2. Connect your [FlipBoard](https://github.com/makeithackin/flipboard) to your Flipper Zero.
3. Open the `Flipboard signal` application.
3. Choose `Start application`.
4. Click a button (or button combination) on your FlipBoard.
- NOTE: You may want to go to configuration and disable playing a tone (press `left` button until you get to the `Off` value).

## Receive music with Flipper Zero

1. Open the `Sub-GHz` application on your Flipper Zero.
2. Choose `Read RAW`.
3. Click `Left` to go to `Configure`.
4. Choose the `Frequency` & `Modulation` you want to receive on.
5. Set `Sound` to `On`.
6. Choose `Back`.
7. Click `OK` to start `Rec`.

## Send music with Sub-GHz Radio

1. Open the `Sub-GHz` application on your Flipper Zero.
2. Choose `Read RAW`.
3. Click `Left` to go to `Configure`.
4. Set `Sound` to `On`.
5. Choose `Back`.
6. Choose `Back` to the main Sub-GHz menu.
7. Choose `Saved`
8. Select the `Flip#.sub` file you want to send.
9. Click `OK` to send.

## Support

If you have need help, we are here for you. Also, we would love your feedback on cool ideas for future FlipBoard applications!  The best way to get support is to join the Flipper Zero Tutorials (Unofficial) Discord community. Here is a [Discord invite](https://discord.gg/KTThkQHj5B) to join my `Flipper Zero Tutorials (Unofficial)` community.

If you want to support my work, you can donate via [https://ko-fi.com/codeallnight](https://ko-fi.com/codeallnight) or you can [buy a FlipBoard](https://www.tindie.com/products/makeithackin/flipboard-macropad-keyboard-for-flipper-zero/) from MakeItHackin with software & tutorials from me (@CodeAllNight).