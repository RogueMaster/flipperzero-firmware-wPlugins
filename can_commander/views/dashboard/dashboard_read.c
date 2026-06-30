#include "dashboard_i.h"

#include <stdio.h>
#include <string.h>

static uint8_t dashboard_input_key_bit(InputKey key) {
    switch(key) {
    case InputKeyLeft:
        return 1U << 0;
    case InputKeyRight:
        return 1U << 1;
    case InputKeyUp:
        return 1U << 2;
    case InputKeyDown:
        return 1U << 3;
    case InputKeyOk:
        return 1U << 4;
    default:
        return 0U;
    }
}

static const AppDashFrameEntry* dashboard_read_get_selected(const AppDashboardModel* model) {
    if(!model || model->read_count == 0U) {
        return NULL;
    }

    uint8_t offset = model->read_selected;
    if(offset >= model->read_count) {
        offset = (uint8_t)(model->read_count - 1U);
    }

    const uint8_t newest =
        (uint8_t)((model->read_head + APP_DASH_READ_HISTORY - 1U) % APP_DASH_READ_HISTORY);
    const uint8_t index = (uint8_t)((newest + APP_DASH_READ_HISTORY - offset) % APP_DASH_READ_HISTORY);
    const AppDashFrameEntry* entry = &model->read_frames[index];
    return entry->valid ? entry : NULL;
}

static const AppDashFrameEntry* dashboard_read_get_by_offset(
    const AppDashboardModel* model,
    uint8_t offset) {
    if(!model || offset >= model->read_count) {
        return NULL;
    }

    const uint8_t newest =
        (uint8_t)((model->read_head + APP_DASH_READ_HISTORY - 1U) % APP_DASH_READ_HISTORY);
    const uint8_t index = (uint8_t)((newest + APP_DASH_READ_HISTORY - offset) % APP_DASH_READ_HISTORY);
    const AppDashFrameEntry* entry = &model->read_frames[index];
    return entry->valid ? entry : NULL;
}

static void dashboard_format_hex_bytes(
    const uint8_t* data,
    uint8_t start,
    uint8_t end,
    uint8_t dlc,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) {
        return;
    }
    out[0] = '\0';

    size_t pos = 0;
    for(uint8_t i = start; i < end && i < 8U; i++) {
        if(i >= dlc) {
            break;
        }
        const int wrote = snprintf(out + pos, out_size - pos, "%s%02X", (pos > 0U) ? " " : "", data[i]);
        if(wrote <= 0) {
            break;
        }
        const size_t step = (size_t)wrote;
        if(step >= (out_size - pos)) {
            break;
        }
        pos += step;
    }
}

static void dashboard_format_frame_header(
    const AppDashFrameEntry* entry,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) {
        return;
    }
    out[0] = '\0';
    if(!entry) {
        return;
    }

    char id_text[12] = {0};
    if(entry->ext) {
        snprintf(id_text, sizeof(id_text), "%08lX", (unsigned long)entry->id);
    } else {
        snprintf(id_text, sizeof(id_text), "%03lX", (unsigned long)(entry->id & 0x7FFU));
    }

    snprintf(
        out,
        out_size,
        "%s %s D%u %s",
        cc_bus_to_string(entry->bus),
        id_text,
        (unsigned)entry->dlc,
        entry->ext ? "EXT" : "STD");
}

static void dashboard_format_frame_preview(
    const AppDashFrameEntry* entry,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) {
        return;
    }
    out[0] = '\0';
    if(!entry) {
        return;
    }

    char id_text[12] = {0};
    if(entry->ext) {
        snprintf(id_text, sizeof(id_text), "%08lX", (unsigned long)entry->id);
    } else {
        snprintf(id_text, sizeof(id_text), "%03lX", (unsigned long)(entry->id & 0x7FFU));
    }

    char data_preview[16] = {0};
    dashboard_format_hex_bytes(entry->data, 0U, 3U, entry->dlc, data_preview, sizeof(data_preview));
    snprintf(
        out,
        out_size,
        "%s %s %s%s",
        cc_bus_to_string(entry->bus),
        id_text,
        data_preview[0] ? data_preview : "--",
        entry->dlc > 3U ? " .." : "");
}

bool dashboard_read_draw(Canvas* canvas, const AppDashboardModel* dashboard) {
    if(
        !dashboard ||
        (dashboard->mode != AppDashboardReadAll && dashboard->mode != AppDashboardFiltered)) {
        return false;
    }

    const bool filtered = (dashboard->mode == AppDashboardFiltered);
    const uint8_t page = (uint8_t)(dashboard->read_page % 3U);
    canvas_set_font(canvas, FontSecondary);

    if(page == 0U) {
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, filtered ? "Filtered" : "Read All");
        if(dashboard->read_overload) {
            char fps_text[24] = {0};
            snprintf(fps_text, sizeof(fps_text), "Rate: %lu fps", (unsigned long)dashboard->read_rate_fps);
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "Input overload");
            canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignCenter, fps_text);
            canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignCenter, "Rendering paused >1000");
            canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "Narrow filter scope");
            return true;
        }

        const AppDashFrameEntry* entry = dashboard_read_get_selected(dashboard);
        if(!entry) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, "Waiting for CAN frames");
            canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "OK Hold  L/R Pg  U/D Nav");
            return true;
        }

        char header[32] = {0};
        char row_a[24] = {0};
        char row_b[24] = {0};
        char ts_line[20] = {0};
        dashboard_format_frame_header(entry, header, sizeof(header));
        dashboard_format_hex_bytes(entry->data, 0U, 4U, entry->dlc, row_a, sizeof(row_a));
        dashboard_format_hex_bytes(entry->data, 4U, 8U, entry->dlc, row_b, sizeof(row_b));
        snprintf(ts_line, sizeof(ts_line), "t=%lums", (unsigned long)entry->ts_ms);

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 13, AlignCenter, AlignTop, header);
        canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignTop, ts_line);

        canvas_set_font(canvas, FontKeyboard);
        canvas_draw_str_aligned(canvas, 64, 34, AlignCenter, AlignTop, row_a[0] ? row_a : "--");
        canvas_draw_str_aligned(canvas, 64, 45, AlignCenter, AlignTop, row_b[0] ? row_b : "--");

        char footer[40] = {0};
        snprintf(
            footer,
            sizeof(footer),
            "F%u/%u %s",
            (unsigned)(dashboard->read_selected + 1U),
            (unsigned)dashboard->read_count,
            dashboard->read_hold ? "HOLD" : "LIVE");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, footer);
    } else if(page == 1U) {
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Recent Frames");
        if(dashboard->read_count == 0U) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, "No frames yet");
            canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "L/R Pg  OK Hold");
            return true;
        }

        uint8_t start = 0U;
        const uint8_t selected = (dashboard->read_selected < dashboard->read_count) ?
                                     dashboard->read_selected :
                                     (uint8_t)(dashboard->read_count - 1U);
        if(selected > 1U) {
            start = (uint8_t)(selected - 1U);
        }

        for(uint8_t line = 0U; line < 4U; line++) {
            const uint8_t offset = (uint8_t)(start + line);
            if(offset >= dashboard->read_count) {
                break;
            }

            const AppDashFrameEntry* entry = dashboard_read_get_by_offset(dashboard, offset);
            if(!entry) {
                continue;
            }

            char preview[32] = {0};
            dashboard_format_frame_preview(entry, preview, sizeof(preview));
            char row[34] = {0};
            snprintf(
                row,
                sizeof(row),
                "%c %s",
                (offset == selected) ? '>' : ' ',
                preview);

            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, 2, (int32_t)(22 + line * 10U), row);
        }

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "U/D Sel  L/R Pg");
    } else {
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Stats");
        char line1[24] = {0};
        char line2[24] = {0};
        char line3[24] = {0};
        char line4[24] = {0};

        snprintf(line1, sizeof(line1), "Total: %lu", (unsigned long)dashboard->read_total);
        snprintf(
            line2,
            sizeof(line2),
            "C0:%lu  C1:%lu",
            (unsigned long)dashboard->read_bus0,
            (unsigned long)dashboard->read_bus1);
        snprintf(
            line3,
            sizeof(line3),
            "STD:%lu EXT:%lu",
            (unsigned long)dashboard->read_std,
            (unsigned long)dashboard->read_ext);
        if(dashboard->read_overload) {
            snprintf(
                line4,
                sizeof(line4),
                "Feed:Paused %lu/s",
                (unsigned long)dashboard->read_rate_fps);
        } else {
            snprintf(line4, sizeof(line4), "Feed:%s", dashboard->read_hold ? "Hold" : "Live");
        }

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 20, line1);
        canvas_draw_str(canvas, 2, 31, line2);
        canvas_draw_str(canvas, 2, 42, line3);
        canvas_draw_str(canvas, 2, 53, line4);
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, "L/R Pg  OK Hold");
    }

    return true;
}

bool dashboard_read_input(App* app, const InputEvent* event) {
    if(!app || !event) {
        return false;
    }

    if(
        event->type != InputTypePress && event->type != InputTypeRepeat &&
        event->type != InputTypeShort &&
        event->type != InputTypeRelease) {
        return false;
    }

    bool consumed = false;
    with_view_model(
        app->dashboard_view,
        AppDashboardModel * model,
        {
            if(model->mode == AppDashboardReadAll || model->mode == AppDashboardFiltered) {
                const uint8_t key_bit = dashboard_input_key_bit(event->key);
                if(event->type == InputTypeRelease) {
                    if(key_bit != 0U) {
                        model->input_hold_mask &= (uint8_t)~key_bit;
                        consumed = true;
                    }
                } else {
                    const bool key_was_held =
                        (key_bit != 0U) && ((model->input_hold_mask & key_bit) != 0U);
                    if(key_bit != 0U) {
                        model->input_hold_mask |= key_bit;
                    }

                    if(event->key == InputKeyOk) {
                        if(!key_was_held) {
                            model->read_hold = !model->read_hold;
                            consumed = true;
                        }
                    } else if(event->key == InputKeyLeft) {
                        if(!key_was_held) {
                            model->read_page = (uint8_t)((model->read_page + 2U) % 3U);
                            consumed = true;
                        }
                    } else if(event->key == InputKeyRight) {
                        if(!key_was_held) {
                            model->read_page = (uint8_t)((model->read_page + 1U) % 3U);
                            consumed = true;
                        }
                    } else if((model->read_page == 0U || model->read_page == 1U) && model->read_count > 0U) {
                        const bool allow_scroll = (!key_was_held || event->type == InputTypeRepeat);
                        if(allow_scroll && event->key == InputKeyUp) {
                            if(model->read_selected > 0U) {
                                model->read_selected--;
                            }
                            consumed = true;
                        } else if(allow_scroll && event->key == InputKeyDown) {
                            if(model->read_selected + 1U < model->read_count) {
                                model->read_selected++;
                            }
                            consumed = true;
                        }
                    }
                }
            }
        },
        true);

    return consumed;
}

void dashboard_read_update(App* app, const CcEvent* event, const char* title) {
    if(!event || event->type != CcEventTypeCanFrame) {
        return;
    }

    with_view_model(
        app->dashboard_view,
        AppDashboardModel * model,
        {
            strncpy(model->title, title, sizeof(model->title) - 1U);
            model->title[sizeof(model->title) - 1U] = '\0';
            model->read_total++;

            if(event->data.can_frame.bus == CcBusCan0) {
                model->read_bus0++;
            } else if(event->data.can_frame.bus == CcBusCan1) {
                model->read_bus1++;
            }

            if(event->data.can_frame.ext) {
                model->read_ext++;
            } else {
                model->read_std++;
            }

            if(model->mode == AppDashboardReadAll || model->mode == AppDashboardFiltered) {
                const uint32_t ts = event->data.can_frame.ts_ms;
                if(model->read_rate_window_start_ms == 0U) {
                    model->read_rate_window_start_ms = ts;
                    model->read_rate_window_count = 0U;
                    model->read_rate_fps = 0U;
                    model->read_overload = false;
                }

                const uint32_t elapsed = ts - model->read_rate_window_start_ms;
                if(elapsed >= 1000U) {
                    const uint32_t sample_ms = (elapsed == 0U) ? 1U : elapsed;
                    model->read_rate_fps =
                        ((uint32_t)model->read_rate_window_count * 1000U) / sample_ms;
                    model->read_rate_window_start_ms = ts;
                    model->read_rate_window_count = 0U;
                    model->read_overload = (model->read_rate_fps > 1000U);
                }

                if(model->read_rate_window_count < 0xFFFFU) {
                    model->read_rate_window_count++;
                }
            } else {
                model->read_overload = false;
                model->read_rate_window_start_ms = 0U;
                model->read_rate_window_count = 0U;
                model->read_rate_fps = 0U;
            }

            if(model->read_hold) {
                return;
            }

            if(
                (model->mode == AppDashboardReadAll || model->mode == AppDashboardFiltered) &&
                model->read_overload) {
                return;
            }

            AppDashFrameEntry* slot = &model->read_frames[model->read_head];
            memset(slot, 0, sizeof(AppDashFrameEntry));
            slot->valid = true;
            slot->ts_ms = event->data.can_frame.ts_ms;
            slot->bus = event->data.can_frame.bus;
            slot->id = event->data.can_frame.id;
            slot->ext = event->data.can_frame.ext;
            slot->dlc = event->data.can_frame.dlc > 8U ? 8U : event->data.can_frame.dlc;
            memcpy(slot->data, event->data.can_frame.data, sizeof(slot->data));

            model->read_head = (uint8_t)((model->read_head + 1U) % APP_DASH_READ_HISTORY);
            if(model->read_count < APP_DASH_READ_HISTORY) {
                model->read_count++;
            }
            model->read_selected = 0U;
        },
        false);
}
