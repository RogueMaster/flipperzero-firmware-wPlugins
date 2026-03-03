#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "protocols/emv/emv.h"
#include "protocols/slix/slix.h"
#include "protocols/mf_classic/mf_classic.h"
#include "protocols/felica/felica.h"
#include "protocols/mf_ultralight/mf_ultralight.h"
#include "protocols/st25tb/st25tb.h"
#include "protocols/iso15693_3/iso15693_3.h"
#include "protocols/mf_plus/mf_plus.h"
#include "protocols/mf_desfire/mf_desfire.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SimpleArray SimpleArray;
typedef struct NfcDevice NfcDevice;
typedef struct FuriString FuriString;

/**
 * @enum NfcCompareChecksType
 * @brief Type of NFC comparison being performed.
 */
typedef enum {
   NfcCompareChecksType_Shallow,
   NfcCompareChecksType_Deep,
   NfcCompareChecksType_Undefined
} NfcCompareChecksType;

/**
 * @enum NfcCompareChecksDiffUnit
 * @brief The data type which is being compared in depth
 */
typedef enum {
   NfcCompareChecksComparedDataType_Blocks,
   NfcCompareChecksComparedDataType_Pages,
   NfcCompareChecksComparedDataType_Bytes,
   NfcCompareChecksComparedDataType_EmvFields,
   NfcCompareChecksComparedDataType_MFPlusFields,
   NfcCompareChecksComparedDataType_MFDesfireFields,
   NfcCompareChecksComparedDataType_Unknown
} NfcCompareChecksDiffUnit;

/**
 * @brief The unit names
 */
static const char* const nfc_compare_checks_diff_unit_strings[] = {
   [NfcCompareChecksComparedDataType_Blocks] = "blocks",
   [NfcCompareChecksComparedDataType_Pages] = "pages",
   [NfcCompareChecksComparedDataType_Bytes] = "bytes",
   [NfcCompareChecksComparedDataType_EmvFields] = "fields",
   [NfcCompareChecksComparedDataType_MFPlusFields] = "fields",
   [NfcCompareChecksComparedDataType_MFDesfireFields] = "fields",
   [NfcCompareChecksComparedDataType_Unknown] = "unknown"};

/**
 * @struct NfcComparatorCompareChecks
 * @brief Structure holding the results of NFC comparison checks.
 */
typedef struct NfcComparatorCompareChecks {
   NfcCompareChecksType compare_type;
   FuriString* nfc_card_path;
   struct {
      bool uid;
      bool uid_length;
      bool protocol;
      bool nfc_data;
   } results;
   struct {
      NfcCompareChecksDiffUnit unit;
      SimpleArray* indices;
      uint16_t count;
      uint16_t total;
   } diff;
} NfcComparatorCompareChecks;

/**
 * @brief Allocate a new NfcComparatorCompareChecks structure.
 * @return Pointer to the allocated structure, or NULL on failure.
 */
NfcComparatorCompareChecks* nfc_comparator_compare_checks_alloc(void);

/**
 * @brief Free a NfcComparatorCompareChecks structure.
 * @param checks Pointer to the structure to free.
 */
void nfc_comparator_compare_checks_free(NfcComparatorCompareChecks* checks);

/**
 * @brief Copy checks data from one compare checks to another
 * @param destination Where the data should be copied to
 * @param data The data that should be copied to the destination
 */
void nfc_comparator_compare_checks_copy(
   NfcComparatorCompareChecks* destination,
   NfcComparatorCompareChecks* data);

/**
 * @brief Reset the fields of a NfcComparatorCompareChecks structure.
 * @param checks Pointer to the structure to reset.
 */
void nfc_comparator_compare_checks_reset(NfcComparatorCompareChecks* checks);

/**
 * @brief Compare two NFC cards and update the checks structure.
 * @param checks Pointer to the checks structure.
 * @param card1 Pointer to the first NFC card.
 * @param card2 Pointer to the second NFC card.
 * @param check_data Whether to compare NFC data.
 */
void nfc_comparator_compare_checks_compare_cards(
   NfcComparatorCompareChecks* checks,
   const NfcDevice* card1,
   const NfcDevice* card2);

#ifdef __cplusplus
}
#endif
