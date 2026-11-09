#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <bt/bt_service/bt.h>
#include <stdatomic.h>
#include "paste_profile.h"
#include "clipboard_screen.h"
#include "keyboard_layout.h"
#include "keyboard_turkish_mac.h"
#include "transfer_protocol.h"
#include "clipboard_library.h"
#include "clipboard_hid.h"
#include "companion_version.h"

#include "keyboard_menu.h"
#include "keyboard_settings_store.h"

#define KEYS_PATH   APP_DATA_PATH(".ble_clipboard_usb.keys")
#define JOB_PATH    APP_DATA_PATH(".macro-v4.tmp")
#define MEMORY_SIZE 4096

typedef struct {
    Gui* gui;
    Bt* bt;
    Storage* storage;
    ViewPort* view_port;
    FuriMessageQueue* input;
    FuriStreamBuffer* rx;
    FuriMutex* mutex;
    FuriHalBleProfileBase* profile;
    FuriHalUsbInterface* previous_usb;
    bool usb_started;
    bool bt_changed;
    atomic_bool connected;
    atomic_bool overflow;
    bool last_connected;
    ClipboardScreen screen;
    ClipboardLibrary library;
    size_t selected;
    enum {
        LibraryList,
        LibraryDetail,
        LibraryConfirmRun,
        LibraryConfirmDelete,
        LibraryAbout,
        LibraryCompanions,
        LibraryKeyboard
    } page;
    ClipboardKeyboardMenu keyboard_menu;
    char run_name[CLIPBOARD_SAVED_NAME];
    uint32_t run_keyboard_revision;
    uint32_t outcome_until;
    char outcome[96];
    ClipboardServer server;
    File* job_file;
    bool on_sd;
    bool reading;
    size_t stored;
    size_t store_size;
    uint8_t memory[MEMORY_SIZE];
    uint8_t cache[256];
    size_t cache_offset;
    size_t cache_size;
} ClipboardApp;

static uint32_t millis(void) {
    // Called only on the app thread; accumulate tick deltas across kernel tick rollover.
    static bool initialized;
    static uint32_t previous;
    static uint64_t elapsed;
    uint32_t tick = furi_get_tick();
    if(initialized) elapsed += (uint32_t)(tick - previous);
    initialized = true;
    previous = tick;
    return (uint32_t)(elapsed * 1000U / furi_kernel_get_tick_frequency());
}

static void status_set(ClipboardApp* app, const char* text) {
    ClipboardScreen screen = {.selected = -1};
    const char* newline = strchr(text, '\n');
    snprintf(
        screen.lines[0],
        sizeof(screen.lines[0]),
        "%.*s",
        newline ? (int)(newline - text) : (int)strlen(text),
        text);
    if(newline) snprintf(screen.lines[1], sizeof(screen.lines[1]), "%s", newline + 1);
    snprintf(
        screen.left,
        sizeof(screen.left),
        "%s",
        app->server.state != ClipboardJobIdle ? "Stop" :
        (!app->profile || !app->usb_started)  ? "Exit" :
        app->page == LibraryList              ? "Keyboard" :
                                                "Back");
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool changed = memcmp(&app->screen, &screen, sizeof(screen)) != 0;
    app->screen = screen;
    furi_mutex_release(app->mutex);
    if(changed) view_port_update(app->view_port);
}

static void draw(Canvas* canvas, void* context) {
    ClipboardApp* app = context;
    ClipboardScreen screen;
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    screen = app->screen;
    furi_mutex_release(app->mutex);
    clipboard_screen_draw(canvas, &screen);
}

static void input_callback(InputEvent* event, void* context) {
    ClipboardApp* app = context;
    furi_message_queue_put(app->input, event, 0);
}

static void bt_status_callback(BtStatus status, void* context) {
    ClipboardApp* app = context;
    atomic_store(&app->connected, status == BtStatusConnected);
}

static uint16_t serial_callback(SerialServiceEvent event, void* context) {
    ClipboardApp* app = context;
    if(event.event == SerialServiceEventTypeDataReceived &&
       furi_stream_buffer_send(app->rx, event.data.buffer, event.data.size, 0) !=
           event.data.size) {
        atomic_store(&app->overflow, true);
    }
    return (uint16_t)furi_stream_buffer_spaces_available(app->rx);
}

static bool send_status(void* context, const uint8_t* data, size_t size) {
    ClipboardApp* app = context;
    if(!app->profile || !atomic_load(&app->connected)) return false;
    bool sent = false;
    for(unsigned attempt = 0; attempt < 8; attempt++) {
        if(ble_profile_paste_tx(app->profile, (uint8_t*)data, (uint16_t)size)) {
            sent = true;
            break;
        }
        if(attempt < 7) furi_delay_ms(2);
    }
    if(sent && size >= 4) {
        if(data[1] == ClipboardDone) {
            snprintf(
                app->outcome,
                sizeof(app->outcome),
                "%s\nCheck USB target",
                size == 9 && data[8] == 1 ? "Cancelled" : "Macro complete");
            app->outcome_until = millis() + 2000;
        } else if(data[1] == ClipboardError) {
            snprintf(app->outcome, sizeof(app->outcome), "Run rejected\nCheck Mac details");
            app->outcome_until = millis() + 3000;
        }
    }
    return sent;
}

static void store_close(void* context) {
    ClipboardApp* app = context;
    if(app->job_file) {
        storage_file_close(app->job_file);
        storage_file_free(app->job_file);
        app->job_file = NULL;
    }
    if(app->on_sd) storage_simply_remove(app->storage, JOB_PATH);
    app->on_sd = app->reading = false;
    app->stored = app->store_size = app->cache_offset = app->cache_size = 0;
    memset(app->memory, 0, sizeof(app->memory));
    memset(app->cache, 0, sizeof(app->cache));
}

static bool store_open(void* context, size_t size) {
    ClipboardApp* app = context;
    store_close(context);
    app->store_size = size;
    if(size <= sizeof(app->memory)) return true;
    if(!storage_simply_mkdir(app->storage, STORAGE_APP_DATA_PATH_PREFIX)) return false;
    app->on_sd = true;
    app->job_file = storage_file_alloc(app->storage);
    if(!storage_file_open(app->job_file, JOB_PATH, FSAM_READ_WRITE, FSOM_CREATE_ALWAYS)) {
        store_close(context);
        return false;
    }
    return true;
}

static bool store_write(void* context, size_t offset, const uint8_t* data, size_t size) {
    ClipboardApp* app = context;
    if(offset != app->stored || offset > app->store_size || size > app->store_size - offset ||
       app->reading)
        return false;
    if(app->on_sd) {
        if(storage_file_write(app->job_file, data, size) != size) return false;
    } else
        memcpy(app->memory + offset, data, size);
    app->stored += size;
    return true;
}

static bool store_read(void* context, size_t offset, uint8_t* output, size_t size) {
    ClipboardApp* app = context;
    if(offset > app->stored || size > app->stored - offset) return false;
    if(!app->on_sd) {
        memcpy(output, app->memory + offset, size);
        return true;
    }
    if(!app->reading) {
        if(!storage_file_sync(app->job_file)) return false;
        app->reading = true;
    }
    if(offset < app->cache_offset || offset - app->cache_offset > app->cache_size ||
       size > app->cache_size - (offset - app->cache_offset)) {
        if(size > sizeof(app->cache) || !storage_file_seek(app->job_file, (uint32_t)offset, true))
            return false;
        size_t count = MIN(sizeof(app->cache), app->stored - offset);
        app->cache_size = storage_file_read(app->job_file, app->cache, count);
        app->cache_offset = offset;
        if(app->cache_size < size) return false;
    }
    memcpy(output, app->cache + offset - app->cache_offset, size);
    return true;
}

static bool load_layout(void* context, uint8_t id, uint8_t target, uint16_t output[128]) {
    ClipboardApp* app = context;
    if(id >= CLIPBOARD_LAYOUT_COUNT || target > 2) return false;
    memset(output, 0, 128 * sizeof(uint16_t));
    if(id == 0) {
        for(unsigned i = 32; i <= 126; i++)
            output[i] = HID_ASCII_TO_KEY(i);
        return true;
    }
    if(id == 1 && target == 0) {
        memcpy(output, clipboard_turkish_mac, sizeof(clipboard_turkish_mac));
        return true;
    }
    char path[96];
    snprintf(path, sizeof(path), EXT_PATH("badusb/assets/layouts/%s"), clipboard_layout_files[id]);
    File* file = storage_file_alloc(app->storage);
    bool loaded = storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING) &&
                  storage_file_read(file, output, 256) == 256;
    storage_file_close(file);
    storage_file_free(file);
    return loaded;
}

static bool usb_connected(void* context) {
    ClipboardApp* app = context;
    return app->usb_started && furi_hal_hid_is_connected();
}

static bool keyboard_press(void* context, uint16_t key) {
    return usb_connected(context) && furi_hal_hid_kb_press(key);
}

static bool mouse_move(void* context, int8_t dx, int8_t dy) {
    return usb_connected(context) && clipboard_hid_mouse_move(dx, dy);
}

static bool random_value(void* context, uint32_t* value) {
    UNUSED(context);
    *value = furi_hal_random_get();
    return true;
}

static bool save_program(void* context, const ClipboardSaved* saved) {
    ClipboardApp* app = context;
    ClipboardReader reader = {app, saved->size, store_read};
    bool ok = clipboard_library_save(&app->library, saved, &reader);
    if(ok) {
        app->selected = 0;
        app->page = LibraryList;
        for(size_t i = 0; i < app->library.count; i++) {
            if(memcmp(saved->id, app->library.entries[i].id, 16) == 0) app->selected = i;
        }
    }
    return ok;
}

static bool keyboard_release(void* context) {
    ClipboardApp* app = context;
    return !app->usb_started || clipboard_hid_release_all();
}

static bool save_keyboard(void* context, const ClipboardKeyboardSettings* settings) {
    ClipboardApp* app = context;
    return clipboard_keyboard_settings_save(app->storage, settings);
}

static bool profile_start(ClipboardApp* app) {
    bt_disconnect(app->bt);
    furi_delay_ms(200);
    bt_keys_storage_set_storage_path(app->bt, KEYS_PATH);
    app->bt_changed = true;
    app->profile = bt_profile_start(app->bt, ble_profile_paste, NULL);
    if(!app->profile) return false;
    ble_profile_paste_set_serial_callback(app->profile, 2048, serial_callback, app);
    bt_set_status_changed_callback(app->bt, bt_status_callback, app);
    furi_hal_bt_start_advertising();
    return true;
}

static void clock_text(char* output, size_t size, uint32_t seconds) {
    snprintf(
        output,
        size,
        "%02lu:%02lu:%02lu",
        (unsigned long)(seconds / 3600),
        (unsigned long)((seconds / 60) % 60),
        (unsigned long)(seconds % 60));
}

static void update_status(ClipboardApp* app, uint32_t now) {
    ClipboardScreen screen = {.selected = -1};
    snprintf(
        screen.left,
        sizeof(screen.left),
        "%s",
        app->server.state == ClipboardJobIdle ? "Exit" : "Stop");
    switch(app->server.state) {
    case ClipboardJobReceiving:
        app->run_name[0] = 0;
        snprintf(screen.lines[0], sizeof(screen.lines[0]), "Receiving macro");
        snprintf(
            screen.lines[1],
            sizeof(screen.lines[1]),
            "%lu / %lu bytes",
            (unsigned long)app->server.received,
            (unsigned long)app->server.total);
        break;
    case ClipboardJobVerified:
        snprintf(screen.lines[0], sizeof(screen.lines[0]), "Macro verified");
        snprintf(
            screen.lines[1],
            sizeof(screen.lines[1]),
            "%s",
            app->server.flags & CLIPBOARD_SAVE_ONLY ? "Saving, no USB output" : "Waiting for Run");
        break;
    case ClipboardJobWaitingUSB:
        snprintf(screen.lines[0], sizeof(screen.lines[0]), "Waiting for USB");
        snprintf(screen.lines[1], sizeof(screen.lines[1]), "Left / Back: cancel");
        break;
    case ClipboardJobRunning:
        snprintf(
            screen.lines[0],
            sizeof(screen.lines[0]),
            "%s",
            app->server.engine.repeat_active ? "Repeating macro" :
            app->server.engine.mouse_active  ? "Mouse movement" :
                                               "Running macro");
        char clock[16];
        clock_text(clock, sizeof(clock), (now - app->server.started_at) / 1000);
        snprintf(screen.lines[1], sizeof(screen.lines[1]), "Elapsed %s", clock);
        if(app->server.engine.mouse_active || app->server.engine.repeat_active) {
            clock_text(
                clock,
                sizeof(clock),
                (clipboard_engine_mouse_remaining(&app->server.engine, now) + 999) / 1000);
            snprintf(screen.lines[2], sizeof(screen.lines[2]), "Left %s", clock);
        } else
            snprintf(
                screen.lines[2],
                sizeof(screen.lines[2]),
                "%s",
                app->run_name[0] ? app->run_name : "USB output");
        snprintf(
            screen.lines[3],
            sizeof(screen.lines[3]),
            "%s",
            app->server.flags & CLIPBOARD_STANDALONE ? "Standalone - Left: Stop" :
                                                       "Mac controlled");
        break;
    default:
        if((int32_t)(app->outcome_until - now) > 0) {
            status_set(app, app->outcome);
            return;
        }
        if(app->selected >= app->library.count)
            app->selected = app->library.count ? app->library.count - 1 : 0;
        if(app->page == LibraryKeyboard) {
            clipboard_keyboard_menu_screen(&app->keyboard_menu, &screen);
        } else if(app->page == LibraryAbout || app->page == LibraryCompanions) {
            clipboard_screen_info(&screen, app->page == LibraryCompanions);
        } else if(app->page == LibraryList || !app->library.count) {
            snprintf(screen.right, sizeof(screen.right), "Info");
            snprintf(screen.left, sizeof(screen.left), "Keyboard");
            snprintf(
                screen.lines[0],
                sizeof(screen.lines[0]),
                "Saved %u/%u | %s",
                (unsigned)app->library.count,
                (unsigned)CLIPBOARD_SAVED_LIMIT,
                app->server.authenticated ? "Mac" : "Offline");
            if(!app->library.count) {
                snprintf(screen.lines[1], sizeof(screen.lines[1]), "Library empty");
                snprintf(
                    screen.lines[2],
                    sizeof(screen.lines[2]),
                    "Keyboard: %s",
                    clipboard_layout_titles[app->server.keyboard_settings.layout]);
                snprintf(screen.lines[3], sizeof(screen.lines[3]), "Save jobs from Mac");
            } else {
                size_t start = app->selected / 3 * 3;
                for(size_t i = 0; i < 3 && start + i < app->library.count; i++) {
                    snprintf(
                        screen.lines[i + 1],
                        sizeof(screen.lines[0]),
                        "%s",
                        app->library.entries[start + i].name);
                    if(start + i == app->selected) screen.selected = (int8_t)(i + 1);
                }
                snprintf(screen.center, sizeof(screen.center), "Open");
            }
        } else {
            const ClipboardSaved* saved = &app->library.entries[app->selected];
            const ClipboardKeyboardSettings* keyboard = &app->server.keyboard_settings;
            ClipboardSaved effective = clipboard_saved_for_keyboard(saved, keyboard);
            uint8_t layout = effective.layout, target = effective.target;
            snprintf(screen.lines[0], sizeof(screen.lines[0]), "%s", saved->name);
            snprintf(screen.left, sizeof(screen.left), "Back");
            if(app->page == LibraryConfirmRun || app->page == LibraryConfirmDelete) {
                snprintf(
                    screen.lines[1],
                    sizeof(screen.lines[1]),
                    "%s",
                    app->page == LibraryConfirmRun ? "Confirm USB output" : "Delete from SD?");
                snprintf(
                    screen.lines[2],
                    sizeof(screen.lines[2]),
                    "%s",
                    app->page == LibraryConfirmRun ?
                        (target != saved->target ? "Saved shortcuts unchanged" :
                                                   "Focus intended target") :
                        "Cannot be undone");
                if(app->page == LibraryConfirmRun) {
                    snprintf(
                        screen.lines[3],
                        sizeof(screen.lines[3]),
                        "%s | %s",
                        clipboard_target_titles[target],
                        clipboard_layout_titles[layout]);
                } else
                    snprintf(
                        screen.lines[3], sizeof(screen.lines[3]), "Mac original is unchanged");
                snprintf(
                    screen.center,
                    sizeof(screen.center),
                    "%s",
                    app->page == LibraryConfirmRun ? "Run" : "Delete");
            } else {
                snprintf(
                    screen.lines[1],
                    sizeof(screen.lines[1]),
                    "%s | %s",
                    clipboard_target_titles[target],
                    clipboard_layout_titles[layout]);
                snprintf(
                    screen.lines[2],
                    sizeof(screen.lines[2]),
                    "%lu bytes",
                    (unsigned long)saved->size);
                snprintf(
                    screen.lines[3],
                    sizeof(screen.lines[3]),
                    "%s",
                    keyboard->override_saved ? "Device keyboard override" :
                                               "Original saved keyboard");
                snprintf(screen.center, sizeof(screen.center), "Run");
                snprintf(screen.right, sizeof(screen.right), "Delete");
            }
        }
        break;
    }
    furi_mutex_acquire(app->mutex, FuriWaitForever);
    bool changed = memcmp(&app->screen, &screen, sizeof(screen)) != 0;
    app->screen = screen;
    furi_mutex_release(app->mutex);
    if(changed) view_port_update(app->view_port);
}

static void local_run(ClipboardApp* app, uint32_t now) {
    if(app->run_keyboard_revision != app->server.keyboard_settings.revision) {
        snprintf(app->outcome, sizeof(app->outcome), "Keyboard changed\nReview before Run");
        app->outcome_until = now + 2500;
        app->page = LibraryDetail;
        return;
    }
    ClipboardSaved saved = app->library.entries[app->selected];
    bool ok = usb_connected(app) && store_open(app, saved.size) &&
              clipboard_library_load(&app->library, &saved, app, store_write);
    // Apply only to the in-memory run; the snapshot's header and checksum stay unchanged.
    saved = clipboard_saved_for_keyboard(&saved, &app->server.keyboard_settings);
    ok = ok && clipboard_server_run_saved(
                   &app->server, &saved, (now ^ clipboard_read_u32(saved.id)) | 1U, now);
    if(ok)
        snprintf(app->run_name, sizeof(app->run_name), "%s", saved.name);
    else {
        store_close(app);
        snprintf(app->outcome, sizeof(app->outcome), "Run rejected\nCheck USB, SD or layout");
        app->outcome_until = now + 3000;
    }
    app->page = LibraryList;
}

int32_t ble_clipboard_app(void* context) {
    // Read-only Info can stay on qFlipper's USB RPC connection without HID/BLE setup.
    if(context && strcmp(context, "--info") == 0) return clipboard_info_app();
    ClipboardApp* app = malloc(sizeof(*app));
    memset(app, 0, sizeof(*app));
    atomic_init(&app->connected, false);
    atomic_init(&app->overflow, false);
    app->gui = furi_record_open(RECORD_GUI);
    app->bt = furi_record_open(RECORD_BT);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->library.storage = app->storage;
    clipboard_library_refresh(&app->library);
    app->input = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->rx = furi_stream_buffer_alloc(2048, 1);
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    ClipboardServerIO io = {
        app,
        send_status,
        store_open,
        store_write,
        store_read,
        store_close,
        load_layout,
        usb_connected,
        keyboard_press,
        keyboard_release,
        mouse_move,
        save_program,
        random_value,
        save_keyboard};
    clipboard_server_init(&app->server, io, furi_hal_version_get_name_ptr());
    clipboard_keyboard_settings_load(app->storage, &app->server.keyboard_settings);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    status_set(app, "Starting...\nIndependent protocol 4");
    storage_simply_remove(app->storage, JOB_PATH);
    if(profile_start(app)) {
        app->previous_usb = furi_hal_usb_get_config();
        furi_hal_usb_unlock();
        app->usb_started = furi_hal_usb_set_config(&usb_hid, NULL);
        if(app->usb_started) clipboard_hid_release_all();
    }
    if(!app->profile || !app->usb_started) status_set(app, "Connection error\nBack to exit");
    bool running = true;
    while(running) {
        InputEvent event;
        if(furi_message_queue_get(app->input, &event, 8) == FuriStatusOk &&
           event.type == InputTypePress) {
            app->outcome_until = 0;
            if(app->server.state != ClipboardJobIdle) {
                if(event.key == InputKeyBack || event.key == InputKeyLeft) {
                    clipboard_server_cancel(&app->server);
                    snprintf(app->outcome, sizeof(app->outcome), "Stopped\nNo automatic restart");
                    app->outcome_until = millis() + 1500;
                    app->page = LibraryList;
                }
            } else if(app->page == LibraryKeyboard) {
                ClipboardKeyboardMenuAction action =
                    clipboard_keyboard_menu_input(&app->keyboard_menu, event.key);
                if(action == ClipboardKeyboardMenuBack)
                    app->page = LibraryList;
                else if(action == ClipboardKeyboardMenuSave) {
                    ClipboardKeyboardResult result =
                        clipboard_server_set_keyboard(&app->server, app->keyboard_menu.draft);
                    if(result == ClipboardKeyboardOk) {
                        clipboard_server_keyboard_changed(&app->server);
                        app->page = LibraryList;
                    } else {
                        const char* message = result == ClipboardKeyboardConflict ?
                                                  "Changed remotely\nReopen Keyboard" :
                                              result == ClipboardKeyboardUnavailable ?
                                                  "Layout unavailable\nCheck SD layout assets" :
                                                  "Keyboard not saved\nCheck SD card";
                        snprintf(app->outcome, sizeof(app->outcome), "%s", message);
                        app->outcome_until = millis() + 2500;
                        if(result == ClipboardKeyboardConflict) app->page = LibraryList;
                    }
                }
            } else if(
                event.key == InputKeyLeft && app->page == LibraryList && app->profile &&
                app->usb_started) {
                clipboard_keyboard_menu_init(&app->keyboard_menu, app->server.keyboard_settings);
                app->page = LibraryKeyboard;
            } else if(event.key == InputKeyBack || event.key == InputKeyLeft) {
                if(app->page == LibraryList)
                    running = false;
                else if(app->page == LibraryCompanions)
                    app->page = LibraryAbout;
                else if(app->page == LibraryAbout || app->page == LibraryDetail)
                    app->page = LibraryList;
                else
                    app->page = LibraryDetail;
            } else if(app->page == LibraryAbout || app->page == LibraryCompanions) {
                if(event.key == InputKeyOk && app->page == LibraryAbout)
                    app->page = LibraryCompanions;
            } else if(app->page == LibraryList) {
                if(event.key == InputKeyRight) app->page = LibraryAbout;
                if(event.key == InputKeyUp && app->selected > 0) app->selected--;
                if(event.key == InputKeyDown && app->selected + 1 < app->library.count)
                    app->selected++;
                if(event.key == InputKeyOk && app->library.count) app->page = LibraryDetail;
            } else if(app->library.count && app->selected < app->library.count) {
                if(event.key == InputKeyRight && app->page == LibraryDetail)
                    app->page = LibraryConfirmDelete;
                else if(event.key == InputKeyOk) {
                    if(app->page == LibraryDetail) {
                        app->run_keyboard_revision = app->server.keyboard_settings.revision;
                        app->page = LibraryConfirmRun;
                    } else if(app->page == LibraryConfirmRun)
                        local_run(app, millis());
                    else if(app->page == LibraryConfirmDelete) {
                        bool ok = clipboard_library_remove(
                            &app->library, &app->library.entries[app->selected]);
                        snprintf(
                            app->outcome,
                            sizeof(app->outcome),
                            "%s",
                            ok ? "Deleted from SD\nMac original unchanged" :
                                 "Delete failed\nCheck SD card");
                        app->outcome_until = millis() + 2000;
                        app->page = LibraryList;
                    }
                }
            }
        }
        if(!running) break;
        bool connected = atomic_load(&app->connected);
        if(connected != app->last_connected) {
            app->last_connected = connected;
            if(!connected) {
                clipboard_server_disconnect(&app->server);
                furi_stream_buffer_reset(app->rx);
            }
        }
        uint32_t now = millis();
        if(atomic_exchange(&app->overflow, false)) {
            uint8_t invalid = 0;
            clipboard_server_disconnect(&app->server);
            clipboard_server_feed(&app->server, &invalid, 1, now);
            furi_stream_buffer_reset(app->rx);
        }
        uint8_t bytes[128];
        size_t size;
        while((size = furi_stream_buffer_receive(app->rx, bytes, sizeof(bytes), 0)) != 0) {
            if(connected) clipboard_server_feed(&app->server, bytes, size, now);
        }
        ClipboardJobState previous = app->server.state;
        clipboard_server_tick(&app->server, now, connected);
        if(previous == ClipboardJobRunning && app->server.state == ClipboardJobIdle) {
            snprintf(
                app->outcome,
                sizeof(app->outcome),
                "%s",
                app->server.engine.state == ClipboardEngineDone ?
                    "Macro complete\nNo automatic restart" :
                    "Run stopped\nCheck USB target");
            app->outcome_until = now + 2000;
            app->page = LibraryList;
        }
        if(app->profile && app->usb_started) update_status(app, now);
    }
    clipboard_server_cancel(&app->server);
    clipboard_server_disconnect(&app->server);
    if(app->usb_started) {
        clipboard_hid_release_all();
        furi_check(furi_hal_usb_set_config(app->previous_usb, NULL));
        app->usb_started = false;
    }
    if(app->bt_changed) {
        bt_set_status_changed_callback(app->bt, NULL, NULL);
        if(app->profile) ble_profile_paste_set_serial_callback(app->profile, 0, NULL, NULL);
        bt_disconnect(app->bt);
        furi_delay_ms(200);
        bt_keys_storage_set_default_path(app->bt);
        furi_check(bt_profile_restore_default(app->bt));
    }
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_mutex_free(app->mutex);
    furi_stream_buffer_free(app->rx);
    furi_message_queue_free(app->input);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_BT);
    furi_record_close(RECORD_GUI);
    memset(app, 0, sizeof(*app));
    free(app);
    return 0;
}
