#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "protocols/emv/emv.h"
#include "protocols/slix/slix.h"
#include "protocols/mf_classic/mf_classic.h"
#include "protocols/felica/felica.h"
#include "protocols/mf_ultralight/mf_ultralight.h"
#include "protocols/st25tb/st25tb.h"
#include "protocols/type_4_tag/type_4_tag.h"
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
 * @enum NfcCompareWorkerType
 * @brief Type of NFC comparison being performed.
 */
typedef enum {
   NfcCompareWorkerType_Shallow,
   NfcCompareWorkerType_Deep,
   NfcCompareWorkerType_Undefined
} NfcCompareWorkerType;

/**
 * @enum NfcCompareWorkerDiffUnit
 * @brief The data type which is being compared in depth
 */
typedef enum {
   NfcCompareWorkerComparedDataType_Blocks,
   NfcCompareWorkerComparedDataType_Pages,
   NfcCompareWorkerComparedDataType_Bytes,
   NfcCompareWorkerComparedDataType_EmvFields,
   NfcCompareWorkerComparedDataType_MFPlusFields,
   NfcCompareWorkerComparedDataType_MFDesfireFields,
   NfcCompareWorkerComparedDataType_Unknown
} NfcCompareWorkerDiffUnit;

/**
 * @brief The unit names
 */
static const char* const nfc_compare_worker_diff_unit_strings[] = {
   [NfcCompareWorkerComparedDataType_Blocks] = "blocks",
   [NfcCompareWorkerComparedDataType_Pages] = "pages",
   [NfcCompareWorkerComparedDataType_Bytes] = "bytes",
   [NfcCompareWorkerComparedDataType_EmvFields] = "fields",
   [NfcCompareWorkerComparedDataType_MFPlusFields] = "fields",
   [NfcCompareWorkerComparedDataType_MFDesfireFields] = "fields",
   [NfcCompareWorkerComparedDataType_Unknown] = "unknown"};

/**
 * @struct NfcComparatorCompareWorker
 * @brief Structure holding the results of NFC comparison checks.
 */
typedef struct NfcComparatorCompareWorker {
   NfcCompareWorkerType compare_type;
   FuriString* nfc_card_path;
   struct {
      bool uid;
      bool uid_length;
      bool protocol;
      bool nfc_data;
   } results;
   struct {
      NfcCompareWorkerDiffUnit unit;
      SimpleArray* indices;
      uint16_t count;
      uint16_t total;
   } diff;
} NfcComparatorCompareWorker;

/**
 * @brief Allocate a new NfcComparatorCompareWorker structure.
 * @return Pointer to the allocated structure, or NULL on failure.
 */
NfcComparatorCompareWorker* nfc_comparator_compare_worker_alloc(void);

/**
 * @brief Free a NfcComparatorCompareWorker structure.
 * @param checks Pointer to the structure to free.
 */
void nfc_comparator_compare_worker_free(NfcComparatorCompareWorker* checks);

/**
 * @brief Copy checks data from one compare checks to another
 * @param destination Where the data should be copied to
 * @param data The data that should be copied to the destination
 */
void nfc_comparator_compare_worker_copy(
   NfcComparatorCompareWorker* destination,
   NfcComparatorCompareWorker* data);

/**
 * @brief Reset the fields of a NfcComparatorCompareWorker structure.
 * @param checks Pointer to the structure to reset.
 */
void nfc_comparator_compare_worker_reset(NfcComparatorCompareWorker* checks);

/**
 * @brief Compare two NFC cards and update the checks structure.
 * @param checks Pointer to the checks structure.
 * @param card1 Pointer to the first NFC card.
 * @param card2 Pointer to the second NFC card.
 * @param check_data Whether to compare NFC data.
 */
void nfc_comparator_compare_worker_compare_cards(
   NfcComparatorCompareWorker* checks,
   const NfcDevice* card1,
   const NfcDevice* card2);

#ifdef __cplusplus
}
#endif
