#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NfcComparatorCompareChecks NfcComparatorCompareChecks;
typedef struct NfcDevice NfcDevice;

/**
 * @brief Is used to compared two MF Desfire cards together
 * @param checks The compare checks to be filled out by the comparison function
 * @param card1 The first card to be compared
 * @param card2 The second card to be compared
 */
void mf_desfire_compare_cards(
   NfcComparatorCompareChecks* checks,
   const NfcDevice* card1,
   const NfcDevice* card2);

/**
 * @enum MFDesfireFields
 * @brief Enumeration of MF Desfire payment card fields for comparison
 */
typedef enum {
   MFDesfireFields_Applications,
   MFDesfireFields_HardwareVersion,
   MFDesfireFields_SoftwareVersion
} MFDesfireFields;

/**
 * @brief Names for MF Plus fields
 */
static const char* const MFDesfireFieldNames[] = {
   [MFDesfireFields_Applications] = "Applications",
   [MFDesfireFields_HardwareVersion] = "Hardware Version",
   [MFDesfireFields_SoftwareVersion] = "Software Version"};

#ifdef __cplusplus
}
#endif
