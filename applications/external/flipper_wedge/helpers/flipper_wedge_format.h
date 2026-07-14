#pragma once

#include <furi.h>

#define FLIPPER_WEDGE_FORMAT_MAX_LEN 128

/** Format UID bytes to hex string with delimiter
 *
 * @param uid UID bytes
 * @param uid_len Length of UID
 * @param delimiter Delimiter string between bytes (can be empty)
 * @param output Output buffer
 * @param output_size Size of output buffer
 */
void flipper_wedge_format_uid(
    const uint8_t* uid,
    uint8_t uid_len,
    const char* delimiter,
    char* output,
    size_t output_size);

/** Format complete output string for HID typing
 *
 * @param nfc_uid NFC UID bytes (can be NULL)
 * @param nfc_uid_len Length of NFC UID
 * @param rfid_uid RFID UID bytes (can be NULL)
 * @param rfid_uid_len Length of RFID UID
 * @param ndef_text NDEF text payload (can be NULL or empty)
 * @param delimiter Delimiter string between bytes
 * @param nfc_first true if NFC should come before RFID
 * @param output Output buffer
 * @param output_size Size of output buffer
 */
void flipper_wedge_format_output(
    const uint8_t* nfc_uid,
    uint8_t nfc_uid_len,
    const uint8_t* rfid_uid,
    uint8_t rfid_uid_len,
    const char* ndef_text,
    const char* delimiter,
    bool nfc_first,
    char* output,
    size_t output_size);

/** Sanitize text for HID keyboard typing
 * Removes non-printable characters and truncates to max length
 *
 * @param input Input text (may contain binary data)
 * @param output Output buffer for sanitized text
 * @param output_size Size of output buffer
 * @param max_len Maximum characters to keep (0 = no limit)
 * @return Number of characters in sanitized output
 */
size_t flipper_wedge_sanitize_text(
    const char* input,
    char* output,
    size_t output_size,
    size_t max_len);
