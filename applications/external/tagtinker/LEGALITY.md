## Legality

This repository is a clean open-source port of publicly documented reverse-engineering research into an infrared communication protocol. The original analysis (frame structure, pulse-position modulation, timing, commands, and CRC) has been openly available on GitHub for over 7 years with no known legal challenges, takedowns, or cease-and-desist notices worldwide.

### How the protocol works (high-level)
The system transmits data using line-of-sight infrared signals at 940 nm with a 1.25 MHz carrier, encoded via pulse-position modulation (PP4 or PP16). Frames include a version field, 32-bit tag identifier (derived from a standard barcode), command byte, optional payload (e.g. image or segment data), a weak 16-bit key (frequently default 0x0000), and a CRC-16 checksum. No strong encryption or technological protection measures control access to the signaling. The behavior is observable and reproducible with compatible hardware when the tags are in the possession of the user.

The project implements independent transmission logic for research and interoperability purposes only.

### Why publishing this code is generally considered lawful research activity
- **Copyright law does not protect ideas, principles, protocols, or functional behavior.**  
  Copyright protects specific expression (source code), not the underlying functionality or observed communication formats. Documenting and independently re-implementing observable signal behavior from lawfully acquired hardware does not infringe copyright in most jurisdictions.

- **European Union law supports observation and study of products.**  
  Under the EU Trade Secrets Directive (2016/943), Article 3, the acquisition of information by "observation, study, disassembly or testing of a product ... that is lawfully in the possession of the acquirer" is considered a **lawful means**, provided there is no contractual obligation to the contrary.  
  The Software Directive (2009/24/EC), Article 5(3), further allows legitimate users to observe, study or test the functioning of a program/device to determine underlying ideas and principles.

- **United States and similar jurisdictions.**  
  Reverse engineering by fair and honest means (analyzing publicly observable signals from devices you lawfully possess) is widely recognized as legitimate research and does not generally constitute misappropriation of trade secrets or copyright infringement. The DMCA §1201(f) provides an additional interoperability exception for circumvention of access controls, though no such controlled access is present in this raw signaling protocol.

- **Practical precedent.**  
  Comparable open-source projects involving protocol analysis, IR tools, sub-GHz decoders, and RFID research have existed publicly for years on platforms like the Flipper Zero with minimal enforcement action when proper boundaries are maintained.

### Important boundaries and responsible use
This project is released **strictly for educational, research, and interoperability purposes** under the GPL-3.0 license.

- Use is permitted only with tags and devices you personally own or have explicit permission to test.
- The repository contains source code only (no pre-built binaries) and includes explicit warnings against misuse.
- Any unauthorized use on third-party systems (such as modifying displays in a retail environment without authorization) may violate laws on fraud, interference with business operations, computer misuse, or similar provisions in your jurisdiction. Such actions are the sole responsibility of the user and are not authorized or endorsed by this project.

Because the repository is public and intended for a worldwide audience, users must ensure compliance with the laws applicable in their own country.

This document is not legal advice. Reverse engineering and protocol research exist in a complex area of law that can vary by jurisdiction and specific facts. If in doubt, consult a qualified lawyer in your country.

For reference:  
- EU Trade Secrets Directive 2016/943 (Article 3)  
- EU Software Directive 2009/24/EC (Article 5(3))  
- US DMCA §1201(f) (interoperability exception)
