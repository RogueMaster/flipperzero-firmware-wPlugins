# YRM100X_PRO

YRM100X_PRO turns Flipper Zero and a compatible YRM100/YRM100X UART module into
a practical UHF RFID toolkit for EPC Gen2 tags.

## Highlights

- Fast Multi inventory plus Full Multi, Forever, Single, and Full Single modes.
- Separate EPC, TID, User, and Reserved bank data with per-bank status.
- Persistent, paged Saved Dumps and standalone complete dump files.
- Selectable-bank Write and Clone for compatible rewritable tags.
- Adaptive target-bank probing that avoids assuming one fixed User or Reserved
  size across different tag ICs.
- Verified Clone retries with visible Attempt X / Y progress.
- Model-aware Test UMI Auto and automatic PC3000/PC3400 conversion for supported
  rewritable tags.
- Generic and Impinj Monza 4QT interpretation profiles.
- Bank-size and rewritability diagnostics.
- Access/Kill password updates, lock controls, and permanent Kill operation.
- Reader connection, antenna information, power, baud rate, region, Gen2 session
  and target configuration.
- Read Forever countdown and browsable scan history.
- RAM-conscious lazy screens and a live free-memory indicator.

The application checks the target rather than assuming every tag has identical
memory. It can fit supported writes to verified bank capacity, while reporting
read-only, locked, protected, missing, too-small, or unknown-model conditions
instead of silently claiming success.

## Hardware

Use the tested five-wire connection:

- Reader VCC (pin 1) to Flipper 5V (pin 1).
- Reader RXD (pin 2) to Flipper USART TX (pin 13).
- Reader TXD (pin 3) to Flipper USART RX (pin 14).
- Reader EN (pin 4) to Flipper 3.3V (pin 9).
- Reader GND (pin 5) to Flipper GND (pin 8, 11, or 18).

The UART lines are crossed. If the reader needs an external regulated 5 V
supply, share ground with Flipper and leave Flipper 5V disconnected. Never join
two 5 V sources. The repository contains a full wiring diagram, power notes,
and photographs of the tested assembly.

## Safety

Use the application only with tags and systems you own or are authorized to
test. Writing, cloning, password changes, locking, Check TAG Rewritable, Test
UMI Auto, and Kill can alter or permanently disable a tag. Keep exactly one
target in range, retain a known-good dump, and read every confirmation screen.

Full usage instructions, build steps, credits, and the MIT license are in the
[YRM100X_PRO repository](https://github.com/AlexeySmirnov74/YRM100X_PRO).
