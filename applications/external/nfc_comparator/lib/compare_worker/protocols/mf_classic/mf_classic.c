#include "mf_classic_i.h"

void mf_classic_compare_cards(
   NfcComparatorCompareWorker* checks,
   const NfcDevice* card1,
   const NfcDevice* card2) {
   const MfClassicData* card1_data = nfc_device_get_data(card1, NfcProtocolMfClassic);
   const uint16_t card1_block_count = mf_classic_get_total_block_num(card1_data->type);
   const MfClassicData* card2_data = nfc_device_get_data(card2, NfcProtocolMfClassic);
   const uint16_t card2_block_count = mf_classic_get_total_block_num(card2_data->type);

   if(card1_data->type != card2_data->type || card1_block_count != card2_block_count) {
      checks->compare_type = NfcCompareWorkerType_Shallow;
      return;
   }

   checks->diff.total = card1_block_count;
   simple_array_init(checks->diff.indices, card1_block_count);

   for(size_t i = 0; i < card1_block_count && i < MF_CLASSIC_TOTAL_BLOCKS_MAX; i++) {
      if(memcmp(card1_data->block[i].data, card2_data->block[i].data, MF_CLASSIC_BLOCK_SIZE) !=
         0) {
         uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
         *idx = i;
         checks->diff.count++;
      }
   }

   checks->diff.unit = NfcCompareWorkerComparedDataType_Blocks;

   return;
}
