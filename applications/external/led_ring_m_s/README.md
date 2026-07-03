# LED Ring & Matrix Studio 🚀



An advanced, low-level hardware tester for **WS2812B / SK6812 NeoPixel** circular rings and square matrices, built natively for the Flipper Zero. 

Unlike basic RGB tools, this application utilizes direct hardware register access (`GPIOA->BSRR`) to guarantee nanosecond-precise timings, allowing smooth, flicker-free animations directly from the Flipper's GPIO pins without requiring external microcontrollers.

## 🛠️ GPIO Wiring Setup

Before launching the application, connect your NeoPixel hardware to the top row of your Flipper Zero pins as follows:

| NeoPixel Pin | Flipper Zero GPIO Pin | Pin Number |
|:---:|:---:|:---:|
| **DI** (Data Input) | **PA7** | Pin 14 |
| **VCC** (+5V) | **5V** | Pin 5 |
| **GND** (Ground) | **GND** | Pin 8 or 11 |

⚠️ **Power Safety Feature:** The application automatically enables the Flipper's internal 5V OTG rail on launch and features a strict **5% to 50% brightness limiter** to protect your Flipper Zero from sudden power trips or hardware damage.

## ⚙️ How to Use

### 1. Hardware Configuration Menu
Upon launch, a wiring guide will be displayed. Press **[OK]** to enter the setup menu where you can configure your hardware geometry:
* **Layout:** Switch between `< Circular Ring >` and `< Square Matrix >` to adapt specific animation patterns.
* **LED Count:** Select the exact number of LEDs on your device. It increments seamlessly through standard commercial sizes (`16` ➡️ `24` ➡️ `32` ➡️ `64`).

### 2. Live Animation Controls
Press **[OK]** in the setup menu to ignite the LEDs. Use the following button mapping in real-time:
* **[OK]**: Cycle through the 4 available animation modes.
* **[Up / Down]**: Adjust light power percentage dynamically (5% to 50%).
* **[Left / Right]**: Modify animation speed factor (`1x` to `5x`).
* **[BACK]**: Turn off all LEDs safely and return to the Setup Menu. Press **[BACK]** again to exit the app.

## 🎨 Visual Animation Modes

1. **Rainbow Spectrum:** A gorgeous, continuous color wheel moving smoothly across the whole layout.
2. **Color Breath:** A deep breathing pulse effect that blends shifting hues with smooth fading intervals.
3. **Knight Rider KITT:** The iconic bouncing scanner bar. Includes a customized fading trail tail that behaves linearly on matrices and loops beautifully on circular rings.
4. **Organic Fire:** Uses the Flipper's native hardware entropy generator (`furi_hal_random_get`) to cast organic flame flickers, packed with micro-compensation heatbeds for square matrix layouts.

## 🔨 How to Build Locally

If you want to clone this repository and build it manually using `uFBT`, navigate to the project folder and run:

```bash
ufbt
```

This will output a compiled `.fap` file ready to be copied into your Flipper's SD Card under `apps/GPIO/`.

## 🎯 Compatibility & Firmware Note
This application was actively developed and tested running on the latest version of **Momentum Firmware** utilizing the `uFBT` development environment. However, since it communicates directly with standard STM32 hardware registers, it maintains full hardware compatibility across alternative compliant firmware forks.

## 📜 License
This project is open-source and available under the MIT License. Contributions and new animation modes are highly welcome!
