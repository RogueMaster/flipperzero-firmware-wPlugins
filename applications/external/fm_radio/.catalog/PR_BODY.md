# Application Submission

FM Radio v2.2 — turns the Flipper Zero into an FM broadcast radio by controlling an external TEA5767 or Si4703 tuner board over I2C.

Features:

- Supports both TEA5767 and Si4703 tuner boards, auto-detected at startup (or force a chip in Settings)
- Station presets loaded from editable .txt files on the SD card, organized by region, with an in-app file viewer
- Seek up/down with adjustable seek strength, plus fine tuning in 0.1 MHz steps
- Volume control on Si4703 (Low / Med / High / Mute) with instant-mute on long OK
- Audio mode: Stereo, Mono (Left), or Mono (Right)
- Start Volume and Resume Station settings; last station and volume persist across runs
- Settings saved automatically to SD card

Source repository: https://github.com/coolshrimp/flipperzero-fm-radio

# Extra Requirements

- Requires an external FM tuner board (TEA5767 or Si4703) wired to the I2C pins: VCC to 3V3 (pin 9), GND to GND (pin 18), SCL to C0 (pin 16), SDA to C1 (pin 15). The Si4703 additionally requires RST to A4 (pin 4) and SEN tied to 3.3V.
- Headphones or a speaker connected to the tuner board's audio output.

# Author Checklist (Fill this out)

- [x] I've read the [contribution guidelines](../blob/HEAD/documentation/Contributing.md) and my PR follows them
- [x] I own the code I'm submitting or have code owner's permission to submit it
- [x] I have performed a self-review of my own code
- [x] I have commented my code, particularly in hard-to-understand areas
- [x] I [have validated](../blob/HEAD/documentation/Contributing.md#validating-manifest) the manifest file(s) with `python3 tools/bundle.py --nolint applications/CATEGORY/APPID/manifest.yml bundle.zip`

# AI usage disclosure (Fill this out):

- [x] Partially AI assisted (clarify below which code was AI assisted and briefly explain what it does).
- [ ] Fully AI generated (explain what all the generated code does in moderate detail).

- AI assistance was used for portions of the app code (Si4703 driver integration, chip auto-detection, and settings/station-list persistence) and for preparing and validating this catalog manifest. The app tunes TEA5767/Si4703 FM radio chips over I2C and manages station preset files on the SD card.

# Reviewer Checklist (Don't fill this out!)

- [ ] Bundle is valid
- [ ] There are no obvious issues with the source code
- [ ] I've ran this application and verified its functionality
