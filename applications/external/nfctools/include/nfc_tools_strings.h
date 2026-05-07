#ifndef NFC_TOOLS_STRINGS_H
#define NFC_TOOLS_STRINGS_H

// ─────────────────────────────────────────────────────────────────────────────
// NFC Tools — Centralized UI strings
// All user-visible labels, titles, buttons, headers and static messages.
// Convention: NTS_<CATEGORY>_<NAME>
// ─────────────────────────────────────────────────────────────────────────────

// ── Application ──────────────────────────────────────────────────────────────
#define NTS_APP_NAME                    "NFC Tools"

// ── Main menu ─────────────────────────────────────────────────────────────────
#define NTS_MAIN_READ                   "Read"
#define NTS_MAIN_WRITE                  "Write"
#define NTS_MAIN_OTHER                  "Other"
#define NTS_MAIN_ABOUT                  "About"

// ── Write (NDEF) menu ─────────────────────────────────────────────────────────
#define NTS_WRITE_TEXT                  "Text"
#define NTS_WRITE_URL                   "URL"
#define NTS_WRITE_CUSTOM_URI            "Custom URI"
#define NTS_WRITE_UNIT_LINK             "Unit.Link"
#define NTS_WRITE_SOCIAL_NETWORKS       "Social Networks"
#define NTS_WRITE_SEARCH                "Search"
#define NTS_WRITE_MAIL                  "Mail"
#define NTS_WRITE_CONTACT               "Contact"
#define NTS_WRITE_PHONE_NUMBER          "Phone Number"
#define NTS_WRITE_SMS                   "SMS"
#define NTS_WRITE_FACETIME              "Facetime"
#define NTS_WRITE_FACETIME_AUDIO        "Facetime Audio"
#define NTS_WRITE_LOCATION              "Location"
#define NTS_WRITE_BITCOIN               "Bitcoin"
#define NTS_WRITE_BLUETOOTH             "Bluetooth"
#define NTS_WRITE_WIFI_NETWORK          "Wi-Fi Network"
#define NTS_WRITE_CUSTOM_DATA           "Custom Data"

// ── Other menu ────────────────────────────────────────────────────────────────
#define NTS_OTHER_ERASE_TAG             "Erase Tag"
#define NTS_OTHER_LOCK_TAG              "Lock Tag"
#define NTS_OTHER_READ_MEMORY           "Read Memory"
#define NTS_OTHER_FORMAT_MEMORY         "Format Memory"
#define NTS_OTHER_SET_PASSWORD          "Set Password"
#define NTS_OTHER_REMOVE_PASSWORD       "Remove Password"
#define NTS_OTHER_NFC_COMMANDS          "NFC Commands"

// ── Submenu headers ───────────────────────────────────────────────────────────
#define NTS_HEADER_WRITE                "Write"
#define NTS_HEADER_OTHER                "Other"
#define NTS_HEADER_SOCIAL_NETWORK       "Social Network"
#define NTS_HEADER_SEARCH_ENGINE        "Search Engine"

// ── Input headers ─────────────────────────────────────────────────────────────
#define NTS_INPUT_URL                   "URL:"
#define NTS_INPUT_URI                   "URI:"
#define NTS_INPUT_TEXT                  "Text:"
#define NTS_INPUT_WIFI_SSID             "WiFi SSID:"
#define NTS_INPUT_UNIT_LINK_ALIAS       "Unit.Link Alias:"
#define NTS_INPUT_NUMBER                "Number:"
#define NTS_INPUT_NUMBER_OR_MAIL        "Number or Mail:"
#define NTS_INPUT_MAC                   "MAC (ex: AABBCCDDEEFF):"
#define NTS_INPUT_NAME                  "Name:"
#define NTS_INPUT_SEARCH                "Search:"
#define NTS_INPUT_BTC_ADDRESS           "BTC Address:"
#define NTS_INPUT_LATITUDE              "Latitude:"
#define NTS_INPUT_LONGITUDE             "Longitude:"
#define NTS_INPUT_USERNAME              "Username:"
#define NTS_INPUT_CONTENT_TYPE         "Content-Type:"
#define NTS_INPUT_RECIPIENT             "Recipient (To):"
#define NTS_INPUT_WIFI_PASSWORD         "WiFi Password:"
#define NTS_INPUT_SUBJECT               "Subject (optional):"
#define NTS_INPUT_MESSAGE_OPT           "Message (optional):"
#define NTS_INPUT_COMPANY               "Company (optional):"
#define NTS_INPUT_ADDRESS               "Address (optional):"
#define NTS_INPUT_PHONE                 "Phone (optional):"
#define NTS_INPUT_EMAIL                 "Email (optional):"
#define NTS_INPUT_WEBSITE               "Website (optional):"
#define NTS_INPUT_BTC_AMOUNT            "BTC Amount (optional):"
#define NTS_INPUT_PASSWORD              "Password:"
#define NTS_INPUT_CURRENT_PASSWORD      "Current password:"
#define NTS_INPUT_DATA                  "Data :"
#define NTS_INPUT_APDU                  "APDU (hex):"

// ── Buttons ───────────────────────────────────────────────────────────────────
#define NTS_BTN_QR_CODE                 "QR Code"
#define NTS_BTN_ASCII                   "ASCII"
#define NTS_BTN_CHIP_INFO               "Chip Info"

// ── Popup / scene titles ──────────────────────────────────────────────────────
#define NTS_POPUP_APPROACH_TAG          "Approach an NFC Tag"
#define NTS_POPUP_BACK_TO_CANCEL        "Back to cancel"
#define NTS_POPUP_BACK_TO_RETURN        "Back to return"
#define NTS_POPUP_ANALYZING             "Analyzing..."
#define NTS_POPUP_HOLD_TAG              "Hold the tag\nnear the Flipper"
#define NTS_POPUP_APPROACH_NTAG         "Approach an NFC Tag\nNTAG213 / 215 / 216"
#define NTS_POPUP_APPROACH_NTAG_FORMAT  "Approach an NFC Tag\nNTAG / ICODE SLI(X)"
#define NTS_POPUP_FORMAT_ALL            "Approach an NFC Tag\nNTAG/ICODE/MFC 1K+4K"
#define NTS_POPUP_LOCK_TAG              "Lock Tag"
#define NTS_POPUP_LOCK_WARNING          "IRREVERSIBLE!\nApproach an NFC Tag\nNTAG213 / 215 / 216"
#define NTS_POPUP_FORMAT_MEMORY         "Format Memory"
#define NTS_POPUP_FORMATTING            "Formatting..."
#define NTS_POPUP_SET_PASSWORD          "Set Password"
#define NTS_POPUP_REMOVE_PASSWORD       "Remove Password"

// ── Write type titles (popup header) ─────────────────────────────────────────
#define NTS_WRITE_TITLE_URL             "Write URL"
#define NTS_WRITE_TITLE_URI             "Write URI"
#define NTS_WRITE_TITLE_TEXT            "Write Text"
#define NTS_WRITE_TITLE_WIFI            "Write Wi-Fi"
#define NTS_WRITE_TITLE_UNIT_LINK       "Write Unit.Link"
#define NTS_WRITE_TITLE_MAIL            "Write Mail"
#define NTS_WRITE_TITLE_PHONE           "Write Phone"
#define NTS_WRITE_TITLE_SMS             "Write SMS"
#define NTS_WRITE_TITLE_FACETIME        "Write Facetime"
#define NTS_WRITE_TITLE_FACETIME_AUDIO  "Write Facetime Audio"
#define NTS_WRITE_TITLE_BLUETOOTH       "Write Bluetooth"
#define NTS_WRITE_TITLE_CUSTOM_DATA     "Write Custom Data"
#define NTS_WRITE_TITLE_SOCIAL          "Write Social"
#define NTS_WRITE_TITLE_LOCATION        "Write Location"
#define NTS_WRITE_TITLE_CONTACT         "Write Contact"
#define NTS_WRITE_TITLE_SEARCH          "Write Search"
#define NTS_WRITE_TITLE_BITCOIN         "Write Bitcoin"
#define NTS_WRITE_TITLE_ERASE           "Erase Tag"
#define NTS_WRITE_TITLE_NDEF            "Write NDEF"

// ── Status / success messages ─────────────────────────────────────────────────
#define NTS_STATUS_WRITE_COMPLETE       "Write complete!"
#define NTS_STATUS_TAG_LOCKED           "Tag locked!"
#define NTS_STATUS_TAG_LOCKED_INFO      "Tag locked!\nRead-only forever\nBack to exit"
#define NTS_STATUS_TAG_ERASED           "Tag erased!\nBack to exit"
#define NTS_STATUS_FORMATTED            "Formatted!"
#define NTS_STATUS_PASSWORD_SET         "Password set"
#define NTS_STATUS_PROTECTION_REMOVED   "Protection removed"
#define NTS_STATUS_PROTECTION_REMOVED_FULL "Protection removed!\nPWD reset to default\nBack to exit"

// ── Error messages ────────────────────────────────────────────────────────────
#define NTS_ERR_FAILED                  "Failed!"
#define NTS_ERR_NO_TAG                  "No tag detected"
#define NTS_ERR_WRITE                   "Write error"
#define NTS_ERR_TAG_CONTACT             "Tag contact error"
#define NTS_ERR_INCOMPATIBLE_NTAG       "Incompatible tag\nOnly NTAG213/215/216"
#define NTS_ERR_INVALID_HEX             "Invalid hex input"

// ── Tag info labels ───────────────────────────────────────────────────────────
#define NTS_LBL_CHIP                    "Chip: "
#define NTS_LBL_UID                     "UID: "
#define NTS_LBL_RAW                     "Raw:\n"
#define NTS_LBL_CHIP_INFO               "Chip Info"
#define NTS_LBL_NO_RECORDS              "No records found"

// ── Chip names ────────────────────────────────────────────────────────────────
#define NTS_CHIP_MF_ULTRALIGHT          "NXP MF Ultralight"
#define NTS_CHIP_MF_ULTRALIGHT_C        "NXP MF Ultralight C"
#define NTS_CHIP_MF_ULTRALIGHT_EV1      "NXP MF Ultralight EV1"
#define NTS_CHIP_NTAG203                "NXP NTAG203"
#define NTS_CHIP_NTAG210                "NXP NTAG210"
#define NTS_CHIP_NTAG212                "NXP NTAG212"
#define NTS_CHIP_NTAG213                "NXP NTAG213"
#define NTS_CHIP_NTAG213_TT             "NXP NTAG213 TT"
#define NTS_CHIP_NTAG215                "NXP NTAG215"
#define NTS_CHIP_NTAG216                "NXP NTAG216"
#define NTS_CHIP_NTAG_I2C_1K            "NXP NTAG I2C 1K"
#define NTS_CHIP_NTAG_I2C_2K            "NXP NTAG I2C 2K"
#define NTS_CHIP_NTAG_I2C_PLUS_1K       "NXP NTAG I2C Plus 1K"
#define NTS_CHIP_NTAG_I2C_PLUS_2K       "NXP NTAG I2C Plus 2K"
#define NTS_CHIP_MIFARE_CLASSIC         "NXP Mifare Classic"
#define NTS_CHIP_MIFARE_CLASSIC_1K      "NXP Mifare Classic 1K"
#define NTS_CHIP_MIFARE_CLASSIC_4K      "NXP Mifare Classic 4K"
#define NTS_CHIP_MIFARE_CLASSIC_MINI    "NXP Mifare Classic Mini"
#define NTS_CHIP_NXP_ICODE              "NXP ICODE"
#define NTS_CHIP_NXP_ICODE_SLI          "NXP ICODE SLI"
#define NTS_CHIP_NXP_ICODE_SLIX         "NXP ICODE SLIX"
#define NTS_CHIP_NXP_ICODE_SLIX2        "NXP ICODE SLIX2"
#define NTS_CHIP_NXP_ICODE_SLIX_S       "NXP ICODE SLIX-S"
#define NTS_CHIP_NXP_ICODE_SLIX_L       "NXP ICODE SLIX-L"
#define NTS_CHIP_ISO15693_TAG           "ISO 15693 Tag"

// ── FeliCa ────────────────────────────────────────────────────────────────────
#define NTS_CHIP_FELICA                 "Sony FeliCa"
#define NTS_LBL_VIEW_BLOCKS             "View Memory"
#define NTS_OTHER_FELICA_WRITE          "FeliCa Write Block"
#define NTS_INPUT_FELICA_BLOCK          "Block 0-13:"
#define NTS_INPUT_FELICA_DATA           "Data (32 hex chars):"
#define NTS_WRITE_TITLE_FELICA          "Write FeliCa Block"
#define NTS_ERR_FELICA_FORMAT           "Invalid input\n2-digit block+\n32 hex chars"
#define NTS_ERR_FELICA_BLOCK_RANGE      "Block out of range\n(00-13 only)"

// ── About strings ─────────────────────────────────────────────────────────────
#define NTS_ABOUT_APP_NAME              "NFC Tools\n"
#define NTS_ABOUT_DEVELOPER             "Developed by: wakdev\n"
#define NTS_ABOUT_WEBSITE               "Website: wakdev.com\n"

// ── NFC Commands run ──────────────────────────────────────────────────────────
#define NTS_NFC_CMDS_WAITING            "Approach an NFC Tag..."

#endif /* NFC_TOOLS_STRINGS_H */
