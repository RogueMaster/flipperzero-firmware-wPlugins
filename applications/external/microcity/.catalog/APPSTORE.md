# MicroCity

A SimCity-style city builder for the Flipper Zero. Zone land for homes, shops and factories, wire the grid, lay the roads, and keep the whole thing solvent while fires break out and traffic chokes your streets. The city lives on a 48x48 tile map you scroll across the 128x64 one-bit screen, and it keeps simulating month after month from January 1900 onward.

MicroCity is a port of the Arduboy game by James Howard, rebuilt by ApertureFox Technology to run natively on the Flipper Zero: no Arduino compatibility layer, direct framebuffer rendering, native input, and saves written to the SD card through the firmware's own storage API.

## The simulation

Every building is scored individually each pass, and the scores drive growth:

- **Power** - buildings off the grid show a power-cut marker and do nothing. Connectivity is recalculated across the whole map as you build.
- **Jobs** - residents need industrial and commercial work nearby; unemployment is punished far harder than anything else.
- **Traffic** - roads feeding a busy building turn into heavy-traffic tiles, which in turn generate pollution.
- **Pollution** - industry, traffic and above all the power plant poison the land around them and stunt growth.
- **Crime** - rises with density and distance from a working police station.
- **Taxes** - 7% to start, adjustable from 0 to 99 in the budget screen. Six percent is where citizens stop resenting you; every point above that costs you growth.

Fires start on their own every two to six game years. They spread to neighbouring buildings, burn them down to rubble, and only a powered fire department close enough to reach them will put them out. Parks never burn.

## Money

You start with $10,000. Once a year, in January, the budget screen opens: taxes collected, road upkeep, police and fire wages, and the net cash flow. Turn **Auto Budget** on and the report is skipped in years that end in the black - it still stops you when the city is losing money or the treasury runs dry.

Cities are saved to the SD card and reloaded from the Save/Load menu, so a city survives leaving the app and powering the device down.

## Controls

- **Arrows** - move the tile cursor; the view scrolls to follow it
- **OK** - build with the selected tool, or confirm a menu choice
- **Back short** - open the toolbar, or back out of a menu
- **Back long** - leave the city and return to the title menu; press and hold again there to quit the app

In the toolbar, **Left** and **Right** cycle through the tools and **OK** picks one. In the budget screen, **Left** and **Right** set the tax rate.

## Credits

Original Arduboy game by **James Howard** (@jameshhoward). Flipper Zero port by **ApertureFox Technology**. Released under the GPL-3.0.
