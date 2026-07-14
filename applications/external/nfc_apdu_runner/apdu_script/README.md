<!--
 * @Author: SpenserCai
 * @Date: 2025-03-08 00:18:57
 * @version: 
 * @LastEditors: SpenserCai
 * @LastEditTime: 2025-03-14 12:01:57
 * @Description: file content
-->
# NFC APDU Runner Predefined Scripts

This directory contains predefined APDU command scripts that can be used directly with the NFC APDU Runner application.

## Usage

1. Copy these script files (`.apduscr` files) to the following directory on your Flipper Zero:
   ```
   /ext/apps_assets/nfc_apdu_runner
   ```

2. You can copy the files to your Flipper Zero using:
   - qFlipper application
   - USB mass storage mode
   - Flipper Zero's file browser

3. After copying, launch the NFC APDU Runner application, select "Load Script" option, and you'll see and be able to select these predefined scripts.

## Script Format

APDU script files (`.apduscr`) are structured text files with the following format:

```
Filetype: APDU Script
Version: 1
CardType: iso14443_4a
Data: ["00A4040007A0000000041010", "00B0000000"]
```

Where:
- `Filetype`: Always "APDU Script"
- `Version`: Script version number, currently 1
- `CardType`: Card type, can be one of the following (case-insensitive):
  - `iso14443_4a`
  - `iso14443_4b`
- `Data`: JSON array containing one or more APDU commands, each as a hexadecimal string

## Example

Here's an example script file:

```
Filetype: APDU Script
Version: 1
CardType: iso14443_4a
Data: ["00A4040007A0000000041010", "00B0000000"]
```

This script will:
1. Select an ISO14443-4A type card
2. Send a SELECT command to select a payment application
3. Send a READ RECORD command to read data

## Available Scripts

The following table lists the predefined scripts available in this directory:

| Script Name            | Purpose                                                                                                             | Parsing Template                                                               |
| ---------------------- | ------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------ |
| get_aid.apduscr        | Attempts to select various AIDs (Application Identifiers) to determine which applications are supported by the card | None                                                                           |
| travel_card_sh.apduscr | Reads information from Shanghai transportation cards                                                                | [TRAVEL_CARD_SH.apdufmt](/tools/ResponseDecoder/format/TRAVEL_CARD_SH.apdufmt) |
| pboc.apduscr           | Reads information from PBOC (China UnionPay) banking cards                                                          | [PBOC.apdufmt](/tools/ResponseDecoder/format/PBOC.apdufmt)                     |
| emv.apduscr            | Reads essential information from EMV payment cards including card number, expiry date, and application data         | [EMV.apdufmt](/tools/ResponseDecoder/format/EMV.apdufmt)                       |

## Creating Custom Scripts

You can create your own custom scripts:

1. Use any text editor to create a new `.apduscr` file
2. Write the script content following the format described above
3. Save the file to the `/ext/apps_assets/nfc_apdu_runner` directory

## Notes

- Ensure APDU commands are correctly formatted, otherwise execution may fail
- Some APDU commands may require specific card types to work properly
- Using unknown or unsafe APDU commands may cause permanent damage to some cards, use with caution

## Parsing Templates

For some scripts, parsing templates (`.apdufmt` files) are available in the `/tools/ResponseDecoder/format/` directory. These templates define how to interpret and display the response data from the card.

To use a parsing template:
1. Run the script using NFC APDU Runner
2. Save the response file (`.apdures`)
3. Use the ResponseDecoder tool with the appropriate template:
   ```
   ./response_decoder --hex "response_file.apdures" --format "template_name.apdufmt"
   ```

## FAQ

**Q: Why can't my script execute?**  
A: Check if the card type is correct and if the APDU command format is valid.

**Q: Where can I find more APDU commands?**  
A: You can refer to the technical specifications of relevant cards or online resources.

**Q: Where can I view the results after executing a script?**  
A: After execution, the application will automatically display the response result for each command. 