#include "ui.h"
#include "settings.h"
#include "storage_helper.h"
#include "version.h"
#include <furi.h>
#include <stdio.h>
#include <string.h>

// Estimated budget for the ~121px browser header (FontPrimary); no Canvas here to measure it.
#define BROWSER_HEADER_MAX_CHARS 16

static void ui_render_groups(SubDupFinderApp *app);
static void ui_render_files_in_group(SubDupFinderApp *app, size_t group_index);
static void ui_render_browser(SubDupFinderApp *app);
static void ui_after_scan_navigate(SubDupFinderApp *app);
static void ui_build_folder_label(const SubDupFinderApp *app, char *out, size_t cap);
static void main_submenu_callback(void *context, uint32_t index);
static void groups_submenu_callback(void *context, uint32_t index);
static void files_in_group_callback(void *context, uint32_t index);
static void browser_submenu_callback(void *context, uint32_t index);
static void confirm_callback(DialogExResult result, void *context);
static void summary_callback(DialogExResult result, void *context);

typedef enum {
    BrowserIndexUse,
    BrowserIndexUp,
    BrowserIndexFolderBase,
    BrowserIndexOverflow = BrowserIndexFolderBase + BROWSE_MAX,
} BrowserIndex;

/* ── Navigation callbacks ─────────────────────────────────────────── */

static uint32_t nav_exit(void *context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t nav_back_to_submenu(void *context) {
    UNUSED(context);
    return SubDupFinderViewSubmenu;
}

static uint32_t nav_back_to_groups(void *context) {
    UNUSED(context);
    return SubDupFinderViewGroups;
}

static uint32_t nav_back_to_files(void *context) {
    UNUSED(context);
    return SubDupFinderViewFiles;
}

static uint32_t nav_back_to_browser(void *context) {
    UNUSED(context);
    return SubDupFinderViewBrowser;
}

/* ── Render helpers ───────────────────────────────────────────────── */

static void ui_render_groups(SubDupFinderApp *app) {
    submenu_reset(app->groups_submenu);
    view_set_previous_callback(submenu_get_view(app->groups_submenu), nav_back_to_submenu);

    for (size_t i = 0; i < app->db.num_groups; i++) {
        char label[64];
        snprintf(label, sizeof(label), "Dup Group %zu (%zu)", i + 1, app->db.groups[i].count);
        submenu_add_item(app->groups_submenu, label, i, groups_submenu_callback, app);
    }
}

static void ui_render_files_in_group(SubDupFinderApp *app, size_t group_index) {
    submenu_reset(app->files_in_group_submenu);
    view_set_previous_callback(submenu_get_view(app->files_in_group_submenu), nav_back_to_groups);

    const DuplicateGroup *group = &app->db.groups[group_index];
    for (size_t i = 0; i < group->count; i++) {
        submenu_add_item(app->files_in_group_submenu, app->db.records[group->start_index + i].path,
                         i, files_in_group_callback, app);
    }
}

static void ui_build_folder_label(const SubDupFinderApp *app, char *out, size_t cap) {
    char name[FULL_PATH_LEN];
    path_display_name(app->scan_dir, name, sizeof(name));
    snprintf(out, cap, "Folder: %s", name);
}

static void ui_render_browser(SubDupFinderApp *app) {
    submenu_reset(app->browser_submenu);
    view_set_previous_callback(submenu_get_view(app->browser_submenu), nav_back_to_submenu);

    char header[BROWSER_HEADER_MAX_CHARS + 1];
    path_header_tail(app->browse_path, BROWSER_HEADER_MAX_CHARS, header, sizeof(header));
    submenu_set_header(app->browser_submenu, header);

    submenu_add_item(app->browser_submenu, "[Use this folder]", BrowserIndexUse,
                     browser_submenu_callback, app);

    if (!path_is_ext_root(app->browse_path)) {
        submenu_add_item(app->browser_submenu, "..", BrowserIndexUp, browser_submenu_callback, app);
    }

    size_t overflow = 0;
    bool listed = storage_list_folders(app->browse_path, app->browse_names, BROWSE_MAX,
                                       &app->browse_count, &overflow);

    for (size_t i = 0; i < app->browse_count; i++) {
        submenu_add_item(app->browser_submenu, app->browse_names[i],
                         (uint32_t)(BrowserIndexFolderBase + i), browser_submenu_callback, app);
    }

    if (!listed) {
        submenu_add_item(app->browser_submenu, "Can't open folder", BrowserIndexOverflow,
                         browser_submenu_callback, app);
    } else if (overflow > 0) {
        char label[32];
        snprintf(label, sizeof(label), "+%zu more not shown", overflow);
        submenu_add_item(app->browser_submenu, label, BrowserIndexOverflow,
                         browser_submenu_callback, app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, SubDupFinderViewBrowser);
}

static void ui_after_scan_navigate(SubDupFinderApp *app) {
    if (app->db.num_groups == 0) {
        popup_set_text(app->popup, "No duplicates", 64, 32, AlignCenter, AlignCenter);
        view_set_previous_callback(popup_get_view(app->popup), nav_back_to_submenu);
        view_dispatcher_switch_to_view(app->view_dispatcher, SubDupFinderViewPopup);
    } else {
        view_dispatcher_switch_to_view(app->view_dispatcher, SubDupFinderViewGroups);
    }
}

// Lines join with '\n', never trailed by one: one DialogEx row per stat reported.
static void ui_build_scan_summary(const ScanStats *stats, char *buf, size_t cap) {
    const char *sep = "";
    size_t offset = 0;

    if (stats->hit_limit) {
        offset += (size_t)snprintf(buf + offset, cap - offset, "%sStopped at %d", sep, MAX_FILES);
        sep = "\n";
    }
    if (stats->unreadable > 0) {
        offset += (size_t)snprintf(buf + offset, cap - offset, "%s%zu unreadable", sep,
                                   stats->unreadable);
        sep = "\n";
    }
    if (stats->name_too_long > 0) {
        snprintf(buf + offset, cap - offset, "%s%zu names too long", sep, stats->name_too_long);
    }
}

/* ── Menu / action callbacks ──────────────────────────────────────── */

static void main_submenu_callback(void *context, uint32_t index) {
    SubDupFinderApp *app = context;
    switch (index) {
        case SubDupFinderSubmenuIndexScan: {
            ScanStats stats;

            popup_set_text(app->popup, "Scanning...", 64, 32, AlignCenter, AlignCenter);
            view_set_previous_callback(popup_get_view(app->popup), nav_back_to_submenu);
            view_dispatcher_switch_to_view(app->view_dispatcher, SubDupFinderViewPopup);

            bool opened = storage_scan_directory(app, app->scan_dir, &stats);
            if (!opened) {
                popup_set_text(app->popup, "Can't open folder", 64, 32, AlignCenter, AlignCenter);
                break;
            }

            ui_render_groups(app);

            if (stats.hit_limit || stats.unreadable > 0 || stats.name_too_long > 0) {
                char scan_summary[96];
                ui_build_scan_summary(&stats, scan_summary, sizeof(scan_summary));
                dialog_ex_set_header(app->summary_dialog, "Scan finished", 64, 4, AlignCenter,
                                     AlignTop);
                dialog_ex_set_text(app->summary_dialog, scan_summary, 64, 20, AlignCenter,
                                   AlignTop);
                view_dispatcher_switch_to_view(app->view_dispatcher, SubDupFinderViewSummary);
            } else {
                ui_after_scan_navigate(app);
            }
            break;
        }
        case SubDupFinderSubmenuIndexFolder:
            snprintf(app->browse_path, sizeof(app->browse_path), "%s", app->scan_dir);
            ui_render_browser(app);
            break;
        case SubDupFinderSubmenuIndexCredits:
            view_dispatcher_switch_to_view(app->view_dispatcher, SubDupFinderViewCredits);
            break;
    }
}

static void browser_submenu_callback(void *context, uint32_t index) {
    SubDupFinderApp *app = context;

    if (index == BrowserIndexUse) {
        snprintf(app->scan_dir, sizeof(app->scan_dir), "%s", app->browse_path);
        app->db.count = 0;
        app->db.num_groups = 0;

        char label[FULL_PATH_LEN + 16];
        ui_build_folder_label(app, label, sizeof(label));
        submenu_change_item_label(app->main_submenu, SubDupFinderSubmenuIndexFolder, label);

        if (settings_save(app->scan_dir)) {
            view_dispatcher_switch_to_view(app->view_dispatcher, SubDupFinderViewSubmenu);
        } else {
            popup_set_text(app->popup, "Couldn't save folder", 64, 32, AlignCenter, AlignCenter);
            view_set_previous_callback(popup_get_view(app->popup), nav_back_to_submenu);
            view_dispatcher_switch_to_view(app->view_dispatcher, SubDupFinderViewPopup);
        }
        return;
    }

    if (index == BrowserIndexUp) {
        char parent[FULL_PATH_LEN];
        if (path_parent(app->browse_path, parent, sizeof(parent))) {
            snprintf(app->browse_path, sizeof(app->browse_path), "%s", parent);
        }
        ui_render_browser(app);
        return;
    }

    if (index >= BrowserIndexFolderBase && index - BrowserIndexFolderBase < app->browse_count) {
        size_t i = index - BrowserIndexFolderBase;
        char next[FULL_PATH_LEN];
        if (path_join(next, sizeof(next), app->browse_path, app->browse_names[i])) {
            snprintf(app->browse_path, sizeof(app->browse_path), "%s", next);
            ui_render_browser(app);
        } else {
            popup_set_text(app->popup, "Path too long", 64, 32, AlignCenter, AlignCenter);
            view_set_previous_callback(popup_get_view(app->popup), nav_back_to_browser);
            view_dispatcher_switch_to_view(app->view_dispatcher, SubDupFinderViewPopup);
        }
        return;
    }

    // BrowserIndexOverflow / anything else: inert, there's nothing to select.
}

static void summary_callback(DialogExResult result, void *context) {
    UNUSED(result);
    ui_after_scan_navigate(context);
}

static void groups_submenu_callback(void *context, uint32_t index) {
    SubDupFinderApp *app = context;
    app->selected_group_index = index;
    ui_render_files_in_group(app, index);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubDupFinderViewFiles);
}

static void files_in_group_callback(void *context, uint32_t index) {
    SubDupFinderApp *app = context;
    const DuplicateGroup *group = &app->db.groups[app->selected_group_index];
    const FileRecord *record = &app->db.records[group->start_index + index];

    strncpy(app->selected_name, record->path, sizeof(app->selected_name) - 1);
    app->selected_name[sizeof(app->selected_name) - 1] = '\0';

    if (!path_join(app->selected_path, sizeof(app->selected_path), app->scanned_dir,
                   record->path)) {
        popup_set_text(app->popup, "Path too long", 64, 32, AlignCenter, AlignCenter);
        view_set_previous_callback(popup_get_view(app->popup), nav_back_to_files);
        view_dispatcher_switch_to_view(app->view_dispatcher, SubDupFinderViewPopup);
        return;
    }

    dialog_ex_set_header(app->confirm_dialog, "Delete file?", 64, 10, AlignCenter, AlignTop);
    // Name only: 63 chars wrap to 3 lines (4 if all caps, between the buttons); a path wouldn't.
    dialog_ex_set_text(app->confirm_dialog, app->selected_name, 64, 20, AlignCenter, AlignTop);
    view_dispatcher_switch_to_view(app->view_dispatcher, SubDupFinderViewConfirm);
}

static void confirm_callback(DialogExResult result, void *context) {
    SubDupFinderApp *app = context;
    if (result == DialogExResultRight) {
        if (!storage_delete_file(app->selected_path)) {
            popup_set_text(app->popup, "Delete failed", 64, 32, AlignCenter, AlignCenter);
            view_set_previous_callback(popup_get_view(app->popup), nav_back_to_files);
            view_dispatcher_switch_to_view(app->view_dispatcher, SubDupFinderViewPopup);
            return;
        }

        uint32_t current_hash = app->db.groups[app->selected_group_index].hash;
        uint32_t current_size = app->db.groups[app->selected_group_index].size;

        db_remove_record(&app->db, app->selected_name);
        process_duplicates(&app->db);
        ui_render_groups(app);

        int new_index = -1;
        for (size_t i = 0; i < app->db.num_groups; i++) {
            if (app->db.groups[i].hash == current_hash && app->db.groups[i].size == current_size) {
                new_index = (int)i;
                break;
            }
        }

        if (new_index == -1) {
            view_dispatcher_switch_to_view(app->view_dispatcher, SubDupFinderViewGroups);
        } else {
            app->selected_group_index = (size_t)new_index;
            ui_render_files_in_group(app, app->selected_group_index);
            view_dispatcher_switch_to_view(app->view_dispatcher, SubDupFinderViewFiles);
        }
    } else {
        view_dispatcher_switch_to_view(app->view_dispatcher, SubDupFinderViewFiles);
    }
}

/* ── View setup ───────────────────────────────────────────────────── */

void ui_setup_views(SubDupFinderApp *app) {
    view_set_previous_callback(submenu_get_view(app->main_submenu), nav_exit);
    view_set_previous_callback(submenu_get_view(app->groups_submenu), nav_back_to_submenu);
    view_set_previous_callback(submenu_get_view(app->files_in_group_submenu), nav_back_to_groups);
    view_set_previous_callback(dialog_ex_get_view(app->confirm_dialog), nav_back_to_files);
    view_set_previous_callback(dialog_ex_get_view(app->summary_dialog), nav_back_to_submenu);
    view_set_previous_callback(widget_get_view(app->credits_widget), nav_back_to_submenu);
    view_set_previous_callback(popup_get_view(app->popup), nav_back_to_submenu);
    view_set_previous_callback(submenu_get_view(app->browser_submenu), nav_back_to_submenu);

    widget_add_string_element(app->credits_widget, 0, 2, AlignLeft, AlignTop, FontPrimary,
                              "Sub Duplicate Finder");
    widget_add_string_element(app->credits_widget, 0, 14, AlignLeft, AlignTop, FontSecondary,
                              "v" APP_VERSION);
    widget_add_string_element(app->credits_widget, 0, 26, AlignLeft, AlignTop, FontPrimary,
                              "Author: Endika");
    widget_add_string_element(app->credits_widget, 0, 40, AlignLeft, AlignTop, FontSecondary,
                              "https://github.com/endika/");
    widget_add_string_element(app->credits_widget, 0, 50, AlignLeft, AlignTop, FontSecondary,
                              "flipper-sub-dup");

    submenu_add_item(app->main_submenu, "Find Duplicates", SubDupFinderSubmenuIndexScan,
                     main_submenu_callback, app);

    char folder_label[FULL_PATH_LEN + 16];
    ui_build_folder_label(app, folder_label, sizeof(folder_label));
    submenu_add_item(app->main_submenu, folder_label, SubDupFinderSubmenuIndexFolder,
                     main_submenu_callback, app);

    submenu_add_item(app->main_submenu, "Credits", SubDupFinderSubmenuIndexCredits,
                     main_submenu_callback, app);

    dialog_ex_set_right_button_text(app->confirm_dialog, "Yes");
    dialog_ex_set_left_button_text(app->confirm_dialog, "No");
    dialog_ex_set_result_callback(app->confirm_dialog, confirm_callback);
    dialog_ex_set_context(app->confirm_dialog, app);

    dialog_ex_set_center_button_text(app->summary_dialog, "OK");
    dialog_ex_set_result_callback(app->summary_dialog, summary_callback);
    dialog_ex_set_context(app->summary_dialog, app);

    view_dispatcher_add_view(app->view_dispatcher, SubDupFinderViewSubmenu,
                             submenu_get_view(app->main_submenu));
    view_dispatcher_add_view(app->view_dispatcher, SubDupFinderViewGroups,
                             submenu_get_view(app->groups_submenu));
    view_dispatcher_add_view(app->view_dispatcher, SubDupFinderViewFiles,
                             submenu_get_view(app->files_in_group_submenu));
    view_dispatcher_add_view(app->view_dispatcher, SubDupFinderViewConfirm,
                             dialog_ex_get_view(app->confirm_dialog));
    view_dispatcher_add_view(app->view_dispatcher, SubDupFinderViewCredits,
                             widget_get_view(app->credits_widget));
    view_dispatcher_add_view(app->view_dispatcher, SubDupFinderViewPopup,
                             popup_get_view(app->popup));
    view_dispatcher_add_view(app->view_dispatcher, SubDupFinderViewSummary,
                             dialog_ex_get_view(app->summary_dialog));
    view_dispatcher_add_view(app->view_dispatcher, SubDupFinderViewBrowser,
                             submenu_get_view(app->browser_submenu));
}
