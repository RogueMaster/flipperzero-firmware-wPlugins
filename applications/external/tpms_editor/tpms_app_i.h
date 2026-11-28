#pragma once

#include "helpers/tpms_types.h"

#include "scenes/tpms_scene.h"
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/widget.h>
#include <gui/modules/text_input.h>
#include <gui/modules/number_input.h>
#include <notification/notification_messages.h>
#include "views/tpms_receiver.h"
#include "views/tpms_receiver_info.h"
#include "tpms_history.h"
#include "tpms_encoder.h"
#include "protocols/proto6/proto6_bridge.h"

#include <lib/subghz/subghz_setting.h>
#include <lib/subghz/types.h>
#include <lib/subghz/devices/devices.h>

typedef struct TPMSApp TPMSApp;

struct TPMSTxRx {
    SubGhzRadioPreset* preset;
    TPMSHistory* history;
    Proto6Bridge* proto6;
    const SubGhzDevice* radio_device;
    TPMSRadioMode radio_mode;
    bool radio_devices_initialized;
    bool radio_external_active;
    bool radio_external_begun;
    bool radio_otg_owned;
    bool radio_auto_probe_done;
    uint16_t idx_menu_chosen;
    TPMSTxRxState txrx_state;
    TPMSHopperState hopper_state;
    uint8_t hopper_timeout;
    uint8_t hopper_idx_frequency;
    TPMSRxKeyState rx_key_state;
};

typedef struct TPMSTxRx TPMSTxRx;

struct TPMSApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    TPMSTxRx* txrx;
    SceneManager* scene_manager;
    NotificationApp* notifications;
    FuriMutex* history_mutex;
    VariableItemList* variable_item_list;
    Submenu* submenu;
    Widget* widget;
    TextInput* text_input;
    NumberInput* number_input;
    TPMSReceiver* tpms_receiver;
    TPMSReceiverInfo* tpms_receiver_info;
    TPMSLock lock;
    SubGhzSetting* setting;
    TPMSRelearn relearn;
    TPMSRelearnType relearn_type;
    uint8_t relearn_protocol_index;
    uint8_t relearn_band_index;
    uint8_t relearn_el_band_index;
    uint8_t relearn_ford_band_index;
    uint8_t relearn_modulation_index;
    uint8_t relearn_rx_band_index;
    bool relearn_autostart;
    bool relearn_auto_rx;
    uint8_t relearn_auto_profile_index;
    uint32_t relearn_auto_next_tick;
    uint8_t relearn_ford_auto_lf_step;
    /* TPMS 4.3 (logic retained from 4.2): AUTO Ford+VDO alternates COMPLETE LF wake cycles, never
       individual telegrams inside one cycle. Keep this independent from the
       existing UHF AUTO profile rotation above. */
    uint8_t relearn_ford_auto_wake_step;
    uint8_t relearn_ford_auto_pair_uhf_profile;
    bool relearn_ford_auto_pair_uhf_valid;
    TPMSFordLFProfile relearn_ford_profile;
    TPMSCWFrequency relearn_cw_frequency;
    TPMSRelearnRepeatMode relearn_repeat_mode;
    TPMSRelearnRXWait relearn_rx_wait;
    bool relearn_cycle_was_active;
    bool relearn_repeat_waiting;
    bool relearn_auto_satisfied;
    /* TPMS 4.1+ wake trace: correlate the first valid RF frame after an
       EL-50449 Ford/VDO wake cycle with the LF profile that was used. */
    uint8_t relearn_last_wake_profile;
    bool relearn_last_wake_valid;
    uint32_t relearn_last_wake_tick;
    uint32_t relearn_repeat_next_tick;
    bool relearn_led_green_phase;
    bool relearn_led_rx_on;
    uint32_t relearn_led_next_tick;
    float receiver_rssi_recent_peak;
    uint32_t receiver_rssi_recent_start_tick;
    bool receiver_rssi_recent_valid;

    /* TPMS 4.0 ordinary Odczyt AUTO modulation. Frequency remains fixed;
       only FSK/OOK/GFSK rotates. Kept separate from Relearn AUTO. */
    bool receiver_modulation_auto;
    uint8_t receiver_modulation_auto_index;
    uint32_t receiver_modulation_auto_next_tick;
    bool editor_profile_loaded;
    TpmsEditValues editor_profile_values;
    char editor_profile_protocol[48];

    /* TPMS 4.5: session autosave for decoded RX frames.
       The setting is persisted on SD; the duplicate cache is session-only. */
    bool autosave_enabled;
    uint32_t autosave_saved_count;
    uint32_t autosave_error_count;
    uint8_t autosave_cache_count;
    uint8_t autosave_cache_next;
    uint32_t autosave_cache_keys[32];
    uint32_t autosave_cache_hashes[32];

    /* One-shot simulator prefill selected from LAB session/saved library. */
    bool simulation_prefill_valid;
    TpmsEditValues simulation_prefill_values;
    uint8_t simulation_lab_source;
    uint8_t simulation_lab_session_slot;
    char simulation_lab_path[128];

    /* Hidden session-only AUTO LAB. Compact profiles are kept on SD, not as
       ten full frames in RAM. */
    bool lab_unlocked;
    uint8_t lab_about_taps;
    uint32_t lab_about_last_tick;
    float lab_target_pressure_kpa;
    /* AUTO LAB RX selection: 0=AUTO, band 1=315/2=433; modulation 1=FSK/2=OOK/3=GFSK. */
    uint8_t lab_band_index;
    uint8_t lab_modulation_index;
    uint8_t lab_auto_profile_index;
    uint8_t lab_sensor_count;
    uint8_t lab_sensor_list_source;
    uint32_t lab_sensor_keys[10];
    uint32_t lab_phase_started_tick;
    uint32_t lab_round;
    uint8_t lab_last_tx_ok;
    uint32_t lab_last_tx_frequency;
    uint8_t lab_last_tx_preset;
    uint8_t lab_last_tx_kind;
    FlipperFormat* lab_format;
    FuriString* lab_protocol;
};

bool tpms_ensure_text_input(TPMSApp* app);
void tpms_release_text_input(TPMSApp* app);
bool tpms_ensure_number_input(TPMSApp* app);
void tpms_release_number_input(TPMSApp* app);

bool tpms_radio_init(TPMSApp* app);
bool tpms_radio_select(TPMSApp* app, TPMSRadioMode mode);
bool tpms_radio_ensure_selected(TPMSApp* app);
void tpms_radio_deinit(TPMSApp* app);
bool tpms_radio_is_external(const TPMSApp* app);
float tpms_radio_get_rssi(const TPMSApp* app);
void tpms_radio_get_setting_text(const TPMSApp* app, char* text, size_t text_size);

void tpms_preset_init(
    void* context,
    const char* preset_name,
    uint32_t frequency,
    uint8_t* preset_data,
    size_t preset_data_size);
bool tpms_set_preset(TPMSApp* app, const char* preset);
void tpms_get_frequency_modulation(TPMSApp* app, FuriString* frequency, FuriString* modulation);
void tpms_begin(TPMSApp* app, uint8_t* preset_data);
uint32_t tpms_rx(TPMSApp* app, uint32_t frequency);
void tpms_idle(TPMSApp* app);
void tpms_rx_end(TPMSApp* app);
void tpms_sleep(TPMSApp* app);
void tpms_hopper_update(TPMSApp* app);

/* Hidden AUTO LAB helpers. Session capture stays capped at 10, while
   explicitly saved profiles live in an SD-card library without a 10-item cap. */
#define TPMS_LAB_MAX_SENSORS 10U
#define TPMS_LAB_SAVED_FOLDER EXT_PATH("apps_data/tpms_editor/lab_saved")

typedef enum {
    TPMSLabListSaved = 0,
    TPMSLabListSession = 1,
} TPMSLabListSource;

typedef enum {
    TPMSLabSimulationNone = 0,
    TPMSLabSimulationSession = 1,
    TPMSLabSimulationSaved = 2,
} TPMSLabSimulationSource;

void tpms_lab_clear_session_files(void);
bool tpms_lab_session_load(
    uint8_t slot, TpmsEditValues* values, char* protocol, size_t protocol_size);
bool tpms_lab_session_delete(uint8_t slot);
uint8_t tpms_lab_session_count(void);
bool tpms_lab_prepare(TPMSApp* app);
void tpms_lab_release(TPMSApp* app, bool clear_files);
bool tpms_lab_start_rx(TPMSApp* app);
void tpms_lab_stop_rx(TPMSApp* app);
bool tpms_lab_capture_decoder(TPMSApp* app, SubGhzProtocolDecoderBase* decoder);
uint8_t tpms_lab_send_burst(TPMSApp* app);
void tpms_lab_saved_migrate_legacy(void);
uint16_t tpms_lab_saved_count(void);
bool tpms_lab_saved_store(const TpmsEditValues* values, const char* protocol);
bool tpms_lab_saved_store_named(
    const TpmsEditValues* values, const char* protocol, const char* label);
bool tpms_lab_saved_delete(const char* path);
