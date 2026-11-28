
# About

This Flipper application reads data from automotive tire pressure sensors (TPMS), showing information on the pressure, tire temperature etc. It supports multiple manufacturer's protocols used across different car brands (Opel, Ford, Toyota, PSA/FCA, Renault/Nissan). The data can be saved, as well as modified and resent.

Apart from reading RF data (Radio Frequency, which for TPMS sensors is usually 433.92 MHz or 315 MHz), it can also be used to "wake up" (trigger) some sensors using LF (Low Frequency) signals on 125 kHz (which is used e.g. during the "relearn" process) to send the RF transmission. This covers (but is not limiting to) job done by tools like EL-50448 (multiple brands) and EL-50449 (mainly Ford). in the TPMS Editor, the EL-50449 mode is additionally able to trigger some VDO sensors used in FCA cars (which is not possible with the EL-50449 tool itself).

The app was created by altruista86, based on TPMS Reader (by wosk) and with ProtoView (by antirez) core. Currently maintained here (by TxF).

# Usage:

## Receiving the data

By default, the TPMS Editor is listening on 433.92 MHz with FSK modulation. Just going to **Receiver** will start listening and will show data catched as a list of TPMS sensor type and ID pairs. The LED blinks in blue.

Clicking OK on a sensor shows more details, in particular the ID, the temperature reported by the sensor (if supported, some sensors do not send this), the tire ressure and the time it passed since the data was received. It will also show the frequency, modulation and signal strength.

Clicking OK once again brings up a menu where you can save the signal, but you can also modify some data (like the ID, the pressure etc) for sending.

By default, the TPMS Editor saves all decoded signals to an autosave directory to have data for analysis purposes (this can be turned off in settings).

## Confguring the receiver

By pressing *Left* in the receiving mode or by going to **Settings**, you can change the receiver frequency (a lot of possible choices) and the modulation (between FSK, OOK and GFSK).

The frequency is usually 433.92 MHz in Europe. In the US market it tends to be 315 MHz, but some cars use 433.92 MHz in the US as well. Usually this is the frequency the key fob is working on (as the cars tend to process both using the same module).

Mostly used modulation schema is FSK (Ford, Stellantis, Renault/Nissan, Toyota), with OOK also used (Opel) and GFSK as well.

## Triggering the sensor

For the triggering, go to **Settings** and then down to **Relearn**. Currently there are three types of Relearn modes supported:
 - **CW Relearn**: Common Wave Relearn, where just a sine wave of a particular frequency is send by the Low Frequency module
 - **EL-50448**: similar to the CW Relearn, sends a sine wave of a 125 kHz
 - **EL-50449**: sending a sine wave of 125 kHz modulated by some data

EL-50448 and EL-50449 are named after popular tools used for triggering the sensors.

The difference between CW Relearn and EL-50448 is that CW Relearn sends a three second sine wave of a chosen frequency with carrier duty 50%, while EL-50448 sends a five second 125 kHz sine wave with carrier duty 12.5% (EL-50449 also sends for five seconds and with carrier duty 12.5%).

The **CW frequency** allows setting CW Relearn frequency to 125 kHz (the default) or 134.2 kHz.

The **50449 profile** is the data that the EL-50449 mode is using to modulate the signal. This is required by some sensors. Currently this supportsFord Schrader sensors (*Ford 5A5A* mode) and some VDO sensors used e.g in FCA cars (*VDO/FCA 615E* mode), and the Auto mode alternates sending both.

Do not forget to set right **RX band** here - 433.92 MHz or 315 MHz to be chosen here.

Wth the **Repeat mode** you choose if the trigger/relearn signal is sent once and then only manually by pressign *Right* (the Manual mode) or every 5s or 10s (**RX pause**), where you can also use *Right* to force it (Auto mode).

Go to **START** to go to start triggering and receiving. The LED becomes green to inform that the Relearn signal is sent (note that all decoders are used while listening, and the TPMS Editor might also catch a signal which was not triggred by the app).

## Sending the data

There are two ways to send the data: using a saved signal or the **Simulation**.

In general, it is easier when you already have the signal your car "knows", that is catched with the receiver (for the tire you are interested in or by any other). You load such signal by going to **Saved**, when you choose between ORG and MOD (original signals or those modified before saving). After opening the file you will see which protocol it concerns. You can edit the data you want (that will mostly be ID and/or pressure) and then go for **Sent edited**. You can also turn on **Auto TX** with a chosen **Auto TX interval** (a counter next to "ON" will show how many signals were sent).

In the **Simulation** mode you have to know a little bit more - like the name of the protocol, ID, flags to be set. Apart from this, the menu looks the same as for sending loaded signals.

## Other information

The list of supported protocols is in **Info**.

In the near future, the TPMS Editor will support showing signals that were not decoded, which should help catching sensor data in a car where this does not work yet. Most likely this will also need recompiling just for debugging purposes, as it then needs bigger memory buffers to store undecoded data. Also, some of this is just noise, like weather station info, so this will likely not be usable in an everyday scenario.

