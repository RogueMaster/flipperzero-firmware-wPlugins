#include "remote.h"
#include "../subghz_remote_app_i.h"

#include <input/input.h>
#include <gui/elements.h>

#include <lib/toolbox/path.h>

#define SUBREM_VIEW_REMOTE_MAX_LABEL_LENGTH 30
#define SUBREM_VIEW_REMOTE_LEFT_OFFSET      10
#define SUBREM_VIEW_REMOTE_RIGHT_OFFSET     0
#define SUBREM_VIEW_REMOTE_MAX_INFO_LENGTH  24
#define SUBREM_VIEW_REMOTE_MAX_INFO_LINES   5

struct SubRemViewRemote {
    View* view;
    SubRemViewRemoteCallback callback;
    void* context;
};

typedef struct {
    char* labels[SubRemSubKeyNameMaxCount];

    SubRemViewRemoteState state;

    uint8_t pressed_btn;
    bool is_external;

    // Details of the last pressed button, empty string = not present
    char protocol[SUBREM_VIEW_REMOTE_MAX_INFO_LENGTH];
    char frequency[SUBREM_VIEW_REMOTE_MAX_INFO_LENGTH];
    char key[SUBREM_VIEW_REMOTE_MAX_INFO_LENGTH];
    char serial[SUBREM_VIEW_REMOTE_MAX_INFO_LENGTH];
    char counter[SUBREM_VIEW_REMOTE_MAX_INFO_LENGTH];
    char manufacture[SUBREM_VIEW_REMOTE_MAX_INFO_LENGTH];
} SubRemViewRemoteModel;

void subrem_view_remote_set_callback(
    SubRemViewRemote* subrem_view_remote,
    SubRemViewRemoteCallback callback,
    void* context) {
    furi_assert(subrem_view_remote);

    subrem_view_remote->callback = callback;
    subrem_view_remote->context = context;
}

void subrem_view_remote_update_data_labels(
    SubRemViewRemote* subrem_view_remote,
    SubRemSubFilePreset** subs_presets) {
    furi_assert(subrem_view_remote);
    furi_assert(subs_presets);

    FuriString* labels[SubRemSubKeyNameMaxCount];
    SubRemSubFilePreset* sub_preset;

    for(uint8_t i = 0; i < SubRemSubKeyNameMaxCount; i++) {
        sub_preset = subs_presets[i];
        switch(sub_preset->load_state) {
        case SubRemLoadSubStateOK:
            if(!furi_string_empty(sub_preset->label)) {
                labels[i] = furi_string_alloc_set(sub_preset->label);
            } else if(!furi_string_empty(sub_preset->file_path)) {
                labels[i] = furi_string_alloc();
                path_extract_filename(sub_preset->file_path, labels[i], true);
            } else {
                labels[i] = furi_string_alloc_set("Empty Label");
            }
            break;

        case SubRemLoadSubStateErrorNoFile:
            labels[i] = furi_string_alloc_set("[X] Can't open file");
            break;

        case SubRemLoadSubStateErrorFreq:
        case SubRemLoadSubStateErrorMod:
        case SubRemLoadSubStateErrorProtocol:
            labels[i] = furi_string_alloc_set("[X] Error in .sub file");
            break;

        default:
            labels[i] = furi_string_alloc_set("");
            break;
        }
    }

    with_view_model(
        subrem_view_remote->view,
        SubRemViewRemoteModel * model,
        {
            for(uint8_t i = 0; i < SubRemSubKeyNameMaxCount; i++) {
                strncpy(
                    model->labels[i],
                    furi_string_get_cstr(labels[i]),
                    SUBREM_VIEW_REMOTE_MAX_LABEL_LENGTH);
                // strncpy doesn't terminate when the source is >= max; the buffer is
                // MAX_LABEL_LENGTH+1, so index MAX is in bounds and forces a NUL.
                model->labels[i][SUBREM_VIEW_REMOTE_MAX_LABEL_LENGTH] = '\0';
            }
        },
        true);

    for(uint8_t i = 0; i < SubRemSubKeyNameMaxCount; i++) {
        furi_string_free(labels[i]);
    }
}

void subrem_view_remote_set_state(
    SubRemViewRemote* subrem_view_remote,
    SubRemViewRemoteState state,
    uint8_t presed_btn) {
    furi_assert(subrem_view_remote);
    with_view_model(
        subrem_view_remote->view,
        SubRemViewRemoteModel * model,
        {
            model->state = state;
            model->pressed_btn = presed_btn;
        },
        true);
}

void subrem_view_remote_set_radio(SubRemViewRemote* subrem_view_remote, bool external) {
    furi_assert(subrem_view_remote);
    with_view_model(
        subrem_view_remote->view,
        SubRemViewRemoteModel * model,
        { model->is_external = external; },
        true);
}

void subrem_view_remote_set_info(
    SubRemViewRemote* subrem_view_remote,
    const char* protocol,
    const char* frequency,
    const char* key,
    const char* serial,
    const char* counter,
    const char* manufacture) {
    furi_assert(subrem_view_remote);
    with_view_model(
        subrem_view_remote->view,
        SubRemViewRemoteModel * model,
        {
            strlcpy(model->protocol, protocol, SUBREM_VIEW_REMOTE_MAX_INFO_LENGTH);
            strlcpy(model->frequency, frequency, SUBREM_VIEW_REMOTE_MAX_INFO_LENGTH);
            strlcpy(model->key, key, SUBREM_VIEW_REMOTE_MAX_INFO_LENGTH);
            strlcpy(model->serial, serial, SUBREM_VIEW_REMOTE_MAX_INFO_LENGTH);
            strlcpy(model->counter, counter, SUBREM_VIEW_REMOTE_MAX_INFO_LENGTH);
            strlcpy(model->manufacture, manufacture, SUBREM_VIEW_REMOTE_MAX_INFO_LENGTH);
        },
        true);
}

static void subrem_view_remote_draw_info(Canvas* canvas, SubRemViewRemoteModel* model) {
    if(model->protocol[0] == '\0') {
        return;
    }

    const char* lines[] = {
        model->protocol,
        model->frequency,
        model->key,
        model->serial,
        model->counter,
        model->manufacture,
    };

    canvas_draw_line(canvas, 0, 68, 63, 68);

    // Only 5 lines fit, the last ones (manufacture first) are dropped when there are more
    FuriString* str = furi_string_alloc();
    uint8_t y = 80;
    uint8_t lines_drawn = 0;
    for(size_t i = 0; i < COUNT_OF(lines); i++) {
        if(lines[i][0] == '\0') continue;
        if(lines_drawn++ == SUBREM_VIEW_REMOTE_MAX_INFO_LINES) break;
        canvas_set_font(canvas, (i == 0) ? FontPrimary : FontSecondary);
        furi_string_set(str, lines[i]);
        elements_string_fit_width(canvas, str, 64);
        canvas_draw_str(canvas, 0, y, furi_string_get_cstr(str));
        y += 11;
    }
    furi_string_free(str);
}

void subrem_view_remote_draw(Canvas* canvas, SubRemViewRemoteModel* model) {
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Statusbar
    canvas_draw_icon(canvas, 0, 0, &I_status_bar);
    if(model->state == SubRemViewRemoteStateOFF) {
        canvas_invert_color(canvas);
        canvas_draw_rbox(canvas, 12, 0, 52 - 12, 13, 2);
        canvas_invert_color(canvas);
        canvas_draw_rframe(canvas, 12, 0, 52 - 12, 13, 2);
        canvas_draw_str_aligned(canvas, 32, 3, AlignCenter, AlignTop, "Preview");
    } else {
        canvas_draw_icon(
            canvas,
            0,
            2,
            (model->is_external) ? &I_External_antenna_20x12 : &I_Internal_antenna_20x12);
        canvas_draw_icon(canvas, 50, 0, &I_Status_cube_14x14);
        if(model->state == SubRemViewRemoteStateSending) {
            canvas_draw_icon_ex(canvas, 52, 3, &I_Pin_arrow_up_7x9, IconRotation90);
        }
    }

    //Labels with their button icons, the sending one is drawn inverted
    const uint8_t list_y = 14;
    const struct {
        uint8_t x;
        uint8_t y;
        const Icon* icon;
    } btn_icons[SubRemSubKeyNameMaxCount] = {
        [SubRemSubKeyNameUp] = {1, 5, &I_ButtonUp_7x4},
        [SubRemSubKeyNameDown] = {1, 15, &I_ButtonDown_7x4},
        [SubRemSubKeyNameLeft] = {2, 23, &I_ButtonLeft_4x7},
        [SubRemSubKeyNameRight] = {2, 33, &I_ButtonRight_4x7},
        [SubRemSubKeyNameOk] = {0, 42, &I_Ok_btn_9x9},
    };

    canvas_set_font(canvas, FontSecondary);
    uint8_t y = list_y;
    for(uint8_t i = 0; i < SubRemSubKeyNameMaxCount; i++) {
        bool is_active = (model->state == SubRemViewRemoteStateSending) &&
                         (model->pressed_btn == i);
        if(is_active) {
            canvas_draw_rbox(canvas, 0, y + 2, 64, 11, 2);
            canvas_set_color(canvas, ColorWhite);
        }

        canvas_draw_icon(canvas, btn_icons[i].x, list_y + btn_icons[i].y, btn_icons[i].icon);
        elements_text_box(
            canvas,
            SUBREM_VIEW_REMOTE_LEFT_OFFSET,
            y + 2,
            64 - SUBREM_VIEW_REMOTE_LEFT_OFFSET - SUBREM_VIEW_REMOTE_RIGHT_OFFSET,
            12,
            AlignLeft,
            AlignBottom,
            model->labels[i],
            false);

        if(is_active) {
            canvas_set_color(canvas, ColorBlack);
        }
        y += 10;
    }

    if(model->state != SubRemViewRemoteStateOFF) {
        subrem_view_remote_draw_info(canvas, model);
    } else {
        canvas_draw_icon(canvas, 2, 128 - 11, &I_ButtonLeft_4x7);
        canvas_draw_str_aligned(canvas, 8, 128 - 4, AlignLeft, AlignBottom, "Back");

        canvas_draw_icon(canvas, 58, 128 - 11, &I_ButtonRight_4x7);
        canvas_draw_str_aligned(canvas, 56, 128 - 4, AlignRight, AlignBottom, "Save");
    }
}

bool subrem_view_remote_input(InputEvent* event, void* context) {
    furi_assert(context);
    SubRemViewRemote* subrem_view_remote = context;

    if(event->key == InputKeyBack && event->type == InputTypePress) {
        bool is_stopping = false;
        with_view_model(
            subrem_view_remote->view,
            SubRemViewRemoteModel * model,
            {
                if(model->state == SubRemViewRemoteStateSending) {
                    is_stopping = true;
                    model->pressed_btn = 0;
                }
            },
            true);

        //Cant send exit the app inside that with_model,locks the model and the app will hang and not unload!
        if(is_stopping)
            subrem_view_remote->callback(
                SubRemCustomEventViewRemoteForcedStop, subrem_view_remote->context);
        else
            subrem_view_remote->callback(
                SubRemCustomEventViewRemoteBack, subrem_view_remote->context);

        return true;
    }
    // BACK button processing end

    if(event->key == InputKeyUp && event->type == InputTypePress) {
        subrem_view_remote->callback(
            SubRemCustomEventViewRemoteStartUP, subrem_view_remote->context);
        return true;
    } else if(event->key == InputKeyDown && event->type == InputTypePress) {
        subrem_view_remote->callback(
            SubRemCustomEventViewRemoteStartDOWN, subrem_view_remote->context);
        return true;
    } else if(event->key == InputKeyLeft && event->type == InputTypePress) {
        subrem_view_remote->callback(
            SubRemCustomEventViewRemoteStartLEFT, subrem_view_remote->context);
        return true;
    } else if(event->key == InputKeyRight && event->type == InputTypePress) {
        subrem_view_remote->callback(
            SubRemCustomEventViewRemoteStartRIGHT, subrem_view_remote->context);
        return true;
    } else if(event->key == InputKeyOk && event->type == InputTypePress) {
        subrem_view_remote->callback(
            SubRemCustomEventViewRemoteStartOK, subrem_view_remote->context);
        return true;
    } else if(event->type == InputTypeRelease) {
        subrem_view_remote->callback(SubRemCustomEventViewRemoteStop, subrem_view_remote->context);
        return true;
    }

    return true;
}

void subrem_view_remote_enter(void* context) {
    furi_assert(context);
}

void subrem_view_remote_exit(void* context) {
    furi_assert(context);
}

SubRemViewRemote* subrem_view_remote_alloc(void) {
    SubRemViewRemote* subrem_view_remote = malloc(sizeof(SubRemViewRemote));

    // View allocation and configuration
    subrem_view_remote->view = view_alloc();
    view_allocate_model(
        subrem_view_remote->view, ViewModelTypeLocking, sizeof(SubRemViewRemoteModel));
    view_set_context(subrem_view_remote->view, subrem_view_remote);
    view_set_orientation(subrem_view_remote->view, ViewOrientationVertical);
    view_set_draw_callback(subrem_view_remote->view, (ViewDrawCallback)subrem_view_remote_draw);
    view_set_input_callback(subrem_view_remote->view, subrem_view_remote_input);
    view_set_enter_callback(subrem_view_remote->view, subrem_view_remote_enter);
    view_set_exit_callback(subrem_view_remote->view, subrem_view_remote_exit);

    with_view_model(
        subrem_view_remote->view,
        SubRemViewRemoteModel * model,
        {
            model->state = SubRemViewRemoteStateIdle;

            for(uint8_t i = 0; i < SubRemSubKeyNameMaxCount; i++) {
                model->labels[i] = malloc(sizeof(char) * SUBREM_VIEW_REMOTE_MAX_LABEL_LENGTH + 1);
                strcpy(model->labels[i], "");
            }

            model->pressed_btn = 0;
            model->is_external = false;

            model->protocol[0] = '\0';
            model->frequency[0] = '\0';
            model->key[0] = '\0';
            model->serial[0] = '\0';
            model->counter[0] = '\0';
            model->manufacture[0] = '\0';
        },
        true);
    return subrem_view_remote;
}

void subrem_view_remote_free(SubRemViewRemote* subghz_remote) {
    furi_assert(subghz_remote);

    with_view_model(
        subghz_remote->view,
        SubRemViewRemoteModel * model,
        {
            for(uint8_t i = 0; i < SubRemSubKeyNameMaxCount; i++) {
                free(model->labels[i]);
            }
        },
        true);
    view_free(subghz_remote->view);
    free(subghz_remote);
}

View* subrem_view_remote_get_view(SubRemViewRemote* subrem_view_remote) {
    furi_assert(subrem_view_remote);
    return subrem_view_remote->view;
}
