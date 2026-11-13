# Flipper-Zero-Tone-TX

![tonetx](tonetx1.png)

Flipper Zero transmitter for RTTL, .FMF and Morse code.

It will transmit your RTTL or FMF files over PMR/LPD frequencies.

USE ONLY IN MOMENTUM DEV


# Tone TX v1.0 for Flipper Zero

Tone TX v1.0 is a specialized Flipper Zero application designed to transmit audio tones, music files (`.fmf` or compatible text formats), and Morse code messages over sub-GHz radio frequencies (**PMR446** and **LPD433 bands**) using async 2-FSK modulation.

---

## Key Features

* **Dual Band Support:** Switch between **PMR446** (channels 1–16) and **LPD433** (channels 69 channels) with real-time frequency calculations.
* **Music & Note Parsing:** Supports `.fmf` files, RTTTL melodies, and custom note lists (automatically parsed into square-wave frequency segments).
* **Morse Code Fallback:** If a text file contains standard text instead of notes, the app automatically translates and transmits it in Morse code (fixed tone at 700 Hz).
* **Persistent File Browser:** Remembers the last selected file path on the SD card so you can quickly resume your workflow after restarting the app.
* **Safe Lab Design:** Built with a pre-transmission squelch opener (lead tone) and automated safety limiters for testing inside verified Faraday cages.

---

## Technical Overview

* **Modulation & Encoding:** 2-FSK Asynchronous (Deviation ~2.38 kHz, keeping it under the 2.5 kHz analog PMR limit).
* **Frequency Ranges:**
  * **PMR446:** 446.00625 MHz to 446.19375 MHz (12.5 kHz step spacing, 16 channels).
  * **LPD433:** 433.075 MHz to 434.775 MHz (25 kHz step spacing, 69 channels).
* **Audio & Tone Limits:** Frequencies clamped between 300 Hz and 3,000 Hz. Morse tone set at 700 Hz with 80 ms unit duration.
* **Storage Path:** Remembers the last opened file location via `/ext/apps_data/tone_tx/last_path.txt`.

---

## How to Use & Controls Guide

### 1. Initial Menu
When launching the app, you will see the main menu:
* **UP / DOWN:** Select an option (*Start*, *Disclaimer*, *About*).
* **OK:** Enter the selected section.
* **BACK:** Exit the application.

---

### 2. Main Transmission Screen
Once you select **Start**, you enter the main control interface:
* **LEFT / RIGHT:** Change the active channel (PMR: 1–16, LPD: 1–69).
* **DOWN:** Switch between frequency bands (**PMR446** and **LPD433**).
* **UP:** Open the file browser to select a `.fmf` or `.txt` file from your SD card.
* **OK:** Start transmitting the loaded sequence.
* **BACK (during TX):** Safely aborts and stops the ongoing transmission.
* **BACK (on Main Screen):** Returns to the initial Start/Disclaimer/About menu.

---

## File Formats Supported

1. **`.fmf` / Music Files:** 
   * Supports metadata headers such as `BPM:`, `Duration:`, `Octave:`, and `Notes:`.
   * Supports RTTTL ringtone strings.
   * Supports plain text sequences of note tokens (e.g., `4C 4E 4G`).
2. **Standard `.txt` Files:**
   * If the text file does not contain music notes, the application automatically encodes and transmits the text content as **Morse Code** (using a 700 Hz audio tone).

---

## Disclaimer & Legal Notice

> **IMPORTANT DISCLAIMER**
> 
> 1. **Lab & Testing Only:** This software and application are provided strictly for research, testing, development, and educational purposes inside a verified Faraday cage.
> 2. **RF Regulations Compliance:** Users are strictly responsible for complying with all local radio frequency laws and telecommunication regulations. Transmitting on restricted or licensed bands (such as PMR446 or LPD433 outside authorized test parameters or without proper licensing where required) is illegal.
> 3. **No Warranty & Limitation of Liability:** The developer assumes no responsibility or liability for any direct, indirect, incidental, or consequential damages, legal consequences, or issues arising from the use, misuse, or inability to use this application.
> 4. **Use at Your Own Risk:** The developer does not encourage, support, or authorize any illegal, unauthorized, or harmful use of this tool. Use this application entirely at your own risk.
