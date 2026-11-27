# CEPAS Reader

Reads Singapore **EZ-Link** and **NETS FlashPay** stored-value cards (the CEPAS standard) and shows the balance and recent trips. The app only reads the card and never writes to it.

## What it shows
- **Balance page:** card type, balance, card number and expiry month.
- **History:** up to 30 transactions, with full MRT/LRT station names (e.g. Jurong East > Orchard), bus service numbers, top-up locations, and date and time (Singapore time).
- **Detail:** press OK on a transaction to see the from/to stations, date and time, and the type code.

## How to use
1. Open the app and hold the card flat against the back of the Flipper.
2. The balance page opens. Press **Right** for the history.
3. Use **Up/Down** to scroll, **OK** to open a transaction, and **Back** to return.
4. On the balance page, press **OK** to scan another card.

## Notes
- Works with card-based EZ-Link and NETS FlashPay cards. For SimplyGo (account-based) cards, the real balance is kept online, so the amount stored on the card may be out of date.
- The raw data from the last read is saved to the app data folder (apps_data/cepas_reader/last_read.txt) for troubleshooting.

Source code and issues: [github.com/smolbun/flipper-cepas](https://github.com/smolbun/flipper-cepas)
