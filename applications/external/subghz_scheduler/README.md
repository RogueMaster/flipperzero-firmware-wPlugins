# SubGHz Scheduler

A Flipper Zero app to send SubGHz signals at a given interval. Individual `*.sub` or playlist `*.txt` files can be used for transmission, and schedule settings can be saved in a `*.sch` file.


<table style="border:0px">
  <tr style="border:0px">
    <td style="border:0px" width="25%" align="left">
    Schedule intervals in H:M:S format <br>
    from 00:00:01 to 99:59:59.
    </td>
    <td style="border:0px" width="20%"><img src="./screenshots/v3/intervals.png" width="100%"></td>
  </tr>
  <tr style="border:0px">
    <td style="border:0px" width="25%" align="left">
    Interval timing modes: <br>
     <ul>
       <li>Relative: Interval is length of time from `END` of one transmission to `START` of next.</li>
       <li>Precise: Interval is length of time from `START` of one transmission to `START` of next.</li>
     </ul>
    </td>
    <td style="border:0px" width="20%"><img src="./screenshots/v3/timing.png" width="100%"></td>
  </tr>
 <tr style="border:0px">
  <td style="border:0px" width="25%" align="left">
    Data Tx Count:<br>
  <ul>
  <li>x1, x2, x3, x4, x5, x6</li>
  </ul>
  </td>
  <td style="border:0px" width="20%"><img src="./screenshots/v3/txcount.png" width="100%"></td>
<tr style="border:0px">
  <td style="border:0px" width="25%" align="left">
    Transmission Modes:<br>
  <ul>
  <li>Normal: Transmit after intervals.</li>
  <li>Immediate: Counts start as interval.</li>
  <li>One-Shot: Wait interval and transmits once.</li>
  </ul>
  </td>
  <td style="border:0px" width="20%"><img src="./screenshots/v3/txmode.png" width="100%"></td>
 </tr>
<tr style="border:0px">
  <td style="border:0px" width="25%" align="left">
   Inter-transmission delay selectable<br>
   from 0ms to 1000ms in 50ms increments.
  </td>
   <td style="border:0px" width="20%"><img src="./screenshots/v3/txdelay.png" width="100%"></td>
  </tr>
  <tr style="border:0px">
  <td style="border:0px" width="25%" align="left">
   Radio Selection:<br>
   <ul>
    <li>Internal</li>
    <li>External (When Present)</li>
   </ul>
  </td>
   <td style="border:0px" width="20%"><img src="./screenshots/v3/radio.png" width="100%"></td>
  </tr>
    <tr style="border:0px">
  <td style="border:0px" width="25%" align="left">
   Save settings for later use.<br>
  </td>
   <td style="border:0px" width="20%"><img src="./screenshots/v3/saveschedule.png" width="100%"></td>
  </tr>
  <tr style="border:0px">
  <td style="border:0px" width="25%" align="left">
   Run view displays:
    <ul>
      <li>TX Mode</li>
      <li>TX Interval</li>
      <li>TX Count</li>
      <li>Radio</li>
      <li>Selected File</li>
      <li>TX Interval Countdown</li>
    </ul>
  </td>
   <td style="border:0px" width="20%"><img src="./screenshots/v3/rundisplay.png" width="100%"></td>
  </tr>
</table>


## Requirements

This app is tested against the current `dev` and `release` branches of the [OFW](https://github.com/flipperdevices/flipperzero-firmware):

* Current OFW Version: 1.4.3
<br>[![Compatibility status:](https://github.com/shalebridge/flipper-subghz-scheduler/actions/workflows/build.yml/badge.svg)](https://github.com/shalebridge/flipper-subghz-scheduler/actions/workflows/build.yml)

It has also been tested against [Momentum 009](https://github.com/Next-Flip/Momentum-Firmware/releases/tag/mntm-009).

> [!NOTE]
> Currently not supported on Xtreme, as development of that firmware was officially [discontinued on Nov 18, 2024](https://github.com/Flipper-XFW/Xtreme-Firmware/commit/54619d013a120897eeade491decf4d1e95217c06). Since then, breaking changes were made to the Furi API. For those who still use Xtreme I am working on a separate port.

## Build

These apps are built using [ufbt](https://pypi.org/project/ufbt/) - a subset of the flipper build tool (fbt) targeted at building apps. Install it with:

```bash
pip install ufbt
```

For build only, run `ufbt` from the terminal in the project directory. To upload, make sure Flipper is connected to your computer and run `ufbt launch`.

The directory contains the following batch script(s) to simplify the upload process:
* `launch_win.bat` - Windows only. Invokes `ufbt` to deploy and launch the app on a flipper over USB, and resets terminal colors in case of error.

Build outputs are found in the `dist` directory for each application.


# To-Do
- [ ] Enable running in the background.
- [ ] `TESTING IN PROGRESS` Enable interval delays for playlists. For example, turning on a light (playlist index 1), then 4 hours later turning it off (playlist index 2), and run that at another arbitrary interval. This can include custom playlist keys or custom app files.
- [ ] `TESTING IN PROGRESS` Separate into multiple modes:  
  - `Periodic`: Current functionality.
  - `Alarm`: To set specific time for transmission.
  - `Custom`: Periodic with custom breaks and tx count between transmissions.
- [ ] More visual feedback of current transmission (like `SubGHz Playlist`).
- [ ] Enable quitting from transmission. Currently, if back is pressed during playlist transmission, transmission will complete before exiting to main menu.
- [ ] Options to select notifications on transmit (vibro, backlight, etc).