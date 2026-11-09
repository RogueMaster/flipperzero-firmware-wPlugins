/**
 * @file metroflip_api.h
 * @brief Application API example.
 *
 * This file contains an API that is internally implemented by the application
 * It is also exposed to plugins to allow them to use the application's API.
 */
#pragma once

#include <stdint.h>
#include "../../metroflip_i.h"
#include <gui/gui.h>
#include <gui/modules/widget_elements/widget_element.h>
#include "../calypso/calypso_i.h"
#include "../calypso/calypso_util.h"
#include <datetime.h>
#include <stdbool.h>

#include "metroflip_card_view.h"

#ifdef __cplusplus
extern "C" {
#endif

// metroflip

void metroflip_exit_widget_callback(GuiButtonType result, InputType type, void* context);
void metroflip_save_widget_callback(GuiButtonType result, InputType type, void* context);
void metroflip_delete_widget_callback(GuiButtonType result, InputType type, void* context);

void metroflip_app_blink_start(Metroflip* metroflip);

void metroflip_app_blink_stop(Metroflip* metroflip);

int bit_slice_to_dec(const char* bit_representation, int start, int end);

void byte_to_binary(uint8_t byte, char* bits);

bool mf_classic_key_cache_load(MfClassicKeyCache* instance, const uint8_t* uid, size_t uid_len);

bool mf_classic_key_cache_get_next_key(
    MfClassicKeyCache* instance,
    uint8_t* sector_num,
    MfClassicKey* key,
    MfClassicKeyType* key_type);

void mf_classic_key_cache_reset(MfClassicKeyCache* instance);

KeyfileManager manage_keyfiles(
    char uid_str[],
    const uint8_t* uid,
    size_t uid_len,
    MfClassicKeyCache* instance,
    uint64_t key_mask_a_required,
    uint64_t key_mask_b_required);

void uid_to_string(const uint8_t* uid, size_t uid_len, char* uid_str, size_t max_len);

void handle_keyfile_case(
    Metroflip* app,
    const char* message_title,
    const char* log_message,
    FuriString* parsed_data,
    char card_type[]);

extern uint8_t read_file[5];
extern uint8_t apdu_success[2];
extern uint8_t select_app[8];

/*****  calypso *****/
int get_calypso_node_offset(const char* binary_string, const char* key, CalypsoApp* structure);
const char* get_network_string(CALYPSO_CARD_TYPE card_type);
const char* get_country_string(int country_num);
bool is_calypso_node_present(const char* binary_string, const char* key, CalypsoApp* structure);
int get_calypso_node_size(const char* key, CalypsoApp* structure);
void free_calypso_structure(CalypsoApp* structure);
CALYPSO_CARD_TYPE guess_card_type(int country_num, int network_num);
uint8_t* read_calypso_data(FlipperFormat* format, const char* app_id, const char* file_id);

// intercode

CalypsoApp* get_intercode_structure_env_holder();

CalypsoApp* get_intercode_structure_contract();

CalypsoApp* get_intercode_structure_event();

CalypsoApp* get_intercode_structure_counter();

//navigo

void show_navigo_event_info(
    NavigoCardEvent* event,
    NavigoCardContract* contracts,
    FuriString* parsed_data);

void show_navigo_special_event_info(NavigoCardSpecialEvent* event, FuriString* parsed_data);

void show_navigo_contract_info(NavigoCardContract* contract, FuriString* parsed_data);

void show_navigo_environment_info(
    NavigoCardEnv* environment,
    NavigoCardHolder* holder,
    FuriString* parsed_data);

// opus

CalypsoApp* get_opus_contract_structure();

CalypsoApp* get_opus_event_structure();

CalypsoApp* get_opus_env_holder_structure();

void show_opus_event_info(
    OpusCardEvent* event,
    OpusCardContract* contracts,
    FuriString* parsed_data);

void show_opus_contract_info(OpusCardContract* contract, FuriString* parsed_data);

void show_opus_environment_info(
    OpusCardEnv* environment,
    OpusCardHolder* holder,
    FuriString* parsed_data);

//ravkav

CalypsoApp* get_ravkav_contract_structure();

CalypsoApp* get_ravkav_event_structure();

CalypsoApp* get_ravkav_env_holder_structure();

void show_ravkav_event_info(RavKavCardEvent* event, FuriString* parsed_data);

void show_ravkav_contract_info(RavKavCardContract* contract, FuriString* parsed_data);

void show_ravkav_environment_info(RavKavCardEnv* environment, FuriString* parsed_data);

extern const Icon I_RFIDDolphinReceive_97x61;
extern const Icon I_icon;
extern const Icon I_DolphinDone_80x58;
extern const Icon I_WarningDolphinFlip_45x42;
extern const Icon I_DolphinMafia_119x62;

/*******************/
#ifdef __cplusplus
}
#endif
