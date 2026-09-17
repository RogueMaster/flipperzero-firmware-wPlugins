#include "irb_app.h"
#include "ir_builder_icons.h"
#include <stdio.h>
#include <string.h>
const char irb_keys[] = "abcdefghijklmnopqrstuvwxyz0123456789 _-";
static bool view_navigation_available(const IrbApp* app) {
    if(app->project.extra_count) return true;
    for(unsigned key = 0; key < IRB_NAV_KEYS; ++key)
        if(app->library.counts[irb_nav_group[key]] || irb_nav_slot(&app->project, key) >= 0)
            return true;
    return false;
}
static unsigned view_extra_slots(const IrbProject* project, uint8_t* slots) {
    uint8_t all[IRB_MAX_BUTTONS];
    unsigned total = irb_project_slots(project, all, true), count = 0;
    for(unsigned i = 0; i < total; ++i)
        if(all[i] >= IRB_SLOTS) slots[count++] = all[i];
    return count;
}
static const char* position_title(const IrbViewModel* model) {
    return model->slot < IRB_SLOTS ? irb_slot_titles[model->slot]
                                   : irb_project_label(&model->project, model->slot);
}
static void fit(Canvas* canvas, int x, int y, unsigned width, const char* text) {
    char buffer[IRB_PATH_SIZE];
    snprintf(buffer, sizeof(buffer), "%s", text);
    size_t size = strlen(buffer);
    while(size && canvas_string_width(canvas, buffer) > width)
        buffer[--size] = 0;
    canvas_draw_str(canvas, x, y, buffer);
}
static void center(Canvas* canvas, unsigned y, const char* text) {
    canvas_draw_str_aligned(canvas, 32, y, AlignCenter, AlignBottom, text);
}
static void header(Canvas* canvas, const char* text) {
    fit(canvas, 2, 10, 60, text);
    canvas_draw_line(canvas, 0, 14, 63, 14);
}
static unsigned wrap_page(Canvas* canvas, const char* text, unsigned y, unsigned bottom,
                          unsigned skip) {
    unsigned lines = 0;
    while(*text) {
        char line[32];
        size_t n = 0, space = 0;
        while(text[n] && text[n] != '\n' && n < sizeof(line) - 1) {
            line[n] = text[n];
            line[n + 1] = 0;
            if(canvas_string_width(canvas, line) > 60) break;
            if(text[n] == ' ') space = n;
            ++n;
        }
        if(text[n] && text[n] != '\n' && space) n = space;
        if(!n && *text != '\n') n = 1;
        memcpy(line, text, n);
        line[n] = 0;
        if(lines++ >= skip) {
            if(y <= bottom) canvas_draw_str(canvas, 2, y, line);
            y += 10;
        }
        text += n;
        if(*text == '\n')
            ++text;
        else
            while(*text == ' ')
                ++text;
    }
    return lines;
}
static void wrap(Canvas* canvas, const char* text, unsigned y, unsigned bottom) {
    wrap_page(canvas, text, y, bottom, 0);
}
static void row(Canvas* canvas, unsigned y, const char* text, bool selected, unsigned tick) {
    if(selected) {
        canvas_draw_box(canvas, 0, y - 9, 64, 12);
        canvas_set_color(canvas, ColorWhite);
    }
    size_t n = strlen(text);
    unsigned offset =
        selected && n && canvas_string_width(canvas, text) > 60 ? (tick / 3) % (n + 8) : 0;
    if(offset > n) offset = 0;
    fit(canvas, 2, y, 60, text + offset);
    canvas_set_color(canvas, ColorBlack);
}
static void list(Canvas* canvas, const char* const* items, unsigned count, unsigned focus,
                 unsigned y) {
    for(unsigned i = 0; i < count; ++i)
        row(canvas, y + i * 13, items[i], i == focus, 0);
}
static void grid(Canvas* canvas, IrbViewModel* m) {
    center(canvas, 10,
           m->simulate ? (m->play ? "Use SIM" : "Build SIM") : (m->play ? "Use TV" : "Build TV"));
    canvas_draw_icon(canvas, 6, 16, m->focus == 0 ? &I_power_hover_19x20 : &I_power_19x20);
    canvas_draw_icon(canvas, 4, 38, &I_power_text_24x5);
    canvas_draw_icon(canvas, 39, 16, m->focus == 1 ? &I_mute_hover_19x20 : &I_mute_19x20);
    canvas_draw_icon(canvas, 39, 38, &I_mute_text_19x5);
    canvas_draw_icon(canvas, 0, 66, &I_ch_text_31x34);
    canvas_draw_icon(canvas, 35, 66, &I_vol_tv_text_29x34);
    canvas_draw_icon(canvas, 3, 53, m->focus == 2 ? &I_ch_up_hover_24x21 : &I_ch_up_24x21);
    canvas_draw_icon(canvas, 38, 53, m->focus == 3 ? &I_volup_hover_24x21 : &I_volup_24x21);
    canvas_draw_icon(canvas, 3, 91, m->focus == 4 ? &I_ch_down_hover_24x21 : &I_ch_down_24x21);
    canvas_draw_icon(canvas, 38, 91, m->focus == 5 ? &I_voldown_hover_24x21 : &I_voldown_24x21);
    // Mark configured controls in the margins without changing the original icons.
    static const uint8_t x[] = {27, 60, 29, 61, 29, 61};
    static const uint8_t y[] = {16, 16, 54, 54, 91, 91};
    static const uint8_t slots[] = {0, 6, 4, 2, 5, 3};
    for(unsigned i = 0; i < 6; ++i) {
        bool set = irb_project_active(&m->project, slots[i]);
        bool complete = i < 2 ? set && irb_project_active(&m->project, slots[i] + 1) : set;
        if(complete)
            canvas_draw_box(canvas, x[i], y[i], 3, 3);
        else if(set || (i < 2 && irb_project_active(&m->project, slots[i] + 1)))
            canvas_draw_frame(canvas, x[i], y[i], 3, 3);
    }
    char text[24];
    snprintf(text, sizeof(text), "%lu/8 set", irb_base_count(&m->project));
    center(canvas, 51, text);
    if(m->navigation_available) {
        if(m->focus >= 6)
            canvas_draw_box(canvas, m->focus == 6 ? 0 : 38, 116, m->focus == 6 ? 37 : 26, 12);
        canvas_set_color(canvas, m->focus == 6 ? ColorWhite : ColorBlack);
        canvas_draw_str(canvas, 1, 125, m->play ? "Keys" : "Menu");
        canvas_set_color(canvas, m->focus == 7 ? ColorWhite : ColorBlack);
        canvas_draw_str(canvas, 39, 125, "Nav>");
        canvas_set_color(canvas, ColorBlack);
        return;
    }
    snprintf(text, sizeof(text), "%s [%lu]", m->play ? "Buttons" : "Menu",
             irb_project_count(&m->project));
    row(canvas, 125, text, m->focus == 6, 0);
}
static void nav_button(Canvas* canvas, int x, int y, int width, int height, bool selected) {
    if(selected) {
        canvas_draw_rbox(canvas, x, y, width, height, 3);
        canvas_set_color(canvas, ColorWhite);
    } else {
        canvas_draw_rframe(canvas, x, y, width, height, 3);
        canvas_set_color(canvas, ColorBlack);
    }
}
static void nav_mark(Canvas* canvas, int x, int y, bool configured) {
    if(configured) canvas_draw_disc(canvas, x, y, 1);
}
static void nav_chevron(Canvas* canvas, int x, int y, unsigned key, bool available) {
    if(!available) {
        canvas_draw_line(canvas, x - 2, y, x + 2, y);
    } else if(key == 0) {
        canvas_draw_line(canvas, x - 4, y + 2, x, y - 2);
        canvas_draw_line(canvas, x, y - 2, x + 4, y + 2);
    } else if(key == 1) {
        canvas_draw_line(canvas, x + 2, y - 4, x - 2, y);
        canvas_draw_line(canvas, x - 2, y, x + 2, y + 4);
    } else if(key == 3) {
        canvas_draw_line(canvas, x - 2, y - 4, x + 2, y);
        canvas_draw_line(canvas, x + 2, y, x - 2, y + 4);
    } else {
        canvas_draw_line(canvas, x - 4, y - 2, x, y + 2);
        canvas_draw_line(canvas, x, y + 2, x + 4, y - 2);
    }
}
static void nav_aux_icon(Canvas* canvas, unsigned key, bool available) {
    if(!available) {
        canvas_draw_line(canvas,
                         key == 5   ? 7
                         : key == 6 ? 28
                                    : 49,
                         92,
                         key == 5   ? 13
                         : key == 6 ? 34
                                    : 55,
                         92);
    } else if(key == 5) {
        canvas_draw_line(canvas, 6, 87, 15, 87);
        canvas_draw_line(canvas, 6, 91, 15, 91);
        canvas_draw_line(canvas, 6, 95, 15, 95);
    } else if(key == 6) {
        canvas_draw_line(canvas, 28, 87, 24, 91);
        canvas_draw_line(canvas, 24, 91, 28, 95);
        canvas_draw_line(canvas, 24, 91, 34, 91);
        canvas_draw_line(canvas, 34, 91, 36, 93);
        canvas_draw_line(canvas, 36, 93, 36, 96);
    } else {
        canvas_draw_frame(canvas, 47, 86, 9, 11);
        canvas_draw_line(canvas, 51, 91, 60, 91);
        canvas_draw_line(canvas, 57, 88, 60, 91);
        canvas_draw_line(canvas, 60, 91, 57, 94);
    }
}
static void navigation(Canvas* canvas, IrbViewModel* m) {
    header(canvas, m->simulate ? "Nav: IR OFF" : (m->play ? "Use nav" : "Build nav"));
    static const uint8_t x[] = {20, 3, 22, 42, 20, 1, 22, 43};
    static const uint8_t y[] = {17, 37, 38, 37, 60, 82, 82, 82};
    static const uint8_t width[] = {24, 19, 20, 19, 24, 20, 20, 20};
    static const uint8_t height[] = {19, 22, 20, 22, 18, 18, 18, 18};
    for(unsigned i = 0; i < IRB_NAV_KEYS; ++i) {
        bool configured = irb_nav_slot(&m->project, i) >= 0;
        bool available = configured || (!m->play && m->counts[irb_nav_group[i]]);
        nav_button(canvas, x[i], y[i], width[i], height[i], m->focus == i);
        if(i < 5 && i != 2)
            nav_chevron(canvas, x[i] + width[i] / 2, y[i] + height[i] / 2, i, available);
        else if(i == 2)
            canvas_draw_str_aligned(canvas, 32, 52, AlignCenter, AlignBottom,
                                    available ? "OK" : "-");
        else
            nav_aux_icon(canvas, i, available);
        nav_mark(canvas, x[i] + width[i] - 4, y[i] + 4, configured);
        canvas_set_color(canvas, ColorBlack);
    }
    char status[28];
    if(m->focus < IRB_NAV_KEYS) {
        uint32_t selected = m->project.nav_positions[m->focus];
        uint32_t total = m->counts[irb_nav_group[m->focus]];
        if(!m->play && selected && total)
            snprintf(status, sizeof(status), "%s %lu/%lu", irb_nav_labels[m->focus],
                     (unsigned long)selected, (unsigned long)total);
        else if(!m->play && irb_project_imported(&m->project, IRB_NAV_SLOT_BASE + m->focus))
            snprintf(status, sizeof(status), "%s imported", irb_nav_labels[m->focus]);
        else
            snprintf(status, sizeof(status), "%s", irb_nav_labels[m->focus]);
        center(canvas, 111, status);
    } else
        center(canvas, 111, "Other buttons");
    snprintf(status, sizeof(status), "Other [%lu] >", (unsigned long)m->project.extra_count);
    row(canvas, 126, status, m->focus == IRB_NAV_KEYS, 0);
}
void irb_draw(Canvas* canvas, void* context) {
    IrbViewModel* m = context;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);
    if(m->busy && m->screen != Scan && (!m->send_job || m->checking)) {
        header(canvas, "IR Builder");
        wrap(canvas, m->status, 35, 80);
        canvas_draw_frame(canvas, 3, 88, 58, 8);
        if(m->progress) canvas_draw_box(canvas, 5, 90, m->progress * 54 / 100, 4);
        center(canvas, 110, m->committing ? "Finishing..." : "Working...");
        if(m->cancelable) center(canvas, 125, "Back: cancel");
        return;
    }
    char buffer[48];
    switch(m->screen) {
    case Home: {
        header(canvas, "IR Builder");
        const char* items[] = {"New remote", "Open .ir", "Continue", "Saved", "Settings", "Help"};
        list(canvas, items, 6, m->focus, 28);
        center(canvas, 108, "Version " IRB_VERSION);
        center(canvas, 121, m->simulate ? "SIM: IR OFF" : "Create & save");
        break;
    }
    case Settings: {
        header(canvas, "Settings");
        const char* items[] = {"Change .ir", "Reset builder"};
        list(canvas, items, 2, m->focus, 31);
        const char* name = strrchr(m->default_library, '/');
        canvas_draw_str(canvas, 2, 67, "TV library:");
        wrap(canvas, name ? name + 1 : m->default_library, 80, 100);
        center(canvas, 125, "For new TVs");
        break;
    }
    case Grid:
        grid(canvas, m);
        break;
    case Navigation:
        navigation(canvas, m);
        break;
    case Pair: {
        header(canvas, m->pair ? "Mute" : "Power");
        const char* items[] = {m->pair ? "Mute" : "On", m->pair ? "Unmute" : "Off"};
        list(canvas, items, 2, m->focus, 36);
        wrap(canvas, m->play ? "OK: send once" : "Select a function to find its signal.", 76, 120);
        break;
    }
    case Position: {
        header(canvas, position_title(m));
        int group = irb_slot_group_index(m->slot);
        snprintf(buffer, sizeof(buffer), "%lu/%lu", (unsigned long)m->position,
                 (unsigned long)(group >= 0 ? m->counts[group] : 0));
        center(canvas, 35, buffer);
        center(canvas, 49, "<  position  >");
        const char* items[] = {"Use number", "Send once", "Auto scan", "Skip"};
        list(canvas, items, 4, m->action, 67);
        center(canvas, 125, "Hold L/R: fast");
        break;
    }
    case ProjectMenu: {
        header(canvas, "Build remote");
        const char* items[] = {"Buttons", "Import .ir", "Name & save", "Help"};
        list(canvas, items, 4, m->focus, 33);
        snprintf(buffer, sizeof(buffer), "%lu/64 keys", irb_project_count(&m->project));
        center(canvas, 109, buffer);
        break;
    }
    case Buttons:
    case Others:
    case Browser:
    case Import:
        header(canvas, m->screen == Buttons  ? "Buttons"
                       : m->screen == Others ? "Other buttons"
                       : m->screen == Import ? "Import keys"
                                             : "Choose file");
        if(m->screen == Browser) {
            const char* path = m->folder;
            while(*path && canvas_string_width(canvas, path) > 60)
                ++path;
            fit(canvas, 2, 25, 60, path);
        }
        for(unsigned i = 0; i < IRB_PAGE_SIZE && m->list_start + i < m->list_count; ++i) {
            char name[IRB_PATH_SIZE + 3];
            snprintf(name, sizeof(name), "%s%s",
                     m->screen == Browser && m->row_directories[i] ? "/ " : "", m->rows[i]);
            row(canvas, m->screen == Browser ? 40 + i * 12 : 30 + i * 13, name,
                m->focus == m->list_start + i, m->tick);
        }
        snprintf(buffer, sizeof(buffer), "%u/%u", m->list_count ? m->focus + 1 : 0, m->list_count);
        center(canvas, 115, buffer);
        if(!m->list_count) center(canvas, 44, "No entries");
        center(canvas, 127,
               m->screen == Browser                                       ? "Hold OK: path"
               : m->play && (m->screen == Buttons || m->screen == Others) ? "OK: send"
                                                                          : "OK: select");
        break;
    case ButtonMenu: {
        header(canvas, irb_project_label(&m->project, m->slot));
        if(irb_slot_is_nav(m->slot)) {
            const char* items[] = {"Send once", "Change code", "Remove"};
            list(canvas, items, 3, m->focus, 31);
        } else {
            const char* items[] = {"Send once", "Change code", "Rename",
                                   "Move up",   "Move down",   "Remove"};
            list(canvas, items, 6, m->focus, 31);
        }
        break;
    }
    case SavedMenu: {
        header(canvas, m->project.name);
        const char* items[] = {"Use remote", "Edit", "Rename", "Duplicate", "Delete"};
        list(canvas, items, 5, m->focus, 32);
        break;
    }
    case Keyboard: {
        header(canvas, "Name");
        const char* tail = m->text;
        while(*tail && canvas_string_width(canvas, tail) > 60)
            ++tail;
        fit(canvas, 2, 30, 60, tail);
        for(unsigned i = 0; i < 42; ++i) {
            char key[2] = {i < 39 ? irb_keys[i] : i == 39 ? '^' : i == 40 ? '<' : '*', 0};
            if(m->upper && i < 26) key[0] -= 'a' - 'A';
            if(i == 36) key[0] = '_';
            unsigned x = 1 + i % 7 * 9, y = 45 + i / 7 * 11;
            if(m->focus == i) {
                canvas_draw_box(canvas, x, y - 8, 8, 11);
                canvas_set_color(canvas, ColorWhite);
            }
            canvas_draw_str(canvas, x + 1, y, key);
            canvas_set_color(canvas, ColorBlack);
        }
        center(canvas, 115, "^ case < del");
        center(canvas, 127, "Hold OK: save");
        break;
    }
    case Message:
        header(canvas, "IR Builder");
        m->message_pages = (wrap_page(canvas, m->message, 29, 99, m->focus * 8) + 7) / 8;
        if(m->message_pages > 1) {
            snprintf(buffer, sizeof(buffer), "< %u/%u >", m->focus + 1, m->message_pages);
            center(canvas, 113, buffer);
        }
        center(canvas, 125, "OK: continue");
        break;
    case Choice:
        header(canvas, "IR Builder");
        wrap(canvas, m->message, 29, 92);
        row(canvas, 109, "Cancel", !m->focus, 0);
        row(canvas, 123, "Confirm", m->focus, 0);
        break;
    case Help: {
        const char* pages[] = {
            "Choose a TV function. Enter its working number, or use Auto scan. Positions start at "
            "1.",
            "Auto scan: OK pauses. L/R choose a nearby code. Replay tests it. Use saves its "
            "number.",
            "Open .ir maps common keys to the controller. Unmatched signals appear after Nav in "
            "Other buttons.",
            "Name & save exports to Infrared / ir_builder. Continue restores your latest draft.",
            "Saved: use, edit, rename, duplicate or delete. Delete can keep the .ir export. Back "
            "returns."};
        header(canvas, "Help");
        wrap(canvas, pages[m->help], 29, 109);
        snprintf(buffer, sizeof(buffer), "< %u/5 >", m->help + 1);
        center(canvas, 125, buffer);
        break;
    }
    case Scan:
        header(canvas, m->simulate ? "Scan: IR OFF" : "Auto scan");
        wrap(canvas, position_title(m), 28, 40);
        snprintf(buffer, sizeof(buffer), "%lu/%lu", (unsigned long)m->scan.current,
                 (unsigned long)m->scan.total);
        canvas_set_font(canvas, FontPrimary);
        center(canvas, 53, buffer);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_frame(canvas, 3, 60, 58, 7);
        unsigned percent = m->checking     ? m->progress
                           : m->scan.total ? (uint64_t)m->scan.last_sent * 100 / m->scan.total
                                           : 0;
        if(percent) canvas_draw_box(canvas, 5, 62, percent * 54 / 100, 3);
        if(m->pausing)
            center(canvas, 87, "Pausing...");
        else if(m->checking)
            wrap(canvas, "Checking library...", 85, 108);
        else if(m->scan.paused) {
            center(canvas, 79, m->scan.finished ? "Finished" : "< paused >");
            const char* items[] = {"Use number", "Replay", m->scan.finished ? "Restart" : "Resume"};
            list(canvas, items, 3, m->action, 89);
        } else {
            center(canvas, 87, "Scanning...");
            center(canvas, 101, "OK: pause");
        }
        center(canvas, 127, "Back: stop");
        break;
    case ScreenCount:
        break;
    }
    if(m->send_job) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, 0, 116, 64, 12);
        canvas_set_color(canvas, ColorBlack);
        snprintf(buffer, sizeof(buffer), m->simulate ? "SIM sent %lu" : "Sent %lu",
                 (unsigned long)m->send_count);
        center(canvas, 126, buffer);
    }
}
void irb_refresh(IrbApp* app) {
    with_view_model(
        app->view, IrbViewModel * m,
        {
            m->project = app->project;
            memcpy(m->counts, app->library.counts, sizeof(m->counts));
            m->screen = app->screen;
            m->focus = app->focus;
            m->slot = app->slot;
            m->position = app->position;
            m->action = app->action;
            m->pair = app->pair;
            m->play = app->play;
            m->simulate = app->simulate;
            m->navigation_available = view_navigation_available(app);
            m->busy = app->worker != NULL;
            m->send_job = app->job && app->job->type == JobSend;
            m->tick = app->tick;
            m->help = app->help;
            m->upper = app->upper;
            snprintf(m->default_library, sizeof(m->default_library), "%s", app->default_library);
            snprintf(m->text, sizeof(m->text), "%s", app->text);
            snprintf(m->message, sizeof(m->message), "%s", app->message);
            m->list_count = m->list_start = 0;
            if(app->screen == Grid && !m->navigation_available && app->focus == 7)
                m->focus = app->focus = 6;
            if(app->screen == Browser) {
                snprintf(m->folder, sizeof(m->folder), "%s", app->page.path);
                m->list_count = app->page.total;
                m->list_start = app->page.start;
                for(unsigned i = 0; i < app->page.count; ++i) {
                    snprintf(m->rows[i], IRB_PATH_SIZE, "%s", app->page.names[i]);
                    m->row_directories[i] = app->page.directories[i];
                }
            } else if(app->screen == Buttons || app->screen == Others || app->screen == Import) {
                uint8_t slots[IRB_MAX_BUTTONS];
                m->list_count = app->screen == Buttons
                                    ? irb_project_slots(&app->project, slots, app->play)
                                : app->screen == Others ? view_extra_slots(&app->project, slots)
                                : app->catalog          ? app->catalog->count + 1
                                                        : 0;
                if(app->focus >= m->list_count) app->focus = m->list_count ? m->list_count - 1 : 0;
                m->focus = app->focus;
                m->list_start = app->focus / IRB_PAGE_SIZE * IRB_PAGE_SIZE;
                for(unsigned i = 0; i < IRB_PAGE_SIZE && m->list_start + i < m->list_count; ++i) {
                    unsigned index = m->list_start + i;
                    const char* name = app->screen == Buttons || app->screen == Others
                                           ? irb_project_label(&app->project, slots[index])
                                       : index ? app->catalog->entries[index - 1].name
                                               : "Add all new";
                    snprintf(m->rows[i], IRB_PATH_SIZE, "%s", name);
                }
            }
        },
        true);
}
