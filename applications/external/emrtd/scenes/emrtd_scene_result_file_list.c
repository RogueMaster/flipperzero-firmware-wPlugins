/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Filip Sedivy
 *
 * Every elementary file and what became of it.
 *
 * The files the chip never announced are listed too. "DG3 absent" and "DG3
 * failed" mean entirely different things and the screen that cannot tell them
 * apart is of no use to anybody.
 */
#include "../emrtd_i.h"

/* The submenu copies each label, so one buffer serves the whole list. */
#define EMRTD_FILE_LIST_LABEL_MAX 32

static void emrtd_scene_result_file_list_callback(void* context, uint32_t index) {
    furi_assert(context);
    Emrtd* app = context;

    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

/** The outcome in one word, with the size where there is one. */
static void
    emrtd_scene_result_file_list_status(char* out, size_t out_size, const EmrtdFileResult* file) {
    switch(file->state) {
    case EmrtdFileStateRead:
        if(file->size >= 1024) {
            snprintf(
                out,
                out_size,
                "%u.%u kB",
                (unsigned)(file->size / 1024),
                (unsigned)(((file->size % 1024) * 10) / 1024));
        } else {
            snprintf(out, out_size, "%u B", (unsigned)file->size);
        }
        break;
    case EmrtdFileStateFailed:
        strlcpy(out, "failed", out_size);
        break;
    case EmrtdFileStateSkipped:
        strlcpy(out, "skipped", out_size);
        break;
    case EmrtdFileStatePending:
    case EmrtdFileStateReading:
        strlcpy(out, "not reached", out_size);
        break;
    default:
        strlcpy(out, "absent", out_size);
        break;
    }
}

void emrtd_scene_result_file_list_on_enter(void* context) {
    furi_assert(context);
    Emrtd* app = context;
    Submenu* submenu = app->submenu;

    submenu_set_header(submenu, "Files");

    for(size_t id = 0; id < EmrtdFileCount; id++) {
        const EmrtdFileInfo* info = emrtd_file_info((EmrtdFileId)id);
        if(info == NULL) {
            continue;
        }

        const EmrtdFileResult* file = &app->result.files[id];
        char status[16];
        emrtd_scene_result_file_list_status(status, sizeof(status), file);

        const char* short_name = info->name;
        if(strncmp(short_name, "EF.", 3) == 0) {
            short_name += 3;
        }

        char label[EMRTD_FILE_LIST_LABEL_MAX];
        snprintf(
            label,
            sizeof(label),
            "%s %s%s",
            short_name,
            status,
            file->hash_state == EmrtdHashStateMismatch ? " !" : "");

        submenu_add_item(submenu, label, id, emrtd_scene_result_file_list_callback, app);
    }

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, EmrtdSceneResultFileList));

    view_dispatcher_switch_to_view(app->view_dispatcher, EmrtdViewSubmenu);
}

bool emrtd_scene_result_file_list_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    Emrtd* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom && event.event < EmrtdFileCount) {
        scene_manager_set_scene_state(app->scene_manager, EmrtdSceneResultFileList, event.event);
        app->selected_file = (EmrtdFileId)event.event;
        scene_manager_next_scene(app->scene_manager, EmrtdSceneResultFileDetail);
        consumed = true;
    }

    return consumed;
}

void emrtd_scene_result_file_list_on_exit(void* context) {
    furi_assert(context);
    Emrtd* app = context;

    submenu_reset(app->submenu);
}
