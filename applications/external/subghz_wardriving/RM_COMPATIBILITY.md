# Sub-GHz Wardriving on RM

This integration retains the September 2026 UART GPS memory and pin-selector updates. Select **NMEA** or **Ubox** under **GPS source**, choose the GPS module baud rate, and select pins **13/14** or **15/16**. The UART parser is built as an embedded `subghz_plugin_gps.fal` asset.

RM does not provide UL's firmware GPS service, so phone RPC GPS is omitted from the source selector. The source can expose that option on firmware that provides the GPS API. Invalid or unsupported saved source values fall back to OFF.

RM's protocol-filter enums, datetime filename API, and `setting_user.txt` path are used. Application ID: `subghz_wardriving`; SD category: `Sub-GHz`.

Build/import validation passed on RM API 88.7. 
