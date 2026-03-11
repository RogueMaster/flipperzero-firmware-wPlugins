#pragma once

#include "dashboard.h"

#define APP_DASH_READ_HISTORY 16U
#define DASH_SPEED_HISTORY    10U
#define DASH_VAL_HISTORY      12U
#define DASH_UNIQUE_HISTORY   12U
#define DASH_DBC_HISTORY      10U
#define DASH_CUSTOM_HISTORY   8U
#define DASH_OBD_DTC_MAX      20U
#define DASH_REVERSE_MAX_IDS  10U

typedef struct {
    bool valid;
    uint32_t ts_ms;
    CcBus bus;
    uint32_t id;
    bool ext;
    uint8_t dlc;
    uint8_t data[8];
} AppDashFrameEntry;

typedef struct {
    bool valid;
    CcBus bus;
    uint32_t rate;
} DashboardSpeedSample;

typedef struct {
    bool valid;
    uint8_t byte_idx;
    uint8_t old_value;
    uint8_t new_value;
} DashboardValChange;

typedef struct {
    bool valid;
    CcBus bus;
    uint32_t id;
    bool ext;
} DashboardUniqueEntry;

typedef struct {
    bool valid;
    uint16_t sid;
    char signal_name[APP_DBC_CFG_SIGNAL_NAME_MAX];
    CcBus bus;
    uint32_t frame_id;
    int64_t raw;
    float value;
    bool in_range;
    bool mapped;
    char mapped_label[APP_DBC_CFG_LABEL_MAX];
    char unit[CC_UNIT_TEXT_LEN + 1U];
} DashboardDbcEntry;

typedef struct {
    AppDashboardMode mode;
    char title[24];
    char label[32];
    char value[24];
    char unit[24];
    char note[40];
    char bit_row[4][24];
    char bit_id_line[24];
    bool bittrack_have_live;
    bool bittrack_have_base;
    char bittrack_live[8][9];
    char bittrack_base[8][9];
    bool bittrack_changed[8][8];
    bool bittrack_frozen[8][8];
    CcBus write_bus;
    uint32_t write_id;
    bool write_ext;
    uint8_t write_dlc;
    uint8_t write_data[8];
    uint32_t write_count_cfg;
    uint32_t write_interval_ms_cfg;
    uint32_t counter;
    AppDashFrameEntry read_frames[APP_DASH_READ_HISTORY];
    uint8_t read_head;
    uint8_t read_count;
    uint8_t read_selected;
    uint8_t read_page;
    bool read_hold;
    uint32_t read_total;
    uint32_t read_bus0;
    uint32_t read_bus1;
    uint32_t read_std;
    uint32_t read_ext;
    uint32_t read_rate_window_start_ms;
    uint16_t read_rate_window_count;
    uint32_t read_rate_fps;
    bool read_overload;
    uint8_t input_hold_mask;
    uint8_t reverse_phase;
    uint8_t reverse_count;
    uint8_t reverse_selected;
    bool reverse_overflow;
    uint32_t reverse_ids[DASH_REVERSE_MAX_IDS];
    bool reverse_ext[DASH_REVERSE_MAX_IDS];
    uint8_t reverse_byte_mask[DASH_REVERSE_MAX_IDS];
    uint32_t reverse_flash_until_ms[DASH_REVERSE_MAX_IDS][8];

    uint8_t mode_page;

    bool speed_has_sample;
    CcBus speed_last_bus;
    uint32_t speed_last_rate;
    uint32_t speed_min_rate;
    uint32_t speed_max_rate;
    uint32_t speed_sum_rate;
    uint32_t speed_total_samples;
    DashboardSpeedSample speed_samples[DASH_SPEED_HISTORY];
    uint8_t speed_head;
    uint8_t speed_count;
    uint8_t speed_selected;

    uint8_t val_selected_byte;
    bool val_known[8];
    uint8_t val_bytes[8];
    uint16_t val_byte_changes[8];
    uint32_t val_total_changes;
    DashboardValChange val_changes[DASH_VAL_HISTORY];
    uint8_t val_head;
    uint8_t val_count;
    uint8_t val_selected;

    uint32_t unique_total;
    bool unique_has_last;
    DashboardUniqueEntry unique_last;
    DashboardUniqueEntry unique_entries[DASH_UNIQUE_HISTORY];
    uint8_t unique_head;
    uint8_t unique_count;
    uint8_t unique_selected;

    bool dbc_has_latest;
    DashboardDbcEntry dbc_latest;
    DashboardDbcEntry dbc_entries[DASH_DBC_HISTORY];
    uint8_t dbc_head;
    uint8_t dbc_count;
    uint8_t dbc_selected;
    DashboardDbcEntry dbc_signals[APP_DBC_CFG_MAX_SIGNALS];
    uint8_t dbc_signal_count;
    uint8_t dbc_signal_selected;

    bool obd_dtc_active;
    bool obd_dtc_complete;
    uint8_t obd_dtc_page;
    uint8_t obd_dtc_selected[3];
    uint8_t obd_dtc_count[3];
    char obd_dtc_codes[3][DASH_OBD_DTC_MAX][6];
    uint16_t obd_dtc_cat_counts[4];

    uint8_t custom_selected_slot;
    bool custom_slot_used[5];
    bool custom_slot_ready[5];
    char custom_slot_name[5][18];
    CcBus custom_slot_bus[5];
    uint32_t custom_slot_id[5];
    bool custom_slot_ext[5];
    bool custom_pending;
    uint8_t custom_pending_slot;
    uint32_t custom_pending_remaining;
    uint32_t custom_pending_interval_ms;
    char custom_recent[DASH_CUSTOM_HISTORY][48];
    uint8_t custom_recent_head;
    uint8_t custom_recent_count;
} AppDashboardModel;

void dashboard_apply_template(
    App* app,
    const char* title,
    const char* label,
    const char* value,
    const char* unit,
    const char* note);

void dashboard_bittrack_prepare_template(App* app);

bool dashboard_read_draw(Canvas* canvas, const AppDashboardModel* dashboard);
bool dashboard_read_input(App* app, const InputEvent* event);
void dashboard_read_update(App* app, const CcEvent* event, const char* title);

bool dashboard_bittrack_draw(Canvas* canvas, const AppDashboardModel* dashboard);
bool dashboard_bittrack_input(App* app, const InputEvent* event);
void dashboard_bittrack_update(App* app, const CcEvent* event);

void dashboard_metric_draw(Canvas* canvas, const AppDashboardModel* dashboard);
bool dashboard_metric_input(App* app, const InputEvent* event);
void dashboard_update_obd(App* app, const CcEvent* event);
void dashboard_update_write(App* app, const CcEvent* event);
void dashboard_update_speed(App* app, const CcEvent* event);
void dashboard_update_valtrack(App* app, const CcEvent* event);
void dashboard_update_unique_ids(App* app, const CcEvent* event);
void dashboard_update_reverse(App* app, const CcEvent* event);
void dashboard_update_dbc_decode(App* app, const CcEvent* event);
void dashboard_update_custom_inject(App* app, const CcEvent* event);
