# Changelog

## v0.5.1

- Add one-pixel measure lines behind the notes, honoring `#BARLINEON` and `#BARLINEOFF`.
- Add By Time / By Bars seeking; bar mode follows actual TJA measure starts, including tempo, time-signature and delay changes, empty measures, and hidden barlines.
- Default to By Bars with one bar per direction; allow independent 1 / 3 / 5 / 10 bar intervals and retain the existing time-based option.
- Save both sets of intervals and preserve explicitly saved seek-mode preferences when upgrading.
- Add patch-version release packaging and builds for official firmware and both Momentum variants.

Thanks to the suggestion in [#1](https://github.com/ZeroNinx/flipper-zero-taiko-player/issues/1).

## v0.5

- Raise app master volume to 200%, retaining the 70% default and independent system volume; boosted peaks are clipped to the PCM range.
- Give Don and Ka one voice each: same-sound hits replace the previous tail with a short fade. Normal Don, drumrolls, and balloons share the Don voice.
- Preserve drum-sound pitch and natural duration across playback speeds while hit timing follows the chart.
- Add saved GPIO pulse widths from 25 to 100 ms in 5 ms steps, defaulting to 70 ms, under GPIO Setting.
- Wait for input while paused instead of repeatedly recalculating and redrawing the static chart.
- Add a separate Momentum mntm-012 build for API 87.1, confirmed working by the author on Momentum CN v1.1.6; retain official and mntm-009 builds.

## v0.4

- Add a saved target display frame rate from 15 to 60 FPS in 1 FPS steps, defaulting to 50 FPS.
- Preserve audio and hit scheduling independently of the selected display frame rate.
- Replace the drum sounds with unmodified TJAPlayer3 SimpleStyle assets and retain their MIT attribution in converted resources and packages.
- Add GPL-3.0-or-later licensing, an original device screenshot, and catalog metadata.
- Split detailed documentation into user, chart-support, and development guides, and add an English README.

## v0.3

- Prevent automatic drumroll and balloon hits from accumulating sound tails; fade out the previous automatic hit before retriggering.
- Raise the automatic-hit cap to 32 hits per second.
- Smooth big-note, drumroll, drum-indicator, and balloon pixel contours.
- Improve balloon head sizing and preserve its complete hit ring.
- Reduce repeated mixer work and reuse drawing text storage.
- Improve allocation checks and cleanup when audio startup fails.

## v0.2

- Add hit-ring feedback for normal and big Don/Ka notes.
- Refine GOGO TIME effects and align the playback-time and difficulty labels.
- Release finished audio workers and buffers promptly.
- Reduce memory peaks when changing difficulty and recover the previous chart on failure.
- Add heap exhaustion, fragmentation, retry, and speed-change rollback tests.

## v0.1

- Initial release with TJA chart playback and PCM backing audio.
- Add scrolling notes, automatic drum sounds, drumrolls, balloons, and GOGO TIME.
- Add difficulty, playback-speed, scroll-speed, seeking, and volume controls.
- Add configurable GPIO drum-hit outputs and saved preferences.
