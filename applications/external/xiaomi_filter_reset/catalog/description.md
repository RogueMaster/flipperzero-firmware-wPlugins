Xiaomi air purifier filters carry an NTAG213 NFC tag that the appliance uses to track
"filter life" and prompt you to replace the filter — often long before the media is
actually spent.

This app clears that usage counter directly on the tag, so the purifier reports 100%
again. Vacuum or replace the real filter media on your own schedule; this only resets
the counter.

How to use:
1. Open the app and choose "Reset filter life".
2. Hold the filter's NFC tag against the back of your Flipper.
3. Wait for the confirmation.

The app authenticates with a password derived from each tag's own UID, so a successful
reset is proof the tag is a supported Xiaomi filter. Foreign tags fail authentication
and are never modified.

There is also a read-only "Check filter life": it authenticates and reads the tag
without writing, so you can confirm a filter is genuine and whether it is fresh or
already used. It does not show a percentage — the tag only stores elapsed usage, and
the full-scale rating lives in the appliance, so any percentage would be a guess.

Note: resetting the counter does not clean the filter. Use it to avoid discarding
filters that still have useful life, or to keep using third-party filters.

Credits: reverse engineering by flamingo-tech and unethical.info.
