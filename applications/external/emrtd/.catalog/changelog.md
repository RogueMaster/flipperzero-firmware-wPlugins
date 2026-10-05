## 1.0
- First release
- Reads any ICAO Doc 9303 document - a passport, a national identity card, a
  residence permit - and decodes the machine readable zone in all three
  layouts: TD1, TD2 and TD3
- PACE with the generic mapping over ECDH, with AES-128, AES-192 and AES-256
- PACE curves: NIST P-192 to P-256 and brainpoolP192r1 to brainpoolP256r1
  (parameter ids 8 to 13); anything above 256 bits is refused by name
- The card access number as an alternative to the machine readable zone
- BAC with 3DES, chosen automatically when the chip does not announce PACE
- Secure Messaging for both cipher families, checked against the ICAO Doc 9303
  test vectors
- Reads EF.COM, EF.SOD and every non-EAC data group; decodes the machine
  readable zone, the additional details and the security information
- Data group hashes checked against the security object, reported per file
- The facial image streamed out of DG2 to the SD card, so a forty kilobyte
  group never has to fit in memory
- Export directory per read: the raw files, the decoded zone, the image, a
  report, and an optional APDU trace
- The trace records each exchange twice under a session - as the radio carried
  it, and with the Secure Messaging taken off - and names the file, the offset,
  the length and the status word behind every one, in a documented format
- The Secure Messaging keys a read derived - the cipher, KSenc, KSmac and the
  counter the session opened with - shown on the result, because they come out
  of what the holder typed and the holder's own document; never written to the
  card
- Credentials are kept on the card between reads by default, and forgotten on
  request
