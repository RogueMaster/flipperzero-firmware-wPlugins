#include "felica_i.h"

void felica_compare_cards(
   NfcComparatorCompareWorker* checks,
   const NfcDevice* card1,
   const NfcDevice* card2) {
   const FelicaData* card1_data = nfc_device_get_data(card1, NfcProtocolFelica);
   const uint8_t card1_block_count = card1_data->blocks_total;
   const FelicaData* card2_data = nfc_device_get_data(card2, NfcProtocolFelica);
   const uint8_t card2_block_count = card2_data->blocks_total;

   if(card1_block_count != card2_block_count ||
      card1_data->workflow_type != card2_data->workflow_type) {
      checks->compare_type = NfcCompareWorkerType_Shallow;
      return;
   }

   checks->diff.total = card1_block_count;
   simple_array_init(checks->diff.indices, card1_block_count);

   for(size_t i = 0; i < card1_block_count && i < FELICA_STANDARD_MAX_BLOCK_COUNT; i++) {
      const uint8_t* card1_block = &card1_data->data.dump[i * (FELICA_DATA_BLOCK_SIZE + 2)];
      const uint8_t* card2_block = &card2_data->data.dump[i * (FELICA_DATA_BLOCK_SIZE + 2)];

      if(memcmp(card1_block, card2_block, FELICA_DATA_BLOCK_SIZE) != 0) {
         uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
         *idx = i;
         checks->diff.count++;
      }
   }

   checks->diff.unit = NfcCompareWorkerComparedDataType_Blocks;

   return;
}
