# UV Meter

A Flipper Zero application designed to measure ultraviolet (UV) radiation levels using the [AS7331](https://ams-osram.com/products/sensor-solutions/ambient-light-color-spectral-proximity-sensors/ams-as7331-spectral-uv-sensor) sensor. It supports individual measurements for UV-A, UV-B, and UV-C wavelengths. The easiest way to hook everything up is to use a breakout board, such as the one from [SparkFun](https://www.sparkfun.com/sparkfun-spectral-uv-sensor-as7331-qwiic.html). The application is available in the [Flipper App Catalog](https://lab.flipper.net/apps/uv_meter_as7331) and comes pre-installed on some custom firmware builds, such as the [Momentum Firmware](https://github.com/Next-Flip/Momentum-Firmware). For more behind-the-scenes info, check out the [post on my website](https://michaelbais.ch/projects/personal/uv_meter_for_flipper).

<p float="left">
    <img src="images/flipper_zero_uv_meter_outside.jpg" alt="AS7331 Sensor Connected to Flipper Zero with UV Meter Adapter PCB" width="49%" />
    <img src="images/flipper_with_sensor.jpg" alt="AS7331 Sensor Connected to Flipper Zero with cables" width="49%" />
</p>



## Motivation

Have you heard that sitting behind a window protects you from sunburn? Sounds good, right? Unless you do a bit of research and find it's only *kind of* true. While potentially some UV-B (the stuff that causes immediate sunburn) gets blocked, a lot of UV-A could still get through. And guess what? UV-A plays a role in developing melanoma—a deadly form of skin cancer. Now it almost sounds even *more* dangerous: you lose the immediate “sunburn feedback” yet still face a long-term risk.

It gets even more complicated because it depends on the specific type of window and possible surface treatments; I have also heard that some car windows might be better in this regard. In the end, things seem less predictable, with more questions than before. And it doesn't stop there: do my sunglasses really work? Does my shirt actually protect me? How bad is it really in the shade or on a cloudy day?

What we need is **data**. Being able to actually *measure* something can be surprisingly empowering. I found the AS7331 sensor, which can independently measure UV-A, UV-B, and UV-C—and this project was born.

In general, it's surprising how low the maximum daily exposure durations actually are (based on the 2024 TLVs and BEIs by the [ACGIH](https://en.wikipedia.org/wiki/American_Conference_of_Governmental_Industrial_Hygienists)). Sure, compared to direct sunlight, you're better off behind a window, in the shade, or under clouds—but probably not *as much* as you'd expect.

In some of my measurements, the safe daily exposure duration, for example, tripled—but when you're starting with just 3 minutes, tripling still leaves you under 10 minutes. My takeaway? I should protect my eyes and skin more than I once thought necessary.



https://github.com/user-attachments/assets/cabb948c-9c79-4a1d-aab7-8789f0833f28



## Wiring

Connect the AS7331 sensor to your Flipper Zero via I²C:

![Flipper Showing the Wiring Screen](images/wiring_screen.png)

| Sensor Pin | Flipper Zero Pin   |
|------------|--------------------|
| **SCL**    | C0 [pin 16]        |
| **SDA**    | C1 [pin 15]        |
| **3V3**    | 3V3 [pin 9]        |
| **GND**    | GND [pin 11 or 18] |

By default, the application scans all possible I²C addresses for the sensor. However, you can manually set a specific address in the settings menu, accessible by pressing the **Enter** button.



## Usage

Once connected, the application automatically displays real-time UV measurements. The main screen shows individual UV-A, UV-B, and UV-C readings. Beneath the numbers you see the currently used unit (µW/cm², W/m² or mW/m²).

![Data Screen with Explanations](images/main_screen_explanation.jpg)

Next to each UV reading is a small meter indicating the raw sensor value. If this value is too high or too low, a warning symbol will appear, signaling potential sensor overexposure or underexposure. This condition will probably result in unreliable measurements. Adjusting the **Gain** and **Exposure Time** settings (similar to camera ISO and shutter speed) at the bottom of the screen can help correct this.

In addition to direct sensor measurements, the application provides an interpretation of this irradiance data on human safety, displayed on the right side of the screen. The time (in minutes) indicates the recommended maximum daily exposure duration during an 8-hour work shift, based on the 2024 Threshold Limit Values (TLVs) and Biological Exposure Indices (BEIs) published by the [American Conference of Governmental Industrial Hygienists (ACGIH)](https://en.wikipedia.org/wiki/American_Conference_of_Governmental_Industrial_Hygienists). According to ACGIH, the TLVs referenced apply only to incoherent UV radiation; coherent UV radiation such as that emitted by lasers is handled differently.

Since UV radiation poses greater risks to the eyes, the app includes a setting to specify whether your eyes are protected. This adjusts the maximum daily exposure duration according to ACGIH recommendations.

The displayed percentages indicate how much each UV type (UV-A, UV-B, UV-C) contributes to the maximum daily exposure. This helps illustrate that higher sensor readings don't necessarily mean a greater health risk; for example, even though UV-A sensor values are typically higher, they usually contribute less to the recommended maximum exposure limit compared to UV-B or UV-C.

When following the maximum daily exposure duration, the TLV/BEI guidelines ensure:

> “[...] nearly all healthy workers may be repeatedly exposed without acute adverse health effects such as erythema and photokeratitis.”



## Magic Numbers

In the source code, specifically the [`uv_meter_data_calculate_effective_results()`](views/uv_meter_data.cpp#L668) function, you'll find some numbers that might look mysterious ("magic numbers"). They're used to calculate the maximum daily UV exposure duration based on sensor readings:

```cpp
// Weighted Spectral Effectiveness
double w_spectral_eff_uv_a = 0.0002824;
double w_spectral_eff_uv_b = 0.3814;
double w_spectral_eff_uv_c = 0.6047;

if(eyes_protected) { // 😎
    // w_spectral_eff_uv_a is the same
    w_spectral_eff_uv_b = 0.2009;
    w_spectral_eff_uv_c = 0.2547;
}
```

You might wonder, "Where do these numbers come from?" Good question! To uncover the full story behind these values, check out the detailed explanation in the [Magic Numbers documentation](magic_numbers/README.md).



## Adapter PCB

As a final touch, I designed a tiny adapter PCB in KiCad that lets me connect the breakout board to the Flipper — no more cable fiddling required. (One might even call its color ~~dark purple~~ ultraviolet.) The front only shows the application icon and minimal labeling to help you avoid plugging things in the wrong way. On the back, the labels show which Flipper pins connect to which sensor pins. See the [adapter_pcb](adapter_pcb) directory for the KiCad and Gerber files.

![Render of the UV Meter Adapter PCB](images/adapter_pcb.jpg)



## Disclaimer

This application is provided for informational purposes only and should not be used as a sole basis for safety-critical decisions. Always follow official guidelines, regulations, and professional advice regarding UV exposure. The developer assumes no responsibility for any damages, injuries, or consequences arising from decisions or actions based on the information provided by this application.
