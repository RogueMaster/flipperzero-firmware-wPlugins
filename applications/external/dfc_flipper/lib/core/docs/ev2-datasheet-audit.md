# MIFARE DESFire EV2 short data sheet audit

Reference: [NXP MF3D(H)x2 product short data sheet, Rev. 3.2 (12 June 2019)](https://www.nxp.com/docs/en/data-sheet/MF3DX2_MF3DHX2_SDS.pdf). This is document 364232, not the full functional specification. Sections 7.7 and 8 explicitly defer detailed protocol behavior to the separate *MF3Dx2 MIFARE DESFire EV2 Functional specification* (reference [1] in the PDF). The findings below compare the advertised features and command set with this repository's reader, emulator, and `full_ev2` build profile. They do not certify wire level conformance or physical RF and security properties.

The NXP PDF is not distributed with this repository.

## Findings, in priority order

1. **Proximity Check is excluded from `full_ev2`.** Section 2.1.5 and Table 13 list PreparePC, ProximityCheck, and VerifyPC as EV2 features. `src/dfc_build_config.h:85-86` enables them only for EV3, and `src/dfc_build_config.h:181-182` rejects enabling them without EV3 generation support. The command builders and reader flow exist (`src/dfc_command.h:276-279`, `src/dfc_reader.c`, `tests/test_dfc_reader.c:788`), but an EV2 profile cannot provide the advertised card behavior. This is a profile coverage gap, not evidence that those commands are absent from the project.

2. **EV2 application capacity remains at the EV1 limit.** Table 1 says EV2 has no fixed application count limit, subject to memory. `src/dfc_common.h:201` and `:275` default to 28, and `src/dfc_emulator_commands.c:1947` enforces 28 even if `DFC_MAX_APPS` is raised. The emulator cannot model an EV2 card with more than 28 applications.

3. **File capacity is global, rather than 32 per application.** Section 2.1.3 and Table 1 allow 32 files *in each application*. `src/dfc_common.h:277-278` defaults `DFC_MAX_FILES` to 32 for the whole credential, and `src/dfc_credential.c:234` applies that global limit. Two applications with 32 files each therefore cannot be represented with default settings. The per application check in `src/dfc_emulator_commands.c:2155-2157` does not remove the global bound.

4. **Two application transactions and multiple keys per access right are incomplete.** Section 2.1.5 describes both. The reader encoder accepts a second AID (`src/dfc_command.h:67-72`), but emulator selection records only the first (`src/dfc_emulator_commands.c:51-88`); transaction snapshots also track one selected application (`src/dfc_emulator_commands.c:2695-2751`). File permissions use four legacy key nibbles and match one authenticated key (`src/dfc_emulator_commands.c:223-277`), with no eight key assignment model. The encoder has partial support for two application selection, while the emulator does not complete either feature.

5. **ISO Append Record is unsupported.** Table 11 lists ISOAppendRecord. The standard APDU dispatcher handles Select, authentication, Read Binary, Update Binary, and Read Record, then returns instruction not supported (`src/dfc_virtual_picc.c:990-1026`). Native WriteRecord is present, but it does not fulfill the standard ISO command.

6. **Memory and frame sizes do not cover the larger EV2 variants.** Section 2.1.2 and Table 1 list 16 and 32 kB cards and up to 256 byte frames for those variants. `src/dfc_build_config.h:39-47` defines only 2, 4, and 8 kB storage gates; `src/dfc_common.h:287-288` defaults a single file to 2048 bytes; and `src/dfc_virtual_picc.c:32` uses a fixed 48 byte ISO-DEP information chunk. Larger transfers can use chaining, but the larger advertised per-frame capacity is not used.

## Features with implementation evidence

| Data sheet | Repository evidence | Audit status |
| --- | --- | --- |
| EV2 First/NonFirst authentication and AES secure messaging, Table 4 and section 7.7 | `src/dfc_reader.c` authentication and protected exchange, `src/dfc_ev2.c`, `src/dfc_ev2_crypto.c`, `tests/test_dfc_reader.c` EV2 session cases | Implemented and exercised against the in-repo virtual card; independent wire vectors still need the functional specification. |
| Key sets and ChangeKeyEV2, Table 6 | `src/dfc_command.h:252-266`, `src/dfc_emulator_commands.c:1955-1985`, reader tests | Present, including the advertised 16 set bound. |
| Delegated applications, Table 7 | `src/dfc_command.h:115-128`, `src/dfc_reader.c`, `tests/test_dfc_reader.c:886` | Present in the EV2 profile. |
| Transaction MAC file and CommitReaderID, Tables 8 and 10 | `src/dfc_command.h:161-182` and `:235-241`, `src/dfc_emulator_commands.c:2543-2644` and `:2791-2837` | Present; cryptographic conformance cannot be established from this short sheet. |
| Virtual Card and originality check, Tables 12 and 14 | `src/dfc_virtual_picc.c` VC selection/authentication, `src/dfc_command.h:82`, `src/dfc_build_config.h:88-104` | Present in the EV2 profile. Signature verification and issuer trust remain outside this command level audit. |
| Five data file types, UpdateRecord, native transactions, sections 7.6 and 8.5-8.7 | `src/dfc_command.h:172-241`, `src/dfc_common.h:247-252` | Present as native command/model support, with capacity limits above. |
| Random ID, configurable ATS, and ISO-DEP, sections 2.1.1, 7.4, and Table 1 | `src/dfc_credential.h:182-185`, `src/dfc_virtual_picc.c:38-41` and `:458-497` | Present at the model/activation level; RF timing and certified hardware behavior are outside scope. |

## Verification boundary

This is a source and data sheet audit. The short sheet has no command byte layouts, session key vectors, MAC inputs, error codes, or test vectors, so matching names and in-repo round trips cannot establish full EV2 interoperability. For protocol fixes or claims of conformance, obtain the functional specification cited as reference [1] in section 11 and test against independent vectors or hardware.
