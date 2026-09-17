#include "irb_app.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void start_job(IrbApp* app, IrbJob* job, Screen next);
static void persist(IrbApp* app, Screen next);
static unsigned position_index(unsigned slot) {
    return irb_slot_is_nav(slot) ? IRB_SLOTS + irb_nav_key_from_slot(slot) : slot;
}
static bool navigation_available(const IrbApp* app) {
    if(app->project.extra_count) return true;
    for(unsigned key = 0; key < IRB_NAV_KEYS; ++key)
        if(app->library.counts[irb_nav_group[key]] || irb_nav_slot(&app->project, key) >= 0)
            return true;
    return false;
}
static bool remembers(Screen screen) {
    return screen == Home || screen == Grid || screen == Pair || screen == ProjectMenu ||
           screen == Buttons || screen == Settings || screen == Navigation || screen == Browser ||
           screen == Import || screen == Others;
}
static unsigned extra_slots(const IrbProject* project, uint8_t* slots) {
    uint8_t all[IRB_MAX_BUTTONS];
    unsigned total = irb_project_slots(project, all, true), count = 0;
    for(unsigned i = 0; i < total; ++i)
        if(all[i] >= IRB_SLOTS) slots[count++] = all[i];
    return count;
}
static void show(IrbApp* app, const char* text, Screen next) {
    if(remembers(app->screen)) app->focus_memory[app->screen] = app->focus;
    snprintf(app->message, sizeof(app->message), "%s", text);
    app->message_return = next;
    app->screen = Message;
    app->focus = 0;
}
static void choice(IrbApp* app, ChoicePurpose purpose, const char* text, Screen next) {
    if(remembers(app->screen)) app->focus_memory[app->screen] = app->focus;
    app->choice = purpose;
    app->choice_return = next;
    snprintf(app->message, sizeof(app->message), "%s", text);
    app->screen = Choice;
    app->focus = 0;
}
static void go(IrbApp* app, Screen screen) {
    if(remembers(app->screen)) app->focus_memory[app->screen] = app->focus;
    if(app->screen == Position && position_index(app->slot) < IRB_POSITION_SLOTS)
        app->position_actions[position_index(app->slot)] = app->action;
    app->screen = screen;
    app->focus = remembers(screen) ? app->focus_memory[screen] : 0;
    app->tick = 0;
}
static IrbJob* new_job(IrbApp* app, JobType type) {
    IrbJob* job = calloc(1, sizeof(*job));
    furi_check(job);
    job->app = app;
    job->type = type;
    job->project = app->project;
    job->slot = app->slot;
    job->position = app->position;
    job->last_percent = UINT32_MAX;
    atomic_init(&job->cancel, false);
    atomic_init(&job->committing, false);
    atomic_init(&job->held, false);
    atomic_init(&job->command, IrbScanNone);
    return job;
}
static void start_job(IrbApp* app, IrbJob* job, Screen next) {
    furi_check(!app->worker);
    app->job = job;
    app->job_return = next;
    app->worker = furi_thread_alloc_ex("IrbWorker", 6144, irb_work, job);
    with_view_model(
        app->view, IrbViewModel * m,
        {
            m->busy = true;
            m->cancelable = true;
            m->committing = false;
            m->progress = 0;
            m->pausing = false;
            m->checking = job->type == JobScan;
            m->send_count = 0;
            memset(&m->scan, 0, sizeof(m->scan));
            snprintf(m->status, sizeof(m->status), "Preparing...");
        },
        false);
    irb_refresh(app);
    furi_thread_start(app->worker);
}
static void finish_job(IrbApp* app) {
    furi_thread_join(app->worker);
    furi_thread_free(app->worker);
    app->worker = NULL;
    IrbJob* job = app->job;
    app->job = NULL;
    app->screen = app->job_return;
    bool cancelled = atomic_load(&job->cancel) && !atomic_load(&job->committing);
    if(job->type == JobSave && job->conflict) {
        choice(app, ConfirmReplace, "Name exists. Replace its project and .ir export?",
               app->keyboard_return);
    } else if(job->type == JobScan) {
        app->position = job->scan.current ? job->scan.current : app->position;
        app->action = app->position_actions[position_index(app->slot)];
        if(job->ok && job->scan.selected && !cancelled) {
            irb_project_set_position(&app->project, app->slot, app->position);
            go(app, app->return_screen);
            persist(app, app->return_screen);
        } else if(!job->ok && !cancelled)
            show(app, job->error, Position);
    } else if(job->ok) {
        switch(job->type) {
        case JobLoad:
            irb_cache_clear(&app->signals);
            app->focus_memory[Grid] = app->focus_memory[Navigation] = 0;
            memset(app->position_actions, 0, sizeof(app->position_actions));
            irb_library_clear(&app->library);
            app->library = job->library;
            memset(&job->library, 0, sizeof(job->library));
            app->project = job->project;
            app->loaded_saved = job->saved;
            app->draft_current = !job->saved;
            app->dirty = false;
            app->play = false;
            if(job->saved)
                snprintf(app->loaded_name, sizeof(app->loaded_name), "%s", app->project.name);
            else
                app->draft = true;
            go(app, job->saved ? SavedMenu : Grid);
            break;
        case JobOpen: {
            irb_cache_clear(&app->signals);
            app->focus_memory[Grid] = app->focus_memory[Navigation] = app->focus_memory[Others] = 0;
            memset(app->position_actions, 0, sizeof(app->position_actions));
            irb_library_clear(&app->library);
            app->library = job->library;
            memset(&job->library, 0, sizeof(job->library));
            app->project = job->project;
            app->loaded_saved = false;
            app->loaded_name[0] = 0;
            app->draft = app->draft_current = true;
            app->dirty = app->play = false;
            go(app, Grid);
            char text[128];
            snprintf(text, sizeof(text),
                     "Opened %lu keys: %lu mapped to the controller, %lu in Other keys.",
                     (unsigned long)job->added, (unsigned long)job->mapped,
                     (unsigned long)(job->added - job->mapped));
            show(app, text, Grid);
            break;
        }
        case JobDraft:
            app->dirty = false;
            app->draft = app->draft_current = true;
            break;
        case JobSave:
            app->project = job->project;
            app->loaded_saved = true;
            app->dirty = false;
            snprintf(app->loaded_name, sizeof(app->loaded_name), "%s", app->project.name);
            show(app,
                 job->error[0] ? job->error
                               : "Saved to Infrared / ir_builder. Use it here or in Infrared.",
                 SavedMenu);
            break;
        case JobBrowse:
            irb_files_clear(&app->files);
            app->files = job->files;
            memset(&job->files, 0, sizeof(job->files));
            app->page = job->page;
            go(app, Browser);
            app->focus = job->position >= app->page.start &&
                                 job->position < app->page.start + app->page.count
                             ? job->position
                             : app->page.start;
            break;
        case JobCatalog:
            free(app->catalog);
            app->catalog = job->catalog;
            job->catalog = NULL;
            go(app, Import);
            break;
        case JobImport: {
            app->project = job->project;
            app->dirty = false;
            app->draft = app->draft_current = true;
            char text[100];
            snprintf(text, sizeof(text), "Added %lu keys: %lu mapped, %lu in Other keys.",
                     (unsigned long)job->added, (unsigned long)job->mapped,
                     (unsigned long)(job->added - job->mapped));
            show(app, text, Import);
            break;
        }
        case JobDelete:
            go(app, Home);
            show(app,
                 job->both ? "Project and export deleted."
                           : "Project deleted. Your .ir export is kept.",
                 Home);
            break;
        case JobSettings:
            snprintf(app->default_library, sizeof(app->default_library), "%s", job->path);
            go(app, Settings);
            break;
        default:
            break;
        }
    } else if(!cancelled && job->type == JobLoad && !job->restore) {
        char error[IRB_ERROR_SIZE];
        snprintf(error, sizeof(error), "%.110s Change the library in Settings.", job->error);
        show(app, error, Home);
    } else if(!cancelled)
        show(app, job->error[0] ? job->error : "Operation failed. Check the SD card.",
             app->job_return);
    if(cancelled && job->type == JobDraft) app->dirty = true;
    if(!job->ok && job->type == JobBrowse) app->focus = app->page.start;
    irb_library_clear(&job->library);
    irb_files_clear(&job->files);
    free(job->catalog);
    free(job);
}
static void persist(IrbApp* app, Screen next) {
    app->dirty = true;
    start_job(app, new_job(app, JobDraft), next);
}
static void load(IrbApp* app, const char* path, bool restore, bool saved) {
    IrbJob* job = new_job(app, JobLoad);
    snprintf(job->path, sizeof(job->path), "%s", path);
    job->restore = restore;
    job->saved = saved;
    start_job(app, job, app->screen);
}
static void open_remote(IrbApp* app, const char* path) {
    IrbJob* job = new_job(app, JobOpen);
    irb_project_init(&job->project);
    snprintf(job->project.library, sizeof(job->project.library), "%s", app->default_library);
    snprintf(job->path, sizeof(job->path), "%s", path);
    start_job(app, job, app->screen);
}
static void set_library(IrbApp* app, const char* path) {
    IrbJob* job = new_job(app, JobSettings);
    snprintf(job->path, sizeof(job->path), "%s", path);
    start_job(app, job, app->screen);
}
static void send_ex(IrbApp* app, bool selected_position, bool repeatable) {
    IrbJob* job = new_job(app, JobSend);
    if(selected_position) irb_project_set_position(&job->project, app->slot, app->position);
    if(!irb_project_active(&job->project, app->slot)) {
        free(job);
        show(app, "No code assigned to this button.", app->screen);
        return;
    }
    job->repeatable = repeatable;
    atomic_store(&job->held, repeatable);
    start_job(app, job, app->screen);
}
static void send(IrbApp* app, bool selected_position) {
    send_ex(app, selected_position, false);
}
static void browse(IrbApp* app, BrowsePurpose purpose, const char* path, unsigned start) {
    if(app->screen == Browser && app->files.entries &&
       app->files.projects == (purpose == BrowseSaved) && irb_path_equal(path, app->files.path)) {
        irb_files_get_page(&app->files, start, &app->page);
        return;
    }
    irb_files_clear(&app->files); // Bound peak memory when entering another folder.
    IrbJob* job = new_job(app, JobBrowse);
    job->page.projects = purpose == BrowseSaved;
    snprintf(job->page.path, sizeof(job->page.path), "%s", path);
    job->page.start = start;
    job->position =
        app->screen == Browser && irb_path_equal(path, app->page.path) ? app->focus : start;
    app->browse_purpose = purpose;
    start_job(app, job, app->screen);
}
static void position(IrbApp* app, unsigned slot, Screen next) {
    int group = irb_slot_group_index(slot);
    if(group < 0) {
        show(app, "To change an imported code, remove it and import another key.", ButtonMenu);
        return;
    }
    app->slot = slot;
    if(!app->library.counts[group]) {
        show(app, "No signals for this function in this library.", next);
        return;
    }
    app->position = irb_project_position(&app->project, slot);
    if(!app->position) app->position = 1;
    app->return_screen = next;
    app->action = app->position_actions[position_index(slot)];
    go(app, Position);
}
static void keyboard(IrbApp* app, TextPurpose purpose, const char* initial) {
    app->text_purpose = purpose;
    app->keyboard_return = app->screen;
    snprintf(app->text, sizeof(app->text), "%s", initial);
    app->upper = false;
    go(app, Keyboard);
}
static void save(IrbApp* app, bool replace) {
    IrbJob* job = new_job(app, JobSave);
    snprintf(job->project.name, sizeof(job->project.name), "%s", app->text);
    snprintf(job->old_name, sizeof(job->old_name), "%s", app->loaded_name);
    job->rename = app->text_purpose == TextRename && app->loaded_saved;
    job->replace = replace;
    start_job(app, job, app->keyboard_return);
}
static void text_done(IrbApp* app) {
    if(!irb_name_valid(app->text, app->text_purpose != TextLabel)) {
        show(app, "Use 1-31 printable characters. File names cannot contain path characters.",
             Keyboard);
        return;
    }
    if(app->text_purpose == TextLabel) {
        if(!irb_project_label_available(&app->project, app->text, app->slot)) {
            show(app, "Button name already exists.", Keyboard);
            return;
        }
        char* label = app->slot < IRB_SLOTS ? app->project.labels[app->slot]
                                            : app->project.extras[app->slot - IRB_SLOTS].label;
        snprintf(label, IRB_NAME_SIZE, "%s", app->text);
        go(app, ButtonMenu);
        persist(app, ButtonMenu);
    } else
        save(app, false);
}
static void confirmed(IrbApp* app) {
    switch(app->choice) {
    case ConfirmNew:
        go(app, Home);
        load(app, app->default_library, false, false);
        break;
    case ConfirmOpen:
        go(app, Home);
        open_remote(app, app->pending_path);
        break;
    case ConfirmEdit:
        app->play = false;
        go(app, Grid);
        persist(app, Grid);
        break;
    case ConfirmReplace:
        save(app, true);
        break;
    case ConfirmDelete: {
        IrbJob* job = new_job(app, JobDelete);
        job->both = app->delete_both;
        start_job(app, job, SavedMenu);
        break;
    }
    case ConfirmLeave:
        go(app, Home);
        break;
    }
}
static void move(IrbApp* app, InputKey key, unsigned count) {
    if(!count) {
        app->focus = 0;
        return;
    }
    if(key == InputKeyUp) app->focus = app->focus ? app->focus - 1 : count - 1;
    if(key == InputKeyDown) app->focus = (app->focus + 1) % count;
}
static void scan_input(IrbApp* app, InputKey key, InputType type) {
    if(key == InputKeyBack && type == InputTypePress) {
        app->consume_back = true;
        atomic_store(&app->job->cancel, true);
        return;
    }
    bool paused = false;
    with_view_model(
        app->view, IrbViewModel * m, { paused = m->scan.paused && !m->sending && !m->checking; },
        false);
    IrbScanCommand command = IrbScanNone;
    if(!paused) {
        if(key == InputKeyOk && type == InputTypePress) {
            app->consume_ok = true;
            command = IrbScanPause;
            app->action = 0;
            with_view_model(app->view, IrbViewModel * m, { m->pausing = true; }, true);
        }
    } else if(type != InputTypeShort && type != InputTypeRepeat)
        return;
    else if(key == InputKeyLeft)
        command = IrbScanPrevious;
    else if(key == InputKeyRight)
        command = IrbScanNext;
    else if(key == InputKeyUp)
        app->action = (app->action + 2) % 3;
    else if(key == InputKeyDown)
        app->action = (app->action + 1) % 3;
    else if(key == InputKeyOk && type == InputTypeShort)
        command = app->action == 0 ? IrbScanUse : app->action == 1 ? IrbScanReplay : IrbScanResume;
    if(command != IrbScanNone) {
        int expected = IrbScanNone;
        atomic_compare_exchange_strong(&app->job->command, &expected, command);
    }
}
static int repeat_slot(IrbApp* app) {
    if(!app->play) return -1;
    if(app->screen == Grid && app->focus >= 2 && app->focus < 6) {
        static const uint8_t slots[] = {4, 2, 5, 3};
        return slots[app->focus - 2];
    }
    if(app->screen == Buttons) {
        uint8_t slots[IRB_MAX_BUTTONS];
        unsigned count = irb_project_slots(&app->project, slots, true);
        if(app->focus < count && slots[app->focus] >= 2 && slots[app->focus] <= 5)
            return slots[app->focus];
    }
    if(app->screen == Navigation &&
       (app->focus == 0 || app->focus == 1 || app->focus == 3 || app->focus == 4))
        return irb_nav_slot(&app->project, app->focus);
    return -1;
}
static void key_event(IrbApp* app, InputKey key, InputType type) {
    if(type == InputTypePress) {
        if(key == InputKeyOk) app->consume_ok = false;
        if(key == InputKeyBack) app->consume_back = false;
    }
    if((type == InputTypeShort || type == InputTypeRepeat) &&
       ((key == InputKeyOk && app->consume_ok) || (key == InputKeyBack && app->consume_back)))
        return;
    if(app->worker) {
        if(key == InputKeyOk && type == InputTypeRelease && app->job->type == JobSend)
            atomic_store(&app->job->held, false);
        if(app->screen == Scan && app->job->type == JobScan)
            scan_input(app, key, type);
        else if(key == InputKeyBack && type == InputTypePress &&
                !atomic_load(&app->job->committing)) {
            app->consume_back = true;
            atomic_store(&app->job->cancel, true);
        }
        return;
    }
    if(key == InputKeyOk && type == InputTypePress) {
        int slot = repeat_slot(app);
        if(slot >= 0) {
            app->slot = slot;
            app->consume_ok = true;
            send_ex(app, false, true);
            return;
        }
    }
    if(type == InputTypeLong && app->screen == Browser && key == InputKeyOk) {
        show(app, app->page.path, Browser);
        return;
    }
    if(type == InputTypeRelease) {
        app->repeats = 0;
        return;
    }
    if(type == InputTypeLong && app->screen == Keyboard && key == InputKeyOk) {
        text_done(app);
        return;
    }
    if(type != InputTypeShort && type != InputTypeRepeat) return;
    if(type == InputTypeRepeat && (key == InputKeyOk || key == InputKeyBack)) return;
    if(type == InputTypeShort || key != app->last_direction)
        app->repeats = 0;
    else
        ++app->repeats;
    app->last_direction = key;
    app->tick = 0;
    switch(app->screen) {
    case Home:
        move(app, key, 6);
        if(key == InputKeyBack) view_dispatcher_stop(app->dispatcher);
        if(key == InputKeyOk) {
            if(app->focus == 0) {
                if(app->draft)
                    choice(app, ConfirmNew,
                           "Start a new remote? Current draft is replaced after loading.", Home);
                else
                    load(app, app->default_library, false, false);
            } else if(app->focus == 1)
                browse(app, BrowseOpen, "/ext/infrared", 0);
            else if(app->focus == 2) {
                if(app->draft)
                    load(app, IRB_DRAFT_PATH, true, false);
                else
                    show(app, "No draft yet. Choose New remote.", Home);
            } else if(app->focus == 3)
                browse(app, BrowseSaved, IRB_PROJECT_DIR, 0);
            else if(app->focus == 4)
                go(app, Settings);
            else {
                app->return_screen = Home;
                app->help = 0;
                go(app, Help);
            }
        }
        break;
    case Settings:
        move(app, key, 2);
        if(key == InputKeyBack) go(app, Home);
        if(key == InputKeyOk) {
            if(!app->focus)
                browse(app, BrowseLibrary, "/ext/infrared", 0);
            else
                set_library(app, IRB_DEFAULT_LIBRARY);
        }
        break;
    case Grid: {
        bool nav = navigation_available(app);
        if(key == InputKeyUp)
            app->focus = app->focus >= 6  ? app->focus - 2
                         : app->focus < 2 ? (app->focus == 1 && nav ? 7 : 6)
                                          : app->focus - 2;
        if(key == InputKeyDown)
            app->focus = app->focus >= 6   ? app->focus - 6
                         : app->focus >= 4 ? (app->focus == 5 && nav ? 7 : 6)
                                           : app->focus + 2;
        if(key == InputKeyLeft || key == InputKeyRight) {
            if(app->focus < 6 || nav) app->focus ^= 1;
        }
        if(key == InputKeyBack) {
            if(app->dirty)
                choice(app, ConfirmLeave, "Draft not saved. Leave and lose unsaved changes?", Grid);
            else
                go(app, app->play ? SavedMenu : Home);
        }
        if(key == InputKeyOk) {
            if(app->focus == 7 && nav)
                go(app, Navigation);
            else if(app->focus == 6)
                go(app, app->play ? Buttons : ProjectMenu);
            else if(app->focus < 2) {
                app->pair = app->focus == 1;
                unsigned first = app->pair ? 6 : 0;
                bool a = irb_project_active(&app->project, first);
                bool b = irb_project_active(&app->project, first + 1);
                if(app->play && a != b) {
                    app->slot = a ? first : first + 1;
                    send(app, false);
                } else
                    go(app, Pair);
            } else {
                static const uint8_t slots[] = {4, 2, 5, 3};
                app->slot = slots[app->focus - 2];
                if(app->play)
                    send(app, false);
                else
                    position(app, app->slot, Grid);
            }
        }
        break;
    }
    case Navigation: {
        if(app->focus == IRB_NAV_KEYS) {
            if(key == InputKeyUp) app->focus = 4;
        } else if(key == InputKeyDown &&
                  (app->focus == 4 || app->focus == 5 || app->focus == 6 || app->focus == 7))
            app->focus = IRB_NAV_KEYS;
        else if(key <= InputKeyLeft)
            app->focus = irb_nav_move(app->focus, key);
        if(key == InputKeyBack) go(app, Grid);
        if(key == InputKeyOk) {
            if(app->focus == IRB_NAV_KEYS) {
                if(app->project.extra_count)
                    go(app, Others);
                else
                    show(app, "No unmatched buttons in this remote.", Navigation);
                break;
            }
            int slot = irb_nav_slot(&app->project, app->focus);
            if(app->play) {
                if(slot >= 0) {
                    app->slot = slot;
                    send(app, false);
                } else {
                    char text[96];
                    snprintf(text, sizeof(text), "No code assigned to %s. Edit the remote first.",
                             irb_nav_labels[app->focus]);
                    show(app, text, Navigation);
                }
            } else if(app->library.counts[irb_nav_group[app->focus]]) {
                position(app, IRB_NAV_SLOT_BASE + app->focus, Navigation);
            } else if(slot >= 0) {
                app->slot = slot;
                app->button_return = Navigation;
                app->return_focus = app->focus;
                go(app, ButtonMenu);
            } else {
                char text[96];
                snprintf(text, sizeof(text), "No %s candidates in this library.",
                         irb_nav_labels[app->focus]);
                show(app, text, Navigation);
            }
        }
        break;
    }
    case Others: {
        uint8_t slots[IRB_MAX_BUTTONS];
        unsigned count = extra_slots(&app->project, slots);
        move(app, key, count);
        if(key == InputKeyBack) go(app, Navigation);
        if(key == InputKeyOk && count) {
            app->slot = slots[app->focus];
            app->return_focus = app->focus;
            app->button_return = Others;
            if(app->play)
                send(app, false);
            else
                go(app, ButtonMenu);
        }
        break;
    }
    case Pair:
        move(app, key, 2);
        if(key == InputKeyBack) go(app, Grid);
        if(key == InputKeyOk) {
            app->slot = (app->pair ? 6 : 0) + app->focus;
            if(app->play)
                send(app, false);
            else
                position(app, app->slot, Pair);
        }
        break;
    case Position: {
        int group = irb_slot_group_index(app->slot);
        if(group < 0) {
            show(app, "This imported button has no position list.", app->return_screen);
            break;
        }
        if(key == InputKeyLeft || key == InputKeyRight)
            app->position = irb_position_step(app->position, app->library.counts[group],
                                              key == InputKeyRight, app->repeats);
        if(key == InputKeyUp) app->action = (app->action + 3) % 4;
        if(key == InputKeyDown) app->action = (app->action + 1) % 4;
        if(key == InputKeyBack) go(app, app->return_screen);
        if(key == InputKeyOk) {
            if(app->action == 1)
                send(app, true);
            else if(app->action == 2) {
                go(app, Scan);
                app->action = 0;
                start_job(app, new_job(app, JobScan), Position);
            } else {
                irb_project_set_position(&app->project, app->slot,
                                         app->action == 3 ? 0 : app->position);
                go(app, app->return_screen);
                persist(app, app->screen);
            }
        }
        break;
    }
    case ProjectMenu:
        move(app, key, 4);
        if(key == InputKeyBack) go(app, Grid);
        if(key == InputKeyOk) {
            if(app->focus == 0)
                go(app, Buttons);
            else if(app->focus == 1)
                browse(app, BrowseImport, "/ext/infrared", 0);
            else if(app->focus == 2) {
                if(irb_project_count(&app->project))
                    keyboard(app, TextSave, app->project.name);
                else
                    show(app, "Choose at least one button first.", ProjectMenu);
            } else {
                app->return_screen = ProjectMenu;
                app->help = 0;
                go(app, Help);
            }
        }
        break;
    case Buttons: {
        uint8_t slots[IRB_MAX_BUTTONS];
        unsigned count = irb_project_slots(&app->project, slots, app->play);
        move(app, key, count);
        if(key == InputKeyBack) go(app, app->play ? Grid : ProjectMenu);
        if(key == InputKeyOk && count) {
            app->slot = slots[app->focus];
            app->return_focus = app->focus;
            app->button_return = Buttons;
            if(app->play)
                send(app, false);
            else
                go(app, ButtonMenu);
        }
        break;
    }
    case ButtonMenu:
        move(app, key, irb_slot_is_nav(app->slot) ? 3 : 6);
        if(key == InputKeyBack) {
            go(app, app->button_return);
            app->focus = app->return_focus;
        }
        if(key == InputKeyOk) {
            if(app->focus == 0)
                send(app, false);
            else if(app->focus == 1)
                position(app, app->slot, ButtonMenu);
            else if(irb_slot_is_nav(app->slot)) {
                irb_project_remove(&app->project, app->slot);
                go(app, app->button_return);
                persist(app, app->screen);
            } else if(app->focus == 2)
                keyboard(app, TextLabel, irb_project_label(&app->project, app->slot));
            else {
                if(app->focus == 5) {
                    irb_project_remove(&app->project, app->slot);
                    go(app, app->button_return);
                } else {
                    irb_project_move(&app->project, app->slot, app->focus == 4);
                    uint8_t slots[IRB_MAX_BUTTONS];
                    unsigned count = app->button_return == Others
                                         ? extra_slots(&app->project, slots)
                                         : irb_project_slots(&app->project, slots, false);
                    go(app, app->button_return);
                    for(unsigned i = 0; i < count; ++i)
                        if(slots[i] == app->slot) app->focus = i;
                }
                persist(app, app->screen);
            }
        }
        break;
    case SavedMenu:
        move(app, key, 5);
        if(key == InputKeyBack) go(app, Home);
        if(key == InputKeyOk) {
            if(app->focus == 0) {
                app->play = true;
                go(app, Grid);
            } else if(app->focus == 1) {
                if(app->draft && !app->draft_current)
                    choice(app, ConfirmEdit, "Edit this remote? This replaces your current draft.",
                           SavedMenu);
                else {
                    app->play = false;
                    go(app, Grid);
                    persist(app, Grid);
                }
            } else if(app->focus < 4)
                keyboard(app, app->focus == 2 ? TextRename : TextDuplicate,
                         app->focus == 2 ? app->project.name : "");
            else {
                app->delete_both = false;
                choice(app, ConfirmDelete,
                       "Delete project only? Keep .ir export. Right: delete both.", SavedMenu);
            }
        }
        break;
    case Browser: {
        unsigned before = app->focus;
        move(app, key, app->page.total);
        if(app->focus / IRB_PAGE_SIZE != before / IRB_PAGE_SIZE) {
            browse(app, app->browse_purpose, app->page.path,
                   app->focus / IRB_PAGE_SIZE * IRB_PAGE_SIZE);
            break;
        }
        if(key == InputKeyBack)
            go(app, app->browse_purpose == BrowseSaved    ? Home
                    : app->browse_purpose == BrowseImport ? ProjectMenu
                    : app->browse_purpose == BrowseOpen   ? Home
                                                          : Settings);
        if(key != InputKeyOk || !app->page.total) break;
        unsigned index = app->focus - app->page.start;
        if(index >= app->page.count) break;
        char path[IRB_PATH_SIZE];
        if(!strcmp(app->page.names[index], "..")) {
            snprintf(path, sizeof(path), "%s", app->page.path);
            char* slash = strrchr(path, '/');
            if(slash && slash > path) *slash = 0;
            browse(app, app->browse_purpose, path, 0);
        } else if(snprintf(path, sizeof(path), "%s/%s", app->page.path, app->page.names[index]) >=
                  (int)sizeof(path))
            show(app, "Path is too long.", Browser);
        else if(app->page.directories[index])
            browse(app, app->browse_purpose, path, 0);
        else if(app->browse_purpose == BrowseSaved)
            load(app, path, true, true);
        else if(app->browse_purpose == BrowseLibrary)
            set_library(app, path);
        else if(app->browse_purpose == BrowseOpen) {
            if(app->draft) {
                snprintf(app->pending_path, sizeof(app->pending_path), "%s", path);
                choice(app, ConfirmOpen,
                       "Open this .ir? Your current draft is replaced after it is verified.",
                       Browser);
            } else
                open_remote(app, path);
        } else {
            IrbJob* job = new_job(app, JobCatalog);
            snprintf(job->path, sizeof(job->path), "%s", path);
            start_job(app, job, Browser);
        }
        break;
    }
    case Import:
        move(app, key, app->catalog ? app->catalog->count + 1 : 0);
        if(key == InputKeyBack) {
            free(app->catalog);
            app->catalog = NULL;
            go(app, ProjectMenu);
        }
        if(key == InputKeyOk && app->catalog) {
            IrbJob* job = new_job(app, JobImport);
            job->entry = (int)app->focus - 1;
            start_job(app, job, Import);
        }
        break;
    case Keyboard:
        if(key == InputKeyBack) go(app, app->keyboard_return);
        if(key == InputKeyLeft) app->focus = app->focus / 7 * 7 + (app->focus + 6) % 7;
        if(key == InputKeyRight) app->focus = app->focus / 7 * 7 + (app->focus + 1) % 7;
        if(key == InputKeyUp) app->focus = (app->focus + 35) % 42;
        if(key == InputKeyDown) app->focus = (app->focus + 7) % 42;
        if(key == InputKeyOk) {
            size_t n = strlen(app->text);
            if(app->focus == 41)
                text_done(app);
            else if(app->focus == 40) {
                if(n) app->text[n - 1] = 0;
            } else if(app->focus == 39)
                app->upper = !app->upper;
            else if(n < IRB_NAME_SIZE - 1) {
                char c = irb_keys[app->focus];
                if(app->upper && app->focus < 26) c -= 'a' - 'A';
                app->text[n] = c;
                app->text[n + 1] = 0;
            }
        }
        break;
    case Message:
        if(key == InputKeyOk || key == InputKeyBack)
            go(app, app->message_return);
        else {
            unsigned pages = 1;
            with_view_model(
                app->view, IrbViewModel * m, { pages = m->message_pages ? m->message_pages : 1; },
                false);
            if(key == InputKeyRight) app->focus = (app->focus + 1) % pages;
            if(key == InputKeyLeft) app->focus = (app->focus + pages - 1) % pages;
        }
        break;
    case Choice:
        move(app, key, 2);
        if(app->choice == ConfirmDelete && (key == InputKeyLeft || key == InputKeyRight)) {
            app->delete_both = !app->delete_both;
            app->focus = 0;
            snprintf(app->message, sizeof(app->message), "%s",
                     app->delete_both
                         ? "Delete project AND .ir export? Left: keep export."
                         : "Delete project only? Keep .ir export. Right: delete both.");
        }
        if(key == InputKeyBack || (key == InputKeyOk && !app->focus))
            go(app, app->choice_return);
        else if(key == InputKeyOk)
            confirmed(app);
        break;
    case Help:
        if(key == InputKeyLeft) app->help = (app->help + 4) % 5;
        if(key == InputKeyRight || key == InputKeyOk) app->help = (app->help + 1) % 5;
        if(key == InputKeyBack) go(app, app->return_screen);
        break;
    case Scan:
        go(app, Position);
        break;
    case ScreenCount:
        break;
    }
}
static bool input(InputEvent* event, void* context) {
    IrbApp* app = context;
    view_dispatcher_send_custom_event(app->dispatcher,
                                      EventInput | ((uint32_t)event->type << 4) | event->key);
    return true;
}
static bool custom(void* context, uint32_t event) {
    IrbApp* app = context;
    if(event == EventDone && app->worker)
        finish_job(app);
    else if((event & 0xF000) == EventInput)
        key_event(app, event & 0xF, (event >> 4) & 0xF);
    irb_refresh(app);
    return true;
}
static void tick(void* context) {
    IrbApp* app = context;
    ++app->tick;
    if(!app->worker && (app->screen == Browser || app->screen == Import || app->screen == Buttons ||
                        app->screen == Others))
        irb_refresh(app);
}
int32_t ir_builder_app(void* argument) {
    IrbApp* app = calloc(1, sizeof(*app));
    app->simulate = argument && !strcmp(argument, "--simulate");
    app->storage = furi_record_open(RECORD_STORAGE);
    app->gui = furi_record_open(RECORD_GUI);
    app->dispatcher = view_dispatcher_alloc();
    app->view = view_alloc();
    irb_project_init(&app->project);
    irb_storage_prepare(app->storage);
    char bundled_error[IRB_ERROR_SIZE] = {0};
    irb_builder_library_install(app->storage, bundled_error);
    irb_settings_load(app->storage, app->default_library);
    irb_recover_file(app->storage, IRB_DRAFT_PATH);
    app->draft = storage_common_stat(app->storage, IRB_DRAFT_PATH, NULL) == FSE_OK;
    view_allocate_model(app->view, ViewModelTypeLocking, sizeof(IrbViewModel));
    view_set_orientation(app->view, ViewOrientationVertical);
    view_set_context(app->view, app);
    view_set_draw_callback(app->view, irb_draw);
    view_set_input_callback(app->view, input);
    view_dispatcher_set_event_callback_context(app->dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->dispatcher, custom);
    view_dispatcher_set_tick_event_callback(app->dispatcher, tick, 200);
    view_dispatcher_add_view(app->dispatcher, 0, app->view);
    view_dispatcher_attach_to_gui(app->dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    irb_refresh(app);
    view_dispatcher_switch_to_view(app->dispatcher, 0);
    view_dispatcher_run(app->dispatcher);
    if(app->worker) {
        atomic_store(&app->job->cancel, true);
        furi_thread_join(app->worker);
        furi_thread_free(app->worker);
        irb_library_clear(&app->job->library);
        irb_files_clear(&app->job->files);
        free(app->job->catalog);
        free(app->job);
    }
    view_dispatcher_remove_view(app->dispatcher, 0);
    view_free(app->view);
    view_dispatcher_free(app->dispatcher);
    irb_library_clear(&app->library);
    irb_files_clear(&app->files);
    irb_cache_clear(&app->signals);
    free(app->catalog);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    free(app);
    return 0;
}
