# TPMS Bridge

Reads the tyre pressure sensors in car wheels and shows what they report
on the Flipper screen: sensor id, pressure, temperature and signal level.
Reception starts as soon as the app is opened, so no computer is needed.

Thirty-nine sensor protocols are decoded, all of them ported from
[rtl_433](https://github.com/merbanan/rtl_433) — the reference for this
kind of radio traffic. You do not choose a protocol: every one of them is
looked for at the same time, and a reading is labelled with whichever
protocol's checksum it satisfies.

## What the stock firmware cannot do

Sub-GHz Read cannot pick these sensors up. *subghz rx* loads an OOK preset
while most of these sensors transmit 2-FSK, and a Read RAW capture throws
away pulses shorter than 50 us — which is exactly the chip duration these
protocols use. This app configures the CC1101 itself and decodes on the
device.

## What the screen shows

The list holds up to eight sensors, four visible at a time. Each row has
the sensor id, pressure in bar, temperature and a four step signal ladder.

**The signal level is what tells the wheels apart.** Walk around the car
and watch which row rises. A row drawn as outlines has been silent for
over a minute, so its readings are old.

The detail screen adds pressure in PSI and kPa, the exact signal level
with a ten second peak hold, a frame counter, the age of the last frame,
and which protocol the frame turned out to be.

The header opens with what the radio is set to, then how many sensors are
known and whether reception is local or handed to USB.

## Keys

- **Up, Down** — on the list, step through the six band and modulation
  settings; on the detail screen, walk from one sensor to the next
- **Up, Down held** — pick a sensor out of the list
- **OK** — details, press again to go back
- **OK, hold** — clear the list, for when you move to another car
- **Left** — periodic waking on and off, a pulse every 5 s
- **Right** — one wake pulse, 0.7 s
- **Back** — from the details to the list, from the list out of the app

## Bands and modulation

The radio holds one configuration at a time, so two things have to be
picked: the band and the modulation.

- **433.92 MHz** is the European band, **315 MHz** the North American and
  Japanese one.
- **FSK** covers 26 of the protocols, including every European OEM sensor;
  **OOK** covers the other 13, mostly Schrader and aftermarket units.

**Up** and **Down** step through five settings:

- **433F** — 433.92 MHz, FSK. The default, and where a European car's own
  sensors are.
- **433O** — 433.92 MHz, OOK
- **315F** — 315 MHz, FSK
- **315O** — 315 MHz, OOK
- **Scan** — all four in turn, four seconds each

The header shows which one is on air. While scanning it reads *AUTO-433f*,
with the prefix and a lowercase letter, because the setting moves on by
itself. Scanning finds a sensor whose kind is not known in advance, at the
cost of hearing any one of them a quarter of the time.

## Waking a sensor at rest

A sensor in a parked wheel stays silent: it transmits when the wheel turns
or when a low frequency field wakes it, the way a garage activation tool
does. On a bench, press **Right** and hold the sensor against the **back**
of the Flipper, where the 125 kHz coil is. In a moving car this is not
needed — the wheels turn and the sensors talk by themselves.

## Which sensors work

**Verified against real hardware: the Renault group sensor only.** A
407003VU0B was read on the bench and on a moving car with all four wheels
reporting at once. That part belongs to the Renault-Nissan family, and
rtl_433 reports the same frame format on Renault Clio, Captur and Zoe.

**Every other protocol is verified against rtl_433, not against a
sensor.** Each decoder is fed a frame built bit by bit, the same bits go
to a real rtl_433 build, and the readings have to match. That catches a
field read from the wrong place or scaled wrongly; it cannot catch a
sensor that turns out to transmit something else entirely.

Decoded protocols, by the make they are usually found on:

- **Renault, Dacia** — Renault, Renault 0435R
- **PSA: Citroen, Peugeot, Fiat, Mitsubishi** — Citroen/PSA
- **Ford** — Ford
- **Toyota, Lexus** — Toyota, PMV-107J
- **Hyundai, Kia** — Hyundai VDO, Elantra 2012, Kia
- **Honda** — Honda TRW
- **BMW, Mini** — BMW Gen2/3, BMW Gen4/5, Schrader MRXBC5A4
- **Mercedes** — Mercedes Sprinter
- **Porsche** — Porsche Boxster and Cayman
- **Audi, and the multi-brand HUF, Beru, Continental and Sensata units** —
  BMW Gen4/5 covers these too
- **Fiat, Abarth, Mazda** — VDO TG1C
- **Chrysler, Jeep, Dodge** — TRW, in its OOK and FSK versions
- **Opel, Saab, Vauxhall, Chevrolet, GM** — Schrader, Schrader EG53MA4,
  GM aftermarket
- **Subaru, Infiniti, Nissan** — Schrader SMD3MA4
- **Motorcycles** — Schrader motorcycle
- **Aston Martin** — SmarTire
- **Nissan** — Nissan
- **Aftermarket and trucks** — Jansite TY02S, Jansite Solar, Jansite
  TY588, Jansite TY468, iMars T240, Airpuxem, Sefis M3 and Careud,
  EezTire and TST-507, TyreGuard 400, Gear Hive, unbranded solar truck
  sensors, EGQ Q85

Two overlaps are worth knowing about, and rtl_433 has the same ones. Jeep
sensors put the same frame on air as Citroen with pressure at twice the
scale, so they read half. The Schrader 3039 fitted to Infiniti, Nissan and
Renault is indistinguishable from the Subaru SMD3MA4 and reads a fifth
low.

## Streaming to a computer

While the app is open it also offers a *tpms_rx* command on the USB CLI.
It prints one line of JSON per frame, carrying the parsed fields, the
protocol name and the raw bytes, so anything can be re-derived later.
Local reception hands the radio over on its own and takes it back when the
session ends.

The repository this comes from also has a desktop application in Python
with a sensor table, charts and CSV export.

## The sensor list

Eight sensors are remembered, keyed by protocol and id together. When a
ninth turns up, whichever has been silent longest makes way.

## Credits

Every protocol here is a port of a decoder from
[rtl_433](https://github.com/merbanan/rtl_433), which is where the field
layouts, checksums and scales come from. The CC1101 configuration for FSK
comes from [ProtoView](https://github.com/antirez/protoview), where it is
proven against real sensors.
