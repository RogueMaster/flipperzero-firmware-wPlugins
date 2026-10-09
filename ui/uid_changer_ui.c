#include "uid_changer_ui.h"
#include "nfc_scan_art.h"
#include <furi.h>

static const char* const gen_names[MagicGenCount] = {
    "MFC GEN1 - UID", "MFC GEN2 - CUID", "MFC GEN3 - APDU",
    "MFC GEN4 - UMC", "MFC GDM  - USCUID",
    "ISO15 GEN1", "ISO15 GEN2", "ISO15 GEN3",
};

const char* mtools_uid_gen_name(MagicGenType gen) {
    return gen < MagicGenCount ? gen_names[gen] : "Unknown";
}

static void uid_draw_popup(Canvas* canvas, const UidFlowModel* model) {
    if(!model->popup_active) return;
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_rbox(canvas, 8, 22, 112, 24, 4);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_rframe(canvas, 8, 22, 112, 24, 4);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 37, AlignCenter, AlignBottom, model->popup_text);
}

static void uid_draw_reading(Canvas* canvas, const UidFlowModel* model) {
    mtools_scan_draw_flipper(canvas, 0, 30);
    mtools_scan_draw_fob(canvas, 77, 39);
    mtools_scan_draw_card(canvas, 107, 39);
    /* The same arrow as Magic Check, rotated to point toward the tags. */
    mtools_scan_draw_arrow(canvas, 64 + model->scan_anim_phase % 2, 39, true);
}

void mtools_uid_flow_draw(Canvas* canvas, void* context) {
    UidFlowModel* model = context;
    const char* steps[] = {"UID", "Type", "Write"};
    static const uint8_t digits[3][5] = {
        {0x2, 0x6, 0x2, 0x2, 0x7},
        {0x7, 0x1, 0x7, 0x4, 0x7},
        {0x7, 0x1, 0x7, 0x1, 0x7},
    };
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, "UID Changer");
    canvas_set_font(canvas, FontSecondary);
    for(size_t step = 0; step < 3; step++) {
        int x = step * 43;
        if(step == model->step) {
            canvas_draw_rbox(canvas, x, 14, 42, 11, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_circle(canvas, x + 6, 19, 4);
        for(int row = 0; row < 5; row++) {
            for(int col = 0; col < 3; col++) {
                if(digits[step][row] & (1U << (2 - col)))
                    canvas_draw_dot(canvas, x + 5 + col, 17 + row);
            }
        }
        canvas_draw_str(canvas, x + 14, step == 1 ? 22 : 23, steps[step]);
        canvas_set_color(canvas, ColorBlack);
    }
    if(model->page == UidPageReading) {
        uid_draw_reading(canvas, model);
    } else if(model->page == UidPageMagic) {
        canvas_set_font(canvas, FontKeyboard);
        const uint8_t start = model->card_type == 2 ? MagicGenIso15693Gen1 : MagicGenMfcGen1a;
        const uint8_t count = model->card_type == 2 ? 3 : 5;
        const uint8_t selected = model->gen - start;
        const uint8_t top = selected >= 4 ? selected - 3 : 0;
        for(uint8_t row = 0; row < 4 && top + row < count; row++) {
            uint8_t index = top + row;
            int y = 27 + row * 9;
            if(index == selected) {
                canvas_draw_rbox(canvas, 2, y, 119, 9, 2);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str(canvas, 6, y + 8, mtools_uid_gen_name(start + index));
            canvas_set_color(canvas, ColorBlack);
        }
        if(count > 4) {
            canvas_draw_line(canvas, 125, 28, 125, 62);
            canvas_draw_box(canvas, 123, 28 + selected * 25 / (count - 1), 5, 9);
        }
    } else if(model->page == UidPageReadResult || model->page == UidPageWrite) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, model->info_count >= 4 ? 31 : 33, model->title);
        canvas_set_font(canvas, FontKeyboard);
        const int first_y = model->info_count >= 4 ? 39 : 42;
        const int spacing = model->info_count >= 4 ? 8 : 9;
        for(uint8_t row = 0; row < model->info_count; row++)
            canvas_draw_str(canvas, 2, first_y + row * spacing, model->info_lines[row]);
        canvas_set_font(canvas, FontSecondary);
    } else {
        const bool has_title = model->title[0] != '\0';
        if(has_title) canvas_draw_str(canvas, 2, 35, model->title);
        for(int row = 0; row < 3; row++) {
            const int row_y = has_title ? 37 + row * 9 : 28 + row * 11;
            if(model->selected == row) {
                canvas_draw_rbox(canvas, 1, row_y, 126, has_title ? 9 : 11, 2);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str(canvas, 4, row_y + (has_title ? 8 : 9), model->lines[row]);
            canvas_set_color(canvas, ColorBlack);
        }
    }
    uid_draw_popup(canvas, model);
}
