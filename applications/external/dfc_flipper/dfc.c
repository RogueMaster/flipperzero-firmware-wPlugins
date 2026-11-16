#include "dfc_i.h"
#include <furi/core/memmgr.h>
#include <storage/storage.h>

#define TAG                         "Dfc"
#define DFC_HARDWARE_TEST_ARGUMENT  "--hardware-test "
#define DFC_HARDWARE_TEST_DIRECTORY "/ext/apps_data/dfc/.hardware-test/"
#define DFC_HARDWARE_PROFILE_PATH   DFC_HARDWARE_TEST_DIRECTORY "profile.csv"

static void dfc_hardware_profile_write(const Dfc* dfc) {
    if(!dfc->hardware_test_mode) return;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, DFC_HARDWARE_PROFILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char row[512];
        int length = snprintf(
            row,
            sizeof(row),
            "cycles_per_us,read_prepare_count,read_prepare_total,read_prepare_max,"
            "read_send_count,read_send_total,read_send_max,continuation_prepare_count,"
            "continuation_prepare_total,continuation_prepare_max,continuation_send_count,"
            "continuation_send_total,continuation_send_max\n"
            "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
            (unsigned long)furi_hal_cortex_instructions_per_microsecond(),
            (unsigned long)dfc->hardware_read_prepare.count,
            (unsigned long)dfc->hardware_read_prepare.total_cycles,
            (unsigned long)dfc->hardware_read_prepare.max_cycles,
            (unsigned long)dfc->hardware_read_send.count,
            (unsigned long)dfc->hardware_read_send.total_cycles,
            (unsigned long)dfc->hardware_read_send.max_cycles,
            (unsigned long)dfc->hardware_continuation_prepare.count,
            (unsigned long)dfc->hardware_continuation_prepare.total_cycles,
            (unsigned long)dfc->hardware_continuation_prepare.max_cycles,
            (unsigned long)dfc->hardware_continuation_send.count,
            (unsigned long)dfc->hardware_continuation_send.total_cycles,
            (unsigned long)dfc->hardware_continuation_send.max_cycles);
        if(length > 0 && (size_t)length < sizeof(row)) {
            storage_file_write(file, row, (size_t)length);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

void dfc_log_memory(const char* stage) {
    size_t free_heap = memmgr_get_free_heap();
    size_t minimum_heap = memmgr_get_minimum_free_heap();
    size_t largest_block = memmgr_heap_get_max_free_block();
    FURI_LOG_I(
        "DfcMemory",
        "%s free=%u minimum=%u largest=%u",
        stage,
        (unsigned)free_heap,
        (unsigned)minimum_heap,
        (unsigned)largest_block);
#ifdef DFC_MEMORY_PROBE
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, APP_DATA_PATH("memory.csv"), FSAM_WRITE, FSOM_OPEN_APPEND)) {
        char line[96];
        int length = snprintf(
            line,
            sizeof(line),
            "%lu,%s,%u,%u,%u\n",
            (unsigned long)furi_get_tick(),
            stage,
            (unsigned)free_heap,
            (unsigned)minimum_heap,
            (unsigned)largest_block);
        if(length > 0 && (size_t)length < sizeof(line)) {
            storage_file_write(file, line, (size_t)length);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
#endif
}

_Static_assert(
    DFC_BUILD_PROFILE == DFC_PROFILE_FULL_EV3,
    "The Flipper app must be built with the full EV3 profile");

bool dfc_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    Dfc* dfc = context;
    return scene_manager_handle_custom_event(dfc->scene_manager, event);
}

bool dfc_back_event_callback(void* context) {
    furi_assert(context);
    Dfc* dfc = context;
    return scene_manager_handle_back_event(dfc->scene_manager);
}

void dfc_tick_event_callback(void* context) {
    furi_assert(context);
    Dfc* dfc = context;
    scene_manager_handle_tick_event(dfc->scene_manager);
}

Dfc* dfc_alloc() {
    Dfc* dfc = malloc(sizeof(Dfc));
    memset(dfc, 0, sizeof(Dfc));

    dfc->view_dispatcher = view_dispatcher_alloc();
    dfc->scene_manager = scene_manager_alloc(&dfc_scene_handlers, dfc);
    view_dispatcher_set_event_callback_context(dfc->view_dispatcher, dfc);
    view_dispatcher_set_custom_event_callback(dfc->view_dispatcher, dfc_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(dfc->view_dispatcher, dfc_back_event_callback);
    view_dispatcher_set_tick_event_callback(dfc->view_dispatcher, dfc_tick_event_callback, 100);

    dfc->nfc = nfc_alloc();

    // Nfc device
    dfc->nfc_device = nfc_device_alloc();
    nfc_device_set_loading_callback(dfc->nfc_device, dfc_show_loading_popup, dfc);

    // Open GUI record
    dfc->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(dfc->view_dispatcher, dfc->gui, ViewDispatcherTypeFullscreen);

    // Open Notification record
    dfc->notifications = furi_record_open(RECORD_NOTIFICATION);

    // Submenu
    dfc->submenu = submenu_alloc();
    view_dispatcher_add_view(dfc->view_dispatcher, DfcViewMenu, submenu_get_view(dfc->submenu));

    // Popup
    dfc->popup = popup_alloc();
    view_dispatcher_add_view(dfc->view_dispatcher, DfcViewPopup, popup_get_view(dfc->popup));

    // Loading
    dfc->loading = loading_alloc();
    view_dispatcher_add_view(dfc->view_dispatcher, DfcViewLoading, loading_get_view(dfc->loading));

    // Text Input
    dfc->text_input = text_input_alloc();
    view_dispatcher_add_view(
        dfc->view_dispatcher, DfcViewTextInput, text_input_get_view(dfc->text_input));

    // TextBox
    dfc->text_box = text_box_alloc();
    view_dispatcher_add_view(
        dfc->view_dispatcher, DfcViewTextBox, text_box_get_view(dfc->text_box));
    dfc->text_box_store = furi_string_alloc();

    // Custom Widget
    dfc->widget = widget_alloc();
    view_dispatcher_add_view(dfc->view_dispatcher, DfcViewWidget, widget_get_view(dfc->widget));

    dfc_credential_storage_init();
    dfc->credential = dfc_credential_alloc();

    // File Browser
    dfc->file_browser = file_browser_alloc(dfc_credential_storage_load_path());
    view_dispatcher_add_view(
        dfc->view_dispatcher, DfcViewFileBrowser, file_browser_get_view(dfc->file_browser));

    return dfc;
}

void dfc_free(Dfc* dfc) {
    furi_assert(dfc);

    if(dfc->listener) {
        nfc_listener_stop(dfc->listener);
        nfc_listener_free(dfc->listener);
        dfc->listener = NULL;
    }

    if(dfc->poller) {
        nfc_poller_stop(dfc->poller);
        nfc_poller_free(dfc->poller);
        dfc->poller = NULL;
    }

    if(dfc->radio_tx_buffer) {
        bit_buffer_free(dfc->radio_tx_buffer);
        dfc->radio_tx_buffer = NULL;
    }

    dfc_hardware_profile_write(dfc);

    nfc_free(dfc->nfc);

    // Nfc device
    nfc_device_free(dfc->nfc_device);

    // Submenu
    view_dispatcher_remove_view(dfc->view_dispatcher, DfcViewMenu);
    submenu_free(dfc->submenu);

    // Popup
    view_dispatcher_remove_view(dfc->view_dispatcher, DfcViewPopup);
    popup_free(dfc->popup);

    // Loading
    view_dispatcher_remove_view(dfc->view_dispatcher, DfcViewLoading);
    loading_free(dfc->loading);

    // TextInput
    view_dispatcher_remove_view(dfc->view_dispatcher, DfcViewTextInput);
    text_input_free(dfc->text_input);

    // TextBox
    view_dispatcher_remove_view(dfc->view_dispatcher, DfcViewTextBox);
    text_box_free(dfc->text_box);
    furi_string_free(dfc->text_box_store);

    // Custom Widget
    view_dispatcher_remove_view(dfc->view_dispatcher, DfcViewWidget);
    widget_free(dfc->widget);

    // File Browser
    view_dispatcher_remove_view(dfc->view_dispatcher, DfcViewFileBrowser);
    file_browser_free(dfc->file_browser);

    // View Dispatcher
    view_dispatcher_free(dfc->view_dispatcher);

    // Scene Manager
    scene_manager_free(dfc->scene_manager);

    // GUI
    furi_record_close(RECORD_GUI);
    dfc->gui = NULL;

    // A loader-forced shutdown can bypass the active scene's exit handler.
    dfc_blink_stop(dfc);

    // Notifications
    furi_record_close(RECORD_NOTIFICATION);
    dfc->notifications = NULL;

    memset(dfc->credential, 0, sizeof(*dfc->credential));
    dfc_credential_free(dfc->credential);
    dfc_credential_storage_deinit();

    if(dfc->dfc_emulator) {
        dfc_emulator_free(dfc->dfc_emulator);
        dfc->dfc_emulator = NULL;
    }

    free(dfc);
}

void dfc_text_store_set(Dfc* dfc, const char* text) {
    strlcpy(dfc->text_store, text, sizeof(dfc->text_store));
}

void dfc_text_store_clear(Dfc* dfc) {
    memset(dfc->text_store, 0, sizeof(dfc->text_store));
}

static const NotificationSequence dfc_sequence_blink_start_blue = {
    &message_blink_start_10,
    &message_blink_set_color_blue,
    &message_do_not_reset,
    NULL,
};

static const NotificationSequence dfc_sequence_blink_stop = {
    &message_blink_stop,
    NULL,
};

void dfc_blink_start(Dfc* dfc) {
    notification_message(dfc->notifications, &dfc_sequence_blink_start_blue);
}

void dfc_blink_stop(Dfc* dfc) {
    notification_message(dfc->notifications, &dfc_sequence_blink_stop);
}

void dfc_show_loading_popup(void* context, bool show) {
    Dfc* dfc = context;

    if(show) {
        // Raise timer priority so that animations can play
        furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);
        view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewLoading);
    } else {
        // Restore default timer priority
        furi_timer_set_thread_priority(FuriTimerThreadPriorityNormal);
    }
}

static const char* dfc_hardware_test_path(const char* args) {
    if(!args) return NULL;
    const size_t argument_length = strlen(DFC_HARDWARE_TEST_ARGUMENT);
    if(strncmp(args, DFC_HARDWARE_TEST_ARGUMENT, argument_length) != 0) return NULL;

    const char* path = args + argument_length;
    const size_t directory_length = strlen(DFC_HARDWARE_TEST_DIRECTORY);
    const size_t path_length = strlen(path);
    const size_t extension_length = strlen(DFC_BINARY_EXTENSION);
    if(strncmp(path, DFC_HARDWARE_TEST_DIRECTORY, directory_length) != 0 ||
       path_length <= directory_length + extension_length || strstr(path, "..") ||
       strcmp(path + path_length - extension_length, DFC_BINARY_EXTENSION) != 0) {
        return NULL;
    }
    return path;
}

int32_t dfc_app(void* p) {
    const char* args = p;
    dfc_log_memory("before app alloc");
    Dfc* dfc = dfc_alloc();
    dfc_log_memory("after app alloc");

    if(args && args[0]) {
        const char* fixture_path = dfc_hardware_test_path(args);
        DfcCredentialLoadStatus status = DfcCredentialLoadStatusOpenFailed;
        if(!fixture_path || !dfc_credential_load_path(dfc->credential, fixture_path, &status)) {
            FURI_LOG_E(TAG, "hardware test fixture rejected: %u", (unsigned)status);
            dfc_free(dfc);
            return 1;
        }
        dfc->hardware_test_mode = true;
        scene_manager_next_scene(dfc->scene_manager, DfcSceneEmulate);
    } else {
        scene_manager_next_scene(dfc->scene_manager, DfcSceneMainMenu);
    }

    view_dispatcher_run(dfc->view_dispatcher);

    dfc_free(dfc);

    return 0;
}
