<!-- Source: https://docs.flipper.net/zero/sub-ghz/read -->
<!-- Mirrored: 2026-07-14 -->

# Reading signals - Flipper Zero Documentation

## Sub-GHz

### Reading signals

The Flipper Zero can read and emulate remote controls using known protocols. For remotes with unknown protocols, signals can be recorded in raw format and replayed. The documentation notes that "you can read, save, and emulate different types of remote controls with known protocols."

#### How to read signals

In read mode, Flipper Zero decodes signals from remote controls based on known protocols. To capture a signal:

1. Go to Main Menu > Sub-GHz
2. Press Read, then press the button on the remote control
3. When captured, press OK, then Save
4. Name the signal and save

#### Configuration menu

Access the configuration menu by pressing Config on the scanning screen. Options include:

- **Frequency configuration**: Switch frequencies manually using left/right arrows
- **Modulation configuration**: Choose from AM270, AM650 (default), FM238, or FM476
- **BIN RAW**: Processes raw signals by eliminating noise and correcting timing errors
- **Lock keyboard**: Prevents accidental key presses while scanning

#### Frequency analyzer

To determine a remote's frequency, use the frequency analyzer feature. It "displays the frequency with the highest received signal strength indicator (RSSI) value, with signal strength higher than 90 dBm."

#### Hopping mode

For unknown frequencies, hopping mode automatically switches between available frequencies until detecting a signal exceeding 90 dBm.

#### Sending signals

Flipper Zero can transmit saved signals at frequencies permitted in your region. Select a saved signal and press Emulate, then Send.

#### Listening to walkie talkies

The Sub-GHz features can receive analog walkie talkie transmissions. Set frequency to the target channel, modulation to FM238, and enable sound for audio playback through the device's buzzer.
