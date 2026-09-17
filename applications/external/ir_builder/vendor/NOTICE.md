The three infrared_signal / infrared_error_code source files in this directory
are from flipperdevices/flipperzero-firmware, tag 1.4.3:
https://github.com/flipperdevices/flipperzero-firmware/tree/1.4.3/lib/infrared/signal

Copyright Flipper Devices Inc. and contributors. Distributed under GPL-3.0;
see ../LICENSE. These helpers are compiled into IR Builder because the official
1.4.3 SDK does not export them to external apps. This preserves the stock
firmware's parsing, validation, serialization, and transmission behavior.

Local defensive fixes: initialize all parsed-message fields, avoid shifting by
32 when computing a full-width mask, reject empty RAW data before allocation,
and reject non-finite RAW duty cycles as well as values outside (0, 1].
