# NFC Sniffer

This is a simple Flipper app that listens for the commands being sent by a NFC reader, and displays them to the Flipper's screen. The commands are also logged to the nfc_sniffer_logs folder in the root of the ext directory of the Flipper.

Note: The app **DOES NOT** capture the response from tags. It is only able to capture the command being sent.

## Supported Protocols

* ISO1443-3 A
* ISO15693-3
* Felicia

ISO1443-3 B is not supported since the Flipper does not allow for setting up that listener.
