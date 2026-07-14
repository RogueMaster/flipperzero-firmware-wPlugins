#include "mf_desfire_i.h"

void mf_desfire_compare_cards(
   NfcComparatorCompareWorker* checks,
   const NfcDevice* card1,
   const NfcDevice* card2) {
   const MfDesfireData* card1_data = nfc_device_get_data(card1, NfcProtocolMfPlus);
   const MfDesfireData* card2_data = nfc_device_get_data(card2, NfcProtocolMfPlus);

   checks->diff.total = 3;
   simple_array_init(checks->diff.indices, checks->diff.total);
   checks->diff.count = 0;

   // Compare Application
   if(!simple_array_is_equal(card1_data->applications, card2_data->applications)) {
      uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
      *idx = MFDesfireFields_Applications;
      checks->diff.count++;
   }

   // Compare Hardware Version
   if(card1_data->version.hw_major != card2_data->version.hw_major ||
      card1_data->version.hw_minor != card2_data->version.hw_minor) {
      uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
      *idx = MFDesfireFields_HardwareVersion;
      checks->diff.count++;
   }

   // Compare Software Version
   if(card1_data->version.sw_major != card2_data->version.sw_major ||
      card1_data->version.sw_minor != card2_data->version.sw_minor) {
      uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
      *idx = MFDesfireFields_SoftwareVersion;
      checks->diff.count++;
   }

   checks->diff.unit = NfcCompareWorkerComparedDataType_MFDesfireFields;

   return;
}
