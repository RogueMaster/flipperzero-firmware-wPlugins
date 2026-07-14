<!-- Source: https://docs.flipper.net/zero/sub-ghz/add-new-remote -->
<!-- Mirrored: 2026-07-14 -->

# Adding new remotes - Flipper Zero Documentation

## Sub-GHz

### Adding new remotes

The documentation explains how to create and pair virtual radio remotes with Flipper Zero when your original remote is lost or you need a backup.

#### How to add a remote manually

To create a virtual remote, follow these steps:

1. Navigate to Main Menu > Sub GHz > Add Manually
2. Select a protocol from the available list and press OK
3. Name your remote and select Save

#### Supported protocols

The guide lists compatible protocols with their transmission frequencies and code types:

| Protocol | Frequency (MHz) | Code Type |
|----------|-----------------|-----------|
| Princeton 433 | 433.92 | Static |
| Nice Flo 12-bit | 433.92 | Static |
| Nice Flo 24-bit | 433.92 | Static |
| Came 12-bit | 433.92 | Static |
| Came 24-bit | 433.92 | Static |
| Linear 300 | 300.00 | Static |
| Came TWEE | 433.92 | Static |
| Gate TX | 433.92 | Static |
| Doorhan 315 | 315.00 | Dynamic |
| Doorhan 433 | 433.92 | Dynamic |
| Liftmaster 315 | 315.00 | Dynamic |
| Liftmaster 390 | 390.00 | Dynamic |
| Security+2.0 (multiple frequencies) | 310/315/390.00 | Dynamic |

#### Pairing procedure

After adding and saving your virtual remote, activate pairing mode on your receiver and transmit the signal from Flipper Zero according to your receiver's specific instructions.
