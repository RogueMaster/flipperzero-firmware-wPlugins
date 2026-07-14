#include "type_4_tag_i.h"

void type_4_tag_compare_cards(
   NfcComparatorCompareWorker* checks,
   const NfcDevice* card1,
   const NfcDevice* card2) {
   const Type4TagData* card1_data = nfc_device_get_data(card1, NfcProtocolType4Tag);
   const uint32_t card1_block_count = simple_array_get_count(card1_data->ndef_data);
   const Type4TagData* card2_data = nfc_device_get_data(card2, NfcProtocolType4Tag);
   const uint32_t card2_block_count = simple_array_get_count(card2_data->ndef_data);

   if(card1_block_count != card2_block_count || card1_data->platform != card2_data->platform) {
      checks->compare_type = NfcCompareWorkerType_Shallow;
      return;
   }

   checks->diff.total = card1_block_count;
   simple_array_init(checks->diff.indices, card1_block_count);

   const uint8_t* block1 = (const uint8_t*)simple_array_cget(card1_data->ndef_data, 0);
   const uint8_t* block2 = (const uint8_t*)simple_array_cget(card2_data->ndef_data, 0);

   for(size_t i = 0; i < card1_block_count && i < TYPE_4_TAG_MF_DESFIRE_NDEF_SIZE; i++) {
      if(block1[i] != block2[i]) {
         uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
         *idx = i;
         checks->diff.count++;
      }
   }

   checks->diff.unit = NfcCompareWorkerComparedDataType_Bytes;

   return;
}
