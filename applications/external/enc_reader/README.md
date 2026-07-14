# Enc reader

This is a simple app for flipper that can read incremental quadrature encoders and show absolute and relative coordinate.

## Pinouts

- 5V - supply
- A4 - enc A
- A7 - enc B

## Configutarion menu

- Start: run reader
- Resolution: set coordinate showing option (raw pulses or mm)
- 5V supply: set the 5V output state

## Main screen

Shows configuration at top and two position values (absolute and relative).

5V output enabled only in this mode.

### Controls

- Left/Back: return to configuration menu
- Ok: set current position as origin
- Right: set all coordinates to 0