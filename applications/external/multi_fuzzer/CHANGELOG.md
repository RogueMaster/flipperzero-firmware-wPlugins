## v1.8
- Time Delay (TD) and Emulation Time (EmT) are now tunable in 0.01s steps (range 0.00–8.00s, shown as X.XX), down from the old 0.1s steps
- Both TD and EmT can now go to 0.00 (the worker substitutes a tiny non-zero real delay), for bleeding-edge timing
- RFID emulation now drives `furi_hal_rfid` directly instead of going through `lfrfid_worker`. This removes the crash/0.1s restart floor introduced upstream when `lfrfid_worker_emulate_start`'s idle guard became a `furi_check` (firmware #3507), letting RFID fuzz below 0.1s on stock firmware (no custom firmware required)
## v1.7
- Fix Fixed prev navigation for AttackTypeLoadFileCustomUids
## v1.6
New systems in RFID Fuzzer:
    - Electra
    - Idteck
    - Gallagher
    - Nexwatch
- Changed with how lists are handled to make adding RFID protocols easier to add
## v1.4
- Fix worker being not in LFRFIDWorkerIdle before next key (limit TD to 0.1)
## v1.3
- New systems in RFID Fuzzer:
    - IoProxXSF
    - Paradox
    - Indala26
    - Viking
    - Pyramid
    - Keri
    - Jablotron
- Fixes for new API
## v1.2
- Fixes for new auto-naming system
## v1.1
- Improved pause during attack
    - Added the ability to switch UID
    - Added the ability to emulate the current UID
    - Added the ability to save UID
- Load key file attack
    - Key file loading now does not depend on the selected protocol

## v1.0

**Supported protocols**
| iButton | RFID        |
|:-:      | :-:         |
| DS1990  | EM4100      |
| Metakom | HIDProx     |
| Cyfral  | PAC/Stanley |
|         | H10301      |

**Suported attack**
|                     | iButton | RFID |
| -                   | :-:     | :-:  |
| Default Values      | +       | +    |
| Load key file       | +       | +    |
| Load UIDs from file | +       | +    |
| BFCustomer ID       | -       | +    |
