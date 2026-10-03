Pack Track keeps your shipments on your Flipper. Add a tracking number on the device, and with a WiFi devboard attached the app fetches the real status for you: a scrollable list showing each package and where it is at a glance, and a detail view with the carrier, full tracking number, last known location and the time of the last scan.

## What you need

For live tracking you need two things, both one-time setup:

- **A WiFi devboard running FlipperHTTP.** Live tracking is not possible without it, because a Flipper Zero has no internet connection of its own.
- **Your own API key from a tracking service.** The app ships with settings for Trace (traceapi.dev), whose free tier covers 1,000 lookups a month and needs no card. The key is yours, it stays on your SD card, and this app hosts nothing and signs you up for nothing.

Without a devboard the app still works as a shipment list you maintain yourself, including the status.

## Setting it up

Everything happens on the Flipper, in one menu:

- **WiFi setup** asks the board to scan, shows the networks it found, and takes your password once. The board stores the credentials itself, so your password never touches the SD card and the board reconnects on its own afterwards.
- **Tracking setup** takes your API key and writes the rest of the configuration for you.
- **Add package** takes a tracking number, a label and a carrier.

Then open the app and it fetches on its own. You can also refresh at any time.

## Controls

- **Up and Down** move through the list, which scrolls automatically
- **OK** opens the detail view for the highlighted shipment
- **Left and Right** page between shipments while in the detail view
- **Left** on the list opens the menu
- **Hold OK** in the detail view deletes a package
- **Back** leaves the detail view, or exits from the list

## Worth knowing

Carriers are detected from the tracking number itself, so UPS, USPS, FedEx, DHL and many others work without telling the app which is which.

**Amazon's own deliveries are not supported.** Tracking numbers beginning with TBA come from Amazon Logistics, which does not publish tracking that other services can read. Amazon orders shipped by UPS or USPS carry those carriers' numbers instead, and those work normally.

Your API key sits in plain text on the SD card, which is fine for a personal device but not a card you lend out. Shipments you keep by hand, without a devboard, are stored the same way.
