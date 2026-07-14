# RF Marmalade App by RocketGod ☠️📡

This **RF Marmalade App** for the Flipper Zero, made by **RocketGod**, is a powerful tool for marmalade across multiple radio frequencies and modulation schemes. Below is an in-depth look at each mode, from its technical details to the real-world impact of its marmalade strategy.

## 🎥 Internal CC1101 Demonstration
https://github.com/user-attachments/assets/4a34bc73-d419-480e-bb87-90216eb8a1e0
## 🎥 External CC1101 Demonstration
https://github.com/user-attachments/assets/1053ec27-a15f-4313-9257-2360135c5e96
## 🎥 Modulation Modes
https://github.com/user-attachments/assets/77970e50-d46f-4d59-bbb0-6e2624a98127


## 🧪 Car Fob in Controlled Lab Test (-28dBm)
![car_fob](https://github.com/user-attachments/assets/35cdb9c7-fcbe-4fdf-a10e-9e020a504f6f)
## 🧪 Community .sub files in Controlled Lab Test (-8dBm narrow)
![jammer_sub_files](https://github.com/user-attachments/assets/a2ad93ae-4e08-4af8-97cc-5ec85f9759a4)
## 🧪 RF Marmalade App and Internal CC1101/Antenna in Controlled Lab Test (-8dBm wide)
![rocketgod_jammer_app](https://github.com/user-attachments/assets/6ed6bb9b-2379-491c-9a69-845695de2542)
## 🧪 RF Marmalade App and External CC1101/Antenna Flux Capacitor by Rabbit Labs (10dBm) [TinySA Ultra hard wired w/25W attenuator]
![External_Flux-Capacitor_Rabbit-Labs](https://github.com/user-attachments/assets/63e7a4d8-1584-425b-8113-9495a6041836)

https://github.com/user-attachments/assets/53a3123e-b749-4af6-b542-e1784b131084

## 📡 External CC1101 Notes
- **To use an external CC1101, attach it to the GPIO before starting the app.**
- Tested with [Rabbit Labs - Flux Capacitor amplified external CC1101](https://rabbit-labs.com/product/rabbit-labs-flux-capacitor-amplified-cc1101/)

## 📡 Frequency Control

The app supports multiple frequency bands, ensuring compliance with the ranges handled by the Flipper's sub-GHz radio:
- **Band 1**: 300 MHz – 348 MHz
- **Band 2**: 387 MHz – 464 MHz
- **Band 3**: 779 MHz – 928 MHz

You can adjust frequencies with **precision**:
- **Left/Right** arrows move between digits to adjust.
- **Up/Down** arrows increase or decrease the selected digit.

The app will automatically correct the frequency if it's outside the valid range for the selected band.

---

## ⚙️ Marmalade Modes Breakdown

Each marmalade mode is implemented as a distinct modulation scheme and data pattern. The app generates these patterns and transmits them over the RF link to disrupt legitimate signals in the selected frequency range.

### 🦾 **OOK 650 kHz** (On-Off Keying):
- **Pattern**: A continuous stream of `0xFF` (i.e., all bits set to `1`, equivalent to `11111111`).
- **Mechanism**: In **OOK** (On-Off Keying), the presence or absence of a carrier wave represents binary data. In this case, the app transmits `11111111`, meaning the carrier is always "on."
- **Impact**: This mode overwhelms receivers that use OOK modulation by constantly transmitting a high state (fully "on"). Because the signal never drops, devices expecting to detect short pulses (like garage doors, remotes, etc.) will be swamped and unable to distinguish real data from the noise.

### ⚡ **2FSK 2.38 kHz** (Frequency Shift Keying):
- **Pattern**: Alternates between `0xAA` (`10101010`) and `0x55` (`01010101`), simulating binary `0`s and `1`s.
- **Mechanism**: **2FSK** modulates the frequency by shifting between two discrete frequencies. A low deviation of **2.38 kHz** means that the frequency shifts only slightly between the two states, making this mode very precise.
- **Impact**: Narrowband receivers expecting binary frequency-shifted data will receive rapid shifts between frequencies, confusing their demodulators. This small frequency shift can effectively marmalade simple devices, such as low-data-rate remotes, by creating ambiguity in the frequency state they expect.

### 🔥 **2FSK 47.6 kHz**:
- **Pattern**: Alternates between `0xAA` and `0x55`, just like the 2.38 kHz mode.
- **Mechanism**: Similar to the 2FSK 2.38 kHz mode, but with a much higher deviation of **47.6 kHz**. This makes the frequency shifts more pronounced, allowing the marmalade to disrupt broader spectrum devices.
- **Impact**: The wider frequency deviation affects a larger bandwidth, making it effective against systems that use wider channels or higher data rates. This mode can cause severe interference across a broader frequency spectrum, marmalade systems with a higher tolerance for noise or frequency shifts.

### 💥 **MSK 99.97 Kb/s** (Minimum Shift Keying):
- **Pattern**: A stream of **random data** (each byte is randomly generated, not a static pattern like the previous modes).
- **Mechanism**: **MSK** is a highly efficient modulation technique where the frequency shifts are minimal, which makes it spectrally efficient (i.e., it occupies less bandwidth). The randomness of the data simulates high-speed communication, creating a noise-like signal.
- **Impact**: By simulating a noisy digital communication channel, this mode is highly effective against digital systems that rely on MSK or similar modulation schemes. The continuous flow of random data saturates the receiver, making it impossible for legitimate data to be detected, especially in high-speed links like telemetry systems.

### 📶 **GFSK 9.99 Kb/s** (Gaussian Frequency Shift Keying):
- **Pattern**: Like the MSK mode, this also uses **random data**.
- **Mechanism**: **GFSK** is a variant of FSK where the frequency shifts are smoothed by a Gaussian filter. This reduces the bandwidth required for transmission, making it more efficient while still being robust against interference.
- **Impact**: **GFSK** is widely used in Bluetooth and low-power RF systems. This mode simulates an authentic transmission with continuous random data, making it ideal for disrupting low-power communications without requiring significant bandwidth. Devices expecting real GFSK signals will be overloaded with random frequency shifts, making proper communication impossible.

### 🚀 **Bruteforce 0xFF**:
- **Pattern**: A continuous stream of `0xFF` (equivalent to `11111111`).
- **Mechanism**: This mode sends a constant, unmodulated signal of `1`s. In the digital domain, `0xFF` means every bit is a `1`, resulting in a strong, uninterrupted carrier wave being transmitted.
- **Impact**: The **Bruteforce 0xFF** mode creates the most aggressive form of marmalade. By transmitting non-stop high bits, it forces constant noise across the frequency, which marmalades nearly any communication within the affected band. Most RF systems rely on alternating data bits (`1`s and `0`s), so flooding the airwaves with pure `1`s causes receivers to lock up, unable to process real signals.

### 🎶 **Sine Wave**:
- **Pattern**: Generates a continuous sine wave signal across the frequency.
- **Mechanism**: Sine waves represent the most basic continuous wave (CW) modulation, commonly used in signal testing.
- **Impact**: Affects devices using analog modulation schemes, causing them to interpret the signal as a valid, albeit meaningless, transmission, effectively drowning out real signals.

### 🟥 **Square Wave**:
- **Pattern**: Alternates between high (`0xFF`) and low (`0x00`) states.
- **Mechanism**: Square waves are simple digital pulses that are straightforward to generate and disruptive across various digital communication schemes.
- **Impact**: Mimics basic digital signals, likely interfering with devices expecting digital pulse sequences like on-off signaling, potentially causing dropouts or erratic behavior.

### 📈 **Sawtooth Wave**:
- **Pattern**: A gradually increasing signal followed by a sharp drop, repeated continuously.
- **Mechanism**: The sawtooth pattern is similar to frequency sweeps used in testing, creating a continuously shifting interference.
- **Impact**: Effective against frequency-sensitive systems by introducing an unpredictable, ramped noise, often disrupting RF links that lack sufficient error correction.

### 🎲 **White Noise**:
- **Pattern**: Randomized data across the signal spectrum.
- **Mechanism**: Emulates white noise, a common form of interference that introduces random amplitude values.
- **Impact**: White noise can disrupt any frequency-based device, particularly analog devices, making it a universal jamming signal for both digital and analog targets.

### 🔺 **Triangle Wave**:
- **Pattern**: Oscillates linearly between high and low values in a symmetric pattern.
- **Mechanism**: The triangular waveform introduces a consistent frequency shift, resembling certain synthetic modulations.
- **Impact**: Affects devices relying on predictable frequency patterns, making it harder for them to differentiate between legitimate and jammed signals.

### 📡 **Chirp Signal**:
- **Pattern**: Frequency increases steadily within the waveform, known as a "chirp."
- **Mechanism**: Common in radar and sonar, chirp signals create a rising frequency pattern over time.
- **Impact**: Causes RF receivers to detect multiple frequencies simultaneously, often confusing or overloading their demodulators.

### 🎲 **Gaussian Noise**:
- **Pattern**: Random values with a Gaussian (bell-curve) distribution.
- **Mechanism**: Gaussian noise introduces random, statistically distributed values that mimic natural noise in RF environments.
- **Impact**: Particularly effective for jamming digital signals relying on Gaussian-based frequency modulation, as it closely resembles natural interference, making the jamming less detectable.

### 💥 **Burst Mode**:
- **Pattern**: Periodic bursts of high (`0xFF`) data with pauses in between.
- **Mechanism**: Sends intense pulses followed by intervals, creating an effect similar to packetized data.
- **Impact**: Effective against burst-based communication systems by mimicking valid data bursts, which can confuse or overload the receiver's data handling.

---

## 💣 Controls

- **Up/Down Buttons**: Modify the currently selected digit in the frequency.
- **Left/Right Buttons**: Move between digits to adjust frequency values.
- **OK Button**: Switch marmalade modes in real-time.
- **Back Button**: Stop the marmalade and exit the app.

---

**Disclaimer**: This app is intended for educational and research purposes by experienced RF users. Ensure compliance with local regulations before using this tool.

<img width="1500" height="1000" alt="rocketgod_logo_transparent" src="https://github.com/user-attachments/assets/ad15b106-152c-4a60-a9e2-4d40dfa8f3c6" />

