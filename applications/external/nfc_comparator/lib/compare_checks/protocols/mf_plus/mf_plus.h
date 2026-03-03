#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NfcComparatorCompareChecks NfcComparatorCompareChecks;
typedef struct NfcDevice NfcDevice;

/**
 * @brief Is used to compared two MF Plus cards together
 * @param checks The compare checks to be filled out by the comparison function
 * @param card1 The first card to be compared
 * @param card2 The second card to be compared
 */
void mf_plus_compare_cards(
   NfcComparatorCompareChecks* checks,
   const NfcDevice* card1,
   const NfcDevice* card2);

/**
 * @enum MFPlusFields
 * @brief Enumeration of MF Plus payment card fields for comparison
 */
typedef enum {
   MFPlusFields_HardwareVersion,
   MFPlusFields_SecurityLevel,
   MFPlusFields_Size,
   MFPlusFields_SoftwareVersion,
   MFPlusFields_Type
} MFPlusFields;

/**
 * @brief Names for MF Plus fields
 */
static const char* const MFPlusFieldNames[] = {
   [MFPlusFields_HardwareVersion] = "Hardware Version",
   [MFPlusFields_SecurityLevel] = "Security Level",
   [MFPlusFields_Size] = "Size",
   [MFPlusFields_SoftwareVersion] = "Software Version",
   [MFPlusFields_Type] = "Type",
};

#ifdef __cplusplus
}
#endif
