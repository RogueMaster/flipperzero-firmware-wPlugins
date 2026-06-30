# iButton Converter

**iButton Converter** is a Flipper Zero application designed for converting iButton key dumps from *Metakom* or *Cyfral* formats into the *Dallas* format. The application supports multiple conversion modes for each format, including well-known and lesser-known encoding schemes.

## Features

- Support for importing Metakom and Cyfral key dump files
- Multiple conversion modes:
  - **Metakom**: 
    - Direct mode
    - Reversed mode
  - **Cyfral**:
    - Standard modes: C1, C2, C3, C4
    - Extended modes: C5, C6, C7
- Output to Dallas iButton format
- Custom naming for the converted output file

## Usage

1. Launch the **iButton Converter** application on your Flipper Zero.
2. Select a key dump file in either Metakom or Cyfral format.
3. Choose the desired conversion mode:
   - For **Metakom**: direct or reversed
   - For **Cyfral**: one of C1–C7
4. Enter the desired filename for the converted key.
5. Save the output in Dallas iButton format.

## Acknowledgments
Conversion is based on documented and reverse-engineered encoding algorithms.

Special thanks to [li0ard](https://github.com/li0ard) for implementing the Cyfral C5–C7 conversion modes and providing consultation on the remaining algorithmic details.

## References

- [Partial list of intercoms and suitable recodings](https://www.rmxlabs.ru/products/rw_keys/conv_table/)
- [Article about conversions from Cyfral to Dallas](https://domofondocs.blogspot.com/2021/12/cyfral-cyfral-dallas.html)
