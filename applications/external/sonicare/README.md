Sonicare Brush Head ID
======================

Each modern brush head for a Philips Sonicare contains a Mifare Ultralight NFC chip (NTAG213).
The chip not only signals to the head unit what type of brush it is (to pre-select the recommended
mode) but also the recommended lifespan. Usually 6 hours, which resemble 180 brushes (2 minutes
each), and, assuming 2 brushes per day, this is 90 days = 3 months. The time after which you should
replace your toothbrush (or the head in this case).

The main unit also writes the time you've used the head back into the NFC chip. If you reach the
6 hours, the "replace head" indicator will light up.

The NFC chip is write-protected with a password that can be derived from the UID and MFG code. The
[algorithm for this](https://gist.github.com/atc1441/41af75048e4c22af1f5f0d4c1d94bb56) has been
reverse-engineered by [atc1441](https://github.com/atc1441).


Features
--------

This Flipper Zero app will scan the NFC chip and show the UID, MFG code, lifespan, as well as
the used time. It'll also generate the NFC password.


TODO
----

* determine brush head type from [data](https://blog.mbirth.uk/2026/03/29/sonicare-brush-head-nfc-data.html)
* allow writing back modified data (different type, reset usage)
