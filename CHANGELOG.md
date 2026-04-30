## Main changes
- Current API: 87.7
* SubGHz: Add support for **42+ Keeloq based systems** (with partial Add Manually support) (see [Full list](/documentation/SubGHzSupportedSystems.md)) (by @zero-mega, @xMasterX, ARF Team)
* SubGHz: Add **Allstar Firefly 318ALD31K** protocol (18 bits, Static) (PR #989 | by @jlaughter)
* SubGHz: Add **Nord ICE** protocol (33 bits, Static)
* SubGHz: **Better support for CAME Atomo** type remotes (TOPD4REN) (decode + button codes) (thx to Roman for raw recordings)
* SubGHz: Add **CAME TOP44FGN** support in CAME TWEE protocol
* SubGHz: Add all 0x0s and all 0xFs KeeLoq MF codes for normal and simple learning
* SubGHz: **Fix CAME TWEE repeats count for button click**
* NFC: Add **ISO15693-3 and SLIX write-back support** (PR #984 | by @DoniyorI)
* NFC: **Fix "MIR" and other EMV cards crash on Read** (by @Dmitry422)
* NFC: Add **Mifare Ultralight C Write Support** (by @haw8411)
* NFC: Add **new parsers SZPPK, SKPPK and SevPPK**, upgrade Plantain parser, fix TwoCities parser (PR #981 | by @mxcdoam)
* OFW PR 4362: NFC: **Fix BusFault** in Write to Initial Card (by @akrylysov)
* OFW PR 4369: NFC: Fix stack buffer overflows in MFUL FAST_READ and DESFire file settings parsers (by @qp-x-qp)
## Other changes
* UI: Various small changes
* Desktop: Disable winter holidays anims 
* OFW PR 4333: NFC: Fix sending 32+ byte ISO 15693-3 commands (by @WillyJL)
* NFC: Fix LED not blinking at SLIX unlock (closes issue #945)
* SubGHz: Improve docs on low level code (PR #949 | by @Dmitry422)
* SubGHz: Fix Alutech AT4N false positives
* SubGHz: Cleanup of extra local variables
* SubGHz: Replaced Cars ignore option with Revers RB2 protocol ignore option
* SubGHz: Moved Starline, ScherKhan, Kia decoders into external app
* SubGHz: Possible Sommer timings fix
* SubGHz: Various fixes
* SubGHz: Nice Flor S remove extra uint64 variable
* SubGHz: Rename Sommer(fsk476) to Sommer (Sommer keeloq works better with FM12K) + added backwards compatibility with older saved files
* Docs: Add full list of supported SubGHz protocols and their frequencies/modulations that can be used for reading remotes - [Docs Link](https://github.com/DarkFlippers/unleashed-firmware/blob/dev/documentation/SubGHzSupportedSystems.md)
* Desktop: Show debug status (D) if clock is enabled and debug flag is on (PR #942 | by @Dmitry422)
* NFC: Fix some typos in Type4Tag protocol (by @WillyJL)
* Clangd: Add clangd parameters in IDE agnostic config file (by @WillyJL)
<br><br>
#### Known NFC post-refactor regressions list: 
- Mifare Mini clones reading is broken (original mini working fine) (OFW)
- While reading some EMV capable cards via NFC->Read flipper may crash due to Desfire poller issue, read those cards via Extra actions->Read specific card type->EMV 

### THANKS TO ALL RM SPONSORS FOR BEING AWESOME!

# MOST OF ALL, THANK YOU TO THE FLIPPER ZERO COMMUNITY THAT KEEPS GROWING OUR PROJECT!
