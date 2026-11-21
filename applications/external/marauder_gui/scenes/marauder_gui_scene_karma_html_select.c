#include "../marauder_gui_app_i.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <storage/storage.h>

#define KARMA_HTML_DIR          MARAUDER_GUI_DATA_DIR "/html"
#define KARMA_HTML_MAX_FILES    32
#define KARMA_HTML_FILENAME_MAX 128

enum {
    KarmaHtmlSelectEventUseDefault,
};

static char karma_html_files[KARMA_HTML_MAX_FILES][KARMA_HTML_FILENAME_MAX];
static size_t karma_html_file_count = 0;

static void marauder_gui_scene_karma_html_load_files(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);

    karma_html_file_count = 0;

    if(storage_dir_open(dir, KARMA_HTML_DIR)) {
        FileInfo file_info;
        char filename[KARMA_HTML_FILENAME_MAX];

        while(storage_dir_read(dir, &file_info, filename, sizeof(filename))) {
            if(!(file_info.flags & FSF_DIRECTORY)) {
                if(strstr(filename, ".html") || strstr(filename, ".htm")) {
                    if(karma_html_file_count < KARMA_HTML_MAX_FILES) {
                        strncpy(
                            karma_html_files[karma_html_file_count],
                            filename,
                            KARMA_HTML_FILENAME_MAX - 1);
                        karma_html_files[karma_html_file_count][KARMA_HTML_FILENAME_MAX - 1] =
                            '\0';
                        karma_html_file_count++;
                    }
                }
            }
        }
        storage_dir_close(dir);
    }

    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);
}

size_t marauder_gui_karma_html_file_count(void) {
    return karma_html_file_count;
}

const char* marauder_gui_karma_html_filename(size_t index) {
    return (index < karma_html_file_count) ? karma_html_files[index] : NULL;
}

void marauder_gui_scene_karma_html_select_on_enter(void* context) {
    MarauderGuiApp* app = context;

    marauder_gui_scene_karma_html_load_files();

    static MarauderMenuItem menu_items[KARMA_HTML_MAX_FILES + 1];
    for(size_t i = 0; i < karma_html_file_count; i++) {
        menu_items[i].label_tr = karma_html_files[i];
        menu_items[i].label_en = karma_html_files[i];
        menu_items[i].description_tr = "";
        menu_items[i].description_en = "";
    }

    menu_items[karma_html_file_count].label_tr = "Varsayilan (Ornek)";
    menu_items[karma_html_file_count].label_en = "Default (Tester)";
    menu_items[karma_html_file_count].description_tr = "";
    menu_items[karma_html_file_count].description_en = "";

    marauder_gui_menu_set_items(app, menu_items, karma_html_file_count + 1, "HTML Secin");
}

bool marauder_gui_scene_karma_html_select_on_event(void* context, SceneManagerEvent event) {
    MarauderGuiApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == (uint32_t)karma_html_file_count) {
            app->karma_use_default_html = true;
        } else if(event.event < karma_html_file_count) {
            app->karma_use_default_html = false;
            strncpy(
                app->karma_custom_html_file,
                karma_html_files[event.event],
                sizeof(app->karma_custom_html_file) - 1);
            app->karma_custom_html_file[sizeof(app->karma_custom_html_file) - 1] = '\0';
        }

        scene_manager_next_scene(app->scene_manager, MarauderGuiSceneKarmaStatus);
        consumed = true;
    }

    return consumed;
}

void marauder_gui_scene_karma_html_select_on_exit(void* context) {
    UNUSED(context);
}
