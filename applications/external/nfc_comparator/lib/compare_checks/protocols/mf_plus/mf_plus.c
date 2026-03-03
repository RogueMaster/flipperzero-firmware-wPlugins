#include "mf_plus_i.h"

void mf_plus_compare_cards(
   NfcComparatorCompareChecks* checks,
   const NfcDevice* card1,
   const NfcDevice* card2) {
   const MfPlusData* card1_data = nfc_device_get_data(card1, NfcProtocolMfPlus);
   const MfPlusData* card2_data = nfc_device_get_data(card2, NfcProtocolMfPlus);

   checks->diff.total = 5;
   simple_array_init(checks->diff.indices, checks->diff.total);
   checks->diff.count = 0;

   // Compare Hardware Version
   if(card1_data->version.hw_major != card2_data->version.hw_major ||
      card1_data->version.hw_minor != card2_data->version.hw_minor) {
      uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
      *idx = MFPlusFields_HardwareVersion;
      checks->diff.count++;
   }

   // Compare Security Level
   if(card1_data->security_level != card2_data->security_level) {
      uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
      *idx = MFPlusFields_SecurityLevel;
      checks->diff.count++;
   }

   // Compare Size
   if(card1_data->size != card2_data->size) {
      uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
      *idx = MFPlusFields_Size;
      checks->diff.count++;
   }

   // Compare Software Version
   if(card1_data->version.sw_major != card2_data->version.sw_major ||
      card1_data->version.sw_minor != card2_data->version.sw_minor) {
      uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
      *idx = MFPlusFields_SoftwareVersion;
      checks->diff.count++;
   }

   // Compare Types
   if(card1_data->type != card2_data->type) {
      uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
      *idx = MFPlusFields_Type;
      checks->diff.count++;
   }

   checks->diff.unit = NfcCompareChecksComparedDataType_MFPlusFields;

   return;
}
