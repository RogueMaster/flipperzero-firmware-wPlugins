
# Flame-RNG

Inspired by [this repo](https://github.com/dipdowel/flipper-fire-rng) I have implemented a similar functionality entirely in the flipper zero


It uses the IR sensor on the Flipper Zero to gather entropy from IR sources (such as a lighter) to seed a random number generator. The idea is that fire and other similar IR sources are not predictable sources of data, and thus it can be a reliable source of entropy. Obviously the flipper doesn't gather enough data (unless you *really* like flicking a lighter near your nearly $200 toy) to get any sort of decent entropy, this is more of a PoC.

  
## Disclaimer
I am NOT responsible for any damage to the flipper or yourself! Be safe with fire, and be safe with your other entropic sources!

I am also NOT responsible if you generate insecure keys using this as a source of entropy!
