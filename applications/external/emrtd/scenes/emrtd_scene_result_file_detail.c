/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * One elementary file in detail, with the first bytes as they came off the
 * chip.
 *
 * The bytes are read back from the export rather than kept in memory: DG2
 * alone is tens of kilobytes against a 186 KB heap, and a preview
 * is not worth holding a file for.
 */
#include "../emrtd_i.h"

/* Twenty monospaced characters is what fits across the screen, which is six
 * bytes and a two digit offset. */
#define EMRTD_PREVIEW_BYTES    120
#define EMRTD_PREVIEW_PER_LINE 6

static const char* emrtd_scene_result_file_detail_state(EmrtdFileState state) {
    switch(state) {
    case EmrtdFileStateRead:
        return "Read";
    case EmrtdFileStateFailed:
        return "Failed";
    case EmrtdFileStateSkipped:
        return "Skipped";
    case EmrtdFileStatePending:
    case EmrtdFileStateReading:
        return "Not reached";
    default:
        return "Not announced by EF.COM";
    }
}

static const char* emrtd_scene_result_file_detail_hash(EmrtdHashState state) {
    switch(state) {
    case EmrtdHashStateMatch:
        return "Matches the hash in EF.SOD";
    case EmrtdHashStateMismatch:
        return "DOES NOT match EF.SOD";
    case EmrtdHashStateNotListed:
        return "EF.SOD does not list it";
    case EmrtdHashStateUnsupportedDigest:
        return "The digest is not supported";
    default:
        return "Not checked";
    }
}

/**
 * Where the export put this file.
 *
 * The export names files after the standard with the dot replaced, so EF.DG1
 * is written as EF_DG1.bin.
 */
static void emrtd_scene_result_file_detail_path(
    const Emrtd* app,
    FuriString* path,
    const EmrtdFileInfo* info) {
    char name[24];
    strlcpy(name, info->name, sizeof(name));
    for(char* c = name; *c != '\0'; c++) {
        if(*c == '.') {
            *c = '_';
        }
    }

    /* The precision bounds the read even if the worker left the path
     * unterminated. */
    furi_string_printf(
        path, "%.*s/%s.bin", (int)sizeof(app->result.export_path), app->result.export_path, name);
}

/** Append the first bytes of the exported file, or say why there are none. */
static void emrtd_scene_result_file_detail_preview(
    Emrtd* app,
    FuriString* body,
    const EmrtdFileInfo* info) {
    if(app->result.export_path[0] == '\0') {
        furi_string_cat_str(
            body, "\e#Bytes\nNothing was written to the\nSD card, so there is no\npreview.\n");
        return;
    }

    FuriString* path = furi_string_alloc();
    emrtd_scene_result_file_detail_path(app, path, info);

    File* file = storage_file_alloc(app->storage);
    if(!storage_file_open(file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        furi_string_cat_printf(
            body, "\e#Bytes\n%s\nis not on the card.\n", furi_string_get_cstr(path));
    } else {
        uint8_t buffer[EMRTD_PREVIEW_BYTES];
        const size_t read = storage_file_read(file, buffer, sizeof(buffer));

        furi_string_cat_printf(body, "\e#First %u bytes\n", (unsigned)read);
        for(size_t offset = 0; offset < read; offset += EMRTD_PREVIEW_PER_LINE) {
            furi_string_cat_printf(body, "\e*%02X ", (unsigned)offset);
            for(size_t i = 0; i < EMRTD_PREVIEW_PER_LINE && offset + i < read; i++) {
                furi_string_cat_printf(body, "%02X ", buffer[offset + i]);
            }
            furi_string_cat_str(body, "\n");
        }

        memset(buffer, 0, sizeof(buffer));
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_string_free(path);
}

void emrtd_scene_result_file_detail_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    Widget* widget = app->widget;

    const EmrtdFileInfo* info = emrtd_file_info(app->selected_file);
    const EmrtdFileResult* file = &app->result.files[app->selected_file];

    FuriString* body = app->text_box_store;
    furi_string_reset(body);

    if(info == NULL) {
        furi_string_cat_str(body, "This file is not in the\nreader's table.\n");
    } else {
        furi_string_cat_printf(body, "\e#%s\n%s\n", info->name, info->description);
        furi_string_cat_printf(
            body, "\e#State\n%s\n", emrtd_scene_result_file_detail_state(file->state));

        if(file->state == EmrtdFileStateRead) {
            furi_string_cat_printf(body, "\e#Size\n%u bytes\n", (unsigned)file->size);
            furi_string_cat_printf(
                body, "\e#Hash\n%s\n", emrtd_scene_result_file_detail_hash(file->hash_state));
            emrtd_scene_result_file_detail_preview(app, body, info);
        } else if(file->state == EmrtdFileStateFailed && file->error != EmrtdErrorNone) {
            furi_string_cat_printf(body, "\e#Why\n%s\n", emrtd_error_text(file->error));
        } else if(file->state == EmrtdFileStateSkipped && info->eac_protected) {
            furi_string_cat_str(
                body,
                "\e#Why\nExtended Access Control\nprotects it. Only a terminal\n"
                "with a certificate from the\nissuing state can read it.\n");
        }
    }

    widget_add_string_element(
        widget, 64, 1, AlignCenter, AlignTop, FontPrimary, info != NULL ? info->label : "File");
    widget_add_text_scroll_element(widget, 0, 13, 128, 51, furi_string_get_cstr(body));

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewWidget);
}

bool emrtd_scene_result_file_detail_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);

    return false;
}

void emrtd_scene_result_file_detail_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    widget_reset(app->widget);
    furi_string_reset(app->text_box_store);
}
