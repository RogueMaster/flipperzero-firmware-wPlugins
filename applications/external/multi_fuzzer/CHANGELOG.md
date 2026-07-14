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
