#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/elements.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>
#include <string.h> 

// LFRFID Hardware libraries
#include <lib/lfrfid/lfrfid_worker.h>
#include <lib/lfrfid/protocols/lfrfid_protocols.h>
#include <toolbox/protocols/protocol_dict.h>

#define SETTINGS_PATH EXT_PATH("apps_data/h10301_writer/settings.dat")
#define WORKER_EV_SUCCESS (1 << 0)

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} AppEvent;

typedef struct {
    uint32_t fc;
    uint32_t cn;
    char status_msg[32];  
    uint8_t edit_mode;    // 0 = FC, 1 = CN
    uint32_t step_size;   // 1, 10, 100, 1000, 10000
    
    // Non-blocking write state
    bool writing;
    uint8_t write_timer;
    LFRFIDWorker* worker;
    ProtocolDict* dict;
    FuriEventFlag* flags;
} H10301WriterApp;

// --- Storage ---

void save_settings(H10301WriterApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage) {
        storage_common_mkdir(storage, EXT_PATH("apps_data/h10301_writer"));
        File* file = storage_file_alloc(storage);
        if(storage_file_open(file, SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            storage_file_write(file, app, sizeof(uint32_t) * 2); 
        }
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
    }
}

void load_settings(H10301WriterApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(storage) {
        File* file = storage_file_alloc(storage);
        if(storage_file_open(file, SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
            storage_file_read(file, app, sizeof(uint32_t) * 2);
        } else {
            app->fc = 0;
            app->cn = 0;
        }
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
    }
    strlcpy(app->status_msg, "Ready to burn.", sizeof(app->status_msg));
}

// --- Hardware Interaction (Non-Blocking) ---

static void rfid_write_cb(LFRFIDWorkerWriteResult result, void* context) {
    FuriEventFlag* flags = context;
    if(result == LFRFIDWorkerWriteOK) {
        furi_event_flag_set(flags, WORKER_EV_SUCCESS);
    }
}

void stop_writing(H10301WriterApp* app) {
    if(!app->writing) return;
    
    lfrfid_worker_stop(app->worker);
    lfrfid_worker_stop_thread(app->worker);
    lfrfid_worker_free(app->worker);
    protocol_dict_free(app->dict);
    furi_event_flag_free(app->flags);
    
    app->writing = false;
    app->write_timer = 0;
}

void start_writing(H10301WriterApp* app) {
    if(app->writing) return;

    app->dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    app->worker = lfrfid_worker_alloc(app->dict);
    app->flags = furi_event_flag_alloc();

    size_t data_size = protocol_dict_get_data_size(app->dict, LFRFIDProtocolH10301);
    uint8_t* data = malloc(data_size);
    memset(data, 0, data_size);
    
    if(data_size >= 3) {
        data[0] = (uint8_t)(app->fc & 0xFF);
        data[1] = (uint8_t)((app->cn >> 8) & 0xFF);
        data[2] = (uint8_t)(app->cn & 0xFF);
    }

    protocol_dict_set_data(app->dict, LFRFIDProtocolH10301, data, data_size);
    free(data);

    lfrfid_worker_start_thread(app->worker);
    lfrfid_worker_write_start(app->worker, LFRFIDProtocolH10301, rfid_write_cb, app->flags);
    
    app->writing = true;
    app->write_timer = 25; // 25 ticks * 400ms = 10 seconds
    strlcpy(app->status_msg, "Hold card (10s)...", sizeof(app->status_msg));
}

// --- UI Rendering ---

static void draw_number_highlight(Canvas* canvas, const char* label, uint32_t value, uint32_t step, bool is_active, bool blink, uint8_t y) {
    char val_str[16];
    snprintf(val_str, sizeof(val_str), "%lu", (unsigned long)value);
    
    if (!is_active) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%s%s", label, val_str);
        canvas_draw_str(canvas, 10, y, buf);
        return;
    }

    int val_len = strlen(val_str);
    int hl_len = 1;
    if(step >= 10) hl_len = 2;
    if(step >= 100) hl_len = 3;
    if(step >= 1000) hl_len = 4;
    if(step >= 10000) hl_len = 5;
    
    if (hl_len > val_len) hl_len = val_len;
    int prefix_len = val_len - hl_len;
    
    char prefix_str[32];
    if (prefix_len > 0) {
        snprintf(prefix_str, sizeof(prefix_str), "%s%.*s", label, prefix_len, val_str);
    } else {
        snprintf(prefix_str, sizeof(prefix_str), "%s", label);
    }
    
    char hl_str[16];
    snprintf(hl_str, sizeof(hl_str), "%s", val_str + prefix_len);
    
    uint16_t prefix_w = canvas_string_width(canvas, prefix_str);
    uint16_t hl_w = canvas_string_width(canvas, hl_str);
    
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_str(canvas, 10, y, prefix_str);
    
    if (blink) {
        canvas_draw_box(canvas, 10 + prefix_w - 1, y - 9, hl_w + 2, 12);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_str(canvas, 10 + prefix_w, y, hl_str);
    
    canvas_set_color(canvas, ColorBlack);
    char step_buf[16];
    snprintf(step_buf, sizeof(step_buf), " [S:%lu]", (unsigned long)step);
    canvas_draw_str(canvas, 10 + prefix_w + hl_w, y, step_buf);
}

static void render_callback(Canvas* canvas, void* ctx) {
    H10301WriterApp* app = ctx;
    if(!app) return;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 11, "H10301 Writer");
    
    canvas_set_font(canvas, FontSecondary);
    if(app->writing) {
        canvas_draw_str(canvas, 2, 22, "Writing in progress...");
        canvas_draw_str(canvas, 2, 33, "Press BACK to cancel");
    } else {
        canvas_draw_str(canvas, 2, 22, "L/R: Step   U/D: Value");
        canvas_draw_str(canvas, 2, 33, "OK: Swap    Hold OK: Burn");
    }
    
    bool blink = (furi_get_tick() / 400) % 2;
    draw_number_highlight(canvas, "FC: ", app->fc, app->step_size, app->edit_mode == 0, blink, 44);
    draw_number_highlight(canvas, "Card: ", app->cn, app->step_size, app->edit_mode == 1, blink, 55);
    
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "Status: %s", app->status_msg);
    canvas_draw_str(canvas, 2, 64, buffer);
}

// --- Callbacks ---

static void input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    AppEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void timer_callback(void* ctx) {
    FuriMessageQueue* event_queue = ctx;
    AppEvent event = {.type = EventTypeTick};
    furi_message_queue_put(event_queue, &event, 0);
}

// --- Main Entry ---

int32_t h10301_writer_app(void* p) {
    UNUSED(p);
    H10301WriterApp* app = malloc(sizeof(H10301WriterApp));
    if(!app) return -1;
    load_settings(app);
    app->edit_mode = 1; 
    app->step_size = 1;
    app->writing = false;

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(AppEvent));
    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* view_port = view_port_alloc();
    NotificationApp* notif = furi_record_open(RECORD_NOTIFICATION);
    
    view_port_draw_callback_set(view_port, render_callback, app);
    view_port_input_callback_set(view_port, input_callback, event_queue);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    FuriTimer* blink_timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, event_queue);
    furi_timer_start(blink_timer, 400); 

    AppEvent event;
    while(1) {
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, FuriWaitForever);
        
        if(event_status == FuriStatusOk) {
            // 1. Handle Success Check if writing
            if(app->writing) {
                uint32_t flags = furi_event_flag_get(app->flags);
                if(flags & WORKER_EV_SUCCESS) {
                    notification_message(notif, &sequence_success);
                    app->cn++;
                    save_settings(app);
                    stop_writing(app);
                    strlcpy(app->status_msg, "Success! (+1)", sizeof(app->status_msg));
                    view_port_update(view_port);
                    continue;
                }
            }

            // 2. Handle Inputs
            if(event.type == EventTypeKey) {
                if(event.input.key == InputKeyBack && event.input.type == InputTypeShort) {
                    if(app->writing) {
                        stop_writing(app);
                        strlcpy(app->status_msg, "Cancelled.", sizeof(app->status_msg));
                    } else {
                        save_settings(app);
                        break; // Exit app
                    }
                }
                
                // Only allow editing if NOT writing
                if(!app->writing && (event.input.type == InputTypeShort || event.input.type == InputTypeRepeat)) {
                    if(event.input.key == InputKeyUp) {
                        if(app->edit_mode == 0) {
                            app->fc = (app->fc + app->step_size > 255) ? 255 : app->fc + app->step_size;
                        } else {
                            app->cn = (app->cn + app->step_size > 65535) ? 65535 : app->cn + app->step_size;
                        }
                    }
                    else if(event.input.key == InputKeyDown) {
                        if(app->edit_mode == 0) {
                            app->fc = (app->fc >= app->step_size) ? app->fc - app->step_size : 0;
                        } else {
                            app->cn = (app->cn >= app->step_size) ? app->cn - app->step_size : 0;
                        }
                    }
                    else if(event.input.key == InputKeyLeft) {
                        if(app->step_size < 10000) app->step_size *= 10;
                    }
                    else if(event.input.key == InputKeyRight) {
                        if(app->step_size > 1) app->step_size /= 10;
                    }
                    else if(event.input.key == InputKeyOk && event.input.type == InputTypeShort) {
                        app->edit_mode = !app->edit_mode;
                        app->step_size = 1;
                    }
                }
                
                if(!app->writing && event.input.key == InputKeyOk && event.input.type == InputTypeLong) {
                    start_writing(app);
                }
            } 
            
            // 3. Handle Timer Tick
            else if(event.type == EventTypeTick) {
                if(app->writing) {
                    if(app->write_timer > 0) {
                        app->write_timer--;
                        int seconds = (app->write_timer * 400) / 1000;
                        snprintf(app->status_msg, sizeof(app->status_msg), "Hold card (%ds)...", seconds);
                    } else {
                        stop_writing(app);
                        strlcpy(app->status_msg, "Write Timeout!", sizeof(app->status_msg));
                        notification_message(notif, &sequence_error);
                    }
                }
            }
            
            view_port_update(view_port);
        }
    }

    if(app->writing) stop_writing(app);
    furi_timer_stop(blink_timer);
    furi_timer_free(blink_timer);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app);
    return 0;
}