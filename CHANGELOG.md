## Main changes
- Current API: 87.8
* SubGHz: Add support for **42+ Keeloq based systems** (with partial Add Manually support) (see [Full list](/documentation/SubGHzSupportedSystems.md)) (by @zero-mega, @xMasterX, ARF Team)
* SubGHz: Add **Allstar Firefly 318ALD31K** protocol (18 bits, Static) (PR #989 | by @jlaughter)
* SubGHz: Add **Nord ICE** protocol (33 bits, Static)
* SubGHz: **Better support for CAME Atomo** type remotes (TOPD4REN) (decode + button codes) (thx to Roman for raw recordings)
* SubGHz: Add **CAME TOP44FGN** support in CAME TWEE protocol
* SubGHz: Add all 0x0s and all 0xFs KeeLoq MF codes for normal and simple learning
* SubGHz: **Fix CAME TWEE repeats count for button click**
* NFC: Add **Mifare Ultralight C Write Support** (by @haw8411)
* SubGHz: Improve Nice FLO decoding (thx to Roman for raw recordings)
* SubGHz: **Fix FAAC SLH wrong decode/encode**, apply little code cleanup
<br><br>
#### Known NFC post-refactor regressions list: 
- Mifare Mini clones reading is broken (original mini working fine) (OFW)
- While reading some EMV capable cards via NFC->Read flipper may crash due to Desfire poller issue, read those cards via Extra actions->Read specific card type->EMV 

### THANKS TO ALL RM SPONSORS FOR BEING AWESOME!

# MOST OF ALL, THANK YOU TO THE FLIPPER ZERO COMMUNITY THAT KEEPS GROWING OUR PROJECT!
