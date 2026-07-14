<!-- Source: https://docs.flipper.net/zero/ibutton -->
<!-- Mirrored: 2026-07-14 -->

# iButton - Flipper Zero Documentation

## iButton

Flipper Zero supports a 1-wire device communication protocol implemented in small electronic keys known as iButton keys. These keys serve purposes including access control, temperature and humidity measurements, and cryptographic key storage. The device can read, write, and emulate iButton access control keys through its built-in iButton module, which supports Dallas, Cyfral, and Metakom key protocols.

### iButton Menu

The iButton application accessible from the main menu offers these functions:

- **Read**: Detects key type, reads and saves the key's unique number
- **Saved**: Emulates, edits, and writes saved keys
- **Add manually**: Generates keys with unique numbers that can be emulated

### iButton Hardware

Flipper Zero includes a built-in iButton module consisting of an iButton pad and three spring-loaded pogo pins located on the iButton PCB. Two pins handle data transfer with output to GPIO pin 17, while the middle pin serves as ground.

The flat part of the pad connects an iButton key (slave) with Flipper Zero (master). "The left data pin and the middle ground pin are used for reading and writing iButton keys." The protruding part allows Flipper Zero to operate as a slave with an iButton reader (master), using "the right data pin and the middle ground pin...for emulation of iButton keys."

### Related Documentation

- Reading iButton keys
- Adding iButton keys manually
- Writing data to iButton keys
- Flipper Zero schematics
- iButton application source code
