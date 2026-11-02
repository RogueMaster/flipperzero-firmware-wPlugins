#pragma once

#include "subghz_wardriving_types.h"

#include "subghz_wardriving_worker.h"
#include <lib/subghz/subghz_setting.h>
#include <lib/subghz/receiver.h>
#include <lib/subghz/transmitter.h>
#include <lib/subghz/protocols/raw.h>
#include <lib/subghz/devices/devices.h>

typedef struct SubGhzWarDrivingTxRx SubGhzWarDrivingTxRx;

typedef void (*SubGhzTxRxNeedSaveCallback)(void* context);

typedef enum {
    SubGhzTxRxStartTxStateOk,
    SubGhzTxRxStartTxStateErrorOnlyRx,
    SubGhzTxRxStartTxStateErrorParserOthers,
    SubGhzTxRxStartTxStateErrorNoMemory,
} SubGhzTxRxStartTxState;

/** Free block a send needs. The BinRAW encoder is the largest: 10240 + 2560 bytes */
// What one allocation costs on top of what it asks for: the allocator header, rounded up to
// the alignment. The largest free block is reported with that header counted in, and a
// request spends it out of the block it lands in, so a free block of exactly N bytes cannot
// serve a request for N. It has to be this and not a byte more, or the block handed back
// after a send reads as too small to take again - every size here is a multiple of the
// alignment, so no rounding is left to do
#define SUBGHZ_TX_BLOCK_OVERHEAD 8

// Block a send of this protocol needs in one piece
size_t subghz_wardriving_txrx_tx_block_need(const char* protocol);

// Hold a send block of this size, growing or shrinking what is already held. The history
// calls this with the widest need among the signals it keeps, so every one of them stays
// sendable without reserving for protocols that are not there. False when no such block
// exists, and the previous one is kept
bool subghz_wardriving_txrx_tx_block_reserve(size_t need);

// Size of the send block currently held, 0 when none is
size_t subghz_wardriving_txrx_tx_block_size(void);

// Free block a send needs when the protocol is not one of the few that reserve more.
// The largest of the rest is Power Smart at 1040 timings of 4 bytes,
// then CAME Atomo at 1024 and Beninca ARC at 800
#define SUBGHZ_TX_MIN_FREE_BLOCK_DEFAULT 4608

/**
 * Allocate SubGhzTxRx
 * 
 * @return SubGhzWarDrivingTxRx* pointer to SubGhzTxRx
 */
SubGhzWarDrivingTxRx* subghz_wardriving_txrx_alloc(void);

/**
 * Free SubGhzTxRx
 * 
 * @param instance Pointer to a SubGhzTxRx
 */
void subghz_wardriving_txrx_free(SubGhzWarDrivingTxRx* instance);

/**
 * Release the RX pipeline (worker, receiver and environment, ~40K) until it is
 * needed again. A no-op while TX/RX is running, and safe to call at any time:
 * the pipeline is transparently rebuilt by the first call that needs it.
 *
 * Invalidates the pointer returned by subghz_wardriving_txrx_get_decoder(), so
 * only call it once the loaded signal is no longer on screen.
 *
 * @param instance Pointer to a SubGhzTxRx
 */
void subghz_wardriving_txrx_rx_pipeline_release(SubGhzWarDrivingTxRx* instance);

/**
 * Check if the database is loaded
 * 
 * @param instance Pointer to a SubGhzTxRx
 * @return bool True if the database is loaded
 */
bool subghz_wardriving_txrx_is_database_loaded(SubGhzWarDrivingTxRx* instance);

/**
 * Set preset 
 * 
 * @param instance Pointer to a SubGhzTxRx
 * @param preset_name Name of preset
 * @param frequency Frequency in Hz
 * @param latitude Latitude in float
 * @param longitude Longitude in float
 * @param preset_data Data of preset
 * @param preset_data_size Size of preset data
 */
void subghz_wardriving_txrx_set_preset(
    SubGhzWarDrivingTxRx* instance,
    const char* preset_name,
    uint32_t frequency,
    float latitude,
    float longitude,
    uint8_t* preset_data,
    size_t preset_data_size);

/**
 * Set TX Power
 * 
 * @param preset_data Data of preset
 * @param preset_data_size Size of preset data
 * @param tx_power Menu Index of TX Power Setting. (Saves iterating in Config enter)
 */
uint8_t* subghz_wardriving_txrx_set_tx_power(
    uint8_t* preset_data,
    size_t preset_data_size,
    uint8_t tx_power);

/**
 * Get name of preset
 * 
 * @param instance Pointer to a SubGhzTxRx
 * @param preset String of preset 
 * @return const char*  Name of preset
 */
const char*
    subghz_wardriving_txrx_get_preset_name(SubGhzWarDrivingTxRx* instance, const char* preset);

/**
 * Get of preset
 * 
 * @param instance Pointer to a SubGhzTxRx
 * @return SubGhzRadioPreset Preset
 */
SubGhzRadioPreset subghz_wardriving_txrx_get_preset(SubGhzWarDrivingTxRx* instance);

/**
 * Get string frequency and modulation
 * 
 * @param instance Pointer to a SubGhzTxRx
 * @param frequency Pointer to a string frequency
 * @param modulation Pointer to a string modulation
 */
void subghz_wardriving_txrx_get_frequency_and_modulation(
    SubGhzWarDrivingTxRx* instance,
    FuriString* frequency,
    FuriString* modulation,
    bool long_name);

/**
 * Get latitude value
 *
 * @param instance Pointer to a SubGhzTxRx
 * @return latitude
*/
float subghz_wardriving_txrx_get_latitude(SubGhzWarDrivingTxRx* instance);

/**
 * Get longitude value
 *
 * @param instance Pointer to a SubGhzTxRx
 * @return longitude
*/
float subghz_wardriving_txrx_get_longitude(SubGhzWarDrivingTxRx* instance);

/**
 * Start TX CC1101
 * 
 * @param instance Pointer to a SubGhzTxRx
 * @param flipper_format Pointer to a FlipperFormat
 * @return SubGhzTxRxStartTxState 
 */
SubGhzTxRxStartTxState
    subghz_wardriving_txrx_tx_start(SubGhzWarDrivingTxRx* instance, FlipperFormat* flipper_format);

/**
 * Start RX CC1101
 * 
 * @param instance Pointer to a SubGhzTxRx
 */
void subghz_wardriving_txrx_rx_start(SubGhzWarDrivingTxRx* instance);

/**
 * Stop TX/RX CC1101
 * 
 * @param instance Pointer to a SubGhzTxRx
 */
void subghz_wardriving_txrx_stop(SubGhzWarDrivingTxRx* instance);

/**
 * Set sleep mode CC1101
 * 
 * @param instance Pointer to a SubGhzTxRx
 */
void subghz_wardriving_txrx_sleep(SubGhzWarDrivingTxRx* instance);

/**
 * Update frequency CC1101 in automatic mode (hopper)
 * 
 * @param instance Pointer to a SubGhzTxRx
 * @param stay_threshold RSSI theshold over which to stay before hopping
 */
void subghz_wardriving_txrx_hopper_update(SubGhzWarDrivingTxRx* instance, float stay_threshold);

/**
 * Get state hopper
 * 
 * @param instance Pointer to a SubGhzTxRx
 * @return SubGhzHopperState 
 */
SubGhzHopperState subghz_wardriving_txrx_hopper_get_state(SubGhzWarDrivingTxRx* instance);

/**
 * Set state hopper
 * 
 * @param instance Pointer to a SubGhzTxRx
 * @param state State hopper
 */
void subghz_wardriving_txrx_hopper_set_state(
    SubGhzWarDrivingTxRx* instance,
    SubGhzHopperState state);

/**
 * Unpause hopper
 * 
 * @param instance Pointer to a SubGhzTxRx
 */
void subghz_wardriving_txrx_hopper_unpause(SubGhzWarDrivingTxRx* instance);

/**
 * Set pause hopper
 * 
 * @param instance Pointer to a SubGhzTxRx
 */
void subghz_wardriving_txrx_hopper_pause(SubGhzWarDrivingTxRx* instance);

/**
 * Speaker on
 * 
 * @param instance Pointer to a SubGhzTxRx 
 */
void subghz_wardriving_txrx_speaker_on(SubGhzWarDrivingTxRx* instance);

/**
 * Speaker off
 * 
 * @param instance Pointer to a SubGhzTxRx 
 */
void subghz_wardriving_txrx_speaker_off(SubGhzWarDrivingTxRx* instance);

/**
 * Speaker mute
 * 
 * @param instance Pointer to a SubGhzTxRx 
 */
void subghz_wardriving_txrx_speaker_mute(SubGhzWarDrivingTxRx* instance);

/**
 * Speaker unmute
 * 
 * @param instance Pointer to a SubGhzTxRx 
 */
void subghz_wardriving_txrx_speaker_unmute(SubGhzWarDrivingTxRx* instance);

/**
 * Set state speaker
 * 
 * @param instance Pointer to a SubGhzTxRx 
 * @param state State speaker
 */
void subghz_wardriving_txrx_speaker_set_state(
    SubGhzWarDrivingTxRx* instance,
    SubGhzSpeakerState state);

/**
 * Get state speaker
 * 
 * @param instance Pointer to a SubGhzTxRx 
 * @return SubGhzSpeakerState 
 */
SubGhzSpeakerState subghz_wardriving_txrx_speaker_get_state(SubGhzWarDrivingTxRx* instance);

/**
 * load decoder by name protocol
 * 
 * @param instance Pointer to a SubGhzTxRx
 * @param name_protocol Name protocol
 * @return bool True if the decoder is loaded 
 */
bool subghz_wardriving_txrx_load_decoder_by_name_protocol(
    SubGhzWarDrivingTxRx* instance,
    const char* name_protocol);

/**
 * Get decoder
 * 
 * @param instance Pointer to a SubGhzTxRx
 * @return SubGhzProtocolDecoderBase* Pointer to a SubGhzProtocolDecoderBase
 */
SubGhzProtocolDecoderBase* subghz_wardriving_txrx_get_decoder(SubGhzWarDrivingTxRx* instance);

/**
 * Set callback for save data
 * 
 * @param instance Pointer to a SubGhzTxRx
 * @param callback Callback for save data
 * @param context Context for callback
 */
void subghz_wardriving_txrx_set_need_save_callback(
    SubGhzWarDrivingTxRx* instance,
    SubGhzTxRxNeedSaveCallback callback,
    void* context);

/**
 * Get pointer to a load data key
 * 
 * @param instance Pointer to a SubGhzTxRx
 * @return FlipperFormat* 
 */
FlipperFormat* subghz_wardriving_txrx_get_fff_data(SubGhzWarDrivingTxRx* instance);

/**
 * Get pointer to a SugGhzSetting
 * 
 * @param instance Pointer to a SubGhzTxRx
 * @return SubGhzSetting* 
 */
SubGhzSetting* subghz_wardriving_txrx_get_setting(SubGhzWarDrivingTxRx* instance);

/**
 * Is it possible to save this protocol
 * 
 * @param instance Pointer to a SubGhzTxRx 
 * @return bool True if it is possible to save this protocol
 */
bool subghz_wardriving_txrx_protocol_is_serializable(SubGhzWarDrivingTxRx* instance);

/**
 * Is it possible to send this protocol
 * 
 * @param instance Pointer to a SubGhzTxRx 
 * @return bool True if it is possible to send this protocol
 */
bool subghz_wardriving_txrx_protocol_is_transmittable(
    SubGhzWarDrivingTxRx* instance,
    bool check_type);

/**
 * Set filter, what types of decoder to use 
 * 
 * @param instance Pointer to a SubGhzTxRx
 * @param filter Filter
 */
void subghz_wardriving_txrx_receiver_set_filter(
    SubGhzWarDrivingTxRx* instance,
    SubGhzProtocolFlag filter);

/**
 * Set callback for receive data
 * 
 * @param instance Pointer to a SubGhzTxRx
 * @param callback Callback for receive data
 * @param context Context for callback
 */
void subghz_wardriving_txrx_set_rx_callback(
    SubGhzWarDrivingTxRx* instance,
    SubGhzReceiverCallback callback,
    void* context);

/**
 * Set callback for Raw decoder, end of data transfer  
 * 
 * @param instance Pointer to a SubGhzTxRx
 * @param callback Callback for Raw decoder, end of data transfer 
 * @param context Context for callback
 */
void subghz_wardriving_txrx_set_raw_file_encoder_worker_callback_end(
    SubGhzWarDrivingTxRx* instance,
    SubGhzProtocolEncoderRAWCallbackEnd callback,
    void* context);

/* Checking if an external radio device is connected
* 
* @param instance Pointer to a SubGhzTxRx
* @param name Name of external radio device
* @return bool True if is connected to the external radio device
*/
bool subghz_wardriving_txrx_radio_device_is_external_connected(
    SubGhzWarDrivingTxRx* instance,
    const char* name);

/* Set the selected radio device to use
*
* @param instance Pointer to a SubGhzTxRx
* @param radio_device_type Radio device type
* @return SubGhzRadioDeviceType Type of installed radio device
*/
SubGhzRadioDeviceType subghz_wardriving_txrx_radio_device_set(
    SubGhzWarDrivingTxRx* instance,
    SubGhzRadioDeviceType radio_device_type);

/* Get the selected radio device to use
*
* @param instance Pointer to a SubGhzTxRx
* @return SubGhzRadioDeviceType Type of installed radio device
*/
SubGhzRadioDeviceType subghz_wardriving_txrx_radio_device_get(SubGhzWarDrivingTxRx* instance);

/* Get RSSI the selected radio device to use
*
* @param instance Pointer to a SubGhzTxRx
* @return float RSSI
*/
float subghz_wardriving_txrx_radio_device_get_rssi(SubGhzWarDrivingTxRx* instance);

/* Get name the selected radio device to use
*
* @param instance Pointer to a SubGhzTxRx
* @return const char* Name of installed radio device
*/
const char* subghz_wardriving_txrx_radio_device_get_name(SubGhzWarDrivingTxRx* instance);

/* Get intelligence whether frequency the selected radio device to use
*
* @param instance Pointer to a SubGhzTxRx
* @return bool True if the frequency is valid
*/
bool subghz_wardriving_txrx_radio_device_is_frequency_valid(
    SubGhzWarDrivingTxRx* instance,
    uint32_t frequency);

bool subghz_wardriving_txrx_radio_device_is_tx_allowed(
    SubGhzWarDrivingTxRx* instance,
    uint32_t frequency);

void subghz_wardriving_txrx_set_debug_pin_state(SubGhzWarDrivingTxRx* instance, bool state);
bool subghz_wardriving_txrx_get_debug_pin_state(SubGhzWarDrivingTxRx* instance);

void subghz_wardriving_txrx_reset_dynamic_and_custom_btns(SubGhzWarDrivingTxRx* instance);

SubGhzReceiver* subghz_wardriving_txrx_get_receiver(
    SubGhzWarDrivingTxRx* instance); // TODO use only in DecodeRaw

/**
 * @brief Set current preset AM650 without additional params
 * 
 * @param instance - instance Pointer to a SubGhzTxRx
 * @param frequency - frequency of preset, if pass 0 then taking default frequency 433.92MHz
 */
void subghz_wardriving_txrx_set_default_preset(SubGhzWarDrivingTxRx* instance, uint32_t frequency);

/**
 * @brief Set current preset by index
 * 
 * @param instance  - instance Pointer to a SubGhzTxRx
 * @param frequency - frequency of new preset
 * @param index - index of preset taken from SubGhzSetting
 * @param tx_power - index of TX Power menu index option to use.
 * @return const char* -  name of preset
 */
const char* subghz_wardriving_txrx_set_preset_internal(
    SubGhzWarDrivingTxRx* instance,
    uint32_t frequency,
    uint8_t index,
    uint8_t tx_power);
