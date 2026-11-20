v1.1:
- Spirit Box: added a real FM broadcast mode using an optional TEA5767 module
  over I2C (external bus on pins 15/16) - genuine FM audio out the module's
  own speaker jack, with real I2C hardware mute and a "module not found"
  fallback instead of hanging if it's not wired/powered
- Spirit Box: default sweep dwell is now 170ms (matches the SB7 Pro's own
  recommended rate) and the adjustable range goes down to 20ms (faster than
  any commercial spirit box)
- EMF Meter: external Hall sensor mode now measures deviation from a
  baseline captured when that mode starts, instead of a Goertzel 50/60Hz
  filter - the filter could hang the whole device after a while, and even
  working, only caught AC ripple, not the slow DC-ish shift a real magnet
  causes
- EMF Meter: fixed a real unit bug where the ADC-to-voltage conversion was
  multiplied by 1000 as if it returned volts, when it already returns mV -
  readings were 1000x too large
- EMF Meter: added an adjustable calibration margin (▲ ▼) subtracted from
  the raw reading before it's classified into a level/color, for re-zeroing
  against a noisy real environment without capping how high it can still
  read if the signal genuinely increases
- About screen: replaced the single scrollable wiring-diagram text with
  one page per module (EMF, REM Pod, Spirit Box) listing just the pins,
  plus a GitHub QR code page, switchable with ◀ ▶
v1.0:
Initial release.
- EMF Meter: internal ambient-RF mode (CC1101 RSSI sweep) and optional
  external Hall sensor mode (Goertzel 50/60Hz mains-hum filter)
- Spirit Box: real CC1101 sub-GHz sweep with raw RF static mirrored to
  the speaker
- REM Pod: capacitive proximity detection via ADC peak-to-peak jitter on
  a copper wire antenna
- Startup splash screen and main menu with a hand-drawn ghost icon
