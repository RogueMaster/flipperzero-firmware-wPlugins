#include "nfc_comparator_compare_worker_i.h"

NfcComparatorCompareWorker* nfc_comparator_compare_worker_alloc() {
   NfcComparatorCompareWorker* checks = malloc(sizeof(NfcComparatorCompareWorker));
   furi_assert(checks);

   checks->compare_type = NfcCompareWorkerType_Undefined;
   checks->nfc_card_path = furi_string_alloc();
   checks->results.uid = false;
   checks->results.uid_length = false;
   checks->results.protocol = false;
   checks->results.nfc_data = false;
   checks->diff.unit = NfcCompareWorkerComparedDataType_Unknown;
   checks->diff.indices = simple_array_alloc(&simple_array_config_uint16_t);
   checks->diff.count = 0;
   checks->diff.total = 0;

   return checks;
}

void nfc_comparator_compare_worker_free(NfcComparatorCompareWorker* checks) {
   furi_assert(checks);
   furi_string_free(checks->nfc_card_path);
   simple_array_free(checks->diff.indices);
   free(checks);
}

void nfc_comparator_compare_worker_copy(
   NfcComparatorCompareWorker* destination,
   NfcComparatorCompareWorker* data) {
   furi_assert(destination);
   furi_assert(data);
   destination->compare_type = data->compare_type;
   furi_string_reset(destination->nfc_card_path);
   furi_string_set(destination->nfc_card_path, data->nfc_card_path);
   destination->results.uid = data->results.uid;
   destination->results.uid_length = data->results.uid_length;
   destination->results.protocol = data->results.protocol;
   destination->results.nfc_data = data->results.nfc_data;
   destination->diff.unit = data->diff.unit;
   simple_array_copy(destination->diff.indices, data->diff.indices);
   destination->diff.count = data->diff.count;
   destination->diff.total = data->diff.total;
}

void nfc_comparator_compare_worker_reset(NfcComparatorCompareWorker* checks) {
   furi_assert(checks);
   checks->compare_type = NfcCompareWorkerType_Undefined;
   furi_string_reset(checks->nfc_card_path);
   checks->results.uid = false;
   checks->results.uid_length = false;
   checks->results.protocol = false;
   checks->results.nfc_data = false;
   checks->diff.unit = NfcCompareWorkerComparedDataType_Unknown;
   simple_array_reset(checks->diff.indices);
   checks->diff.count = 0;
   checks->diff.total = 0;
}

void nfc_comparator_compare_worker_compare_cards(
   NfcComparatorCompareWorker* checks,
   const struct NfcDevice* card1,
   const struct NfcDevice* card2) {
   furi_assert(checks);
   furi_assert(card1);
   furi_assert(card2);

   // Reset difference counter
   checks->diff.count = 0;
   checks->diff.total = 0;

   // Compare protocols
   checks->results.protocol = nfc_device_get_protocol(card1) == nfc_device_get_protocol(card2);

   // Get UIDs and lengths
   size_t uid_len1 = 0, uid_len2 = 0;
   const uint8_t* uid1 = nfc_device_get_uid(card1, &uid_len1);
   const uint8_t* uid2 = nfc_device_get_uid(card2, &uid_len2);

   // Compare UID length
   checks->results.uid_length = (uid_len1 == uid_len2);

   // Compare UID bytes if lengths match
   if(checks->results.uid_length) {
      checks->results.uid = (memcmp(uid1, uid2, uid_len1) == 0);
   } else {
      checks->results.uid = false;
   }

   // compare NFC data
   if(checks->compare_type == NfcCompareWorkerType_Deep) {
      checks->results.nfc_data = nfc_device_is_equal(card1, card2);

      // COMPARE LOGIC
      if(!checks->results.nfc_data && checks->results.protocol) {
         switch(nfc_device_get_protocol(card1)) {
         // Felica
         case NfcProtocolFelica:
            felica_compare_cards(checks, card1, card2);
            break;

         // NTAG/Mifare Ultralight
         case NfcProtocolMfUltralight:
            mf_ultralight_compare_cards(checks, card1, card2);
            break;

         // Mifare Classic
         case NfcProtocolMfClassic:
            mf_classic_compare_cards(checks, card1, card2);
            break;

         // ST25TB
         case NfcProtocolSt25tb:
            st25tb_compare_cards(checks, card1, card2);
            break;

         // Type 4 Tag
         case NfcProtocolType4Tag:
            type_4_tag_compare_cards(checks, card1, card2);
            break;

         // ISO15693-3
         case NfcProtocolIso15693_3:
            iso15693_3_compare_cards(checks, card1, card2);
            break;

         // SLIX
         case NfcProtocolSlix:
            slix_compare_cards(checks, card1, card2);
            break;

         // EMV
         case NfcProtocolEmv:
            emv_compare_cards(checks, card1, card2);
            break;

         // MF Plus
         case NfcProtocolMfPlus:
            mf_plus_compare_cards(checks, card1, card2);
            break;

         // MF Desfire
         case NfcProtocolMfDesfire:
            mf_desfire_compare_cards(checks, card1, card2);
            break;

         default:
            checks->compare_type = NfcCompareWorkerType_Shallow;
            break;
         }
      }
   }
}
