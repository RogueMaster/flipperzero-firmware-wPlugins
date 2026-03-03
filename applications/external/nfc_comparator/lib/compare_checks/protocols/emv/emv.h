#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NfcComparatorCompareChecks NfcComparatorCompareChecks;
typedef struct NfcDevice NfcDevice;

/**
 * @brief Is used to compared two EMV cards together
 * @param checks The compare checks to be filled out by the comparison function
 * @param card1 The first card to be compared
 * @param card2 The second card to be compared
 */
void emv_compare_cards(
   NfcComparatorCompareChecks* checks,
   const NfcDevice* card1,
   const NfcDevice* card2);

/**
 * @enum EmvFields
 * @brief Enumeration of EMV payment card fields for comparison
 */
typedef enum {
   EmvField_AID,
   EmvField_ApplicationLabel,
   EmvField_ApplicationName,
   EmvField_CardNumber,
   EmvField_CardHolder,
   EmvField_CountryCode,
   EmvField_CurrencyCode,
   EmvField_ExpirationDate
} EmvFields;

/**
 * @brief Names for EMV fields
 */
static const char* const EmvFieldNames[] = {
   [EmvField_AID] = "AID",
   [EmvField_ApplicationLabel] = "Application Label",
   [EmvField_ApplicationName] = "Application Name",
   [EmvField_CardNumber] = "Card Number",
   [EmvField_CardHolder] = "Cardholder Name",
   [EmvField_CountryCode] = "Country Code",
   [EmvField_CurrencyCode] = "Currency Code",
   [EmvField_ExpirationDate] = "Expiration Date"};

#ifdef __cplusplus
}
#endif
