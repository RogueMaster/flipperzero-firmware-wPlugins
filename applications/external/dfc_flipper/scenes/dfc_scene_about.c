#include "../dfc_i.h"
#include <dolphin/dolphin.h>
#include <stdint.h>

#define DFC_ABOUT_LINE_WIDTH 24

static const char* const about_paragraphs[] = {
    "DFC is an independent, third-party implementation of the MIFARE DESFire native command protocol.",
    "It is not developed, authorized, licensed, or endorsed by NXP Semiconductors.",
    "\"DESFire\" is used here solely to describe protocol compatibility, not to claim affiliation.",
    "No guarantee of compatibility or functionality is made. Users assume all risks associated with its use.",
    "MIFARE and DESFire are trademarks of NXP B.V. This software is not associated with, sponsored by, or endorsed by NXP.",
};

static size_t dfc_word_len(const char* word) {
    size_t length = 0;
    while(word[length] && word[length] != ' ') {
        length++;
    }

    return length;
}

static void dfc_about_append_line(
    FuriString* output,
    const char* const* words,
    const size_t* word_lengths,
    size_t word_count,
    bool justify) {
    size_t text_length = 0;
    for(size_t i = 0; i < word_count; i++) {
        text_length += word_lengths[i];
    }

    size_t gaps = word_count > 0 ? word_count - 1 : 0;
    size_t base_spaces = gaps > 0 ? 1 : 0;
    size_t extra_spaces = 0;

    if(justify && gaps > 0 && text_length + gaps < DFC_ABOUT_LINE_WIDTH) {
        size_t spaces = DFC_ABOUT_LINE_WIDTH - text_length;
        base_spaces = spaces / gaps;
        extra_spaces = spaces % gaps;
    }

    for(size_t i = 0; i < word_count; i++) {
        furi_string_cat_printf(output, "%.*s", (int)word_lengths[i], words[i]);

        if(i < gaps) {
            size_t spaces = base_spaces + (i < extra_spaces ? 1 : 0);
            for(size_t j = 0; j < spaces; j++) {
                furi_string_push_back(output, ' ');
            }
        }
    }

    furi_string_push_back(output, '\n');
}

static void dfc_about_append_paragraph(FuriString* output, const char* paragraph) {
    const char* line_words[DFC_ABOUT_LINE_WIDTH];
    size_t line_word_lengths[DFC_ABOUT_LINE_WIDTH];
    size_t line_word_count = 0;
    size_t line_length = 0;

    const char* cursor = paragraph;
    while(*cursor) {
        while(*cursor == ' ') {
            cursor++;
        }

        if(!*cursor) {
            break;
        }

        size_t word_length = dfc_word_len(cursor);
        size_t next_length = line_word_count == 0 ? word_length : line_length + 1 + word_length;

        if(line_word_count > 0 && next_length > DFC_ABOUT_LINE_WIDTH) {
            dfc_about_append_line(output, line_words, line_word_lengths, line_word_count, true);
            line_word_count = 0;
            line_length = 0;
        }

        line_words[line_word_count] = cursor;
        line_word_lengths[line_word_count] = word_length;
        line_word_count++;
        line_length = line_word_count == 1 ? word_length : line_length + 1 + word_length;
        cursor += word_length;
    }

    if(line_word_count > 0) {
        dfc_about_append_line(output, line_words, line_word_lengths, line_word_count, false);
    }
}

static void dfc_about_build_text(FuriString* output) {
    for(size_t i = 0; i < COUNT_OF(about_paragraphs); i++) {
        dfc_about_append_paragraph(output, about_paragraphs[i]);

        if(i + 1 < COUNT_OF(about_paragraphs)) {
            furi_string_push_back(output, '\n');
        }
    }
}

void dfc_scene_about_widget_callback(GuiButtonType result, InputType type, void* context) {
    Dfc* dfc = context;
    if(type == InputTypeShort) {
        view_dispatcher_send_custom_event(dfc->view_dispatcher, result);
    }
}

void dfc_scene_about_on_enter(void* context) {
    Dfc* dfc = context;

    furi_string_reset(dfc->text_box_store);
    dfc_about_build_text(dfc->text_box_store);

    text_box_set_font(dfc->text_box, TextBoxFontText);
    text_box_set_text(dfc->text_box, furi_string_get_cstr(dfc->text_box_store));
    view_dispatcher_switch_to_view(dfc->view_dispatcher, DfcViewTextBox);
}

bool dfc_scene_about_on_event(void* context, SceneManagerEvent event) {
    Dfc* dfc = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == GuiButtonTypeLeft) {
            consumed = scene_manager_previous_scene(dfc->scene_manager);
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        consumed = scene_manager_previous_scene(dfc->scene_manager);
    }
    return consumed;
}

void dfc_scene_about_on_exit(void* context) {
    Dfc* dfc = context;

    // Clear views
    text_box_reset(dfc->text_box);
}
