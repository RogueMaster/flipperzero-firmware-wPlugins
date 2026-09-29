Pack Track keeps your shipments on your Flipper: a clean, scrollable list showing each package's label, carrier and status at a glance, with a detail view for the full tracking number, last known location and date.

It works two ways, and the first one needs nothing but your Flipper.

## Manual mode, the default

Your packages live in a plain text file on the SD card, one per line, at apps_data/package_tracker/packages.txt. The app creates it for you on first launch with example entries, so you can see the format immediately and edit it with qFlipper or an SD card reader. No internet, no account, no backend, nothing to sign up for.

Each line holds a label, carrier, tracking number, status, location and date, separated by pipes. Status can be pending, transit, out, delivered or exception, and each one draws its own glyph in the list so you can read the whole lot in a second.

## Live mode, entirely optional

If you want real status instead of typing it yourself, Pack Track can fetch it over a WiFi devboard running FlipperHTTP. Press RIGHT and it looks up each package and updates the list.

Everything that makes this work is **yours and stays yours**: your WiFi credentials, your account with whichever tracking service you already use, and your API key. The app hardcodes no provider and talks to no server of its own. You supply the request URL and tell it which fields to read out of the response, in a config file on your own SD card. Nothing is hosted, and there is nothing to sign up for here either.

Worth knowing: your WiFi password and API key sit in plain text on the SD card, which is fine for a personal device but not a card you lend out. Live mode also needs the devboard attached while it refreshes.

## Controls

- **Up and Down** move through the list, which scrolls automatically
- **OK** opens the detail view for the highlighted shipment
- **Left and Right** page between shipments while in the detail view
- **Right** on the list refreshes everything, in live mode
- **Back** leaves the detail view, or exits from the list
