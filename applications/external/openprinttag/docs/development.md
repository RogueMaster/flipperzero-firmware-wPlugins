# Development notes

## Building

The app is built with [ufbt](https://pypi.org/project/ufbt/) and has no other dependencies:

    python -m ufbt            # build
    python -m ufbt launch     # build, install and start on a connected Flipper

It builds against the latest release and release candidate SDKs. Code is formatted with clang-format 18 (the lint workflow checks it).

## Layout of the source

- openprinttag.c, openprinttag_i.h: app setup, shared data types and declarations
- scenes/: one file per scene (start, read, read success/error, display, write = update, create)
- ndef_parser.c: finds the OpenPrintTag record in the tag memory (capability container, TLVs, records)
- cbor_parser.c, cbor_encoder.c: the CBOR subset the format uses, including indefinite lengths and floats
- openprinttag_parser.c: reads the meta, main and auxiliary sections into the data structures
- openprinttag_writer.c: re-encodes the auxiliary section, keeping every field except the consumed weight
- openprinttag_builder.c: builds the memory image of a new tag
- tag_write.c: writes blocks to a tag (shared by update and create) and the retry logic for reads
- tag_view.c: the three-page read screen
- numpad.c: the number pad

## How tags are written

The SDK does not export the ISO 15693 write commands, so frames are sent through the NFC layer from a poller started with nfc_poller_start_ex(). Requests are addressed to the UID of the tag that was read. Each block is read first, only blocks that differ are written, and every write is verified by reading the block back.

## Specification

The format is described at https://specs.openprinttag.org/. Points that shape the code:

- An update must keep every field it does not know, so the auxiliary section is copied pair by pair
- The auxiliary region must start on a block boundary of the tag
- The NDEF message should fill the whole tag
