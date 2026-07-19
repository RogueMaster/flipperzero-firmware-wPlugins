v1.5: First public release of **Flipper Share IR**
- Async chunked md5 hash calculation for large files on Send / Receive.
- UI: fixed layout on long names
- File manager now memorizes the last file used

v1.0: Flipper Share IR — file transfer over the infrared channel
- New app derived from Flipper Share: the transport is now fully infrared
  (onboard IR LED + TSOP receiver) instead of Sub-GHz.
- Custom half-duplex IR modem: fixed-mark + multi-level-space PPM at a 38 kHz
  carrier, with a distinctive sync so nearby TVs/AV gear are not triggered.
- Per-packet integrity upgraded from CRC8 to CRC16; MD5 verification of the whole
  file after reception is kept.
- Automatic retransmission of lost/corrupted packets (continues until success or
  a manual restart); receiver requests missing block ranges in bounded chunks.
- Data whitening keeps the modulated envelope irregular (avoids TSOP AGC
  suppression of repetitive payloads).
- Tunable physical layer (bits/symbol, timings) and a decoupled, configurable
  DATA packet size, all in ir_modem_config.h / ir_share.h (recompile to change).
