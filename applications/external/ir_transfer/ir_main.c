#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/elements.h>
#include <gui/modules/widget.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/variable_item_list.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <dialogs/dialogs.h>
#include <ir_transfer_icons.h>
#include <stdlib.h>
#include <infrared.h>
#include "infrared/infrared_signal.h"
#include <infrared_worker.h>
#include "views/landing_view.h"
#include "views/receiving_view.h"
#include "views/sending_view.h"
#include "infrared/infrared_transfer.h"
#include "ir_main.h"

#define BACKLIGHT_ON          1
#define TEXT_VIEWER_EXTENSION "*"
#define INIT_ADDR             0x10
#define NAME_ADDR             0xf0
#define REC_ADDR              0x1
#define CHECKSUM_ADDR         0x30
#define CHUNK_SIZE            1
#define MAX_FILENAME_SIZE     32
#define MAX_BLOCK_SIZE        32
#define FILE_PATH_RECV        "/ext/tmpfile"
#define FILE_PATH_FOLDER      "/ext/downloads"
#define TAG                   "IR Transfer"
#define THREAD_SIGNAL_DONE    0x01
#define TIMEOUT_MS            5000

App* app_alloc() {
    App* app = (App*)malloc(sizeof(App));
    Gui* gui = furi_record_open(RECORD_GUI);

    app->send_path = furi_string_alloc();

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage_dir_exists(storage, FILE_PATH_FOLDER)) {
        if(storage_simply_mkdir(storage, FILE_PATH_FOLDER)) {
            FURI_LOG_I("ALLOC", "Download_folder OK");
        } else {
            FURI_LOG_E("ALLOC", "Download_folder NOT OK");
        }
    }
    furi_record_close(RECORD_STORAGE);

    furi_string_set(app->send_path, "none");

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);

    app->temp_buffer_size = 32;
    app->temp_buffer = (char*)malloc(app->temp_buffer_size);

    app->landing_view = landing_view_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, LandingView, app->landing_view);

    app->rec_view = receiving_view_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, RecView, app->rec_view);

    app->send_view = sending_view_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, SendView, app->send_view);

    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    view_dispatcher_switch_to_view(app->view_dispatcher, LandingView);

    return app;
}

void app_free(App* app) {
#ifdef BACKLIGHT_ON
    notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
#endif
    furi_record_close(RECORD_NOTIFICATION);
    furi_string_free(app->send_path);
    free(app->temp_buffer);
    view_dispatcher_remove_view(app->view_dispatcher, LandingView);
    landing_view_free(app->landing_view);
    view_dispatcher_remove_view(app->view_dispatcher, RecView);
    receiving_view_free(app->rec_view);
    view_dispatcher_remove_view(app->view_dispatcher, SendView);
    sending_view_free(app->send_view);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t main_app(void* _p) {
    UNUSED(_p);
    App* app = app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    app_free(app);
    return 0;
}
