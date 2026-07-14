## v1.7

- Add option to configure merge gap in range 1ms-1s.
- Add option to multiplicate signals when loading single or merging multiple files up to 64 times.
Multiplied signals (other then RAWs) will be seperated by synchronization gap
that comes from internal decoder. The gap is by decoders to synchronize signals without preamble.
- Removed limit of 16 merged files. There is no limit now (beside RAM capacity).
- Rewritten gap clamping mechanizm. Now long gaps are not clamped to 32ms.
The original length has been preserved instead.
- Optimized loading via transmit deserialzier.

## v1.6

- Fixed MPU fault stack overflow error on some flipper firmwares.
The bottle neck is internal deserializer called when loading recognized signals.
The change bumps the application stack from 2KB to 3KB as it seems sufficient value.

Note:
If you ever encounter this issue in the future please let me know on GitHub.
I don't want to raise this value too high if it's not needed because raising
the stack reduces the already very limited heap resources.

## v1.5

- Added merge feature for .sub files
- Added jitter normalizer option
- Added support for loading as RAW any other protocol supported by the firmware.
This was achieved by using the device's internal decoder.

## v1.4

- Added cut feature
- Added loading and saving screen views
- Added marks indicating there is more signal data on that side
- Reorganized overlapped views
- Extended max visible basename length to fit default RAW filename with its full date
- Optimized loading longer .sub files and implement limiter
- View will now automatically switch when swiched to invisible marker

## v1.3

- Apply proper icons for listed .sub files

## v1.2

- Made A/B markers recompute its position correctly in the waveform mode.
When the markers were aligned to a signal edge in the waveform mode,
its possition relative to that edge changed while zooming in/out.

## v1.1

- Adjusted namings
- Highlighted the selected A..B range with a dotted line along the top of the wave area
- Auto-number saved trims to avoid overwriting, show saved filename

## v1.0

- Allow editing RAW Sub-GHz files