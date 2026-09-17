#include "view_delete_success.h"

#define UHF_DELETE_SUCCESS_SCROLL_PERIOD_MS  200U
#define UHF_DELETE_SUCCESS_EPC_VISIBLE_CHARS 24U

static void uhf_reader_view_delete_success_timer_callback(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    if(!App || !App->ViewDeleteSuccess) return;

    bool redraw = false;
    with_view_model(
        App->ViewDeleteSuccess,
        UHFReaderDeleteModel * model,
        {
            const size_t length = furi_string_size(model->SelectedTagEpc);
            if(length > UHF_DELETE_SUCCESS_EPC_VISIBLE_CHARS) {
                model->ScrollOffset = (model->ScrollOffset + 1U) % length;
                redraw = true;
            } else {
                model->ScrollOffset = 0U;
            }
        },
        redraw);
}

/**
 * @brief      Delete Success Draw Callback.
 * @details    This function is called when the user confirmed deleting a saved UHF tag.
 * @param      canvas    The canvas - Canvas object for drawing the screen.
 * @param      model  The view model - model for the view with variables required for drawing.
*/
void uhf_reader_view_delete_success_draw_callback(Canvas* canvas, void* model) {
    UHFReaderDeleteModel* MyModel = (UHFReaderDeleteModel*)model;
    char index_string[12];

    //Clearing the canvas, setting the color, font and content displayed.
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 4, 11, " Successfully Deleted!");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 4, 22, "Name:");

    //Displaying the name of the saved UHF Tag deleted.
    canvas_draw_str(canvas, 32, 22, furi_string_get_cstr(MyModel->SelectedTagName));

    //Displaying the index of the saved tag as shown in the Index file.
    snprintf(index_string, sizeof(index_string), "%lu", (unsigned long)MyModel->SelectedTagIndex);
    canvas_draw_str(canvas, 4, 33, "EPC Index:");
    canvas_draw_str(canvas, 53, 33, index_string);

    //Displaying the EPC scrolling across the screen
    canvas_draw_str(canvas, 4, 44, "EPC:");
    const char* scrolling_text = furi_string_get_cstr(MyModel->SelectedTagEpc);

    // Calculate the start and end indices of the substring to draw
    uint32_t StartPos = MyModel->ScrollOffset;

    //Calculate the length of the scrolling text
    size_t length = strlen(scrolling_text);

    //I am sure there is a better way to do this that involves slightly safer memory management...
    char VisiblePart[UHF_DELETE_SUCCESS_EPC_VISIBLE_CHARS + 1U];
    memset(VisiblePart, ' ', UHF_DELETE_SUCCESS_EPC_VISIBLE_CHARS);
    VisiblePart[UHF_DELETE_SUCCESS_EPC_VISIBLE_CHARS] = '\0';

    //Fill the array up with the epc values
    for(uint32_t i = 0; i < UHF_DELETE_SUCCESS_EPC_VISIBLE_CHARS; i++) {
        uint32_t CharIndex = StartPos + i;
        if(CharIndex < length) {
            VisiblePart[i] = scrolling_text[CharIndex];
        }
    }

    //Draw the visible part of the epc
    canvas_draw_str(canvas, 28, 44, VisiblePart);

    //Exit button
    elements_button_center(canvas, "Exit");
}

/**
 * @brief      Callback for delete success input.
 * @details    This function is called when the user presses a button while on the delete success screen.
 * @param      event    The event - InputEvent object.
 * @param      context  The context - UHFReaderApp object.
 * @return     true if the event was handled, false otherwise.
*/
bool uhf_reader_view_delete_success_input_callback(InputEvent* event, void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    //Return to the saved menu after deleting the saved epc
    if(event->type == InputTypeShort) {
        if(event->key == InputKeyOk) {
            UHF_I(
                "DELETE",
                "EXIT pressed count=%lu free=%lu",
                (unsigned long)App->NumberOfSavedTags,
                (unsigned long)memmgr_get_free_heap());
            uhf_debug_flush();
            view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewSaved);
            return true;
        }
    }

    return false;
}

/**
 * @brief      Delete Success enter callback function.
 * @details    This function is called when the view transitions to the delete success screen.
 * @param      context  The context - UHFReaderApp object.
*/
void uhf_reader_view_delete_success_enter_callback(void* context) {
    //Grab the period for the timer
    int32_t Period = furi_ms_to_ticks(UHF_DELETE_SUCCESS_SCROLL_PERIOD_MS);
    UHFReaderApp* App = (UHFReaderApp*)context;
    const uint32_t count_before = App->NumberOfSavedTags;
    UHF_I(
        "DELETE",
        "SUCCESS VIEW ENTER index=%lu count=%lu free=%lu",
        (unsigned long)App->SelectedTagIndex,
        (unsigned long)count_before,
        (unsigned long)memmgr_get_free_heap());

    //Call the helper functions below to update the saved UHF tag submenu
    delete_and_update_entry(context, App->SelectedTagIndex);
    update_dictionary_keys(context);
    UHF_I(
        "DELETE",
        "SUCCESS DATA READY count=%lu->%lu free=%lu",
        (unsigned long)count_before,
        (unsigned long)App->NumberOfSavedTags,
        (unsigned long)memmgr_get_free_heap());
    uhf_debug_flush();
    dolphin_deed(DolphinDeedRfidReadSuccess);
    uhf_notify_success(App);
    //Start the timer for the delete success screen
    furi_assert(App->Timer == NULL);
    App->Timer = furi_timer_alloc(
        uhf_reader_view_delete_success_timer_callback, FuriTimerTypePeriodic, context);
    furi_timer_start(App->Timer, Period);
    UHF_I("DELETE", "SUCCESS VIEW READY timer=1");
}

/**
 * @brief      Callback when the user exits the delete success screen.
 * @details    This function is called when the user exits the delete success screen.
 * @param      context  The context - UHFReaderApp object.
*/
void uhf_reader_view_delete_success_exit_callback(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    UHF_I("DELETE", "SUCCESS VIEW EXIT begin timer=%d", App->Timer ? 1 : 0);

    //Stop and free the timer
    if(App->Timer) {
        furi_timer_stop(App->Timer);
        furi_timer_free(App->Timer);
        App->Timer = NULL;
    }
    UHF_I("DELETE", "SUCCESS VIEW EXIT end free=%lu", (unsigned long)memmgr_get_free_heap());
}

/**
 * @brief      Allocates the delete success view.
 * @details    This function allocates all variables for the delete success view.
 * @param      context  The context - UHFReaderApp object.
*/
void view_delete_success_alloc(UHFReaderApp* App) {
    //Allocating the view and setting all callback functions
    App->ViewDeleteSuccess = view_alloc();
    view_set_draw_callback(App->ViewDeleteSuccess, uhf_reader_view_delete_success_draw_callback);
    view_set_input_callback(App->ViewDeleteSuccess, uhf_reader_view_delete_success_input_callback);
    view_set_previous_callback(App->ViewDeleteSuccess, uhf_reader_navigation_saved_exit_callback);
    view_set_enter_callback(App->ViewDeleteSuccess, uhf_reader_view_delete_success_enter_callback);
    view_set_exit_callback(App->ViewDeleteSuccess, uhf_reader_view_delete_success_exit_callback);
    view_set_context(App->ViewDeleteSuccess, App);

    //Allocating the view model
    view_allocate_model(
        App->ViewDeleteSuccess, ViewModelTypeLocking, sizeof(UHFReaderDeleteModel));
    with_view_model(
        App->ViewDeleteSuccess,
        UHFReaderDeleteModel * model,
        {
            model->SelectedTagEpc = furi_string_alloc_set("ABCDEF12");
            model->SelectedTagIndex = 1U;
            model->SelectedTagName = furi_string_alloc_set("Default Name");
            model->ScrollOffset = 0U;
        },
        false);
    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewDeleteSuccess, App->ViewDeleteSuccess);
}

/**
 * @brief      Frees the delete success view.
 * @details    This function frees all variables for the delete success view.
 * @param      context  The context - UHFReaderApp object.
*/
void view_delete_success_free(UHFReaderApp* App) {
    if(!App->ViewDeleteSuccess) return;

    with_view_model(
        App->ViewDeleteSuccess,
        UHFReaderDeleteModel * model,
        {
            if(model->SelectedTagName) {
                furi_string_free(model->SelectedTagName);
                model->SelectedTagName = NULL;
            }
            if(model->SelectedTagEpc) {
                furi_string_free(model->SelectedTagEpc);
                model->SelectedTagEpc = NULL;
            }
        },
        false);

    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewDeleteSuccess);
    view_free(App->ViewDeleteSuccess);
    App->ViewDeleteSuccess = NULL;
}
