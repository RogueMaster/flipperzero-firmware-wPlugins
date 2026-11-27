v1.0:
- First release: reads Singapore EZ-Link and NETS FlashPay (CEPAS) cards. Read-only, never writes to the card.
- Main page: card name, balance, card number and expiry month.
- History of up to 30 transactions, with full MRT/LRT station names, bus service numbers, top-up locations, and date and time (Singapore time).
- Detail screen for each transaction: from/to stations, date and time, type code.
- Supports the different balance-command formats used by EZ-Link and NETS FlashPay cards.
- Saves the raw data from the last read to /ext/apps_data/cepas_reader/last_read.txt, for troubleshooting.
