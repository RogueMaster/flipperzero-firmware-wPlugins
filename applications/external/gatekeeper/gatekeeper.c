#include "gatekeeper.h"
#include "gatekeeper_icons.h"

// ════════════════════════════════════════════════════════════════════════════
// Icons (8x8 pixel bitmaps drawn manually)
// ════════════════════════════════════════════════════════════════════════════

#define GK_ICON_COUNT 6

// Draw icon at (x, y) — 8x8 pixels
static void gk_draw_icon(Canvas* canvas, int x, int y, uint8_t icon) {
    const Icon* icons[GK_ICON_COUNT] = {
        &I_card, &I_gamepad, &I_heart, &I_key, &I_lock, &I_pc,
    };
    if(icon < GK_ICON_COUNT) {
        canvas_draw_icon(canvas, x, y, icons[icon]);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Storage
// ════════════════════════════════════════════════════════════════════════════

static void gk_save(GkApp* app) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(s);
    if(storage_file_open(f, GATEKEEPER_STORAGE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, &app->password_len, 1);
        storage_file_write(f, app->password, app->password_len);
        storage_file_write(f, &app->entry_count, 1);
        for(uint8_t i = 0; i < app->entry_count; i++) {
            uint8_t nl = (uint8_t)strlen(app->entries[i].name);
            uint8_t tl = (uint8_t)strlen(app->entries[i].text);
            storage_file_write(f, &nl, 1);
            storage_file_write(f, app->entries[i].name, nl);
            storage_file_write(f, &tl, 1);
            storage_file_write(f, app->entries[i].text, tl);
            storage_file_write(f, &app->entries[i].icon, 1);
        }
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
}

static bool gk_load(GkApp* app) {
    Storage* s = furi_record_open(RECORD_STORAGE);
    File* f = storage_file_alloc(s);
    bool ok = false;
    if(storage_file_open(f, GATEKEEPER_STORAGE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t plen = 0;
        storage_file_read(f, &plen, 1);
        if(plen > 0 && plen <= MAX_PASSWORD_LEN) {
            storage_file_read(f, app->password, plen);
            app->password_len = plen;
        }
        uint8_t ec = 0;
        storage_file_read(f, &ec, 1);
        for(uint8_t i = 0; i < ec && i < MAX_ENTRIES; i++) {
            uint8_t nl = 0, tl = 0;
            storage_file_read(f, &nl, 1);
            if(nl > 0 && nl < MAX_NAME_LEN) {
                storage_file_read(f, app->entries[i].name, nl);
                app->entries[i].name[nl] = '\0';
            }
            storage_file_read(f, &tl, 1);
            if(tl > 0 && tl < MAX_TEXT_LEN) {
                storage_file_read(f, app->entries[i].text, tl);
                app->entries[i].text[tl] = '\0';
            }
            uint8_t ic = 0;
            storage_file_read(f, &ic, 1);
            app->entries[i].icon = (ic < GK_ICON_COUNT) ? ic : 0;
            app->entry_count++;
        }
        ok = true;
    }
    storage_file_close(f);
    storage_file_free(f);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

// ════════════════════════════════════════════════════════════════════════════
// HID typing
// ════════════════════════════════════════════════════════════════════════════

static void gk_type_string(const char* str) {
    for(size_t i = 0; str[i]; i++) {
        uint16_t key = HID_ASCII_TO_KEY(str[i]);
        if(key != HID_KEYBOARD_NONE) {
            furi_hal_hid_kb_press(key);
            furi_delay_ms(30);
            furi_hal_hid_kb_release(key);
            furi_delay_ms(30);
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Draw helpers
// ════════════════════════════════════════════════════════════════════════════

static void gk_draw_circles(Canvas* c, uint8_t filled) {
    const int r = 7, spacing = 22;
    int xs = (128 - (MAX_PASSWORD_LEN * spacing - (spacing - r * 2))) / 2 + r;
    int y  = 34;
    for(uint8_t i = 0; i < MAX_PASSWORD_LEN; i++) {
        int cx = xs + i * spacing;
        canvas_draw_circle(c, cx, y, r);
        if(i < filled) {
            for(int dy = -(r-1); dy <= (r-1); dy++) {
                int half = r*r - dy*dy, h = 0;
                while((h+1)*(h+1) <= half) h++;
                if(h > 0) canvas_draw_line(c, cx-h, y+dy, cx+h, y+dy);
            }
        }
    }
}

// ════════════════════════════════════════════════════════════════════════════
// GkViewModel — stored inside each View's model slot
// view_allocate_model + with_view_model is the correct Flipper API.
// The draw/input callbacks receive `void* model` = pointer to GkViewModel.
// ════════════════════════════════════════════════════════════════════════════

typedef struct {
    GkApp* app;
} GkViewModel;

// ════════════════════════════════════════════════════════════════════════════
// VIEW: Combo (set-password / confirm / unlock)
// ════════════════════════════════════════════════════════════════════════════

static void combo_draw_cb(Canvas* canvas, void* model) {
    GkApp* app = ((GkViewModel*)model)->app;
    if(!app) return;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    if(app->state == STATE_SET_PASSWORD) {
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Set master combo");
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignTop, "Up/Down/Left/Right");
    } else if(app->state == STATE_SET_PASSWORD_CONFIRM) {
        canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Confirm combo");
        canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignTop, "Repeat the same combo");
    } else {
        canvas_draw_str_aligned(canvas, 64, 4, AlignCenter, AlignTop, "Enter combo:");
    }

    gk_draw_circles(canvas, app->input_len);

    if(app->state == STATE_UNLOCK && app->unlock_failed) {
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "Wrong combo!");
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_rframe(canvas, 40, 51, 48, 12, 2);
        // Стрелка влево (Back)
        canvas_draw_line(canvas, 48, 57, 44, 57);
        canvas_draw_line(canvas, 44, 57, 47, 54);
        canvas_draw_line(canvas, 44, 57, 47, 60);
        canvas_draw_str(canvas, 54, 61, "Clear");
    }
}

static bool combo_input_cb(InputEvent* event, void* context) {
    furi_assert(context);
    View* view = context;
    bool consumed = false;

    with_view_model(view, GkViewModel* m, {
        GkApp* app = m->app;
        if(app && event->type == InputTypeShort) {
            if(event->key == InputKeyBack) {
                app->input_len = 0;
                app->unlock_failed = false;
                consumed = true;
            } else {
                uint8_t btn = 0;
                switch(event->key) {
                case InputKeyUp:    btn = GK_BTN_UP;    break;
                case InputKeyDown:  btn = GK_BTN_DOWN;  break;
                case InputKeyLeft:  btn = GK_BTN_LEFT;  break;
                case InputKeyRight: btn = GK_BTN_RIGHT; break;
                default: break;
                }

                if(btn && app->input_len < MAX_PASSWORD_LEN) {
                    app->input_buf[app->input_len++] = btn;
                    consumed = true;

                    if(app->input_len >= MAX_PASSWORD_LEN) {
                        // Full combo entered — handle by state
                        if(app->state == STATE_SET_PASSWORD) {
                            memcpy(app->password_temp, app->input_buf, MAX_PASSWORD_LEN);
                            app->input_len = 0;
                            app->state = STATE_SET_PASSWORD_CONFIRM;

                        } else if(app->state == STATE_SET_PASSWORD_CONFIRM) {
                            if(memcmp(app->input_buf, app->password_temp, MAX_PASSWORD_LEN) == 0) {
                                memcpy(app->password, app->password_temp, MAX_PASSWORD_LEN);
                                app->password_len = MAX_PASSWORD_LEN;
                                gk_save(app);
                                app->input_len = 0;
                                app->state = STATE_MAIN_MENU;
                                app->menu_cursor = -1;
                                app->menu_scroll = 0;
                                view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID_MENU);
                            } else {
                                app->input_len = 0;
                                app->state = STATE_SET_PASSWORD;
                            }

                        } else { // STATE_UNLOCK
                            app->unlock_failed = false;
                            if(memcmp(app->input_buf, app->password, MAX_PASSWORD_LEN) == 0) {
                                app->input_len = 0;
                                app->state = STATE_MAIN_MENU;
                                app->menu_cursor = -1;
                                app->menu_scroll = 0;
                                view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID_MENU);
                            } else {
                                app->unlock_failed = true;
                                app->input_len = 0;
                            }
                        }
                    }
                }
            }
        }
    }, true);

    return consumed;
}

// ════════════════════════════════════════════════════════════════════════════
// VIEW: Main menu
// ════════════════════════════════════════════════════════════════════════════

static void menu_draw_cb(Canvas* canvas, void* model) {
    GkApp* app = ((GkViewModel*)model)->app;
    if(!app) return;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // "+ Add Password" строка
    bool add_sel = (app->menu_cursor == -1);
    if(add_sel) {
        canvas_draw_rbox(canvas, 1, 0, 126, 12, 2);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 6, AlignCenter, AlignCenter, "+ Add Password");
    canvas_set_color(canvas, ColorBlack);

    // Список записей
    int start = app->menu_scroll;
    for(int i = 0; i < 5 && (start + i) < (int)app->entry_count; i++) {
        int idx = start + i;
        int y = 13 + i * 12;
        bool sel = (app->menu_cursor == idx);
        if(sel) {
            canvas_draw_rbox(canvas, 1, y, 126, 12, 2);
            canvas_set_color(canvas, ColorWhite);
        }
        gk_draw_icon(canvas, 4, y + 2, app->entries[idx].icon);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 16, y + 9, app->entries[idx].name);
        canvas_set_color(canvas, ColorBlack);
    }

    // Скроллбар
    if(app->entry_count > 5) {
        int bh = 51 * 5 / (int)app->entry_count;
        int by = 13 + 51 * app->menu_scroll / (int)app->entry_count;
        canvas_draw_line(canvas, 127, 13, 127, 64);
        canvas_draw_box(canvas, 126, by, 2, bh);
    }
}

static bool menu_input_cb(InputEvent* event, void* context) {
    furi_assert(context);
    View* view = context;
    bool consumed = false;

    with_view_model(view, GkViewModel* m, {
        GkApp* app = m->app;
        if(app && event->type == InputTypeShort) {
            consumed = true;
            switch(event->key) {
            case InputKeyUp:
                if(app->menu_cursor > -1) {
                    app->menu_cursor--;
                    if(app->menu_cursor >= 0 && app->menu_cursor < app->menu_scroll)
                        app->menu_scroll--;
                }
                break;
            case InputKeyDown:
                if(app->menu_cursor < (int)app->entry_count - 1) {
                    app->menu_cursor++;
                    if(app->menu_cursor >= app->menu_scroll + 4) app->menu_scroll++;
                }
                break;
            case InputKeyOk:
                if(app->menu_cursor == -1) {
                    if(app->entry_count < MAX_ENTRIES) {
                        memset(app->edit_name, 0, MAX_NAME_LEN);
                        memset(app->edit_text, 0, MAX_TEXT_LEN);
                        text_input_set_header_text(app->name_input, "Entry name");
                        text_input_set_result_callback(
                            app->name_input, on_name_done, app,
                            app->edit_name, MAX_NAME_LEN - 1, true);
                        view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID_NAME_INPUT);
                    }
                } else {
                    app->selected_entry = app->menu_cursor;
                    app->deploy_progress = 0;
                    app->state = STATE_DEPLOY;
                    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID_DEPLOY);
                }
                break;
            case InputKeyBack:
                app->input_len = 0;
                app->unlock_failed = false;
                app->state = STATE_UNLOCK;
                view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID_COMBO);
                break;
            default: 
                consumed = false;
                break;
            }
        }
    }, true);

    return consumed;
}

// ════════════════════════════════════════════════════════════════════════════
// VIEW: Deploy
// ════════════════════════════════════════════════════════════════════════════

static void deploy_draw_cb(Canvas* canvas, void* model) {
    GkApp* app = ((GkViewModel*)model)->app;
    if(!app) return;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    float prog = (app->deploy_progress < 0) ? 0 : app->deploy_progress;
    bool idle   = (app->deploy_progress == 0);
    bool typing = (app->deploy_progress > 0 && app->deploy_progress < 100);
    bool done   = (app->deploy_progress >= 100);

    // ── USB флешка из assets ─────────────────────────────────────────────
    // I_Drive генерируется ufbt из assets/Drive.png (112x35)
    // Центрируем по горизонтали, вертикально в верхней части
    canvas_draw_icon(canvas, 8, 7, &I_Drive);

    // Название пароля поверх флешки — центр тела флешки ~x=50
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 50, 23, AlignCenter, AlignCenter,
        app->entries[app->selected_entry].name);

    // ── Нижняя панель ───────────────────────────────────────────────────

    // Кнопка Delete (левая)
    if(!typing) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_rframe(canvas, 1, 51, 44, 12, 2);
        canvas_draw_str(canvas, 5, 61, "< Delete");
    }

    // Прогресс / статус по центру
    canvas_set_font(canvas, FontSecondary);
    if(done && app->deploy_not_connected) {
        canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignCenter, "No HID host!");
    } else if(done) {
        canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignCenter, "Done!");
    } else {
        char pct[8];
        snprintf(pct, sizeof(pct), "%d%%", (int)prog);
        canvas_draw_str_aligned(canvas, 64, 57, AlignCenter, AlignCenter, pct);
    }

    // Кнопка Deploy (правая) с иконкой кнопки OK
    if(idle) {
        canvas_draw_rframe(canvas, 83, 51, 44, 12, 2);
        canvas_draw_str(canvas, 87, 60, "Deploy");
        // Иконка кнопки OK — кружок с точкой
        canvas_draw_circle(canvas, 120, 57, 4);
        canvas_draw_disc(canvas, 120, 57, 1);
    }
}

static bool deploy_input_cb(InputEvent* event, void* context) {
    furi_assert(context);
    View* view = context;
    bool consumed = false;

    with_view_model(view, GkViewModel* m, {
        GkApp* app = m->app;
        if(app && event->type == InputTypeShort) {
            if(event->key == InputKeyOk && app->deploy_progress == 0) {
                app->deploy_progress = -1.0f;
                app->deploy_not_connected = false;
                view_dispatcher_send_custom_event(app->view_dispatcher, GK_EVENT_START_DEPLOY);
                consumed = true;
            }
            if(event->key == InputKeyLeft && app->deploy_progress == 0) {
                // Request delete confirmation
                app->delete_entry_index = app->selected_entry;
                app->delete_failed = false;
                app->input_len = 0;
                app->state = STATE_DELETE_CONFIRM;
                view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID_DELETE_CONFIRM);
                consumed = true;
            }
            // BACK: разрешаем выход только когда деплой не активен
            if(event->key == InputKeyBack &&
               (app->deploy_progress <= 0 || app->deploy_progress >= 100.0f)) {
                // Дожидаемся завершения потока если он ещё жив
                if(app->deploy_thread) {
                    furi_thread_join(app->deploy_thread);
                    furi_thread_free(app->deploy_thread);
                    app->deploy_thread = NULL;
                }
                app->state = STATE_MAIN_MENU;
                app->deploy_progress = 0;
                app->deploy_not_connected = false;
                view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID_MENU);
                consumed = true;
            }
        }
    }, true);

    return consumed;
}

// ════════════════════════════════════════════════════════════════════════════
// VIEW: Delete Confirmation
// ════════════════════════════════════════════════════════════════════════════

static void delete_confirm_draw_cb(Canvas* canvas, void* model) {
    GkApp* app = ((GkViewModel*)model)->app;
    if(!app) return;
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Delete password?");
    canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignTop, "Enter combo to confirm:");

    gk_draw_circles(canvas, app->input_len);

    if(app->delete_failed) {
        canvas_draw_str_aligned(canvas, 64, 54, AlignCenter, AlignTop, "Wrong combo!");
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_rframe(canvas, 40, 51, 48, 12, 2);
        // Стрелка влево (Back)
        canvas_draw_line(canvas, 48, 57, 44, 57);
        canvas_draw_line(canvas, 44, 57, 47, 54);
        canvas_draw_line(canvas, 44, 57, 47, 60);
        canvas_draw_str(canvas, 51, 61, "Cancel");
    }
}

static bool delete_confirm_input_cb(InputEvent* event, void* context) {
    furi_assert(context);
    View* view = context;
    bool consumed = false;

    with_view_model(view, GkViewModel* m, {
        GkApp* app = m->app;
        if(app && event->type == InputTypeShort) {
            if(event->key == InputKeyBack) {
                app->input_len = 0;
                app->delete_failed = false;
                app->state = STATE_DEPLOY;
                view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID_DEPLOY);
                consumed = true;
            } else {
                uint8_t btn = 0;
                switch(event->key) {
                case InputKeyUp:    btn = GK_BTN_UP;    break;
                case InputKeyDown:  btn = GK_BTN_DOWN;  break;
                case InputKeyLeft:  btn = GK_BTN_LEFT;  break;
                case InputKeyRight: btn = GK_BTN_RIGHT; break;
                default: break;
                }

                if(btn && app->input_len < MAX_PASSWORD_LEN) {
                    app->input_buf[app->input_len++] = btn;
                    consumed = true;

                    if(app->input_len >= MAX_PASSWORD_LEN) {
                        // Check password
                        app->delete_failed = false;
                        if(memcmp(app->input_buf, app->password, MAX_PASSWORD_LEN) == 0) {
                            // Password correct - delete entry
                            int idx = app->delete_entry_index;
                            for(int i = idx; i < (int)app->entry_count - 1; i++) {
                                app->entries[i] = app->entries[i + 1];
                            }
                            app->entry_count--;
                            gk_save(app);
                            
                            app->input_len = 0;
                            app->state = STATE_MAIN_MENU;
                            app->menu_cursor = -1;
                            view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID_MENU);
                        } else {
                            // Wrong password
                            app->delete_failed = true;
                            app->input_len = 0;
                        }
                    }
                }
            }
        }
    }, true);

    return consumed;
}

// ════════════════════════════════════════════════════════════════════════════
// TextInput callbacks
// ════════════════════════════════════════════════════════════════════════════

static void on_name_done(void* ctx) {
    GkApp* app = ctx;
    text_input_set_header_text(app->text_input, "BadUSB text");
    text_input_set_result_callback(
        app->text_input, on_text_done, app,
        app->edit_text, MAX_TEXT_LEN - 1, true);
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID_TEXT_INPUT);
}

static void on_text_done(void* ctx) {
    GkApp* app = ctx;
    // После ввода текста — выбор иконки
    app->icon_cursor = 0;
    app->state = STATE_ICON_PICK;
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID_ICON_PICK);
}

// ════════════════════════════════════════════════════════════════════════════
// VIEW: Icon Picker
// ════════════════════════════════════════════════════════════════════════════

static void icon_pick_draw_cb(Canvas* canvas, void* model) {
    GkApp* app = ((GkViewModel*)model)->app;
    if(!app) return;

    // Белый фон
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, 0, 0, 128, 64);
    canvas_set_color(canvas, ColorBlack);

    // Заголовок
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Select icon");

    // 6 иконок (8x8), шаг 18px, центрирование: total_width = 5*18 = 90, start = (128-90)/2 = 19
    const int iy = 16;
    const int sel_size = 20;
    const int step = 18;
    const int start_cx = (128 - step * (GK_ICON_COUNT - 1)) / 2; // = 19
    const Icon* icons[GK_ICON_COUNT] = {&I_card, &I_gamepad, &I_heart, &I_key, &I_lock, &I_pc};

    for(int i = 0; i < GK_ICON_COUNT; i++) {
        int cx = start_cx + i * step;
        bool sel = (app->icon_cursor == (uint8_t)i);
        if(sel) {
            // Выделение — чёрный квадрат, иконка белая
            canvas_draw_box(canvas, cx - sel_size/2, iy, sel_size, sel_size);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_icon(canvas, cx - 4, iy + 6, icons[i]);
            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_icon(canvas, cx - 4, iy + 6, icons[i]);
        }
    }

    // Название выбранной иконки
    canvas_set_font(canvas, FontPrimary);
    const char* labels[GK_ICON_COUNT] = {"Card","Gamepad","Heart","Key","Lock","PC"};
    canvas_draw_str_aligned(canvas, 64, 39, AlignCenter, AlignTop, labels[app->icon_cursor]);

    // Кнопка Skip (левая)
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_rframe(canvas, 1, 51, 48, 12, 2);
    canvas_draw_str(canvas, 12, 61, "< Skip");

    // Кнопка Select (правая)
    canvas_draw_rframe(canvas, 79, 51, 48, 12, 2);
    canvas_draw_str(canvas, 83, 61, "Select");
    canvas_draw_circle(canvas, 118, 57, 4);
    canvas_draw_disc(canvas, 118, 57, 1);
}

static bool icon_pick_input_cb(InputEvent* event, void* context) {
    furi_assert(context);
    View* view = context;
    bool consumed = false;

    with_view_model(view, GkViewModel* m, {
        GkApp* app = m->app;
        if(app && event->type == InputTypeShort) {
            if(event->key == InputKeyLeft) {
                if(app->icon_cursor > 0) app->icon_cursor--;
                else app->icon_cursor = GK_ICON_COUNT - 1;
                consumed = true;
            } else if(event->key == InputKeyRight) {
                if(app->icon_cursor < GK_ICON_COUNT - 1) app->icon_cursor++;
                else app->icon_cursor = 0;
                consumed = true;
            } else if(event->key == InputKeyOk || event->key == InputKeyBack) {
                // OK — сохранить с выбранной иконкой, BACK — сохранить с иконкой 0
                if(app->entry_count < MAX_ENTRIES) {
                    uint8_t idx = app->entry_count;
                    strlcpy(app->entries[idx].name, app->edit_name, MAX_NAME_LEN);
                    strlcpy(app->entries[idx].text, app->edit_text, MAX_TEXT_LEN);
                    app->entries[idx].icon = (event->key == InputKeyOk) ? app->icon_cursor : 3; // 3 = key
                    app->entry_count++;
                    gk_save(app);
                }
                app->state = STATE_MAIN_MENU;
                app->menu_cursor = -1;
                view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID_MENU);
                consumed = true;
            }        }
    }, true);

    return consumed;
}

// ════════════════════════════════════════════════════════════════════════════
// HID state callback
// ════════════════════════════════════════════════════════════════════════════

static void gk_hid_state_cb(bool connected, void* ctx) {
    if(connected) {
        furi_semaphore_release((FuriSemaphore*)ctx);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// Deploy worker
// ════════════════════════════════════════════════════════════════════════════

static int32_t deploy_worker(void* ctx) {
    GkApp* app = ctx;
    const char* text = app->entries[app->selected_entry].text;

    // ── Шаг 1: переключаем USB в HID, ждём колбэк о подключении ─────────────
    FuriSemaphore* sem = furi_semaphore_alloc(1, 0);
    FuriHalUsbInterface* prev = furi_hal_usb_get_config();

    furi_hal_hid_set_state_callback(gk_hid_state_cb, sem);
    furi_hal_usb_set_config(&usb_hid, NULL);

    // Ждём до 8 секунд (80 × 100мс), показываем прогресс 0→40%
    bool connected = false;
    for(int i = 0; i < 80; i++) {
        app->deploy_progress = (float)i * 40.0f / 80.0f;
        view_dispatcher_send_custom_event(app->view_dispatcher, GK_EVENT_REPAINT);
        if(furi_semaphore_acquire(sem, 100) == FuriStatusOk) {
            connected = true;
            break;
        }
    }

    furi_hal_hid_set_state_callback(NULL, NULL);
    furi_semaphore_free(sem);

    if(!connected) {
        app->deploy_not_connected = true;
        app->deploy_progress = 100.0f;
        view_dispatcher_send_custom_event(app->view_dispatcher, GK_EVENT_REPAINT);
        furi_hal_usb_set_config(prev, NULL);
        return 0;
    }

    // ── Шаг 2: пауза 500мс — хост регистрирует устройство ───────────────────
    app->deploy_progress = 40.0f;
    view_dispatcher_send_custom_event(app->view_dispatcher, GK_EVENT_REPAINT);
    furi_delay_ms(500);

    // ── Шаг 3: вводим текст ──────────────────────────────────────────────────
    app->deploy_progress = 50.0f;
    view_dispatcher_send_custom_event(app->view_dispatcher, GK_EVENT_REPAINT);
    gk_type_string(text);
    furi_hal_hid_kb_release_all();

    // ── Шаг 4: прогресс 50→100% ─────────────────────────────────────────────
    for(int i = 50; i <= 100; i++) {
        app->deploy_progress = (float)i;
        view_dispatcher_send_custom_event(app->view_dispatcher, GK_EVENT_REPAINT);
        furi_delay_ms(10);
    }

    // ── Шаг 5: восстанавливаем USB режим ────────────────────────────────────
    furi_hal_usb_set_config(prev, NULL);

    return 0;
}

// ════════════════════════════════════════════════════════════════════════════
// ViewDispatcher callbacks
// ════════════════════════════════════════════════════════════════════════════

static bool gk_custom_event_cb(void* ctx, uint32_t event) {
    GkApp* app = ctx;
    if(event == GK_EVENT_START_DEPLOY && !app->deploy_thread) {
        app->deploy_thread = furi_thread_alloc_ex(
            "GK_Deploy", 2048, deploy_worker, app);
        furi_thread_start(app->deploy_thread);
    }
    // GK_EVENT_REPAINT: проверяем завершение потока и освобождаем его
    if(event == GK_EVENT_REPAINT && app->deploy_thread) {
        if(app->deploy_progress >= 100.0f) {
            furi_thread_join(app->deploy_thread);
            furi_thread_free(app->deploy_thread);
            app->deploy_thread = NULL;
        }
    }
    return true;
}

static bool gk_navigation_cb(void* ctx) {
    GkApp* app = ctx;
    // Back not consumed by current View
    if(app->state == STATE_UNLOCK ||
       app->state == STATE_SET_PASSWORD ||
       app->state == STATE_SET_PASSWORD_CONFIRM) {
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }
    // TextInput cancelled → back to menu
    app->state = STATE_MAIN_MENU;
    app->menu_cursor = -1;
    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID_MENU);
    return true;
}

// ════════════════════════════════════════════════════════════════════════════
// Helpers
// ════════════════════════════════════════════════════════════════════════════

static View* gk_view_alloc(GkApp* app, ViewDrawCallback draw, ViewInputCallback input) {
    View* v = view_alloc();
    view_allocate_model(v, ViewModelTypeLocking, sizeof(GkViewModel));
    with_view_model(v, GkViewModel * m, { m->app = app; }, true);
    view_set_draw_callback(v, draw);
    view_set_input_callback(v, input);
    view_set_context(v, v);  // Pass the view itself as context
    return v;
}

// ════════════════════════════════════════════════════════════════════════════
// Alloc / Free / Entry point
// ════════════════════════════════════════════════════════════════════════════

static GkApp* gk_alloc() {
    GkApp* app = malloc(sizeof(GkApp));
    memset(app, 0, sizeof(GkApp));

    app->gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, gk_custom_event_cb);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, gk_navigation_cb);
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->view_combo  = gk_view_alloc(app, combo_draw_cb,  combo_input_cb);
    app->view_menu   = gk_view_alloc(app, menu_draw_cb,   menu_input_cb);
    app->view_deploy = gk_view_alloc(app, deploy_draw_cb, deploy_input_cb);
    app->view_delete_confirm = gk_view_alloc(app, delete_confirm_draw_cb, delete_confirm_input_cb);
    app->view_icon_pick = gk_view_alloc(app, icon_pick_draw_cb, icon_pick_input_cb);

    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID_COMBO,  app->view_combo);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID_MENU,   app->view_menu);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID_DEPLOY, app->view_deploy);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID_DELETE_CONFIRM, app->view_delete_confirm);
    view_dispatcher_add_view(app->view_dispatcher, VIEW_ID_ICON_PICK, app->view_icon_pick);

    app->name_input = text_input_alloc();
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, VIEW_ID_NAME_INPUT, text_input_get_view(app->name_input));
    view_dispatcher_add_view(
        app->view_dispatcher, VIEW_ID_TEXT_INPUT, text_input_get_view(app->text_input));

    return app;
}

static void gk_free(GkApp* app) {
    if(app->deploy_thread) {
        furi_thread_join(app->deploy_thread);
        furi_thread_free(app->deploy_thread);
    }
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID_COMBO);
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID_MENU);
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID_DEPLOY);
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID_DELETE_CONFIRM);
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID_ICON_PICK);
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID_NAME_INPUT);
    view_dispatcher_remove_view(app->view_dispatcher, VIEW_ID_TEXT_INPUT);
    view_free(app->view_combo);
    view_free(app->view_menu);
    view_free(app->view_deploy);
    view_free(app->view_delete_confirm);
    view_free(app->view_icon_pick);
    text_input_free(app->name_input);
    text_input_free(app->text_input);
    view_dispatcher_free(app->view_dispatcher);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t gatekeeper_app(void* p) {
    UNUSED(p);
    GkApp* app = gk_alloc();

    bool has_data = gk_load(app);
    app->state = (!has_data || app->password_len == 0) ? STATE_SET_PASSWORD : STATE_UNLOCK;
    app->menu_cursor = -1;
    app->menu_scroll = 0;

    view_dispatcher_switch_to_view(app->view_dispatcher, VIEW_ID_COMBO);
    view_dispatcher_run(app->view_dispatcher);

    gk_free(app);
    return 0;
}
