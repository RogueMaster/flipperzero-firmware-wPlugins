# **ProtoPirate**

### _for Flipper Zero_

## **⚠️ Warning: Important Security & Project Update**
Read message by following link below:

https://protopirate.net/ProtoPirate

Main repo is located at: https://protopirate.net/ProtoPirate/ProtoPirate 

All others are read only mirrors!


ProtoPirate is an experimental rolling-code analysis toolkit developed by members of **The Pirates' Plunder**.

The app currently supports decoding for multiple automotive key-fob families (Kia, Ford, Subaru, Suzuki, VW, and more), with the goal of being a drop-in Flipper app (.fap) that is free, open source, and can be used on any Flipper Zero firmware. 

App is intended for educational and security purposes only, and has no signal transmission enabled by default. This prevents users from accidentally desyncing their keyfobs, making it safe for non-specialists.

## **Supported Protocols**

Protocols are split into **AM** and **FM** registries. The active registry is chosen from the receiver selected preset.

### **AM protocols**


| Protocol                 | Decoder | Encoder | Signal Encoding | Modulation | Encryption                     | CRC          | Frequency       |
| ------------------------ | ------- | ------- | --------------- | ---------- | ------------------------------ | ------------ | --------------- |
| Chrysler V0              | ✅       | ✅       | PWM             | AM650      | Rolling Code                   | Checksum     | 315.00 / 433.92 |
| Fiat V0                  | ✅       | ✅       | Manchester      | AM650      | Rolling Code (static emu only) | ❌            | 315.00 / 433.92 |
| Fiat V1                  | ✅       | ❌       | Manchester      | AM650      | Rolling Code                   | CRC8         | 315.00 / 433.92 |
| Ford V0                  | ✅       | ✅       | Manchester      | AM650      | Rolling Code                   | ✅ + Checksum | 315.00 / 433.92 |
| Honda V1                 | ✅       | ✅       | Manchester      | AM650      | Rolling Code                   | CRC4         | 315.00 / 433.92 |
| Kia V1                   | ✅       | ✅       | Manchester      | AM650      | Rolling Code                   | CRC4         | 315.00 / 433.92 |
| Porsche Touareg          | ✅       | ❌       | PWM             | AM650      | Rolling Code                   | ❌            | 315.00 / 433.92 |
| PSA (Peugeot/Citroen)    | ✅       | ✅       | Manchester      | AM650      | XTEA/XOR                       | CRC8         | 315.00 / 433.92 |
| StarLine                 | ✅       | ✅       | PWM             | AM650      | KeeLoq                         | ❌            | 315.00 / 433.92 |
| Subaru                   | ✅       | ✅       | PPM             | AM650      | Rolling Code                   | ❌            | 315.00 / 433.92 |
| VAG (VW/Audi/Seat/Skoda) | ✅       | ✅       | Manchester      | AM650      | AUT64/XTEA                     | ❌            | 434.42          |


### **FM protocols**


| Protocol                      | Decoder | Encoder | Signal Encoding | Modulation | Encryption                   | CRC        | Frequency       |
| ----------------------------- | ------- | ------- | --------------- | ---------- | ---------------------------- | ---------- | --------------- |
| Ford V1                       | ✅       | ✅       | Manchester      | F4         | Rolling Code                 | CRC16      | 315.00 / 433.92 |
| Ford V2                       | ✅       | ✅       | Manchester      | F4         | Rolling Code (simple replay) | ❌          | 434.25          |
| Ford V3                       | ✅       | ❌       | Manchester      | F4         | Rolling Code                 | ❌          | 434.25          |
| Honda Static                  | ✅       | ✅       | PWM             | Honda1     | Static Code                  | Checksum   | 315.00 / 433.92 |
| Kia V0 / Suzuki V0 / Honda V0 | ✅       | ✅       | PWM             | FM476      | Rolling Code                 | CRC8       | 315.00 / 433.92 |
| Kia V2                        | ✅       | ✅       | Manchester      | FM476      | Rolling Code                 | CRC4       | 315.00 / 433.92 |
| Kia V3 / V4                   | ✅       | ✅       | PWM             | FM476      | KeeLoq                       | CRC4 (BF)  | 315.00 / 433.92 |
| Kia V5                        | ✅       | ✅       | PWM             | FM476      | Rolling Code                 | ✅          | 315.00 / 433.92 |
| Kia V6                        | ✅       | ✅       | Manchester      | FM476      | AES128                       | CRC8       | 315.00 / 433.92 |
| Kia V7                        | ✅       | ✅       | Manchester      | FM476      | Rolling Code                 | CRC8       | 315.00 / 433.92 |
| Land Rover V0                 | ✅       | ✅       | PWM             | F4         | Rolling Code                 | Check+Tail | 315.00 / 433.92 |
| Mazda V0                      | ✅       | ✅       | Manchester      | FM (F2?)   | Rolling Code                 | Checksum   | 315.00 / 433.92 |
| Mitsubishi V0                 | ✅       | ❌       | PWM             | FM476      | Rolling Code                 | ❌          | 315.00 / 433.92 |
| PSA (Peugeot/Citroen)         | ✅       | ✅       | Manchester      | FM (F3?)   | XTEA/XOR                     | CRC8       | 315.00 / 433.92 |
| Scher-Khan                    | ✅       | ❌       | PWM             | FM         | Magic Code                   | ❌          | 315.00 / 433.92 |


*More Coming Soon*

## **Features**

### 📡 Protocol Receiver

Real-time signal capture and decoding with animated radar display. Supports frequency hopping.

### 📂 Sub Decode

Load and analyze existing `.sub` files from your SD card. Browse `/ext/subghz/` to decode previously captured signals.

### ⏱️ Timing Tuner

Tool for protocol developers to compare real fob signal timing against protocol definitions.

- **Protocol Definition**: Expected short/long pulse durations and tolerance
- **Received Signal**: Measured timing from real fob (avg, min, max, sample count)
- **Analysis**: Difference from expected, jitter measurements
- **Conclusion**: Whether timing matches or needs adjustment with specific recommendations

## **Credits**

The following contributors are recognized for helping us keep open sourced projects and the freeware community alive.

### **App Development**

- RocketGod
- MMX
- Leeroy
- gullradriel
- Skorp - Thanks, I sneaked a lot from Weather App!
- Vadim's Radio Driver

### **Protocol Magic**

- L0rdDiakon
- YougZ
- RocketGod
- MMX
- DoobTheGoober
- Skorp
- Slackware
- Trikk
- Wootini
- Li0ard
- Leeroy
- Ash

### **Reverse Engineering Support**

- DoobTheGoober
- MMX
- NeedNotApply
- RocketGod
- Slackware
- Trikk
- Li0ard

## **Community & Support**

Join **The Pirates' Plunder** on Discord for development updates, testing, protocol research, community support, and a bunch of badasses doing fun shit:

➡️ **[https://discord.gg/thepirates](https://discord.gg/thepirates)**

<img alt="rocketgod_logo_transparent" src="https://github.com/user-attachments/assets/ad15b106-152c-4a60-a9e2-4d40dfa8f3c6" />
