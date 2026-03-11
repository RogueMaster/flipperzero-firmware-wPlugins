#include "emv_i.h"

void emv_compare_cards(
   NfcComparatorCompareWorker* checks,
   const NfcDevice* card1,
   const NfcDevice* card2) {
   const EmvData* card1_data = nfc_device_get_data(card1, NfcProtocolEmv);
   const EmvData* card2_data = nfc_device_get_data(card2, NfcProtocolEmv);

   checks->diff.total = 8;
   simple_array_init(checks->diff.indices, checks->diff.total);
   checks->diff.count = 0;

   // Compare AID
   if(card1_data->emv_application.aid_len != card2_data->emv_application.aid_len ||
      memcmp(
         card1_data->emv_application.aid,
         card2_data->emv_application.aid,
         card1_data->emv_application.aid_len) != 0) {
      uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
      *idx = EmvField_AID;
      checks->diff.count++;
   }

   // Compare application label
   if(strcmp(
         card1_data->emv_application.application_label,
         card2_data->emv_application.application_label) != 0) {
      uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
      *idx = EmvField_ApplicationLabel;
      checks->diff.count++;
   }

   // Compare application name
   if(strcmp(
         card1_data->emv_application.application_name,
         card2_data->emv_application.application_name) != 0) {
      uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
      *idx = EmvField_ApplicationName;
      checks->diff.count++;
   }

   // Compare PAN (card number)
   if(card1_data->emv_application.pan_len != card2_data->emv_application.pan_len ||
      memcmp(
         card1_data->emv_application.pan,
         card2_data->emv_application.pan,
         card1_data->emv_application.pan_len) != 0) {
      uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
      *idx = EmvField_CardNumber;
      checks->diff.count++;
   }

   // Compare cardholder name
   if(strcmp(
         card1_data->emv_application.cardholder_name,
         card2_data->emv_application.cardholder_name) != 0) {
      uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
      *idx = EmvField_CardHolder;
      checks->diff.count++;
   }

   // Compare country code
   if(card1_data->emv_application.country_code != card2_data->emv_application.country_code) {
      uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
      *idx = EmvField_CountryCode;
      checks->diff.count++;
   }

   // Compare currency code
   if(card1_data->emv_application.currency_code != card2_data->emv_application.currency_code) {
      uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
      *idx = EmvField_CurrencyCode;
      checks->diff.count++;
   }

   // Compare expiration date
   if(card1_data->emv_application.exp_month != card2_data->emv_application.exp_month ||
      card1_data->emv_application.exp_year != card2_data->emv_application.exp_year) {
      uint16_t* idx = simple_array_get(checks->diff.indices, checks->diff.count);
      *idx = EmvField_ExpirationDate;
      checks->diff.count++;
   }

   checks->diff.unit = NfcCompareWorkerComparedDataType_EmvFields;

   return;
}
