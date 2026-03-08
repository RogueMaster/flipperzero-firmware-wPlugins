#include "dashboard_i.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DASH_BITTRACK_BYTES 8U
#define DASH_BITTRACK_BITS  8U

static void dashboard_bittrack_set_unknown(char out[DASH_BITTRACK_BITS + 1U]) {
    if(!out) {
        return;
    }

    memcpy(out, "--------", DASH_BITTRACK_BITS + 1U);
}

static void dashboard_bittrack_rebuild_rows(AppDashboardModel* model) {
    if(!model) {
        return;
    }

    char bytes[DASH_BITTRACK_BYTES][DASH_BITTRACK_BITS + 1U] = {{0}};

    for(uint8_t i = 0U; i < DASH_BITTRACK_BYTES; i++) {
        dashboard_bittrack_set_unknown(bytes[i]);
        for(uint8_t bit = 0U; bit < DASH_BITTRACK_BITS; bit++) {
            if(model->bittrack_frozen[i][bit]) {
                bytes[i][bit] = 'X';
            } else {
                const char live = model->bittrack_live[i][bit];
                bytes[i][bit] = ((live == '0') || (live == '1')) ? live : '-';
            }
        }
        bytes[i][DASH_BITTRACK_BITS] = '\0';
    }

    snprintf(model->bit_row[0], sizeof(model->bit_row[0]), "0:%s 1:%s", bytes[0], bytes[1]);
    snprintf(model->bit_row[1], sizeof(model->bit_row[1]), "2:%s 3:%s", bytes[2], bytes[3]);
    snprintf(model->bit_row[2], sizeof(model->bit_row[2]), "4:%s 5:%s", bytes[4], bytes[5]);
    snprintf(model->bit_row[3], sizeof(model->bit_row[3]), "6:%s 7:%s", bytes[6], bytes[7]);
}

static void dashboard_bittrack_reset_tracking_model(AppDashboardModel* model) {
    if(!model) {
        return;
    }

    memset(model->bittrack_changed, 0, sizeof(model->bittrack_changed));
    memset(model->bittrack_frozen, 0, sizeof(model->bittrack_frozen));

    if(model->bittrack_have_live) {
        for(uint8_t i = 0U; i < DASH_BITTRACK_BYTES; i++) {
            memcpy(model->bittrack_base[i], model->bittrack_live[i], DASH_BITTRACK_BITS + 1U);
        }
        model->bittrack_have_base = true;
    } else {
        for(uint8_t i = 0U; i < DASH_BITTRACK_BYTES; i++) {
            dashboard_bittrack_set_unknown(model->bittrack_base[i]);
        }
        model->bittrack_have_base = false;
    }

    dashboard_bittrack_rebuild_rows(model);
}

static uint8_t dashboard_bittrack_key_bit(InputKey key) {
    switch(key) {
    case InputKeyRight:
        return 1U << 0;
    case InputKeyOk:
        return 1U << 1;
    default:
        return 0U;
    }
}

bool dashboard_bittrack_draw(Canvas* canvas, const AppDashboardModel* dashboard) {
    if(!dashboard || dashboard->mode != AppDashboardBittrack) {
        return false;
    }

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Bit Tracker");

    canvas_set_font(canvas, FontKeyboard);
    canvas_draw_str(canvas, 2, 18, dashboard->bit_row[0]);
    canvas_draw_str(canvas, 2, 29, dashboard->bit_row[1]);
    canvas_draw_str(canvas, 2, 40, dashboard->bit_row[2]);
    canvas_draw_str(canvas, 2, 51, dashboard->bit_row[3]);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, dashboard->bit_id_line);
    return true;
}

void dashboard_bittrack_prepare_template(App* app) {
    with_view_model(
        app->dashboard_view,
        AppDashboardModel * model,
        {
            model->mode = AppDashboardBittrack;
            model->bittrack_have_live = false;
            model->bittrack_have_base = false;
            memset(model->bittrack_changed, 0, sizeof(model->bittrack_changed));
            memset(model->bittrack_frozen, 0, sizeof(model->bittrack_frozen));
            for(uint8_t i = 0U; i < DASH_BITTRACK_BYTES; i++) {
                dashboard_bittrack_set_unknown(model->bittrack_live[i]);
                dashboard_bittrack_set_unknown(model->bittrack_base[i]);
            }
            snprintf(model->bit_row[0], sizeof(model->bit_row[0]), "0:-------- 1:--------");
            snprintf(model->bit_row[1], sizeof(model->bit_row[1]), "2:-------- 3:--------");
            snprintf(model->bit_row[2], sizeof(model->bit_row[2]), "4:-------- 5:--------");
            snprintf(model->bit_row[3], sizeof(model->bit_row[3]), "6:-------- 7:--------");
            strncpy(model->bit_id_line, "ID: --", sizeof(model->bit_id_line) - 1U);
            model->bit_id_line[sizeof(model->bit_id_line) - 1U] = '\0';
        },
        true);
}

bool dashboard_bittrack_input(App* app, const InputEvent* event) {
    if(!app || !event) {
        return false;
    }

    if(
        event->type != InputTypePress && event->type != InputTypeShort &&
        event->type != InputTypeRelease) {
        return false;
    }

    bool consumed = false;
    with_view_model(
        app->dashboard_view,
        AppDashboardModel * model,
        {
            if(model->mode == AppDashboardBittrack) {
                const uint8_t key_bit = dashboard_bittrack_key_bit(event->key);
                if(key_bit != 0U) {
                    if(event->type == InputTypeRelease) {
                        model->input_hold_mask &= (uint8_t)~key_bit;
                        consumed = true;
                    } else {
                        const bool key_was_held = (model->input_hold_mask & key_bit) != 0U;
                        model->input_hold_mask |= key_bit;
                        if(!key_was_held) {
                            if(event->key == InputKeyOk) {
                                for(uint8_t i = 0U; i < DASH_BITTRACK_BYTES; i++) {
                                    for(uint8_t bit = 0U; bit < DASH_BITTRACK_BITS; bit++) {
                                        if(model->bittrack_changed[i][bit]) {
                                            model->bittrack_frozen[i][bit] = true;
                                        }
                                    }
                                }
                                dashboard_bittrack_rebuild_rows(model);
                                consumed = true;
                            } else if(event->key == InputKeyRight) {
                                dashboard_bittrack_reset_tracking_model(model);
                                consumed = true;
                            }
                        }
                    }
                }
            }
        },
        true);

    return consumed;
}

void dashboard_bittrack_update(App* app, const CcEvent* event) {
    if(!event || event->type != CcEventTypeTool || event->data.tool.tool != CcToolBittrack) {
        return;
    }

    const char* text = event->data.tool.text;
    unsigned long frame_id = 0;
    bool parsed_id = false;
    const char* id_pos = strstr(text, "id=0x");
    if(id_pos) {
        char* end = NULL;
        frame_id = strtoul(id_pos + 5, &end, 16);
        parsed_id = (end != (id_pos + 5));
    }

    char bits[DASH_BITTRACK_BYTES][DASH_BITTRACK_BITS + 1U] = {{0}};
    bool bit_valid[DASH_BITTRACK_BYTES] = {false};
    bool any_valid = false;
    for(uint8_t i = 0; i < DASH_BITTRACK_BYTES; i++) {
        dashboard_bittrack_set_unknown(bits[i]);
        char key[5] = {0};
        snprintf(key, sizeof(key), "b%u=", (unsigned)i);
        const char* p = strstr(text, key);
        if(!p) {
            continue;
        }

        p += strlen(key);
        bool valid = true;
        for(uint8_t k = 0; k < DASH_BITTRACK_BITS; k++) {
            const char c = p[k];
            if(c != '0' && c != '1') {
                valid = false;
                break;
            }
            bits[i][k] = c;
        }
        bits[i][DASH_BITTRACK_BITS] = '\0';

        if(!valid) {
            dashboard_bittrack_set_unknown(bits[i]);
        } else {
            bit_valid[i] = true;
            any_valid = true;
        }
    }

    with_view_model(
        app->dashboard_view,
        AppDashboardModel * model,
        {
            model->mode = AppDashboardBittrack;
            strncpy(model->title, "Bit Tracker", sizeof(model->title) - 1U);
            model->title[sizeof(model->title) - 1U] = '\0';

            if(any_valid) {
                model->bittrack_have_live = true;

                if(!model->bittrack_have_base) {
                    for(uint8_t i = 0U; i < DASH_BITTRACK_BYTES; i++) {
                        if(bit_valid[i]) {
                            memcpy(model->bittrack_base[i], bits[i], DASH_BITTRACK_BITS + 1U);
                        } else {
                            dashboard_bittrack_set_unknown(model->bittrack_base[i]);
                        }
                    }
                    model->bittrack_have_base = true;
                }

                for(uint8_t i = 0U; i < DASH_BITTRACK_BYTES; i++) {
                    if(!bit_valid[i]) {
                        continue;
                    }

                    memcpy(model->bittrack_live[i], bits[i], DASH_BITTRACK_BITS + 1U);
                    for(uint8_t bit = 0U; bit < DASH_BITTRACK_BITS; bit++) {
                        const char cur = model->bittrack_live[i][bit];
                        if(cur != '0' && cur != '1') {
                            continue;
                        }

                        char base = model->bittrack_base[i][bit];
                        if(base != '0' && base != '1') {
                            model->bittrack_base[i][bit] = cur;
                            base = cur;
                        }

                        if(!model->bittrack_frozen[i][bit] && cur != base) {
                            model->bittrack_changed[i][bit] = true;
                        }
                    }
                }
            }

            dashboard_bittrack_rebuild_rows(model);

            if(parsed_id) {
                snprintf(model->bit_id_line, sizeof(model->bit_id_line), "ID: 0x%lX", frame_id);
            } else {
                strncpy(model->bit_id_line, "ID: --", sizeof(model->bit_id_line) - 1U);
                model->bit_id_line[sizeof(model->bit_id_line) - 1U] = '\0';
            }
        },
        true);
}
