#include "include/app/habitflow_app.h"
#include "include/app/hf_app_presenter.h"
#include "include/app/hf_app_state.h"
#include "include/app/hf_views.h"
#include "include/application/hf_session_service.h"
#include "include/domain/habit.h"
#include "include/persistence/habit_store.h"
#include "include/ports/hf_clock_port.h"
#include "include/version.h"
#include <furi.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_input.h>
#include <gui/modules/widget.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void hf_switch(HabitFlowApp *app, HfViewId v) {
    app->current = v;
    view_dispatcher_switch_to_view(app->vd, v);
}

static void dialog_result_cb(DialogExResult result, void *context);

void hf_dialog_rebind(HabitFlowApp *app) {
    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_result_callback(app->dialog, dialog_result_cb);
}

void hf_app_save(HabitFlowApp *app) {
    habit_store_save(&app->store);
}

void hf_build_credits(HabitFlowApp *app) {
    furi_assert(app && app->credits);
    widget_reset(app->credits);
    snprintf(app->credits_buf, sizeof(app->credits_buf),
             "\e#%s\e#\n\nVersion: %s\n\nAuthor: %s\n\n%s", APP_NAME, APP_VERSION, APP_AUTHOR,
             APP_REPO_URL);
    widget_add_text_scroll_element(app->credits, 0, 0, 128, 64, app->credits_buf);
}

void text_input_ok_cb(void *context) {
    HabitFlowApp *app = context;
    snprintf(app->edit_buf.name, sizeof(app->edit_buf.name), "%s", app->text_buf);
    hf_switch(app, HfViewEdit);
}

static void popup_mastered_cb(void *context) {
    HabitFlowApp *app = context;
    hf_switch(app, HfViewDetail);
}

static void hf_show_yesterday_dialog(HabitFlowApp *app) {
    const size_t idx = app->yq.indices[app->yq.pos];
    const Habit *h = &app->store.habits[idx];
    char body[64];
    snprintf(body, sizeof(body), "Did you complete\n\"%s\"\nyesterday?", h->name);
    dialog_ex_reset(app->dialog);
    hf_dialog_rebind(app);
    dialog_ex_set_header(app->dialog, "Yesterday?", 64, 2, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog, body, 64, 14, AlignCenter, AlignTop);
    dialog_ex_set_left_button_text(app->dialog, "No");
    dialog_ex_set_right_button_text(app->dialog, "Yes");
}

static void hf_finish_yesterday_queue(HabitFlowApp *app) {
    hf_session_close_yesterday_flow(&app->store, hf_clock_today_packed(), &app->yq);
}

static void dialog_result_cb(DialogExResult result, void *context) {
    HabitFlowApp *app = context;
    if (app->dlg_mode == HfDlgYesterday) {
        const bool yes = (result == DialogExResultRight);
        const size_t idx = app->yq.indices[app->yq.pos];
        hf_habit_apply_overnight(&app->store.habits[idx], yes);
        hf_app_save(app);
        app->yq.pos++;
        if (app->yq.pos >= app->yq.count) {
            hf_finish_yesterday_queue(app);
            hf_switch(app, HfViewMain);
        } else {
            hf_show_yesterday_dialog(app);
        }
        return;
    }
    if (app->dlg_mode == HfDlgResetStreak) {
        if (result == DialogExResultRight) {
            hf_habit_reset_streak(&app->store.habits[app->detail_index]);
            hf_app_save(app);
        }
        hf_switch(app, HfViewDetail);
        return;
    }
    if (app->dlg_mode == HfDlgDeleteHabit) {
        if (result == DialogExResultRight) {
            habit_store_delete_at(&app->store, app->edit_index);
            hf_switch(app, HfViewManage);
        } else {
            hf_switch(app, HfViewEdit);
        }
        return;
    }
}

static bool app_navigation_cb(void *context) {
    HabitFlowApp *app = context;
    if (app->current == HfViewCredits) {
        hf_switch(app, HfViewMain);
        return true;
    }
    if (app->current == HfViewTextInput) {
        hf_switch(app, HfViewEdit);
        return true;
    }
    if (app->current == HfViewDialog) {
        if (app->dlg_mode == HfDlgYesterday) {
            dialog_result_cb(DialogExResultLeft, app);
        } else {
            hf_switch(app, app->dlg_mode == HfDlgResetStreak ? HfViewDetail : HfViewEdit);
        }
        return true;
    }
    if (app->current == HfViewPopupMastered) {
        hf_switch(app, HfViewDetail);
        return true;
    }
    if (app->current == HfViewMain) {
        view_dispatcher_stop(app->vd);
        return true;
    }
    return false;
}

int32_t habitflow_app_run(void) {
    HabitFlowApp *app = malloc(sizeof(HabitFlowApp));
    if (!app) {
        return -1;
    }
    memset(app, 0, sizeof(*app));
    habit_store_init(&app->store);
    habit_store_load(&app->store);

    hf_session_on_resume(&app->store, hf_clock_today_packed(), &app->yq);

    app->gui = furi_record_open(RECORD_GUI);
    app->vd = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->vd, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_set_event_callback_context(app->vd, app);
    view_dispatcher_set_navigation_event_callback(app->vd, app_navigation_cb);

    app->main = view_alloc();
    view_allocate_model(app->main, ViewModelTypeLocking, sizeof(HfCtx));
    view_set_context(app->main, app);
    view_set_draw_callback(app->main, hf_views_main_draw);
    view_set_input_callback(app->main, hf_views_main_input);
    with_view_model(app->main, HfCtx * m, { m->app = app; }, true);
    view_dispatcher_add_view(app->vd, HfViewMain, app->main);

    app->credits = widget_alloc();
    hf_build_credits(app);
    view_dispatcher_add_view(app->vd, HfViewCredits, widget_get_view(app->credits));

    app->detail = view_alloc();
    view_allocate_model(app->detail, ViewModelTypeLocking, sizeof(HfCtx));
    view_set_context(app->detail, app);
    view_set_draw_callback(app->detail, hf_views_detail_draw);
    view_set_input_callback(app->detail, hf_views_detail_input);
    with_view_model(app->detail, HfCtx * dm, { dm->app = app; }, true);
    view_dispatcher_add_view(app->vd, HfViewDetail, app->detail);

    app->manage = view_alloc();
    view_allocate_model(app->manage, ViewModelTypeLocking, sizeof(HfCtx));
    view_set_context(app->manage, app);
    view_set_draw_callback(app->manage, hf_views_manage_draw);
    view_set_input_callback(app->manage, hf_views_manage_input);
    with_view_model(app->manage, HfCtx * mm, { mm->app = app; }, true);
    view_dispatcher_add_view(app->vd, HfViewManage, app->manage);

    app->edit = view_alloc();
    view_allocate_model(app->edit, ViewModelTypeLocking, sizeof(HfCtx));
    view_set_context(app->edit, app);
    view_set_draw_callback(app->edit, hf_views_edit_draw);
    view_set_input_callback(app->edit, hf_views_edit_input);
    with_view_model(app->edit, HfCtx * em, { em->app = app; }, true);
    view_dispatcher_add_view(app->vd, HfViewEdit, app->edit);

    app->text_input = text_input_alloc();
    text_input_set_result_callback(app->text_input, text_input_ok_cb, app, app->text_buf,
                                   sizeof(app->text_buf), false);
    view_dispatcher_add_view(app->vd, HfViewTextInput, text_input_get_view(app->text_input));

    app->dialog = dialog_ex_alloc();
    hf_dialog_rebind(app);
    view_dispatcher_add_view(app->vd, HfViewDialog, dialog_ex_get_view(app->dialog));

    app->popup_mastered = popup_alloc();
    popup_set_context(app->popup_mastered, app);
    popup_set_callback(app->popup_mastered, popup_mastered_cb);
    view_dispatcher_add_view(app->vd, HfViewPopupMastered, popup_get_view(app->popup_mastered));

    if (app->yq.count > 0) {
        app->dlg_mode = HfDlgYesterday;
        hf_show_yesterday_dialog(app);
        hf_switch(app, HfViewDialog);
    } else {
        hf_switch(app, HfViewMain);
    }

    view_dispatcher_run(app->vd);

    view_dispatcher_remove_view(app->vd, HfViewPopupMastered);
    popup_free(app->popup_mastered);

    view_dispatcher_remove_view(app->vd, HfViewDialog);
    dialog_ex_free(app->dialog);

    view_dispatcher_remove_view(app->vd, HfViewTextInput);
    text_input_free(app->text_input);

    view_dispatcher_remove_view(app->vd, HfViewEdit);
    view_free(app->edit);

    view_dispatcher_remove_view(app->vd, HfViewManage);
    view_free(app->manage);

    view_dispatcher_remove_view(app->vd, HfViewDetail);
    view_free(app->detail);

    view_dispatcher_remove_view(app->vd, HfViewCredits);
    widget_free(app->credits);

    view_dispatcher_remove_view(app->vd, HfViewMain);
    view_free(app->main);

    view_dispatcher_free(app->vd);
    furi_record_close(RECORD_GUI);
    free(app);
    return 0;
}
