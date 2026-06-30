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
void metroflip_back_button_widget_callback(GuiButtonType result, InputType type, void* context);
void metroflip_next_button_widget_callback(GuiButtonType result, InputType type, void* context);
void calypso_save_button_widget_callback(GuiButtonType result, InputType type, void* context);
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


extern const Icon I_Suica_AsakusaA;
extern const Icon I_Suica_BigStar;
extern const Icon I_Suica_ChiyodaC;
extern const Icon I_Suica_CrackingEgg;
extern const Icon I_Suica_DashLine;
extern const Icon I_Suica_EmptyArrowDown;
extern const Icon I_Suica_EmptyArrowRight;
extern const Icon I_Suica_EntrySlider1;
extern const Icon I_Suica_EntrySlider2;
extern const Icon I_Suica_EntrySlider3;
extern const Icon I_Suica_EntryStopL1;
extern const Icon I_Suica_EntryStopL2;
extern const Icon I_Suica_EntryStopL3;
extern const Icon I_Suica_EntryStopR1;
extern const Icon I_Suica_EntryStopR2;
extern const Icon I_Suica_EntryStopR3;
extern const Icon I_Suica_FilledArrowDown;
extern const Icon I_Suica_FilledArrowRight;
extern const Icon I_Suica_GinzaG;
extern const Icon I_Suica_HanzomonZ;
extern const Icon I_Suica_HibiyaH;
extern const Icon I_Suica_JRLogo;
extern const Icon I_Suica_KeikyuKK;
extern const Icon I_Suica_KeikyuLogo;
extern const Icon I_Suica_MarunouchiHonanchoMb;
extern const Icon I_Suica_MarunouchiM;
extern const Icon I_Suica_MinusSign0;
extern const Icon I_Suica_MinusSign1;
extern const Icon I_Suica_MinusSign2;
extern const Icon I_Suica_MinusSign3;
extern const Icon I_Suica_MinusSign4;
extern const Icon I_Suica_MinusSign5;
extern const Icon I_Suica_MinusSign6;
extern const Icon I_Suica_MinusSign7;
extern const Icon I_Suica_MinusSign8;
extern const Icon I_Suica_MinusSign9;
extern const Icon I_Suica_MitaI;
extern const Icon I_Suica_MobileLogo;
extern const Icon I_Suica_MoneyStack1;
extern const Icon I_Suica_MoneyStack2;
extern const Icon I_Suica_MoneyStack3;
extern const Icon I_Suica_MoneyStack4;
extern const Icon I_Suica_NambokuN;
extern const Icon I_Suica_Nothing;
extern const Icon I_Suica_OedoE;
extern const Icon I_Suica_OsakaMetroLogo;
extern const Icon I_Suica_PenguinHappyBirthday;
extern const Icon I_Suica_PenguinTodaysVIP;
extern const Icon I_Suica_PlusSign1;
extern const Icon I_Suica_PlusSign2;
extern const Icon I_Suica_PlusSign3;
extern const Icon I_Suica_PlusStar;
extern const Icon I_Suica_QuestionMarkBig;
extern const Icon I_Suica_QuestionMarkSmall;
extern const Icon I_Suica_RinkaiR;
extern const Icon I_Suica_ShinjukuS;
extern const Icon I_Suica_ShopPin;
extern const Icon I_Suica_SmallStar;
extern const Icon I_Suica_StoreFan1;
extern const Icon I_Suica_StoreFan2;
extern const Icon I_Suica_StoreFrame;
extern const Icon I_Suica_StoreLightningHorizontal;
extern const Icon I_Suica_StoreLightningVertical;
extern const Icon I_Suica_StoreP1Counter;
extern const Icon I_Suica_StoreReceiptDashLine;
extern const Icon I_Suica_StoreReceiptFrame1;
extern const Icon I_Suica_StoreReceiptFrame2;
extern const Icon I_Suica_StoreSlidingDoor;
extern const Icon I_Suica_TWRLogo;
extern const Icon I_Suica_ToeiLogo;
extern const Icon I_Suica_TokyoMetroLogo;
extern const Icon I_Suica_TokyoMonorailLogo;
extern const Icon I_Suica_TokyuLogo;
extern const Icon I_Suica_TopUpIcon;
extern const Icon I_Suica_TopUpPage2_0001;
extern const Icon I_Suica_TopUpPage2_0002;
extern const Icon I_Suica_TopUpPage2_0003;  
extern const Icon I_Suica_TopUpPage2_0004;
extern const Icon I_Suica_TopUpPage2_0005;
extern const Icon I_Suica_TopUpPage2_0006;
extern const Icon I_Suica_TopUpPage2_0007;
extern const Icon I_Suica_TopUpPage2_0008;
extern const Icon I_Suica_TopUpPage2_0009;
extern const Icon I_Suica_TopUpPage2_0010;
extern const Icon I_Suica_TopUpPage2_0011;
extern const Icon I_Suica_TopUpPage2_0012;
extern const Icon I_Suica_TopUpPage2_0013;
extern const Icon I_Suica_TopUpPage2_0014;
extern const Icon I_Suica_TopUpPage2_0015;
extern const Icon I_Suica_TopUpPage2_0016;
extern const Icon I_Suica_TopUpPage2_0017;
extern const Icon I_Suica_TopUpPage2_0018;
extern const Icon I_Suica_TopUpPage2_0019;
extern const Icon I_Suica_TopUpPage2_0020;
extern const Icon I_Suica_TopUpPage2_0021;
extern const Icon I_Suica_TopUpPage2_0022;
extern const Icon I_Suica_TopUpPage2_0023;
extern const Icon I_Suica_TopUpPage2_0024;
extern const Icon I_Suica_TopUpPage2_0025;
extern const Icon I_Suica_TopUpPage2_0026;
extern const Icon I_Suica_TopUpPage2_0027;
extern const Icon I_Suica_TozaiT;
extern const Icon I_Suica_VendingCan1;
extern const Icon I_Suica_VendingCan2;
extern const Icon I_Suica_VendingCan3;
extern const Icon I_Suica_VendingCan4;
extern const Icon I_Suica_VendingFlap1;
extern const Icon I_Suica_VendingFlap2;
extern const Icon I_Suica_VendingFlap3;
extern const Icon I_Suica_VendingFlapHollow;
extern const Icon I_Suica_VendingMachine1;
extern const Icon I_Suica_VendingMachine2;
extern const Icon I_Suica_VendingMachine3;
extern const Icon I_Suica_VendingMachine4;
extern const Icon I_Suica_VendingMachine5;
extern const Icon I_Suica_VendingMachine6;
extern const Icon I_Suica_VendingPage2Full1;
extern const Icon I_Suica_VendingPage2Full2;
extern const Icon I_Suica_VendingPage2Full3;
extern const Icon I_Suica_VendingPage2Full4;
extern const Icon I_Suica_VendingPage2Full5;
extern const Icon I_Suica_VendingPage2Full6;
extern const Icon I_Suica_VendingPage2Full7;
extern const Icon I_Suica_VendingPage2Full8;
extern const Icon I_Suica_VendingPage2Full9;
extern const Icon I_Suica_VendingPage2Full10;
extern const Icon I_Suica_VendingPage2Full11;
extern const Icon I_Suica_VendingPage2Full12;
extern const Icon I_Suica_VendingPage2Full13;
extern const Icon I_Suica_VendingPage2Full14;
extern const Icon I_Suica_VendingThankYou;
extern const Icon I_Suica_YenKanji;
extern const Icon I_Suica_YenSign;
extern const Icon I_Suica_YurakuchoY;
extern const Icon I_Suica_YurikamomeLogo;
extern const Icon I_Suica_YurikamomeU;
extern const Icon I_Suica_CardIcon;
extern const Icon I_Suica_ShopIcon;
extern const Icon I_Suica_VendingIcon;
extern const Icon I_Suica_TrainIcon;
extern const Icon I_Suica_BdayCakeIcon;
extern const Icon I_Suica_UnknownIcon;

/*******************/
#ifdef __cplusplus
}
#endif
