v1.0:
Initial release.

- Add and delete shipments on the device; no file editing required
- Live tracking over a WiFi devboard running FlipperHTTP, using your own
  tracking service and API key
- WiFi set up from the device: the board scans, you pick a network and enter
  the password once, and the board stores the credentials itself
- Ready-made settings for Trace (traceapi.dev); any other JSON service can be
  configured by hand
- Fetches when the app opens, and on demand
- Status glyphs for pending, in transit, out for delivery, delivered and
  exception, with carrier, location and last scan time in the detail view
