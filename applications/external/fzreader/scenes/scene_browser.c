/**
 * @file scene_browser.c
 * @brief System file browser and message dialogs used by the reader.
 */

#include "scene_browser.h"

#include <dialogs/dialogs.h>
#include <storage/storage.h>

#include <string.h>

#include "../formats/reader_book.h"

void scene_browser_folder(FuriString* folder, const char* book) {
    furi_string_set_str(folder, STORAGE_EXT_PATH_PREFIX);
    if(book == NULL || book[0] == '\0') {
        return;
    }

    /* The folder of the book: everything before the last separator of its path. */
    const char* separator = strrchr(book, '/');
    if(separator != NULL && separator != book) {
        furi_string_set_strn(folder, book, (size_t)(separator - book));
    }
}

bool scene_browser_pick(FuriString* path, const FuriString* folder) {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);

    DialogsFileBrowserOptions options;
    /* The browser takes a single mask: the first extension of the registry. */
    dialog_file_browser_set_basic_options(&options, reader_book_extension_mask(), NULL);

    FuriString* start = furi_string_alloc_set(folder);
    bool selected = dialog_file_browser_show(dialogs, path, start, &options);
    furi_string_free(start);

    furi_record_close(RECORD_DIALOGS);
    return selected;
}

void scene_browser_show_error(const char* header, const char* text) {
    DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);

    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, header, 64, 0, AlignCenter, AlignTop);
    dialog_message_set_text(message, text, 64, 32, AlignCenter, AlignCenter);
    dialog_message_set_buttons(message, NULL, "OK", NULL);
    dialog_message_show(dialogs, message);
    dialog_message_free(message);

    furi_record_close(RECORD_DIALOGS);
}
