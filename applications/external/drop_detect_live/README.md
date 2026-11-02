# Live drop detector

This is the part that runs the trained model on the Flipper itself. It watches the motion sensor in real time, and when it decides the device has been dropped, it plays a loud siren for about a second.

It comes in two forms that share the same detection code:

- FliPort Live, a normal app you open from the menu. Good for trying it out.
- A background service with no screen, meant to be built into custom firmware so the alarm works even when no app is open.

## How it works

A worker thread reads the accelerometer and gyroscope at 100 Hz into a buffer that holds the last 2.5 seconds. Every quarter second it runs that buffer through the model, the same small network described in [../../MODEL_CARD.md](../../MODEL_CARD.md). If the answer is "drop" and the model is confident enough, it sounds the siren, clears the buffer, and carries on. Anything else and it just keeps watching.

## Running the app

```powershell
cd flipper-zero\drop-dect
ufbt launch
```

Then open `Apps > Tools > FliPort Live`. The model file ships inside the app, so there's nothing else to copy. Hold the device still and nothing happens; drop it onto something soft and the siren goes off within about half a second. The screen shows the current guess and a bar for each class.

## Running it as a background service

Stock firmware closes an app when you leave it, so an always-on detector has to be built into the firmware. The steps are in [RUN.md](RUN.md). In short: copy this folder into the firmware source tree, uncomment the service block at the bottom of `application.fam`, put `model.json` on the SD card where the service looks for it, and rebuild the firmware. The detection code is identical to the app; only the entry point and the missing screen differ.

## Files

```
model.c / model.h      the network and the model.json reader
monitor.c / monitor.h  the loop: buffer, run the model, trigger the alarm
alarm.c / alarm.h       the siren
imu.c / imu.h           reading the motion sensor
drop_dect_app.c         the app
drop_dect_svc.c         the background service
driver/                 the sensor driver (copied from ../expo_app)
assets/model.json       the trained weights
```

## Things worth knowing

- The weights load from `model.json` at startup. The numbers used to normalize the sensor readings are not in that file, so they live in `model.c`. If you retrain, copy the new `model.json` into `assets/`. If you change how features are built in `model/scripts/data.py`, update those constants too. The script prints them when it runs.
- The motion sensor sits on the external GPIO pins. The background service holds those pins the whole time it runs, which can clash with other apps that use the same bus. The app version only holds them while it's open.
- This only runs the model. Training stays on the PC.
