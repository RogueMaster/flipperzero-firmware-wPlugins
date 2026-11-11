#include "hamcram.h"

#include <furi_hal_rtc.h>
#include <notification/notification_messages.h>
#include <stdio.h>
#include <string.h>

#define HAMCRAM_SETTINGS_PATH  APP_DATA_PATH("settings.bin")
#define HAMCRAM_SETTINGS_MAGIC 0x32434843 /* "CHC2" */

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t skip_figures;
    uint8_t shuffle;
    uint8_t feedback;
} HamCramSettingsFile;

/* ------------------------------------------------------------- settings */

static void hamcram_load_settings(HamCram* app) {
    app->settings.skip_figures = true;
    app->settings.shuffle = true;
    app->settings.feedback = true;

    HamCramSettingsFile data;
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, HAMCRAM_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING) &&
       storage_file_read(file, &data, sizeof(data)) == sizeof(data) &&
       data.magic == HAMCRAM_SETTINGS_MAGIC) {
        app->settings.skip_figures = data.skip_figures != 0;
        app->settings.shuffle = data.shuffle != 0;
        app->settings.feedback = data.feedback != 0;
    }
    storage_file_close(file);
    storage_file_free(file);
}

void hamcram_save_settings(HamCram* app) {
    HamCramSettingsFile data = {
        .magic = HAMCRAM_SETTINGS_MAGIC,
        .skip_figures = app->settings.skip_figures,
        .shuffle = app->settings.shuffle,
        .feedback = app->settings.feedback,
    };
    File* file = storage_file_alloc(app->storage);
    if(storage_file_open(file, HAMCRAM_SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(file, &data, sizeof(data));
    }
    storage_file_close(file);
    storage_file_free(file);
}

/* ---------------------------------------------------------------- pools */

static bool hamcram_pool_before(const HamCramPoolEntry* a, const HamCramPoolEntry* b) {
    if(a->element != b->element) return a->element < b->element;
    return a->valid_from > b->valid_from; /* newest edition first */
}

/* Every .hcp file in the assets folder is a pool. Shipping a new edition is
 * a matter of adding a file; nothing here knows about specific pools. */
static void hamcram_scan_pools(HamCram* app) {
    app->pool_count = 0;
    File* dir = storage_file_alloc(app->storage);
    if(storage_dir_open(dir, HAMCRAM_POOL_DIR)) {
        FileInfo info;
        char name[sizeof(app->pools[0].file)];
        char path[HAMCRAM_PATH_MAX];
        while(app->pool_count < HAMCRAM_MAX_POOLS &&
              storage_dir_read(dir, &info, name, sizeof(name))) {
            size_t length = strlen(name);
            size_t ext = strlen(HAMCRAM_POOL_EXT);
            if(file_info_is_dir(&info) || length <= ext) continue;
            if(strcmp(name + length - ext, HAMCRAM_POOL_EXT) != 0) continue;

            PoolHeader header;
            snprintf(path, sizeof(path), "%s/%s", HAMCRAM_POOL_DIR, name);
            if(!pool_read_header(app->storage, path, &header)) continue;

            HamCramPoolEntry entry;
            memset(&entry, 0, sizeof(entry));
            strlcpy(entry.file, name, sizeof(entry.file));
            strlcpy(entry.name, header.name, sizeof(entry.name));
            strlcpy(entry.years, header.years, sizeof(entry.years));
            entry.element = header.element;
            entry.valid_from = header.valid_from;
            entry.valid_to = header.valid_to;

            /* Insertion sort: lowest license class first */
            uint8_t at = app->pool_count++;
            while(at > 0 && hamcram_pool_before(&entry, &app->pools[at - 1])) {
                app->pools[at] = app->pools[at - 1];
                at--;
            }
            app->pools[at] = entry;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);
}

bool hamcram_pool_expired(const HamCram* app, const HamCramPoolEntry* entry) {
    return app->today > entry->valid_to;
}

void hamcram_save_progress(HamCram* app) {
    if(app->pool && app->progress && app->progress->dirty) {
        progress_save(app->progress, app->storage, app->save_path, app->pool);
    }
}

void hamcram_close_pool(HamCram* app) {
    hamcram_save_progress(app);
    session_free(app->session);
    progress_free(app->progress);
    pool_close(app->pool);
    app->session = NULL;
    app->progress = NULL;
    app->pool = NULL;
    app->pool_entry = NULL;
}

bool hamcram_open_pool(HamCram* app, uint8_t index) {
    furi_check(index < app->pool_count);
    hamcram_close_pool(app);

    const HamCramPoolEntry* entry = &app->pools[index];
    char path[HAMCRAM_PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s", HAMCRAM_POOL_DIR, entry->file);
    app->pool = pool_open(app->storage, path);
    if(!app->pool) return false;

    /* technician.hcp keeps its progress in technician.sav */
    size_t stem = strlen(entry->file) - strlen(HAMCRAM_POOL_EXT);
    snprintf(
        app->save_path, sizeof(app->save_path), APP_DATA_PATH("%.*s.sav"), (int)stem, entry->file);

    app->pool_entry = entry;
    app->progress = progress_alloc(app->pool->header.num_questions);
    progress_load(app->progress, app->storage, app->save_path, app->pool);
    app->session = session_alloc(app->pool->header.num_questions);
    return true;
}

void hamcram_show_message(HamCram* app, const char* header, const char* text) {
    app->message_header = header;
    app->message_text = text;
    scene_manager_next_scene(app->scene_manager, HamCramSceneMessage);
}

void hamcram_cat_wrapped(FuriString* out, const char* text) {
    size_t column = 0;
    while(*text) {
        size_t word = strcspn(text, " \n");
        if(column > 0 && column + 1 + word > HAMCRAM_WRAP_CHARS) {
            furi_string_push_back(out, '\n');
            column = 0;
        } else if(column > 0 && word > 0) {
            furi_string_push_back(out, ' ');
            column++;
        }
        for(size_t i = 0; i < word; i++)
            furi_string_push_back(out, text[i]);
        column += word;
        text += word;
        if(*text == '\n') {
            furi_string_push_back(out, '\n');
            column = 0;
        }
        if(*text) text++;
    }
}

/* Silent on purpose: people study in quiet places. The vibration still
 * follows the Flipper's own vibro setting. */
static const NotificationSequence hamcram_sequence_wrong = {
    &message_vibro_on,
    &message_red_255,
    &message_delay_100,
    &message_vibro_off,
    &message_delay_100,
    &message_red_0,
    NULL,
};

/* Passing an exam earns two green blinks and a buzz */
static const NotificationSequence hamcram_sequence_passed = {
    &message_vibro_on,
    &message_green_255,
    &message_delay_100,
    &message_vibro_off,
    &message_green_0,
    &message_delay_100,
    &message_green_255,
    &message_delay_250,
    &message_green_0,
    NULL,
};

void hamcram_feedback(HamCram* app, bool correct) {
    if(!app->settings.feedback) return;
    notification_message(
        app->notifications, correct ? &sequence_blink_green_100 : &hamcram_sequence_wrong);
}

void hamcram_feedback_exam(HamCram* app, bool passed) {
    if(!app->settings.feedback) return;
    if(passed) {
        notification_message(app->notifications, &hamcram_sequence_passed);
    } else {
        notification_message(app->notifications, &hamcram_sequence_wrong);
    }
}

void hamcram_submenu_callback(void* context, uint32_t index) {
    HamCram* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void hamcram_start_study(HamCram* app, uint16_t first, uint16_t count) {
    if(session_start_study(
           app->session,
           app->pool,
           app->progress,
           first,
           count,
           app->settings.skip_figures,
           app->settings.shuffle)) {
        scene_manager_next_scene(app->scene_manager, HamCramSceneQuiz);
    } else {
        hamcram_show_message(
            app, "Nothing to study", "Every question here needs\na figure. See Settings.");
    }
}

/* ------------------------------------------------------------ app shell */

static bool hamcram_custom_event_callback(void* context, uint32_t event) {
    HamCram* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool hamcram_back_event_callback(void* context) {
    HamCram* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static HamCram* hamcram_alloc(void) {
    HamCram* app = malloc(sizeof(HamCram));
    memset(app, 0, sizeof(HamCram));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->text = furi_string_alloc();

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&hamcram_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, hamcram_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, hamcram_back_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HamCramViewSubmenu, submenu_get_view(app->submenu));
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HamCramViewWidget, widget_get_view(app->widget));
    app->dialog = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HamCramViewDialog, dialog_ex_get_view(app->dialog));
    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        HamCramViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));
    app->quiz_view = quiz_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HamCramViewQuiz, quiz_view_get_view(app->quiz_view));

    DateTime now;
    furi_hal_rtc_get_datetime(&now);
    app->today = (uint32_t)now.year * 10000 + now.month * 100 + now.day;

    hamcram_load_settings(app);
    hamcram_scan_pools(app);
    return app;
}

static void hamcram_free(HamCram* app) {
    hamcram_close_pool(app);

    view_dispatcher_remove_view(app->view_dispatcher, HamCramViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, HamCramViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, HamCramViewDialog);
    view_dispatcher_remove_view(app->view_dispatcher, HamCramViewVariableItemList);
    view_dispatcher_remove_view(app->view_dispatcher, HamCramViewQuiz);
    submenu_free(app->submenu);
    widget_free(app->widget);
    dialog_ex_free(app->dialog);
    variable_item_list_free(app->variable_item_list);
    quiz_view_free(app->quiz_view);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);
    furi_string_free(app->text);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t hamcram_app(void* p) {
    UNUSED(p);
    HamCram* app = hamcram_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, HamCramSceneStart);
    view_dispatcher_run(app->view_dispatcher);

    hamcram_free(app);
    return 0;
}
