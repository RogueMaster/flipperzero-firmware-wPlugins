#include "iso15693_3_i.h"

void iso15693_3_compare_cards(
   NfcComparatorCompareChecks* checks,
   const NfcDevice* card1,
   const NfcDevice* card2) {
   const Iso15693_3Data* card1_data = nfc_device_get_data(card1, NfcProtocolIso15693_3);
   const uint8_t card1_block_size = card1_data->system_info.block_size;
   const uint16_t card1_block_count = card1_data->system_info.block_count;
   const Iso15693_3Data* card2_data = nfc_device_get_data(card2, NfcProtocolIso15693_3);
   const uint8_t card2_block_size = card2_data->system_info.block_size;
   const uint16_t card2_block_count = card2_data->system_info.block_count;

   if(card1_block_size != card2_block_size || card1_block_count != card2_block_count ||
      card1_data->system_info.ic_ref != card2_data->system_info.ic_ref) {
      checks->compare_type = NfcCompareChecksType_Shallow;
      return;
   }

   checks->diff.total = card1_block_count;
   simple_array_init(checks->diff.indices, card1_block_count);

   for(size_t i = 0; i < card1_block_count && i < ISO15693_3_MAX_BLOCK_COUNT; i++) {
      const uint8_t* block1 =
         (const uint8_t*)simple_array_cget(card1_data->block_data, i * card1_block_size);
      const uint8_t* block2 =
         (const uint8_t*)simple_array_cget(card2_data->block_data, i * card2_block_size);

      if(memcmp(block1, block2, card1_block_size) != 0) {
         uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
         *idx = i;
         checks->diff.count++;
      }
   }

   checks->diff.unit = NfcCompareChecksComparedDataType_Blocks;

   return;
}
