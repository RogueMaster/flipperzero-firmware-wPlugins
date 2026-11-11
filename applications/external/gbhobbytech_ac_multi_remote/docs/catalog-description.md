# A/C Master

Control supported air conditioners with the Flipper Zero infrared transmitter. Select a saved unit or use the setup wizard to try manufacturer-specific protocol variants. If identification does not work, learn and replay the Normal and Off signals from an existing remote.

## Features

- Control power, temperature, mode and fan speed for supported decoded protocols.
- Save and name multiple unit profiles.
- Try ordered protocol variants selected by manufacturer.
- Learn and replay Normal and Off signals as a fallback.

## Compatibility

The bundled library includes Mitsubishi Electric, Mitsubishi Heavy Industries, Gree and Fujitsu variants. Protocol coverage varies by model. Mitsubishi Electric 144-bit control and the MHI control path have been confirmed locally. Other included variants still need hardware verification. Learned signal profiles provide replay only when the signal cannot be decoded for direct state control.
