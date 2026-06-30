/*
@amb3rz
Flipper Zero I2C Explorer

Connect a I2C device to pins:
 - 3V3 (3V3, pin 9)  = VCC
 - GND (GND, pin 18) = GND
 - SCL (C0, pin 16)  = SCL
 - SDA (C1, pin 15)  = SDA

Use 3.3V I2C logic levels only!
Higher voltages may cause damage.
*/

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/text_input.h>
#include <locale/locale.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <stream/stream.h>
#include <toolbox/stream/file_stream.h>

#include <i2c_explorer_icons.h>

typedef enum {
    ExplColDevAddr = 0,
    ExplColRegDump,
    ExplColRegBits,
    ExplColMenu,
    ExplColMax
} I2CExplUIColumn;

typedef enum {
    ExplMenuSaveDump = 0,
    ExplMenuLoad,
    ExplMenuRegSize,
    ExplMenuAbout,
    ExplMenuMax
} I2CExplMenuRows;

const char* expl_menu_titles[ExplMenuMax] = {"Dump...", "Load...", "---", "About..."};
const char* reg_size_title_alternatives[2] = {"8b regs", "2B regs"};

typedef enum {
    AppEventTypeTick,
    AppEventTypeKey,
} AppEventType;

typedef struct {
    AppEventType type; // The reason for this event.
    InputEvent input; // This data is specific to keypress data.
} AppEvent;

#define VIEW_ROW_COUNT 5

typedef struct {
    FuriString* buffer; // Scratch string buffer

    I2CExplUIColumn selected_column;
    I2CExplUIColumn start_column;

    bool reg_mode_16b;

    uint8_t scan_hits[16];
    int16_t selected_addr;

    int16_t start_reg;
    uint16_t reg_dump[VIEW_ROW_COUNT];
    int16_t selected_reg;

    uint8_t selected_bit;

    I2CExplMenuRows selected_menu;
} I2CExplAppState;

typedef struct {
    Gui* gui;
    ViewPort* view_port;

    FuriMessageQueue* queue; // Message queue (AppEvent items to process).
    FuriMutex* mutex; // Used to provide thread safe access to data for callbacks.
    I2CExplAppState* data; // Data accessed by multiple threads (acquire the mutex before accessing!)
} I2CExplAppContext;

typedef enum {
    I2cGpioReady = 0,
    I2cGpioSclLow = 1,
    I2cGpioSdaLow = 2
} I2cBusGpioState;

const GpioPin* pinSCL = &gpio_ext_pc0;
const GpioPin* pinSDA = &gpio_ext_pc1;

I2cBusGpioState poll_i2c_gpio_state() {
    furi_hal_gpio_init(pinSCL, GpioModeInput, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_init(pinSDA, GpioModeInput, GpioPullNo, GpioSpeedVeryHigh);

    I2cBusGpioState result = I2cGpioReady;
    result |= furi_hal_gpio_read(pinSCL) ? I2cGpioReady : I2cGpioSclLow;
    result |= furi_hal_gpio_read(pinSDA) ? I2cGpioReady : I2cGpioSdaLow;

    // Make it so floating SCL/SDA are detected promptly
    // Not a legal bus state but seems to be tolerated?
    furi_hal_gpio_init(pinSCL, GpioModeAnalog, GpioPullDown, GpioSpeedLow);
    furi_hal_gpio_init(pinSDA, GpioModeAnalog, GpioPullDown, GpioSpeedLow);

    return result;
}

// Invoked when input (button press) is detected.  We queue a message and then return to the caller.
static void input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* queue = (FuriMessageQueue*)ctx;

    // Bubble up to top level
    AppEvent event = {.type = AppEventTypeKey, .input = *input_event};
    furi_message_queue_put(queue, &event, FuriWaitForever);
}

// Invoked by the timer on every tick.  We queue a message and then return to the caller.
static void tick_callback(void* ctx) {
    FuriMessageQueue* queue = ctx;

    AppEvent event = {.type = AppEventTypeTick};
    // It's OK to loose this event if system overloaded (so we don't pass a wait value for 3rd parameter.)
    furi_message_queue_put(queue, &event, 0);
}

static void scroll_vert(void* ctx, bool down) {
    I2CExplAppContext* app_context = ctx;
    I2CExplAppState* data = app_context->data;

    if(data->selected_column == ExplColDevAddr) {
        int16_t last_addr = -1;

        for(uint8_t addr = 0; addr <= 0x7f; addr++) {
            if(!down && addr == data->selected_addr) {
                data->selected_addr = last_addr;
                break;
            }

            if((data->scan_hits[addr >> 3] & (1 << (addr & 0x07))) != 0) {
                if(down && addr > data->selected_addr) {
                    data->selected_addr = addr;
                    break;
                }

                last_addr = addr;
            }
        }

        if(last_addr == 0x7f && (data->scan_hits[data->selected_addr >> 3] &
                                 (1 << (data->selected_addr & 0x07))) == 0) {
            data->selected_addr = -1;
        }
    } else if(data->selected_column == ExplColRegDump) {
        data->selected_reg = (data->selected_reg + (down ? 1 : 255)) % 256;

        if(data->selected_reg < data->start_reg) {
            data->start_reg = data->selected_reg;
        } else if(data->selected_reg >= data->start_reg + VIEW_ROW_COUNT) {
            data->start_reg = data->selected_reg - VIEW_ROW_COUNT + 1;
        }
    } else if(data->selected_column == ExplColRegBits) {
        uint8_t bit_count = data->reg_mode_16b ? 16 : 8;
        data->selected_bit = (data->selected_bit + (down ? -1 : 1) + bit_count) % bit_count;
    } else if(data->selected_column == ExplColMenu) {
        data->selected_menu = (data->selected_menu + (down ? 1 : -1) + ExplMenuMax) % ExplMenuMax;
    }
}

static void scroll_horiz(void* ctx, bool right) {
    I2CExplAppContext* app_context = ctx;
    I2CExplAppState* data = app_context->data;

    data->selected_column += (right ? 1 : -1) + ExplColMax;
    data->selected_column %= ExplColMax;

    if(data->selected_column - data->start_column >= 2) {
        data->start_column = data->selected_column - 1;
    } else if(data->selected_column < data->start_column) {
        data->start_column = data->selected_column;
    }
}

static bool load_dump_file(I2CExplAppContext* app_context, const char* file_path) {
    I2CExplAppState* data = app_context->data;
    bool success = false;

    if(data->selected_addr < 0 ||
       (data->scan_hits[data->selected_addr >> 3] & (1 << (data->selected_addr & 0x07))) == 0) {
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);

    if(!file_stream_open(stream, file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        goto load_dump_bail_1;
    };

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);
    FuriString* str_line = furi_string_alloc();
    FuriString* str_val = furi_string_alloc();

    uint8_t target_addr = data->selected_addr;

    while(!stream_eof(stream)) {
        if(!stream_read_line(stream, str_line)) {
            goto load_dump_bail_2;
        }

        furi_string_trim(str_line);
        if(furi_string_size(str_line) == 0 || furi_string_get_char(str_line, 0) == '#') {
            // Comment, ignore
            continue;
        }

        size_t mid_pos = furi_string_search_char(str_line, ':', 0);
        if(mid_pos == FURI_STRING_FAILURE) {
            goto load_dump_bail_2;
        }

        furi_string_set(str_val, str_line);

        furi_string_left(str_line, mid_pos);
        furi_string_trim(str_line);

        furi_string_right(str_val, mid_pos + 1);
        furi_string_trim(str_val);

        if(furi_string_equal_str(str_line, "Address") && furi_string_size(str_val) == 2) {
            // Override target device address for subsequent reg writes:
            // (you can do this more than once)
            target_addr = strtol(furi_string_get_cstr(str_val), NULL, 16);
            continue;
        } else if(furi_string_equal_str(str_line, "Delay") && furi_string_size(str_val) > 0) {
            uint32_t sleep_time = strtol(furi_string_get_cstr(str_val), NULL, 0);
            // This is a blocking wait where the UI will be unresponsive, so for sanity, we cap at 10s.
            // Use more than one delay if you want more than this.
            if(sleep_time > 10000) {
                sleep_time = 10000;
            }

            furi_delay_ms(sleep_time);
            continue;
        }

        if(furi_string_size(str_line) != 2 || furi_string_size(str_val) < 2) {
            goto load_dump_bail_2;
        }

        uint8_t reg = strtol(furi_string_get_cstr(str_line), NULL, 16);
        uint16_t value = strtol(furi_string_get_cstr(str_val), NULL, 16);

        if(furi_string_size(str_val) == 2) {
            furi_hal_i2c_write_reg_8(
                &furi_hal_i2c_handle_external, target_addr << 1, reg, value, 5);
        } else if(furi_string_size(str_val) == 4) {
            // Blindly uses default endianness
            furi_hal_i2c_write_reg_16(
                &furi_hal_i2c_handle_external, target_addr << 1, reg, value, 5);
        } else {
            goto load_dump_bail_2;
        }
    }
    success = true;

load_dump_bail_2:
    furi_string_free(str_val);
    furi_string_free(str_line);
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    file_stream_close(stream);

load_dump_bail_1:
    stream_free(stream);

    furi_record_close(RECORD_STORAGE);

    return success;
}

static bool expl_i2c_read_reg(
    uint8_t address,
    uint8_t reg,
    bool read_16b,
    uint16_t* value,
    uint16_t timeout) {
    if(read_16b) {
        return furi_hal_i2c_read_reg_16(
            &furi_hal_i2c_handle_external, address << 1, reg, value, timeout);
    } else {
        uint8_t value_8b = 0;
        bool result = furi_hal_i2c_read_reg_8(
            &furi_hal_i2c_handle_external, address << 1, reg, &value_8b, timeout);
        *value = value_8b;
        return result;
    }
}

static bool save_dump_file(I2CExplAppContext* app_context, const char* file_path) {
    I2CExplAppState* data = app_context->data;
    bool success = false;

    if(data->selected_addr < 0 ||
       (data->scan_hits[data->selected_addr >> 3] & (1 << (data->selected_addr & 0x07))) == 0) {
        return false;
    }

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = file_stream_alloc(storage);

    if(!file_stream_open(stream, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        goto save_dump_bail_1;
    };

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    for(uint16_t reg = 0; reg <= 0xFF; reg++) {
        uint16_t value = 0;
        if(!expl_i2c_read_reg(data->selected_addr, reg, data->reg_mode_16b, &value, 5)) {
            continue;
        }

        size_t count = stream_write_format(
            stream, data->reg_mode_16b ? "%02x: %04x\n" : "%02x: %02x\n", reg, value);
        if(count < 7) {
            goto save_dump_bail_2;
        }
    }
    success = true;

save_dump_bail_2:
    furi_hal_i2c_release(&furi_hal_i2c_handle_external);

    file_stream_close(stream);

save_dump_bail_1:
    stream_free(stream);

    furi_record_close(RECORD_STORAGE);

    return success;
}

static void dialog_message(DialogsApp* context, bool error, const char* text) {
    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_text(message, text, 88, 32, AlignCenter, AlignCenter);
    dialog_message_set_icon(message, &I_i2c_explorer, 15, 25);
    if(error) {
        dialog_message_set_buttons(message, "Back", NULL, NULL);
    } else {
        dialog_message_set_buttons(message, NULL, "OK", NULL);
    }
    dialog_message_show(context, message);
    dialog_message_free(message);
}

typedef struct {
    ViewDispatcher* view_dispatcher;
    bool success;
} StandaloneViewContext;

static uint32_t standalone_view_cancel_callback(void* ctx) {
    (void)ctx;
    return VIEW_NONE;
}

static bool
    launch_view_standalone(I2CExplAppContext* app_context, View* view, StandaloneViewContext* ctx) {
    // Pain in the butt -- need to instantiate the whole view dispatcher framework to launch stuff like
    // a text input.

    ViewDispatcher* view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(view_dispatcher, app_context->gui, ViewDispatcherTypeFullscreen);

    ctx->success = false;
    ctx->view_dispatcher = view_dispatcher;
    view_dispatcher_set_event_callback_context(view_dispatcher, &ctx);

    view_dispatcher_add_view(view_dispatcher, 0, view);

    // Pressing the BACK button will cancel.
    view_set_previous_callback(view, standalone_view_cancel_callback);

    // Show dialog.
    view_dispatcher_switch_to_view(view_dispatcher, 0);
    view_dispatcher_run(view_dispatcher);

    view_dispatcher_remove_view(view_dispatcher, 0);
    view_dispatcher_free(view_dispatcher);

    return ctx->success;
}

static void text_input_updated_callback(void* ctx) {
    StandaloneViewContext* context = (StandaloneViewContext*)ctx;
    context->success = true;
    view_dispatcher_switch_to_view(context->view_dispatcher, VIEW_NONE);
}

static bool get_text_input(
    I2CExplAppContext* app_context,
    char* header,
    char* buffer,
    uint16_t buffer_size) {
    StandaloneViewContext ctx = {0};
    TextInput* text_input = text_input_alloc();

    bool clear_previous_text = false;
    text_input_set_result_callback(
        text_input, text_input_updated_callback, &ctx, buffer, buffer_size, clear_previous_text);

    text_input_set_header_text(text_input, header);

    bool result = launch_view_standalone(app_context, text_input_get_view(text_input), &ctx);

    text_input_free(text_input);

    return result;
}

static void select_item(void* ctx) {
    I2CExplAppContext* app_context = ctx;
    I2CExplAppState* data = app_context->data;

    if(data->selected_column == ExplColDevAddr || data->selected_column == ExplColRegDump) {
        scroll_horiz(ctx, true);
    } else if(data->selected_column == ExplColRegBits) {
        // Crude having this here....
        furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

        uint16_t value = 0;
        if(expl_i2c_read_reg(
               data->selected_addr, data->selected_reg, data->reg_mode_16b, &value, 5)) {
            value ^= 1 << data->selected_bit;
            if(data->reg_mode_16b) {
                furi_hal_i2c_write_reg_16(
                    &furi_hal_i2c_handle_external,
                    data->selected_addr << 1,
                    data->selected_reg,
                    value,
                    5);
            } else {
                furi_hal_i2c_write_reg_8(
                    &furi_hal_i2c_handle_external,
                    data->selected_addr << 1,
                    data->selected_reg,
                    value,
                    5);
            }
        }

        furi_hal_i2c_release(&furi_hal_i2c_handle_external);
    } else if(data->selected_column == ExplColMenu) {
        bool valid_addr_selected =
            data->selected_addr >= 0 &&
            (data->scan_hits[data->selected_addr >> 3] & (1 << (data->selected_addr & 0x07))) != 0;

        view_port_enabled_set(app_context->view_port, false);
        gui_remove_view_port(app_context->gui, app_context->view_port);
        DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);

        switch(data->selected_menu) {
        case ExplMenuSaveDump:
            if(valid_addr_selected) {
                char filename_str[32];
                snprintf(
                    filename_str, sizeof(filename_str), "reg_dump_0x%02x", data->selected_addr);

                if(get_text_input(
                       app_context, "Enter dump filename:", filename_str, sizeof(filename_str))) {
                    FuriString* full_path = furi_string_alloc_set("/ext/apps/GPIO/");
                    furi_string_cat_str(full_path, filename_str);
                    furi_string_cat_str(full_path, ".i2c");

                    if(save_dump_file(app_context, furi_string_get_cstr(full_path))) {
                        dialog_message(dialogs, false, "Dump saved to file!");
                    } else {
                        dialog_message(dialogs, true, "Error creating dump file");
                    }

                    furi_string_free(full_path);
                }
            }
            break;

        case ExplMenuLoad:
            if(valid_addr_selected) {
                FuriString* file_path = furi_string_alloc_set("/any/apps/GPIO");

                DialogsFileBrowserOptions browser_options;
                dialog_file_browser_set_basic_options(&browser_options, "i2c", NULL);
                browser_options.hide_ext = false;

                bool success =
                    dialog_file_browser_show(dialogs, file_path, file_path, &browser_options);

                if(success) {
                    if(load_dump_file(app_context, furi_string_get_cstr(file_path))) {
                        dialog_message(dialogs, false, "Dump written to I2C!");
                    } else {
                        dialog_message(dialogs, true, "Error loading dump");
                    }
                }

                furi_string_free(file_path);
            }
            break;

        case ExplMenuRegSize:
            data->reg_mode_16b ^= true;
            data->selected_bit = data->selected_bit % 8; // Just to clamp it
            break;

        case ExplMenuAbout:
            dialog_message(
                dialogs,
                true,
                "I2C Explorer\n"
                "@4mb3rz 2025\n"
                "GND(18) <-> GND\n"
                "C0(16) <-> SCL\n"
                "C1(15) <-> SDA\n"
                "3V3(9) <-> VCC");
            break;

        default:
        }

        furi_record_close(RECORD_DIALOGS);
        gui_add_view_port(app_context->gui, app_context->view_port, GuiLayerFullscreen);
        view_port_enabled_set(app_context->view_port, true);
    }
}

typedef enum {
    FrameNone = 0,
    FrameInactive,
    FrameActive
} frame_state;

static void draw_framed_text(
    Canvas* canvas,
    int xcoord,
    int ycoord,
    int round,
    frame_state state,
    const char* string) {
    uint16_t str_width = canvas_string_width(canvas, string);
    uint16_t str_height = canvas_current_font_height(canvas) - round /*HAACK!*/;
    if(state == FrameActive) {
        canvas_draw_rbox(canvas, xcoord - 1, ycoord - 1, str_width + 2, str_height, round);
        canvas_invert_color(canvas);
    } else if(state == FrameInactive) {
        canvas_draw_rframe(canvas, xcoord - 1, ycoord - 1, str_width + 2, str_height, round);
    }
    canvas_draw_str_aligned(canvas, xcoord, ycoord, AlignLeft, AlignTop, string);
    if(state == FrameActive) {
        canvas_invert_color(canvas);
    }
}

// Invoked by the draw callback to render the screen. We render our UI on the callback thread.
static void render_callback(Canvas* canvas, void* ctx) {
    // Attempt to aquire context, so we can read the data.
    I2CExplAppContext* app_context = ctx;
    I2CExplAppState* data = app_context->data;
    if(furi_mutex_acquire(app_context->mutex, 200) != FuriStatusOk) {
        return;
    }

    Font font_heading = FontPrimary;
    Font font_body = FontKeyboard;
    Font font_notice = FontSecondary;

    canvas_set_font(canvas, font_heading);
    uint16_t heading_height = canvas_current_font_height(canvas);

    // Poll SCL/SDA status
    // Hacky to do this here, but it does give better responsiveness (and it's not slow like scanning)
    I2cBusGpioState gpio_state = poll_i2c_gpio_state();
    bool levelSCL = !(gpio_state & I2cGpioSclLow);
    bool levelSDA = !(gpio_state & I2cGpioSdaLow);
    bool not_ready = gpio_state != I2cGpioReady;

    canvas_set_font(canvas, font_heading);

    char* addr_banner = "I2C Devices:";
    uint16_t addr_banner_width = canvas_string_width(canvas, addr_banner);

    int16_t xoffset = 0;
    if(data->start_column >= ExplColRegDump) {
        xoffset -= addr_banner_width - 11;
        if(data->reg_mode_16b) {
            xoffset -= 10;
        }
    }
    if(data->start_column >= ExplColRegBits) {
        xoffset -= addr_banner_width -
                   14; // eh. Not the right width field, but we don't have the right one here
        if(data->reg_mode_16b) {
            xoffset -= addr_banner_width - 12;
        }
    }

    draw_framed_text(
        canvas,
        xoffset + 5,
        2,
        0,
        (data->selected_column == ExplColDevAddr) ? FrameActive : FrameInactive,
        addr_banner);

    canvas_set_font(canvas, font_notice);
    //uint16_t notice_height = canvas_current_font_height(canvas);

    if(not_ready) {
        // Draw some pretty background graphics while waiting for the bus to be connected
        int16_t wave_incr = 5;
        bool phase = false;
        for(int16_t x = xoffset; x < ((int16_t)canvas_width(canvas)) * 4 / 3 + xoffset;
            x += wave_incr) {
            phase = !phase;
            for(int16_t y = heading_height * 5 / 2; y < heading_height * 4; y += heading_height) {
                uint16_t x1 = x < 0 ? 0 : x;
                uint16_t x2 = x + wave_incr < 0 ? 0 : x + wave_incr;
                uint16_t y2 = y - (phase ? wave_incr : 0);
                canvas_draw_line(canvas, x1, y, x1, y - wave_incr);
                canvas_draw_line(canvas, x1, y2, x2, y2);
            }
        }

        canvas_draw_icon(canvas, xoffset + 30, 2 + heading_height, &I_Grumpy_Flipper);

        uint16_t notice_y = VIEW_ROW_COUNT * heading_height;
        char* scl_str = "!SCL(C0)";
        uint16_t scl_width = canvas_string_width(canvas, scl_str);
        if(!levelSCL) {
            draw_framed_text(canvas, 5, notice_y, 0, FrameActive, scl_str);
        }
        char* sda_str = "!SDA(C1)";
        uint16_t sda_width = canvas_string_width(canvas, sda_str);
        if(!levelSDA) {
            draw_framed_text(canvas, 10 + scl_width, notice_y, 0, FrameActive, sda_str);
        }

        draw_framed_text(canvas, 15 + scl_width + sda_width, notice_y, 0, FrameNone, "Use 3.3V");
    }

    bool valid_addr_selected =
        data->selected_addr >= 0 &&
        (data->scan_hits[data->selected_addr >> 3] & (1 << (data->selected_addr & 0x07))) != 0;

    // Collect detected I2C devices into list
    uint8_t visible_addrs[VIEW_ROW_COUNT] = {0};
    uint8_t visible_addr_count = 0;
    bool found_selected_addr = false;
    for(uint8_t addr = 0; addr <= 0x7f; addr++) {
        if((data->scan_hits[addr >> 3] & (1 << (addr & 0x07))) != 0) {
            if(data->selected_addr == addr) {
                found_selected_addr = true;
            }
            visible_addrs[visible_addr_count] = addr;
            visible_addr_count++;
            if(visible_addr_count >= VIEW_ROW_COUNT) {
                if(found_selected_addr || !valid_addr_selected) {
                    break;
                } else {
                    // Forced scroll (crude, doesn't keep place)
                    for(int i = 0; i < VIEW_ROW_COUNT - 1; i++) {
                        visible_addrs[i] = visible_addrs[i + 1];
                    }
                    visible_addr_count--;
                }
            }
        }
    }

    canvas_set_font(canvas, font_body);
    uint16_t body_height = canvas_current_font_height(canvas);
    if(!not_ready) {
        for(int i = 0; i < visible_addr_count; i++) {
            bool is_selected = (visible_addrs[i] == data->selected_addr);

            int16_t xcoord = 5 + xoffset;
            int16_t ycoord = 2 + heading_height + (body_height - 1) * i;

            furi_string_printf(data->buffer, "Addr 0x%02x", visible_addrs[i]);

            frame_state framing = is_selected ? FrameInactive : FrameNone;
            if(is_selected && data->selected_column == ExplColDevAddr) {
                framing = FrameActive;
            }
            draw_framed_text(
                canvas, xcoord, ycoord, 1, framing, furi_string_get_cstr(data->buffer));
        }
    }

    canvas_set_font(canvas, font_heading);
    char* reg_banner = "Regs:";

    int16_t x0 = addr_banner_width + 5 + xoffset;
    draw_framed_text(
        canvas,
        x0 + 5,
        2,
        0,
        (data->selected_column == ExplColRegDump) ? FrameActive : FrameInactive,
        reg_banner);

    if(data->start_column <= ExplColRegDump) {
        canvas_draw_str_aligned(canvas, canvas_width(canvas) - 10, 2, AlignLeft, AlignTop, ">>");
    }

    canvas_set_font(canvas, font_body);
    uint16_t reg_col_width =
        canvas_string_width(canvas, data->reg_mode_16b ? "[00]0x0000" : "[00]0x00") + 5;

    if(!not_ready && valid_addr_selected) {
        for(uint16_t i = 0; i < VIEW_ROW_COUNT; i++) {
            uint16_t reg = i + data->start_reg;
            bool is_selected = (reg == data->selected_reg);

            int16_t xcoord = x0 + 5;
            int16_t ycoord = 2 + heading_height + (body_height - 1) * i;

            furi_string_printf(
                data->buffer,
                data->reg_mode_16b ? "[%02x]0x%04x" : "[%02x]0x%02x",
                reg,
                data->reg_dump[i]);

            frame_state framing = is_selected ? FrameInactive : FrameNone;
            if(is_selected && data->selected_column == ExplColRegDump) {
                framing = FrameActive;
            }
            draw_framed_text(
                canvas, xcoord, ycoord, 1, framing, furi_string_get_cstr(data->buffer));
        }
    }

    canvas_set_font(canvas, font_heading);
    char* bits_banner = "Bits:";
    uint16_t bits_col_width =
        canvas_string_width(canvas, "00000000") * (data->reg_mode_16b ? 2 : 1);

    x0 = addr_banner_width + 5 + reg_col_width + 5 + xoffset;
    draw_framed_text(
        canvas,
        x0 + 5,
        2,
        0,
        (data->selected_column == ExplColRegBits) ? FrameActive : FrameInactive,
        bits_banner);

    canvas_set_font(canvas, font_body);
    if(!not_ready && valid_addr_selected) {
        for(uint16_t i = 0; i < VIEW_ROW_COUNT; i++) {
            uint16_t reg = i + data->start_reg;
            bool is_selected = (reg == data->selected_reg);

            int16_t xcoord = x0 + 5;
            int16_t ycoord = 2 + heading_height + (body_height - 1) * i;

            furi_string_printf(
                data->buffer, data->reg_mode_16b ? "%016b" : "%08b", data->reg_dump[i]);

            canvas_draw_str_aligned(
                canvas, xcoord, ycoord, AlignLeft, AlignTop, furi_string_get_cstr(data->buffer));
            if(is_selected) {
                uint16_t bit_xwidth = canvas_string_width(canvas, "0");
                uint8_t msb_index = data->reg_mode_16b ? 15 : 7;
                furi_string_left(data->buffer, msb_index - data->selected_bit);
                uint16_t bit_xoffset =
                    canvas_string_width(canvas, furi_string_get_cstr(data->buffer));
                if(data->selected_bit < msb_index) {
                    bit_xoffset += 1; // haack??
                }

                if(data->selected_column == ExplColRegBits) {
                    // draw_framed_text isn't quite flexible enough here to get the alignment well
                    uint16_t fudge = 1;
                    canvas_draw_rbox(
                        canvas,
                        xcoord + bit_xoffset - 1,
                        ycoord - 1,
                        bit_xwidth + 2 - fudge,
                        body_height - fudge,
                        1);
                    canvas_invert_color(canvas);
                    canvas_draw_str_aligned(
                        canvas,
                        xcoord + bit_xoffset,
                        ycoord,
                        AlignLeft,
                        AlignTop,
                        (data->reg_dump[i] & (1 << data->selected_bit)) ? "1" : "0");
                    canvas_invert_color(canvas);
                }
            }
        }
    }

    canvas_set_font(canvas, font_heading);
    char* menu_banner = "More:";
    //uint16_t bits_banner_width = canvas_string_width(canvas, bits_banner);

    x0 += bits_col_width + 10;
    draw_framed_text(
        canvas,
        x0 + 5,
        2,
        0,
        (data->selected_column == ExplColMenu) ? FrameActive : FrameInactive,
        menu_banner);

    expl_menu_titles[ExplMenuRegSize] = reg_size_title_alternatives[data->reg_mode_16b ? 1 : 0];

    for(uint16_t i = 0; i < ExplMenuMax; i++) {
        bool is_selected = (i == data->selected_menu);

        int16_t xcoord = 5 + x0;
        int16_t ycoord = 2 + heading_height + (body_height - 1) * i;

        frame_state framing = is_selected ? FrameInactive : FrameNone;
        if(is_selected && data->selected_column == ExplColMenu) {
            bool always_active_menu = (i >= ExplMenuRegSize);
            framing = (valid_addr_selected || always_active_menu) ? FrameActive : FrameInactive;
        }
        draw_framed_text(canvas, xcoord, ycoord, 1, framing, expl_menu_titles[i]);
    }

    // Release the context, so other threads can update the data.
    furi_mutex_release(app_context->mutex);
}

static void update_i2c_status(void* ctx, bool forced) {
    I2CExplAppContext* app_context = ctx;
    I2CExplAppState* data = app_context->data;
    // Our main loop invokes this method after acquiring the mutex, so we can safely access the protected data.

    uint8_t addr = 0;

    if(!forced && poll_i2c_gpio_state() != I2cGpioReady) {
        // Don't waste time when the bus is not online. This reduces GUI sluggishness when disconnected.
        return;
    }

    furi_hal_i2c_acquire(&furi_hal_i2c_handle_external);

    for(addr = 0; addr <= 0x7f; addr++) {
        if(furi_hal_i2c_is_device_ready(&furi_hal_i2c_handle_external, addr << 1, 2)) {
            data->scan_hits[addr >> 3] |= 1 << (addr & 0x07);
        } else {
            data->scan_hits[addr >> 3] &= ~(1 << (addr & 0x07));
        }
    }

    if(data->selected_addr >= 0) {
        if((data->scan_hits[data->selected_addr >> 3] & (1 << (data->selected_addr & 0x07))) !=
           0) {
            for(int i = 0; i < VIEW_ROW_COUNT; i++) {
                uint16_t reg = i + data->start_reg;
                if(!expl_i2c_read_reg(
                       data->selected_addr, reg, data->reg_mode_16b, &data->reg_dump[i], 5)) {
                    data->reg_dump[i] = 0x00; // TODO something smarter?
                }
            }
        }
    }

    furi_hal_i2c_release(&furi_hal_i2c_handle_external);
}

int32_t i2c_explorer_app(void* p) {
    UNUSED(p);

    // Configure our initial data.
    I2CExplAppContext* app_context = malloc(sizeof(I2CExplAppContext));
    app_context->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app_context->data = malloc(sizeof(I2CExplAppState));
    app_context->data->buffer = furi_string_alloc();
    app_context->data->selected_column = ExplColDevAddr;
    app_context->data->start_column = ExplColDevAddr;

    app_context->data->selected_addr = -1;
    memset(app_context->data->scan_hits, 0, sizeof(app_context->data->scan_hits));

    app_context->data->reg_mode_16b = false;

    app_context->data->start_reg = 0;
    memset(app_context->data->reg_dump, 0, sizeof(app_context->data->reg_dump));
    app_context->data->selected_reg = 0;

    app_context->data->selected_bit = 7;

    app_context->data->selected_menu = 0;

    // Get initial data
    update_i2c_status(app_context, true);

    // Queue for events (tick or input)
    app_context->queue = furi_message_queue_alloc(8, sizeof(AppEvent));

    // Set ViewPort callbacks
    app_context->view_port = view_port_alloc();
    view_port_draw_callback_set(app_context->view_port, render_callback, app_context);
    view_port_input_callback_set(app_context->view_port, input_callback, app_context->queue);

    // Open GUI and register view_port
    app_context->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app_context->gui, app_context->view_port, GuiLayerFullscreen);

    // Update the screen fairly frequently (every 1000 milliseconds = 1 second.)
    FuriTimer* timer = furi_timer_alloc(tick_callback, FuriTimerTypePeriodic, app_context->queue);
    furi_timer_start(timer, 500);

    // Main loop
    AppEvent event;
    bool processing = true;
    do {
        if(furi_message_queue_get(app_context->queue, &event, FuriWaitForever) == FuriStatusOk) {
            switch(event.type) {
            case AppEventTypeKey:

                // This is absolute trash architecture shouldn't be here at top level all...
                if(event.input.type == InputTypeShort || event.input.type == InputTypeRepeat) {
                    if(event.input.key == InputKeyUp) {
                        furi_mutex_acquire(app_context->mutex, FuriWaitForever);
                        scroll_vert(app_context, false);
                        update_i2c_status(app_context, false);
                        furi_mutex_release(app_context->mutex);

                    } else if(event.input.key == InputKeyDown) {
                        furi_mutex_acquire(app_context->mutex, FuriWaitForever);
                        scroll_vert(app_context, true);
                        update_i2c_status(app_context, false);
                        furi_mutex_release(app_context->mutex);

                    } else if(event.input.key == InputKeyLeft) {
                        furi_mutex_acquire(app_context->mutex, FuriWaitForever);
                        scroll_horiz(app_context, false);
                        //update_i2c_status(app_context, false);
                        furi_mutex_release(app_context->mutex);

                    } else if(event.input.key == InputKeyRight) {
                        furi_mutex_acquire(app_context->mutex, FuriWaitForever);
                        scroll_horiz(app_context, true);
                        //update_i2c_status(app_context, false);
                        furi_mutex_release(app_context->mutex);

                    } else if(event.input.key == InputKeyOk) {
                        furi_mutex_acquire(app_context->mutex, FuriWaitForever);
                        select_item(app_context);
                        update_i2c_status(app_context, false);
                        furi_mutex_release(app_context->mutex);

                    } else if(event.input.key == InputKeyBack) {
                        // Press of back button exits the program.
                        processing = false;
                    }
                }
                break;

            case AppEventTypeTick:
                // Every timer tick we update the i2c status.
                furi_mutex_acquire(app_context->mutex, FuriWaitForever);
                // Force this update even if the bus doesn't look ready, just in case something is funny
                // (may not be necessary)
                update_i2c_status(app_context, true);
                furi_mutex_release(app_context->mutex);
                break;

            default:
                break;
            }

            // Send signal to update the screen (callback will get invoked at some point later.)
            view_port_update(app_context->view_port);
        } else {
            // We had an issue getting message from the queue, so exit application.
            processing = false;
        }
    } while(processing);

    // Free resources
    furi_timer_free(timer);
    view_port_enabled_set(app_context->view_port, false);
    gui_remove_view_port(app_context->gui, app_context->view_port);
    view_port_free(app_context->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app_context->queue);
    furi_mutex_free(app_context->mutex);
    furi_string_free(app_context->data->buffer);
    free(app_context->data);
    free(app_context);

    // Reset GPIO from probing state
    furi_hal_gpio_init(pinSCL, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(pinSDA, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    return 0;
}
