
#pragma once

#include <math.h>
#include <furi.h>
#include <furi_hal.h>
#include <lib/flipper_format/flipper_format.h>
#include <lib/subghz/types.h>
#include "helpers/tpms_capture.h"
#include "tpms_encoder.h"

typedef struct TPMSHistory TPMSHistory;

/** History state add key */
typedef enum {
    TPMSHistoryStateAddKeyUnknown,
    TPMSHistoryStateAddKeyTimeOut,
    TPMSHistoryStateAddKeyNewDada,
    TPMSHistoryStateAddKeyUpdateData,
    TPMSHistoryStateAddKeyReplaceData,
    TPMSHistoryStateAddKeyOverflow,
} TPMSHistoryStateAddKey;

/** Allocate TPMSHistory
 * 
 * @return TPMSHistory* 
 */
TPMSHistory* tpms_history_alloc(void);

/** Free TPMSHistory
 * 
 * @param instance - TPMSHistory instance
 */
void tpms_history_free(TPMSHistory* instance);

/** Clear history
 * 
 * @param instance - TPMSHistory instance
 */
void tpms_history_reset(TPMSHistory* instance);

/** Get frequency to history[idx]
 * 
 * @param instance  - TPMSHistory instance
 * @param idx       - record index  
 * @return frequency - frequency Hz
 */
uint32_t tpms_history_get_frequency(TPMSHistory* instance, uint16_t idx);

SubGhzRadioPreset* tpms_history_get_radio_preset(TPMSHistory* instance, uint16_t idx);

/** Get preset to history[idx]
 * 
 * @param instance  - TPMSHistory instance
 * @param idx       - record index  
 * @return preset   - preset name
 */
const char* tpms_history_get_preset(TPMSHistory* instance, uint16_t idx);

/** Get history index write 
 * 
 * @param instance  - TPMSHistory instance
 * @return idx      - current record index  
 */
uint16_t tpms_history_get_item(TPMSHistory* instance);

/** Get type protocol to history[idx]
 * 
 * @param instance  - TPMSHistory instance
 * @param idx       - record index  
 * @return type      - type protocol  
 */
uint8_t tpms_history_get_type_protocol(TPMSHistory* instance, uint16_t idx);

/** Get name protocol to history[idx]
 * 
 * @param instance  - TPMSHistory instance
 * @param idx       - record index  
 * @return name      - const char* name protocol  
 */
const char* tpms_history_get_protocol_name(TPMSHistory* instance, uint16_t idx);

/** Get string item menu to history[idx]
 * 
 * @param instance  - TPMSHistory instance
 * @param output    - FuriString* output
 * @param idx       - record index
 */
void tpms_history_get_text_item_menu(TPMSHistory* instance, FuriString* output, uint16_t idx);


/** Store/read RSSI captured for the most recently decoded TPMS frame. */
bool tpms_history_set_frame_rssi(TPMSHistory* instance, uint16_t idx, float rssi);
bool tpms_history_get_frame_rssi(TPMSHistory* instance, uint16_t idx, int16_t* rssi_dbm);

/** Optional LF wake profile correlated with this received frame. */
bool tpms_history_set_wake_profile(TPMSHistory* instance, uint16_t idx, uint8_t wake_profile);
bool tpms_history_get_wake_profile(TPMSHistory* instance, uint16_t idx, uint8_t* wake_profile);

/** Get string the remaining number of records to history
 * 
 * @param instance  - TPMSHistory instance
 * @param output    - FuriString* output
 * @return bool - true when all rolling slots are occupied
 */
bool tpms_history_get_text_space_left(TPMSHistory* instance, FuriString* output);

/** Add protocol to history
 * 
 * @param instance  - TPMSHistory instance
 * @param context    - SubGhzProtocolCommon context
 * @param preset    - SubGhzRadioPreset preset
 * @return TPMSHistoryStateAddKey;
 */
TPMSHistoryStateAddKey
    tpms_history_add_to_history(TPMSHistory* instance, void* context, SubGhzRadioPreset* preset);

/** Get SubGhzProtocolCommonLoad to load into the protocol decoder bin data
 * 
 * @param instance  - TPMSHistory instance
 * @param idx       - record index
 * @return SubGhzProtocolCommonLoad*
 */
FlipperFormat* tpms_history_get_raw_data(TPMSHistory* instance, uint16_t idx);

/** Index of the history item changed by the most recent successful add/update. */
bool tpms_history_get_last_affected_index(TPMSHistory* instance, uint16_t* idx);

/** Store a compact signed RAW capture for a history item. */
bool tpms_history_set_raw_capture(
    TPMSHistory* instance,
    uint16_t idx,
    const int16_t* samples,
    uint16_t count,
    bool truncated);

/** Copy the compact signed RAW capture from a history item. */
bool tpms_history_copy_raw_capture(
    TPMSHistory* instance,
    uint16_t idx,
    int16_t* output,
    uint16_t capacity,
    uint16_t* count,
    bool* truncated);
