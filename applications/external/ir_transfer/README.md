
# IR Transfer (Flipper Zero) — NEC Only

File transfer over infrared between two Flipper Zero devices using the **NEC** protocol for both control and data.

[WARNING] This repo is public for testing purpose only atm, It is not working properly as intented

---

## ✨ Features

- **Send / Receive** any file from the SD card.
- Robust, simple flow:
  - Filename sent first (**null-terminated**, no length field).
  - File size sent as **4 bytes big-endian**.
  - File data sent **byte-by-byte via NEC** frames.
  - Final **XOR checksum** (over all data bytes) confirms success.
- **UI**:
  - **Landing**: choose _Send_ or _Receive_.
  - **Send**: file picker, status + progress bar, _Send/Cancel_.
  - **Receive**: target path, status + progress bar, _Reset_ when idle, _Cancel_ during activity.
- **Output folder**: `/ext/downloads` (created automatically).
- **Name collision** handling (suffix appended if needed).

---

## 📦 Requirements

- Flipper Zero firmware repo (Momentum or official) with **FBT** toolchain.
- This project lives under your firmware’s `applications_user/` directory.

---

## 🚀 Install

```bash
cd <firmware-root>                    # e.g. ~/flipper/Momentum-Firmware
mkdir -p applications_user
cd applications_user
git clone https://github.com/WBP20/ir_transfer.git
```

---

## 🛠️ Build & Run

From the **firmware root**:

```bash
./fbt launch APPSRC=applications_user/ir_transfer
```

If you changed code and the build seems stale:

```bash
./fbt distclean
./fbt launch APPSRC=applications_user/ir_transfer
```

> You might see an unrelated warning (e.g. an external app with a bad `appid`); it **does not** affect IR Transfer.

---

## 🧭 Usage

### Sender

1. Open the app → **Send**.
2. Pick a file under `/ext/...`.
3. Press **Send** to start (press **OK** again to **Cancel**).
4. On completion: status shows **File sent**.

### Receiver

1. Open the app → **Receive** (status: **awaiting signal**).
2. During transfer: progress bar and **Cancel** (center button).
3. After success: the file is saved to `/ext/downloads/<filename>` (suffix added if the name already exists).

---

## 📡 Protocol (NEC)

- **Filename**: sent as bytes **terminated by `'\0'`** (no length).
- **Size**: 4 bytes **big-endian** total size.
- **Data**: **1 byte per NEC frame** (simple and reliable).
- **Checksum**: single-byte **XOR** of all data bytes, sent at the end.
- **Receiver workflow**:

  - Write incoming bytes to temporary file `/ext/tmpfile`.
  - On checksum match:

    - Move to `/ext/downloads/<original_name>` (add suffix if needed).

  - On checksum mismatch:

    - Remove the temporary file and show error.

---

## 📂 Project Structure

```
ir_transfer/
├─ application.fam
├─ ir_main.c
├─ ir_main.h
├─ views/
│  ├─ landing_view.c/.h
│  ├─ sending_view.c/.h
│  └─ receiving_view.c/.h
├─ infrared/
│  ├─ infrared_transfer.c
│  └─ infrared_transfer.h
└─ utils/
   └─ draw_utils.h
```

---

## 🧰 Troubleshooting

- **Build noise from other apps**
  External app warnings in your firmware tree don’t affect this app.

---

## 🗺️ Roadmap

- Optional batching/optimizations while staying NEC.
- (Future branch) Higher throughput mode (RAW/Manchester) with CRC/ACK.

---

## 📝 License

MIT 2.0
