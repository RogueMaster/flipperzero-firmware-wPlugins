# Application Submission

Xbox POST Code Reader v0.14 — turns a Flipper Zero into a portable Xbox boot diagnostic reader. It emulates the MAX6958 POST display at I2C address 0x38 and captures four-digit POST codes directly from a supported Xbox motherboard, then decodes them offline.

Features:

- Emulates the MAX6958 POST display at I2C address 0x38
- Captures four-digit POST codes without blocking the Flipper controls
- Shows one current code or a scrolling 128-entry live log
- Decodes selected codes locally using 452 bundled console-aware records
- Browses known codes by All, POST, Error, SMC, SP, CPU, or OS category
- Saves captures as automatically numbered SD-card logs and reads them back
- Streams live codes to the companion WebXboxPOSTTool over USB Web Serial
- Shows Xbox One and Xbox Series wiring directly in the app

This is a Flipper Zero adaptation of the PicoDurangoPOST project.

Source repository: https://github.com/coolshrimp/flipperzero-xbox-post-code-reader

## Note for the reviewer

The screenshots are the best we can replicate: this app switches the USB into UART/Web Serial mode during live capture, so the qFlipper on-device screenshot function cannot capture the app's screens. The included images were produced with the Flipper screen simulator at the native two-color resolution (128x64, scaled 4x) so they still accurately represent the on-device UI.

# Extra Requirements

- External wiring to the Xbox motherboard POST header (SDA, SCL, GND only). No power is taken from or supplied to the console by the Flipper. The bus must be 3.3 V compatible; use level shifting where required. No other external hardware is needed.

# Author Checklist (Fill this out)

- [x] I've read the [contribution guidelines](../blob/HEAD/documentation/Contributing.md) and my PR follows them
- [x] I own the code I'm submitting or have code owner's permission to submit it
- [x] I have performed a self-review of my own code
- [x] I have commented my code, particularly in hard-to-understand areas
- [x] I [have validated](../blob/HEAD/documentation/Contributing.md#validating-manifest) the manifest file(s) with `python3 tools/bundle.py --nolint applications/CATEGORY/APPID/manifest.yml bundle.zip`

# AI usage disclosure (Fill this out):

- [x] Partially AI assisted (clarify below which code was AI assisted and briefly explain what it does).
- [ ] Fully AI generated (explain what all the generated code does in moderate detail).

- AI assistance was used for portions of the app code (the I2C POST capture state machine, the offline decode UI, SD-card logging, and USB Web Serial streaming) and for preparing and validating this catalog manifest. The app emulates a MAX6958 POST display over I2C, captures four-digit Xbox POST codes, and decodes them from a bundled offline database; it does not transmit or modify the console.

# Reviewer Checklist (Don't fill this out!)

- [ ] Bundle is valid
- [ ] There are no obvious issues with the source code
- [ ] I've ran this application and verified its functionality
