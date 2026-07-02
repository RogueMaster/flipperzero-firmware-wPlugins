# APGS02 Sleep Button Capture

Source file: `APGS02_Sleep_Button_Test.ir`

These captures were taken from the Daikin APGS02 remote to identify what the physical `SLEEP` button changes in the Daikin64 frame.

## Captures

| Capture | Decoded Daikin64 bytes | Notes |
|---------|------------------------|-------|
| `Sleep_Off_Baseline` | `94 26 18 14 03 20 12 16` | Baseline with Sleep off |
| `Sleep_On` | `B6 26 18 14 03 20 12 16` | Sleep enabled |
| `Sleep_Off_After_Toggle` | `A4 26 18 14 03 21 12 16` | Sleep disabled again |

## Interpretation

The physical `SLEEP` button toggles bit `0x02` in the first displayed byte's low nibble. The checksum high nibble changes with it.

The third capture also changes `0x20` to `0x21` in a clock/timer byte. That appears to be elapsed clock state rather than the Sleep flag itself.

This confirms the app's current Sleep implementation: `state.sleep` maps to the Daikin64 sleep flag bit.
