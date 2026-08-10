# TPMS Bridge

Reads Renault tyre pressure sensors on 433.92 MHz and shows what they
report right on the Flipper screen: sensor ID, pressure, temperature and
signal level. No computer needed — reception starts as soon as the app is
opened.

The signal level is what tells the wheels apart. Walk around the car with
the Flipper and watch which row goes up: that is the wheel you are
standing next to.

## What the screen shows

The list holds one row per sensor — ID, pressure in bar, temperature and a
four-step signal ladder. Up to eight sensors are remembered, four are
visible at once and the rest scroll. Outlined bars mean the sensor has been
quiet for over a minute and its readings are stale.

Pressing OK opens the details of the selected sensor:

- pressure in bar, PSI and kPa
- temperature in degrees Celsius
- current signal level in dBm, and the peak held over the last 10 seconds
- how many frames arrived and how long ago the last one did
- protocol flags and the two bytes whose meaning is not known

## Keys

- **Up / Down** — select a sensor
- **OK** — open the details, press again to go back
- **OK, long press** — clear the list, handy before moving to another car
- **Right** — one wake pulse, 0.7 s
- **Left** — periodic wake pulses every 5 s, on and off
- **Back** — leave the details, or exit the app

## Waking a sensor at rest

A TPMS sensor is silent while the car is parked. It transmits when the
wheel turns, or when it is hit by a 125 kHz field — the same way factory
activation tools work.

That is what the Right and Left keys do. The field comes from the RFID
coil on the back of the Flipper, so the sensor has to be held right
against it. When periodic waking is on, the word "wake" appears in the
header.

Inside a moving car none of this is needed: the wheels turn and the
sensors transmit by themselves.

## Supported sensors

Verified on a real 407003VU0B sensor. This is the Renault group protocol
described by rtl_433, which reports it on Renault Clio, Captur and Zoe —
including the 407004CB0B sensor fitted to the Zoe — and possibly on Dacia
Sandero. Cars of that family share these sensors, so other Renault, Dacia
and Renault-based Nissan models are likely to work as well.

Any sensor of this family will be picked up as long as its CRC checks out.
Sensors that speak a different protocol will not show up at all: this app
decodes the Renault frame format only, not every TPMS on the market.

## Streaming to a computer

While the app is open it also registers a CLI command, so a computer on
USB can receive the same frames as line-delimited JSON:

- tpms_rx — 433.92 MHz, decoded frames as JSON
- tpms_rx 433920000 json wake — the same, waking the sensor periodically
- tpms_rx 433920000 raw wake — raw timings, for working out why nothing
  decodes

Every frame is one line, carrying both the decoded fields and the raw nine
bytes. The desktop application that goes with it — a sensor table, charts
and CSV export — lives in the source repository.

The radio can only serve one of them at a time. Local reception hands it
over to the USB session and takes it back once that session is done.

## Credits

The protocol was cross-checked against
[rtl_433](https://github.com/merbanan/rtl_433) and the CC1101 preset comes
from [ProtoView](https://github.com/antirez/protoview), where it is proven
against real Renault sensors.

Licensed under the MIT License.
