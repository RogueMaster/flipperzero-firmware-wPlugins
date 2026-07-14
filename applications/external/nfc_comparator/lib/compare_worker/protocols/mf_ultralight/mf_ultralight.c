#include "mf_ultralight_i.h"

void mf_ultralight_compare_cards(
   NfcComparatorCompareWorker* checks,
   const NfcDevice* card1,
   const NfcDevice* card2) {
   const MfUltralightData* card1_data = nfc_device_get_data(card1, NfcProtocolMfUltralight);
   const uint16_t card1_block_count = mf_ultralight_get_pages_total(card1_data->type);
   const MfUltralightData* card2_data = nfc_device_get_data(card2, NfcProtocolMfUltralight);
   const uint16_t card2_block_count = mf_ultralight_get_pages_total(card2_data->type);

   if(card1_data->type != card2_data->type || card1_block_count != card2_block_count) {
      checks->compare_type = NfcCompareWorkerType_Shallow;
      return;
   }

   checks->diff.total = card1_block_count;
   simple_array_init(checks->diff.indices, card1_block_count);

   for(size_t i = 0; i < card1_block_count && i < MF_ULTRALIGHT_MAX_PAGE_NUM; i++) {
      if(memcmp(card1_data->page[i].data, card2_data->page[i].data, MF_ULTRALIGHT_PAGE_SIZE) !=
         0) {
         uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
         *idx = i;
         checks->diff.count++;
      }
   }

   checks->diff.unit = NfcCompareWorkerComparedDataType_Pages;

   return;
}
