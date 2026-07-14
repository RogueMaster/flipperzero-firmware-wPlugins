#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NfcComparatorCompareWorker NfcComparatorCompareWorker;
typedef struct NfcDevice NfcDevice;

/**
 * @brief Is used to compared two Type 4 Tag cards together
 * @param checks The compare checks to be filled out by the comparison function
 * @param card1 The first card to be compared
 * @param card2 The second card to be compared
 */
void type_4_tag_compare_cards(
   NfcComparatorCompareWorker* checks,
   const NfcDevice* card1,
   const NfcDevice* card2);

#ifdef __cplusplus
}
#endif
