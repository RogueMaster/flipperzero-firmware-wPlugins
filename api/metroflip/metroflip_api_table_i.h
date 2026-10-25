#include "metroflip_api.h"
#include "metroflip_card_view.h"
#include "../../metroflip_i.h"

/*
 * A list of app's private functions and objects to expose for plugins.
 * It is used to generate a table of symbols for import resolver to use.
 * TBD: automatically generate this table from app's header files
 */
static constexpr auto metroflip_api_table = sort(create_array_t<sym_entry>(
    // card view
    API_METHOD(metroflip_card_view_alloc, View*, (Metroflip*)),
    API_METHOD(metroflip_card_view_set_title, void, (View*, const char*)),
    API_METHOD(metroflip_card_view_add_page, uint8_t, (View*, const char*)),
    API_METHOD(
        metroflip_card_view_add_field,
        void,
        (View*, uint8_t, const char*, const char*, bool)),
    API_METHOD(metroflip_card_view_set_save, void, (View*, bool)),
    API_METHOD(metroflip_card_view_set_delete, void, (View*, bool)),
    API_METHOD(metroflip_card_view_show, void, (Metroflip*)),

    // metroflip stuff
    API_METHOD(metroflip_exit_widget_callback, void, (GuiButtonType, InputType, void*)),
    API_METHOD(metroflip_save_widget_callback, void, (GuiButtonType, InputType, void*)),
    API_METHOD(metroflip_delete_widget_callback, void, (GuiButtonType, InputType, void*)),
    API_METHOD(metroflip_app_blink_start, void, (Metroflip*)),
    API_METHOD(metroflip_app_blink_stop, void, (Metroflip*)),
    API_METHOD(bit_slice_to_dec, int, (const char*, int, int)),
    API_METHOD(byte_to_binary, void, (uint8_t, char*)),
    API_METHOD(read_calypso_data, uint8_t*, (FlipperFormat*, const char*, const char*)),
    API_VARIABLE(read_file, uint8_t[5]),
    API_VARIABLE(apdu_success, uint8_t[2]),
    API_VARIABLE(select_app, uint8_t[8]),
    API_METHOD(mf_classic_key_cache_load, bool, (MfClassicKeyCache*, const uint8_t*, size_t)),
    API_METHOD(
        mf_classic_key_cache_get_next_key,
        bool,
        (MfClassicKeyCache*, uint8_t*, MfClassicKey*, MfClassicKeyType*)),
    API_METHOD(mf_classic_key_cache_reset, void, (MfClassicKeyCache*)),
    API_METHOD(
        manage_keyfiles,
        KeyfileManager,
        (char[], const uint8_t*, size_t, MfClassicKeyCache*, uint64_t, uint64_t)),
    API_METHOD(uid_to_string, void, (const uint8_t*, size_t, char*, size_t)),
    API_METHOD(
        handle_keyfile_case,
        void,
        (Metroflip*, const char*, const char*, FuriString*, char[])),

    // calypso
    API_METHOD(get_calypso_node_offset, int, (const char*, const char*, CalypsoApp*)),
    API_METHOD(get_network_string, const char*, (CALYPSO_CARD_TYPE)),
    API_METHOD(get_country_string, const char*, (int)),
    API_METHOD(is_calypso_node_present, bool, (const char*, const char*, CalypsoApp*)),
    API_METHOD(get_calypso_node_size, int, (const char*, CalypsoApp*)),
    API_METHOD(free_calypso_structure, void, (CalypsoApp*)),
    API_METHOD(guess_card_type, CALYPSO_CARD_TYPE, (int, int)),

    // intercode
    API_METHOD(get_intercode_structure_env_holder, CalypsoApp*, ()),
    API_METHOD(get_intercode_structure_contract, CalypsoApp*, ()),
    API_METHOD(get_intercode_structure_event, CalypsoApp*, ()),
    API_METHOD(get_intercode_structure_counter, CalypsoApp*, ()),

    // navigo
    API_METHOD(show_navigo_event_info, void, (NavigoCardEvent*, NavigoCardContract*, FuriString*)),
    API_METHOD(show_navigo_special_event_info, void, (NavigoCardSpecialEvent*, FuriString*)),
    API_METHOD(show_navigo_contract_info, void, (NavigoCardContract*, FuriString*)),
    API_METHOD(show_navigo_environment_info, void, (NavigoCardEnv*, NavigoCardHolder*, FuriString*)),

    // opus
    API_METHOD(get_opus_contract_structure, CalypsoApp*, ()),
    API_METHOD(get_opus_event_structure, CalypsoApp*, ()),
    API_METHOD(get_opus_env_holder_structure, CalypsoApp*, ()),
    API_METHOD(show_opus_event_info, void, (OpusCardEvent*, OpusCardContract*, FuriString*)),
    API_METHOD(show_opus_contract_info, void, (OpusCardContract*, FuriString*)),
    API_METHOD(show_opus_environment_info, void, (OpusCardEnv*, OpusCardHolder*, FuriString*)),

    // ravkav
    API_METHOD(get_ravkav_contract_structure, CalypsoApp*, ()),
    API_METHOD(get_ravkav_event_structure, CalypsoApp*, ()),
    API_METHOD(get_ravkav_env_holder_structure, CalypsoApp*, ()),
    API_METHOD(show_ravkav_event_info, void, (RavKavCardEvent*, FuriString*)),
    API_METHOD(show_ravkav_contract_info, void, (RavKavCardContract*, FuriString*)),
    API_METHOD(show_ravkav_environment_info, void, (RavKavCardEnv*, FuriString*)),

    API_VARIABLE(I_RFIDDolphinReceive_97x61, Icon),
    API_VARIABLE(I_icon, Icon),

    API_VARIABLE(I_WarningDolphinFlip_45x42, Icon),
    API_VARIABLE(I_DolphinDone_80x58, Icon),
    API_VARIABLE(I_DolphinMafia_119x62, Icon)));
