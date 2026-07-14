
# Flipper Cigarette

<div align=center>
<img src="screenshots/banner.gif" width="600" />
</div>

Finally a way to smoke a cigarette on the [Flipper Zero](https://www.flipperzero.one)


## Screenshots


<p  align="center">
<img src="screenshots/screenshot_1.png" width="40%" hspace="5">
<img src="screenshots/screenshot_2.png" width="40%" hspace="0">
<img src="screenshots/screenshot_3.png" width="40%" hspace="5">
<img src="screenshots/screenshot_4.png" width="40%" hspace="0">
</p>




## Installation

Download the **flipper_cigarette.fap** from the [latest release](https://www.flipperzero.one) and copy it to the **apps/Games** directory on your **Flipper Zero** 


## Build

Clone both the repository for the firmware and  and copy **flipper_cigarette** directory to the **applications_user** folders within the firmware:

```
git clone https://github.com/flipperdevices/flipperzero-firmware.git
git clone https://github.com/fuckmaz/flipper_cigarette.git flipperzero-firmware/applications_user/flipper_cigarette
```

Plug in your **Flipper Zero** and build the app from within the firmware base-directory:
```
cd flipperzero-firmware
./fbt launch_app APPSRC=applications_user/flipper_cigarette
```

The **Flipper Cigarette** app will automatically be launched on your Flipper after compilation is done.


## TODO

- add content for "about"
- include counter for cigarettes smoked
- cancer progress-bar
- add different cigarettes to choose from

xoxo - maz <3

## Stargazers over time
[![Stargazers over time](https://starchart.cc/fuckmaz/flipper_cigarette.svg?background=%23000000&axis=%23ffffff&line=%23f848dd)](https://starchart.cc/fuckmaz/flipper_cigarette)
