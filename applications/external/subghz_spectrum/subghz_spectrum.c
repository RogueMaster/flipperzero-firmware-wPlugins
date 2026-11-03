/* subghz_spectrum.c — Main application and UI for Sub-GHz Spectrum Analyzer.
 * Displays real-time RSSI across a selectable frequency band via CC1101. */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/view.h>
#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <input/input.h>
#include <storage/storage.h>
#include <toolbox/stream/file_stream.h>
#include <gui/elements.h>
#include <stdio.h>

#include <datetime/datetime.h>

#include "spectrum_types.h"
#include "spectrum_worker.h"
#include "../common/suite_helpers.h"

const SpectrumBandConfig spectrum_band_configs[] = {
    [SpectrumBand315] = {310000000, 320000000, 100, "315 MHz"},
    [SpectrumBand433] = {425000000, 445000000, 100, "433 MHz"},
    [SpectrumBand868] = {860000000, 880000000, 100, "868 MHz"},
    [SpectrumBand915] = {900000000, 930000000, 100, "915 MHz"},
    [SpectrumBandCustom] = {430000000, 440000000, 100, "Custom"},
};

#define TAG "SubGhzSpectrum"

/* View IDs */
typedef enum {
    SpectrumViewSpectrum,
    SpectrumViewBandSelect,
    SpectrumViewSettings,
    SpectrumViewCount,
} SpectrumViewId;

/* Step size options in kHz */
static const uint32_t step_values[] = {10, 25, 50, 100, 200};
static const char* step_names[] = {"10 kHz", "25 kHz", "50 kHz", "100 kHz", "200 kHz"};
#define STEP_COUNT 5

/* Main app context */
typedef struct {
    ViewDispatcher* view_dispatcher;
    View* spectrum_view;
    Submenu* band_submenu;
    VariableItemList* settings_list;
    VariableItem* peak_item; /* Peak-hold settings item, synced to the live toggle */
    Gui* gui;
    SpectrumWorker* worker;

    /* Current configuration */
    SpectrumBand current_band;
    uint8_t step_index;
    bool peak_hold;
    bool logging;

    /* Logging — guarded by log_mutex.  The display state is held solely in the
       custom view's locking model (single source of truth), so there is no
       separate SpectrumData copy or data_mutex to keep in step. */
    FuriMutex* log_mutex;
    Storage* storage;
    File* log_file;
} SpectrumApp;

/** Reset a SpectrumData struct to safe initial values */
static void spectrum_data_reset(SpectrumData* data) {
    memset(data, 0, sizeof(SpectrumData));
    for(uint8_t i = 0; i < SPECTRUM_NUM_BINS; i++) {
        data->peak_rssi[i] = -200.0f;
    }
}

/* ─── Spectrum View Draw ─── */

static void spectrum_draw_bar_graph(Canvas* canvas, SpectrumData* data) {
    /* Header line */
    canvas_set_font(canvas, FontSecondary);
    char header[40];
    uint32_t freq_khz = data->max_rssi_freq / 1000;
    int32_t rssi_int = (int32_t)data->max_rssi;
    {
        int ret = snprintf(
            header,
            sizeof(header),
            "%lu.%01lu MHz  RSSI:%ld dBm",
            (unsigned long)(freq_khz / 1000),
            (unsigned long)((freq_khz % 1000) / 100),
            (long)rssi_int);
        if(ret < 0 || (size_t)ret >= sizeof(header)) header[sizeof(header) - 1] = '\0';
    }
    canvas_draw_str(canvas, 0, 8, header);

    /* Draw bars */
    uint8_t graph_y = SPECTRUM_HEADER_HEIGHT;
    uint8_t graph_h = SPECTRUM_GRAPH_HEIGHT;

    for(uint8_t i = 0; i < data->num_bins && i < 128; i++) {
        float rssi = data->rssi[i];
        /* Normalize RSSI to 0-1 range */
        float norm = (rssi - SPECTRUM_RSSI_MIN) / (SPECTRUM_RSSI_MAX - SPECTRUM_RSSI_MIN);
        if(norm < 0.0f) norm = 0.0f;
        if(norm > 1.0f) norm = 1.0f;

        uint8_t bar_h = (uint8_t)(norm * graph_h);
        if(bar_h > 0) {
            canvas_draw_line(canvas, i, graph_y + graph_h - bar_h, i, graph_y + graph_h);
        }

        /* Peak hold markers */
        if(data->peak_hold) {
            float peak_norm =
                (data->peak_rssi[i] - SPECTRUM_RSSI_MIN) / (SPECTRUM_RSSI_MAX - SPECTRUM_RSSI_MIN);
            if(peak_norm < 0.0f) peak_norm = 0.0f;
            if(peak_norm > 1.0f) peak_norm = 1.0f;
            uint8_t peak_y = graph_y + graph_h - (uint8_t)(peak_norm * graph_h);
            canvas_draw_dot(canvas, i, peak_y);
        }
    }

    /* Cursor */
    if(data->cursor_pos < data->num_bins) {
        uint8_t cx = data->cursor_pos;
        /* Dotted vertical line */
        for(uint8_t y = graph_y; y < graph_y + graph_h; y += 2) {
            canvas_draw_dot(canvas, cx, y);
        }
        /* Cursor frequency */
        uint32_t cursor_freq = data->freq_start + (uint32_t)data->cursor_pos * data->freq_step;
        char cursor_str[24];
        uint32_t cfreq_khz = cursor_freq / 1000;
        {
            int ret = snprintf(
                cursor_str,
                sizeof(cursor_str),
                "%lu.%03lu",
                (unsigned long)(cfreq_khz / 1000),
                (unsigned long)(cfreq_khz % 1000));
            if(ret < 0 || (size_t)ret >= sizeof(cursor_str))
                cursor_str[sizeof(cursor_str) - 1] = '\0';
        }
        uint8_t str_x = (cx > 80) ? cx - 40 : cx + 2;
        canvas_draw_str(canvas, str_x, graph_y + graph_h - 2, cursor_str);
    }
}

static void spectrum_draw_waterfall(Canvas* canvas, SpectrumData* data) {
    /* Header */
    canvas_set_font(canvas, FontSecondary);
    char header[40];
    uint32_t wf_freq_khz = data->max_rssi_freq / 1000;
    int32_t wf_rssi_int = (int32_t)data->max_rssi;
    {
        int ret = snprintf(
            header,
            sizeof(header),
            "WF %lu.%01lu MHz  Pk:%ld",
            (unsigned long)(wf_freq_khz / 1000),
            (unsigned long)((wf_freq_khz % 1000) / 100),
            (long)wf_rssi_int);
        if(ret < 0 || (size_t)ret >= sizeof(header)) header[sizeof(header) - 1] = '\0';
    }
    canvas_draw_str(canvas, 0, 8, header);

    /* Draw waterfall rows */
    uint8_t wf_y = SPECTRUM_HEADER_HEIGHT;
    uint8_t wf_h = SPECTRUM_GRAPH_HEIGHT;
    uint8_t rows_to_draw = (wf_h < SPECTRUM_WATERFALL_ROWS) ? wf_h : SPECTRUM_WATERFALL_ROWS;

    for(uint8_t row = 0; row < rows_to_draw; row++) {
        /* Index from oldest to newest */
        uint8_t data_row = (data->waterfall_row + row + 1) % SPECTRUM_WATERFALL_ROWS;
        for(uint8_t col = 0; col < data->num_bins && col < 128; col++) {
            /* Dithered rendering: pixel on if intensity > threshold */
            uint8_t intensity = data->waterfall[data_row][col];
            /* Simple 2x2 ordered dither */
            uint8_t threshold = ((row & 1) * 2 + (col & 1)) * 64;
            if(intensity > threshold) {
                canvas_draw_dot(canvas, col, wf_y + row);
            }
        }
    }
}

static void spectrum_view_draw_callback(Canvas* canvas, void* model) {
    SpectrumData* data = model;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(data->num_bins == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Starting scan...");
        return;
    }

    if(data->view_mode == SpectrumViewBar) {
        spectrum_draw_bar_graph(canvas, data);
    } else {
        spectrum_draw_waterfall(canvas, data);
    }
}

static bool spectrum_view_input_callback(InputEvent* event, void* context) {
    SpectrumApp* app = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    /* OK toggles CSV logging.  This touches only app->logging / log_file, which
       are guarded by log_mutex — never the display model — so an in-progress SD
       write from the worker can't be torn out from under it, and the draw lock
       is never held across file I/O. */
    if(event->key == InputKeyOk) {
        furi_mutex_acquire(app->log_mutex, FuriWaitForever);
        app->logging = !app->logging;
        if(app->logging) {
            FuriString* path = furi_string_alloc();
            DateTime dt;
            datetime_timestamp_to_datetime(furi_hal_rtc_get_timestamp(), &dt);
            furi_string_printf(
                path,
                EXT_PATH("subghz_spectrum/scan_%04d%02d%02d_%02d%02d%02d.csv"),
                dt.year,
                dt.month,
                dt.day,
                dt.hour,
                dt.minute,
                dt.second);
            storage_simply_mkdir(app->storage, EXT_PATH("subghz_spectrum"));
            if(!storage_file_open(
                   app->log_file, furi_string_get_cstr(path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                app->logging = false;
                FURI_LOG_E(TAG, "Failed to open log file");
            } else {
                const char* csv_header = "timestamp_ms,frequency_hz,rssi_dbm\n";
                storage_file_write(app->log_file, csv_header, strlen(csv_header));
            }
            furi_string_free(path);
        } else {
            storage_file_close(app->log_file);
        }
        furi_mutex_release(app->log_mutex);
        return true;
    }

    /* Left/Right/Up/Down mutate the display model in place.  The locking view
       model serialises against the worker callback and the draw callback, so no
       separate data copy/mutex is needed. */
    bool consumed = false;
    with_view_model(
        app->spectrum_view,
        SpectrumData * data,
        {
            if(data->num_bins != 0) {
                switch(event->key) {
                case InputKeyLeft:
                    if(data->cursor_pos > 0) data->cursor_pos--;
                    consumed = true;
                    break;
                case InputKeyRight:
                    if(data->cursor_pos + 1 < data->num_bins) data->cursor_pos++;
                    consumed = true;
                    break;
                case InputKeyUp:
                    data->view_mode = (data->view_mode == SpectrumViewBar) ?
                                          SpectrumViewWaterfall :
                                          SpectrumViewBar;
                    consumed = true;
                    break;
                case InputKeyDown:
                    data->peak_hold = !data->peak_hold;
                    if(!data->peak_hold) {
                        for(uint8_t i = 0; i < SPECTRUM_NUM_BINS; i++) {
                            data->peak_rssi[i] = -200.0f;
                        }
                    }
                    app->peak_hold = data->peak_hold; /* keep stored value in sync */
                    consumed = true;
                    break;
                default:
                    break;
                }
            }
        },
        true);

    /* Sync the Settings "Peak Hold" item with the live Down-key toggle so the
       two no longer diverge. */
    if(consumed && event->key == InputKeyDown && app->peak_item) {
        variable_item_set_current_value_index(app->peak_item, app->peak_hold ? 1 : 0);
        variable_item_set_current_value_text(app->peak_item, app->peak_hold ? "On" : "Off");
    }

    return consumed;
}

/* ─── Worker Callback ─── */

static void spectrum_worker_callback(const SpectrumSweep* sweep, void* context) {
    SpectrumApp* app = context;

    /* Fold the sweep into the display model in place.  The locking view model
       serialises with the draw and input callbacks, so there is no second
       SpectrumData copy and no 7 KB per-sweep memcpy. */
    with_view_model(
        app->spectrum_view,
        SpectrumData * data,
        {
            /* Clamp the producer's bin count at the copy site (S4): never copy
               more than the destination holds, whatever the worker reports. */
            uint8_t n = sweep->num_bins;
            if(n > SPECTRUM_NUM_BINS) n = SPECTRUM_NUM_BINS;

            memcpy(data->rssi, sweep->rssi, sizeof(float) * n);
            data->num_bins = n;
            data->freq_start = sweep->freq_start;
            data->freq_step = sweep->freq_step;
            data->max_rssi = sweep->max_rssi;
            data->max_rssi_freq = sweep->max_rssi_freq;

            /* Peak hold */
            if(data->peak_hold) {
                for(uint8_t i = 0; i < n; i++) {
                    if(data->rssi[i] > data->peak_rssi[i]) {
                        data->peak_rssi[i] = data->rssi[i];
                    } else {
                        data->peak_rssi[i] -= 0.5f; /* slow decay */
                        if(data->peak_rssi[i] < SPECTRUM_RSSI_MIN) {
                            data->peak_rssi[i] = SPECTRUM_RSSI_MIN;
                        }
                    }
                }
            }

            /* Waterfall */
            data->waterfall_row = (data->waterfall_row + 1) % SPECTRUM_WATERFALL_ROWS;
            for(uint8_t i = 0; i < n; i++) {
                float norm =
                    (data->rssi[i] - SPECTRUM_RSSI_MIN) / (SPECTRUM_RSSI_MAX - SPECTRUM_RSSI_MIN);
                if(norm < 0.0f) norm = 0.0f;
                if(norm > 1.0f) norm = 1.0f;
                data->waterfall[data->waterfall_row][i] = (uint8_t)(norm * 255.0f);
            }
        },
        true);

    /* CSV logging runs outside the view lock, reading straight from the
       worker-owned sweep, and emits a single batched write per sweep instead of
       up to 128 tiny writes (which previously ran while holding the draw lock).
       log_mutex is a dedicated lock, so the draw path is never blocked by it. */
    furi_mutex_acquire(app->log_mutex, FuriWaitForever);
    if(app->logging && app->log_file) {
        uint8_t n = sweep->num_bins;
        if(n > SPECTRUM_NUM_BINS) n = SPECTRUM_NUM_BINS;
        uint32_t ts = furi_get_tick();
        FuriString* csv = furi_string_alloc();
        for(uint8_t i = 0; i < n; i++) {
            uint32_t freq = sweep->freq_start + (uint32_t)i * sweep->freq_step;
            furi_string_cat_printf(
                csv,
                "%lu,%lu,%ld\n",
                (unsigned long)ts,
                (unsigned long)freq,
                (long)(int32_t)sweep->rssi[i]);
        }
        storage_file_write(app->log_file, furi_string_get_cstr(csv), furi_string_size(csv));
        furi_string_free(csv);
    }
    furi_mutex_release(app->log_mutex);
}

/* ─── View Exit ─── */

static void spectrum_view_exit_callback(void* context) {
    SpectrumApp* app = context;
    if(spectrum_worker_is_running(app->worker)) {
        spectrum_worker_stop(app->worker);
    }
}

/* ─── Band Selection ─── */

static void spectrum_settings_callback(void* context, uint32_t index) {
    UNUSED(index);
    SpectrumApp* app = context;
    view_dispatcher_switch_to_view(app->view_dispatcher, SpectrumViewSettings);
}

static void spectrum_band_select_callback(void* context, uint32_t index) {
    SpectrumApp* app = context;
    app->current_band = (SpectrumBand)index;

    /* Stop worker if running */
    if(spectrum_worker_is_running(app->worker)) {
        spectrum_worker_stop(app->worker);
    }

    /* Reset the display model.  The worker is stopped (and joined) above, so
       there is no concurrent access to serialise against here. */
    with_view_model(
        app->spectrum_view,
        SpectrumData * data,
        {
            spectrum_data_reset(data);
            data->peak_hold = app->peak_hold;
            data->view_mode = SpectrumViewBar;
        },
        true);

    /* Start worker with new band */
    const SpectrumBandConfig* cfg = &spectrum_band_configs[app->current_band];
    spectrum_worker_start(
        app->worker, cfg->start_freq, cfg->end_freq, step_values[app->step_index]);

    view_dispatcher_switch_to_view(app->view_dispatcher, SpectrumViewSpectrum);
}

/* ─── Settings ─── */

static void spectrum_step_change_callback(VariableItem* item) {
    SpectrumApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx >= STEP_COUNT) idx = 0;
    app->step_index = idx;
    variable_item_set_current_value_text(item, step_names[idx]);
}

static void spectrum_peak_change_callback(VariableItem* item) {
    SpectrumApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->peak_hold = (idx == 1);
    variable_item_set_current_value_text(item, idx ? "On" : "Off");
    /* Apply to the live model too, so the Settings control and the Down-key
       toggle stay in sync in both directions. */
    with_view_model(
        app->spectrum_view,
        SpectrumData * data,
        {
            data->peak_hold = app->peak_hold;
            if(!data->peak_hold) {
                for(uint8_t i = 0; i < SPECTRUM_NUM_BINS; i++) data->peak_rssi[i] = -200.0f;
            }
        },
        true);
}

static void spectrum_view_mode_change_callback(VariableItem* item) {
    SpectrumApp* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    SpectrumViewMode mode = (idx == 0) ? SpectrumViewBar : SpectrumViewWaterfall;
    variable_item_set_current_value_text(item, idx ? "Waterfall" : "Bar");
    with_view_model(app->spectrum_view, SpectrumData * data, { data->view_mode = mode; }, true);
}

/* ─── Navigation ─── */

static uint32_t spectrum_navigation_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t spectrum_navigation_band_select(void* context) {
    UNUSED(context);
    return SpectrumViewBandSelect;
}

static bool spectrum_back_event_callback(void* context) {
    SpectrumApp* app = context;
    /* Stop worker before exiting */
    if(spectrum_worker_is_running(app->worker)) {
        spectrum_worker_stop(app->worker);
    }
    if(app->logging) {
        storage_file_close(app->log_file);
        app->logging = false;
    }
    return false; /* Let view_dispatcher handle the back navigation */
}

/* ─── App Lifecycle ─── */

static SpectrumApp* spectrum_app_alloc(void) {
    SpectrumApp* app = malloc(sizeof(SpectrumApp));
    furi_assert(app);
    memset(app, 0, sizeof(SpectrumApp));

    app->log_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    /* GUI */
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->log_file = storage_file_alloc(app->storage);

    /* View Dispatcher */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, spectrum_back_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* Spectrum View (custom) */
    app->spectrum_view = view_alloc();
    view_allocate_model(app->spectrum_view, ViewModelTypeLocking, sizeof(SpectrumData));
    /* Initialise the model (peak_rssi floor etc.) — this is now the single
       source of truth for all spectrum state. */
    with_view_model(
        app->spectrum_view, SpectrumData * data, { spectrum_data_reset(data); }, false);
    view_set_draw_callback(app->spectrum_view, spectrum_view_draw_callback);
    view_set_input_callback(app->spectrum_view, spectrum_view_input_callback);
    view_set_context(app->spectrum_view, app);
    view_set_exit_callback(app->spectrum_view, spectrum_view_exit_callback);
    view_set_previous_callback(app->spectrum_view, spectrum_navigation_band_select);
    view_dispatcher_add_view(app->view_dispatcher, SpectrumViewSpectrum, app->spectrum_view);

    /* Band Select Submenu */
    app->band_submenu = submenu_alloc();
    submenu_set_header(app->band_submenu, "Select Band");
    submenu_add_item(
        app->band_submenu,
        "315 MHz (310-320)",
        SpectrumBand315,
        spectrum_band_select_callback,
        app);
    submenu_add_item(
        app->band_submenu,
        "433 MHz (425-445)",
        SpectrumBand433,
        spectrum_band_select_callback,
        app);
    submenu_add_item(
        app->band_submenu,
        "868 MHz (860-880)",
        SpectrumBand868,
        spectrum_band_select_callback,
        app);
    submenu_add_item(
        app->band_submenu,
        "915 MHz (900-930)",
        SpectrumBand915,
        spectrum_band_select_callback,
        app);
    submenu_add_item(app->band_submenu, "[Settings]", 0xFFFF, spectrum_settings_callback, app);
    view_set_previous_callback(submenu_get_view(app->band_submenu), spectrum_navigation_exit);
    view_dispatcher_add_view(
        app->view_dispatcher, SpectrumViewBandSelect, submenu_get_view(app->band_submenu));

    /* Settings */
    app->settings_list = variable_item_list_alloc();
    VariableItem* item;
    item = variable_item_list_add(
        app->settings_list, "Step Size", STEP_COUNT, spectrum_step_change_callback, app);
    app->step_index = 3; /* Default: 100 kHz */
    variable_item_set_current_value_index(item, app->step_index);
    variable_item_set_current_value_text(item, step_names[app->step_index]);

    item = variable_item_list_add(
        app->settings_list, "Peak Hold", 2, spectrum_peak_change_callback, app);
    app->peak_item = item; /* kept in sync with the live Down-key toggle */
    variable_item_set_current_value_index(item, 0);
    variable_item_set_current_value_text(item, "Off");

    item = variable_item_list_add(
        app->settings_list, "View Mode", 2, spectrum_view_mode_change_callback, app);
    variable_item_set_current_value_index(item, 0); /* Default: Bar */
    variable_item_set_current_value_text(item, "Bar");

    view_set_previous_callback(
        variable_item_list_get_view(app->settings_list), spectrum_navigation_band_select);
    view_dispatcher_add_view(
        app->view_dispatcher,
        SpectrumViewSettings,
        variable_item_list_get_view(app->settings_list));

    /* Worker */
    app->worker = spectrum_worker_alloc();
    spectrum_worker_set_callback(app->worker, spectrum_worker_callback, app);

    /* Defaults */
    app->current_band = SpectrumBand433;
    app->peak_hold = false;
    app->logging = false;

    return app;
}

static void spectrum_app_free(SpectrumApp* app) {
    /* Stop worker */
    if(spectrum_worker_is_running(app->worker)) {
        spectrum_worker_stop(app->worker);
    }
    spectrum_worker_free(app->worker);

    /* Close log */
    if(app->logging) {
        storage_file_close(app->log_file);
    }
    storage_file_free(app->log_file);

    /* Views */
    view_dispatcher_remove_all(app->view_dispatcher, SpectrumViewCount);

    view_free(app->spectrum_view);
    submenu_free(app->band_submenu);
    variable_item_list_free(app->settings_list);
    view_dispatcher_free(app->view_dispatcher);

    furi_mutex_free(app->log_mutex);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);

    free(app);
}

/* ─── Entry Point ─── */

int32_t subghz_spectrum_app(void* p) {
    UNUSED(p);

    SpectrumApp* app = spectrum_app_alloc();

    /* Start on band selection */
    view_dispatcher_switch_to_view(app->view_dispatcher, SpectrumViewBandSelect);
    view_dispatcher_run(app->view_dispatcher);

    spectrum_app_free(app);
    return 0;
}
