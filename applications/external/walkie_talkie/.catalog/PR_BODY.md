# Application Submission

Walkie Talkie v1.1 — a receive-only FRS channel activity and RSSI scanner using the Flipper Zero's built-in CC1101 radio.

Features:

- All 22 standard FRS channels (462/467 MHz) in a scrollable channel list
- Channel scanner: scans up or down, pauses automatically when a signal is detected, and resumes after the transmission ends
- Auto-squelch with adjustable sensitivity
- Live RSSI readout with a 5-bar signal-strength meter
- Subchannel (privacy-code) labels 1-38 for matching a handheld radio's display

Receive-only: the app never transmits, and it does not decode CTCSS/DCS. The CC1101 data mirror is a digital receive-data stream, not demodulated analog voice, so the app is presented as a channel activity/RSSI scanner rather than a voice receiver.

Source repository: https://github.com/coolshrimp/flipperzero-walkie-talkie

# Extra Requirements

- None. Uses the Flipper Zero's built-in CC1101 radio; no external hardware required.

# Author Checklist (Fill this out)

- [x] I've read the [contribution guidelines](../blob/HEAD/documentation/Contributing.md) and my PR follows them
- [x] I own the code I'm submitting or have code owner's permission to submit it
- [x] I have performed a self-review of my own code
- [x] I have commented my code, particularly in hard-to-understand areas
- [x] I [have validated](../blob/HEAD/documentation/Contributing.md#validating-manifest) the manifest file(s) with `python3 tools/bundle.py --nolint applications/CATEGORY/APPID/manifest.yml bundle.zip`

# AI usage disclosure (Fill this out):

- [x] Partially AI assisted (clarify below which code was AI assisted and briefly explain what it does).
- [ ] Fully AI generated (explain what all the generated code does in moderate detail).

- AI assistance was used for portions of the app code (channel scanner debouncing, squelch handling, and UI) and for preparing and validating this catalog manifest. The app tunes the built-in CC1101 to the 22 FRS channels and reports channel activity and RSSI; it does not transmit.

# Reviewer Checklist (Don't fill this out!)

- [ ] Bundle is valid
- [ ] There are no obvious issues with the source code
- [ ] I've ran this application and verified its functionality
