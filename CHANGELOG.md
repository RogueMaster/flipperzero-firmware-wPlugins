## Main changes
- SubGHz:
    - OFW: Added protocol for Dickert MAHS garage door remote control
    - Fix rare crash when opening Read mode via Frequency analyzer
    - Refactor frequency analyzer code for better readability (by @derskythe | PR #782)
- 125kHz RFID: 
    - OFW: Add lfrfid GProxII support
- NFC:
    - OFW: Fix plantain balance string
    - OFW: Now fifo size in ST25 chip is calculated properly
* Docs: Remove not printable symbols and update docs (by @derskythe | PR #783)
* OFW: Fix cumulative error in infrared signals
* OFW: iButton ID writing (Enable ID writing for ds1971 and ds1996)
* Apps: **Check out more Apps updates and fixes by following** [this link](https://github.com/xMasterX/all-the-plugins/commits/dev)
## Other changes
* Archive: Fix BadUSB favourite path check
* Settings: Show free flash amount in internal storage info (by @Willy-JL)
* Misc: Fix typo in comment in QueueTools.py (by @eltociear | PR #785)
* OFW PR 3840: GUI: NumberInput small improvements (by @Willy-JL)
* OFW PR 3838: SubGhz: Fix RPC status for ButtonRelease event (by @Skorpionm)
* OFW: scripts: improved size validator for updater image
* OFW: Desktop: seaprate callbacks for dolphin and storage subscriptions
* OFW: Make file extensions case-insensitive
* OFW: Remove internal storage folder if corresponding flag set
* OFW: **Added a text input that only accepts full numbers (int)**
* OFW: FuriEventLoop Pt.2
* OFW: Images linting: ensure that all images conform specification
* OFW: **Storage: remove LFS**
* OFW: NFC: Change the plantain last number display from "?" to "X"
* OFW: CCID App: Refactor
* OFW: Refactor detected protocols list 
* OFW: fix: Ensure proper closure of variadic function in `mjs_array`
* OFW: **Added** `-Wundef` **to compiler options**
* OFW: toolchain: v39
* OFW: Furi: update string documentation
* OFW: Fix typo in "charge me" screen. 
* OFW: Reordered VS-Code Tasks to follow the `Release` > `Debug` schema
* OFW: Remove unused entries from .editorconfig
<br><br>
#### Known NFC post-refactor regressions list: 
- Mifare Mini clones reading is broken (original mini working fine) (OFW)
- NFC CLI was removed with refactoring (OFW) (will be back soon)

### THANKS TO ALL RM SPONSORS FOR BEING AWESOME!

# MOST OF ALL, THANK YOU TO THE FLIPPER ZERO COMMUNITY THAT KEEPS GROWING OUR PROJECT!
