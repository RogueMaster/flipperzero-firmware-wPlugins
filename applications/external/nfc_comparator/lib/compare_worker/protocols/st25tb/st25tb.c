#include "st25tb_i.h"

void st25tb_compare_cards(
   NfcComparatorCompareWorker* checks,
   const NfcDevice* card1,
   const NfcDevice* card2) {
   const St25tbData* card1_data = nfc_device_get_data(card1, NfcProtocolSt25tb);
   const uint8_t card1_block_count = st25tb_get_block_count(card1_data->type);
   const St25tbData* card2_data = nfc_device_get_data(card2, NfcProtocolSt25tb);
   const uint8_t card2_block_count = st25tb_get_block_count(card2_data->type);

   if(card1_data->type != card2_data->type || card1_block_count != card2_block_count) {
      checks->compare_type = NfcCompareWorkerType_Shallow;
      return;
   }

   checks->diff.total = card1_block_count;
   simple_array_init(checks->diff.indices, card1_block_count);

   for(size_t i = 0; i < card1_block_count && i < ST25TB_MAX_BLOCKS; i++) {
      if(memcmp(&card1_data->blocks[i], &card2_data->blocks[i], ST25TB_BLOCK_SIZE) != 0) {
         uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
         *idx = i;
         checks->diff.count++;
      }
   }

   checks->diff.unit = NfcCompareWorkerComparedDataType_Blocks;

   return;
}
