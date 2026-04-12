#include "ham_scanner_app.h"
#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include <gui/elements.h>
#include <furi_hal_speaker.h>
#include <subghz/devices/devices.h>


static void radio_scanner_apply_freq(RadioScannerApp* app);

#define RADIO_SCANNER_DEFAULT_RSSI        (-100.0f)
#define RADIO_SCANNER_DEFAULT_SENSITIVITY (-85.0f)
#define SUBGHZ_DEVICE_NAME "cc1101_int"
#define RSSI_SAMPLES 5
#define RSSI_DELAY_MS 3
#define CHANNEL_SETTLE_MS 15
#define SCAN_PASS_DELAY 10
#define MENU_ITEMS_COUNT 3

static const uint32_t pmr446_channels[] = {
    446006250, 446018750, 446031250, 446043750,
    446056250, 446068750, 446081250, 446093750,
    446106250, 446118750, 446131250, 446143750,
    446156250, 446168750, 446181250, 446193750
};

static const uint32_t frs_gmrs_channels[] = {
    462562500, 462587500, 462612500, 462637500,
    462662500, 462687500, 462712500,
    467562500, 467587500, 467612500, 467637500,
    467662500, 467687500, 467712500,
    462550000, 462575000, 462600000, 462625000,
    462650000, 462675000, 462700000, 462725000
};

// ================= DRAW =================

static void draw_scanner(Canvas* canvas, RadioScannerApp* app) {

    canvas_clear(canvas);
canvas_set_font(canvas, FontSecondary);

// Channel
char ch[16];
snprintf(ch, sizeof(ch), "CH-%02d", (int)app->channel_index + 1);
canvas_draw_str(canvas, 2, 10, ch);

// Profile
const char* profile =
    (app->profile == ProfilePMR) ? "PMR" : "FRS";

canvas_draw_str_aligned(canvas, 64, 10, AlignCenter, AlignCenter, profile);
canvas_draw_str(canvas, 90, 10, app->scanning ? "SCAN" : "HOLD");

    canvas_set_font(canvas, FontBigNumbers);

    char freq[32];
    snprintf(freq, sizeof(freq), "%.3f", (double)app->frequency / 1000000);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, freq);

float target = (app->rssi + 100) / 8.0f;
app->bars_smooth = (0.5f * target) + (0.5f * app->bars_smooth);

int bars = (int)app->bars_smooth;

if(bars < 0) bars = 0;
if(bars > 10) bars = 10;

for(int i = 0; i < bars; i++) {
    canvas_draw_box(canvas, 70 + i * 5, 60, 4, 6);
}

canvas_set_color(canvas, ColorBlack); 
canvas_set_font(canvas, FontSecondary);
canvas_set_color(canvas, ColorBlack);

const char* mode =
    (app->resume_mode == ResumeLock) ? "LOCK" :
    (app->resume_mode == ResumeDelay) ? "DLY" : "HOLD";

canvas_draw_str(canvas, 2, 60, mode);
}

static void draw_squelch(Canvas* canvas, RadioScannerApp* app) {
    char buf[32];
    snprintf(buf, sizeof(buf), "SQL: %.0f dBm", (double)app->sensitivity);

    canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, buf);
}

static float radio_scanner_get_avg_rssi(RadioScannerApp* app) {

    int samples;

    switch(app->scan_speed) {
        case ScanSpeedFast:
            samples = 4;
            break;

        case ScanSpeedBalanced:
            samples = 5;
            break;

        case ScanSpeedAccurate:
            samples = 6;
            break;

        default:
            samples = 5;
            break;
    }

    float total = 0;

    for(int i = 0; i < samples; i++) {
        total += subghz_devices_get_rssi(app->radio_device);
        furi_delay_ms(RSSI_DELAY_MS);
    }

    return total / samples;
}

static float radio_scanner_fine_rssi(RadioScannerApp* app, uint32_t base_freq) {

    const int offsets[] = {-10000, -5000, 0, 5000, 10000};
    float best = -120.0f;

    for(size_t i = 0; i < 5; i++) {

        app->frequency = base_freq + offsets[i];

        radio_scanner_apply_freq(app);
        furi_delay_ms(10);

        float rssi = radio_scanner_get_avg_rssi(app);

        if(rssi > best)
            best = rssi;
    }

float center_rssi;

app->frequency = base_freq;
radio_scanner_apply_freq(app);
furi_delay_ms(10);

center_rssi = radio_scanner_get_avg_rssi(app);

return (best * 0.7f) + (center_rssi * 0.3f);
}

static float radio_scanner_smooth_rssi(RadioScannerApp* app, float new_rssi) {
    const float alpha = 0.2f; // smoothing factor (0.1 = very smooth, 0.5 = fast)

    app->rssi_smoothed = (alpha * new_rssi) + ((1.0f - alpha) * app->rssi_smoothed);
    return app->rssi_smoothed;
}

static void draw_settings(Canvas* canvas, RadioScannerApp* app) {
    const char* items[] = {"Resume Mode", "Squelch"};

    for(int i = 0; i < 2; i++) {

        if(i == app->settings_index) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, 14 + i * 14, 128, 12);

            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 4, 24 + i * 14, items[i]);

            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 4, 24 + i * 14, items[i]);
        }
    }
}

static void handle_settings_input(RadioScannerApp* app, InputEvent* event) {

    if(event->key == InputKeyUp) {
    if(app->settings_index == 0)
        app->settings_index = 1;
    else
        app->settings_index--;
    }
    else if(event->key == InputKeyDown) {
        app->settings_index = (app->settings_index + 1) % 2;
    }

    else if(event->key == InputKeyOk) {

        if(app->settings_index == 0)
            app->ui_screen = UiScreenResumeMode;

        else if(app->settings_index == 1)
            app->ui_screen = UiScreenSquelch;
    }
}

static void draw_resume_mode(Canvas* canvas, RadioScannerApp* app) {

    char delay_str[32];
    ScanResumeMode modes[] = {
        ResumeLock,
        ResumeDelay,
        ResumeHold
    };

    for(int i = 0; i < 3; i++) {

        ScanResumeMode mode = modes[i];
        const char* label;

        if(mode == ResumeLock) {
            label = "LOCK";
        }
        else if(mode == ResumeHold) {
            label = "HOLD";
        }
        else {
            snprintf(delay_str, sizeof(delay_str), "DELAY (%lums)", app->delay_ms);
            label = delay_str;
        }

        if(mode == app->resume_mode) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, 14 + i * 14, 128, 12);

            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 4, 24 + i * 14, label);

            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 4, 24 + i * 14, label);
        }
    }
}

static void draw_menu(Canvas* canvas, RadioScannerApp* app) {
    const char* items[] = {"Profiles", "Settings", "Scan Speed"};

    for(int i = 0; i < MENU_ITEMS_COUNT; i++) {
        if(i == app->menu_index) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, 14 + i * 14, 128, 12);
            canvas_set_color(canvas, ColorWhite);
        }

        if(i == 2) {

            canvas_draw_str(canvas, 4, 24 + i * 14, "Speed");
            int slider_x = 50;
            int slider_y = 18 + i * 14;
            canvas_draw_frame(canvas, slider_x, slider_y, 60, 6);

            int fill_width = (app->scan_speed + 1) * 18;
            canvas_draw_box(canvas, slider_x + 1, slider_y + 1, fill_width, 4);

            int knob_x = slider_x + (app->scan_speed * 30);
            canvas_draw_box(canvas, knob_x, slider_y - 1, 4, 8);

            const char* label =
                (app->scan_speed == ScanSpeedFast) ? "F" :
                (app->scan_speed == ScanSpeedBalanced) ? "M" :
                "A";

            canvas_draw_str(canvas, 115, 24 + i * 14, label);

        } else {
            canvas_draw_str(canvas, 4, 24 + i * 14, items[i]);
        }

        canvas_set_color(canvas, ColorBlack);
    }
}

static void draw_profiles(Canvas* canvas, RadioScannerApp* app) {
    const char* items[] = {"PMR", "FRS/GMRS"};

    for(int i = 0; i < 2; i++) {

        if(i == app->profile) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, 14 + i * 14, 128, 12);

            canvas_set_color(canvas, ColorWhite);
            canvas_draw_str(canvas, 4, 24 + i * 14, items[i]);

            canvas_set_color(canvas, ColorBlack);
        } else {
            canvas_draw_str(canvas, 4, 24 + i * 14, items[i]);
        }
    }
}


static void radio_scanner_draw_callback(Canvas* canvas, void* ctx) {
    RadioScannerApp* app = ctx;

    canvas_clear(canvas);

    switch(app->ui_screen) {

        case UiScreenScanner:
            draw_scanner(canvas, app);
            break;

        case UiScreenMenu:
            draw_menu(canvas, app);
            break;

        case UiScreenProfiles:
            draw_profiles(canvas, app);
            break;
        
        case UiScreenSettings:
            draw_settings(canvas, app);
            break;
            
        case UiScreenResumeMode:
            draw_resume_mode(canvas, app);
            break;

        case UiScreenSquelch:
            draw_squelch(canvas, app);
            break;

        default:
            break;
    }
}


// ============================================ INPUT =================

static void radio_scanner_input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* q = ctx;
    furi_message_queue_put(q, event, FuriWaitForever);
}

static void handle_squelch_input(RadioScannerApp* app, InputEvent* event) {

if(event->key == InputKeyRight) {
    if(app->sensitivity < -60.0f)
        app->sensitivity += 2;
}
else if(event->key == InputKeyLeft) {
    if(app->sensitivity > -100.0f)
        app->sensitivity -= 2;
}

    else if(event->key == InputKeyOk)
        app->ui_screen = UiScreenSettings;
}

static void handle_scanner_input(RadioScannerApp* app, InputEvent* event) {

    if(event->key == InputKeyOk) {
        app->scanning_before_menu = app->scanning;
        app->scanning = false;
        app->ui_screen = UiScreenMenu;
        app->menu_index = 0;
    }

    else if(event->key == InputKeyLeft || event->key == InputKeyRight) {
        app->scanning = !app->scanning;
    }

    else if(event->key == InputKeyUp && !app->scanning) {

        app->channel_index = (app->channel_index + 1) % app->channel_count;
        app->frequency = app->channels[app->channel_index];

        radio_scanner_apply_freq(app);

        app->scanning = false;
    }

    else if(event->key == InputKeyDown && !app->scanning) {

        if(app->channel_index == 0)
            app->channel_index = app->channel_count - 1;
        else
            app->channel_index--;

        app->frequency = app->channels[app->channel_index];
        radio_scanner_apply_freq(app);
        app->scanning = false;
    }
}

static void handle_menu_input(RadioScannerApp* app, InputEvent* event) {

if(app->menu_index == 2) {

    if(event->key == InputKeyRight) {
        if(app->scan_speed < ScanSpeedAccurate)
            app->scan_speed++;
        return;
    }

    else if(event->key == InputKeyLeft) {
        if(app->scan_speed > ScanSpeedFast)
            app->scan_speed--;
        return;
    }
}


if(event->key == InputKeyUp) {
    if(app->menu_index == 0)
        app->menu_index = MENU_ITEMS_COUNT - 1;
    else
        app->menu_index--;
}

else if(event->key == InputKeyDown) {
    app->menu_index = (app->menu_index + 1) % MENU_ITEMS_COUNT;
}

else if(event->key == InputKeyOk) {

    if(app->menu_index == 0)
        app->ui_screen = UiScreenProfiles;

    else if(app->menu_index == 1)
        app->ui_screen = UiScreenSettings;

    else
        app->ui_screen = UiScreenScanner;
    }
}


static void handle_resume_mode_input(RadioScannerApp* app, InputEvent* event) {

    if(event->key == InputKeyUp)
        app->resume_mode = (app->resume_mode + 2) % 3;

    else if(event->key == InputKeyDown)
        app->resume_mode = (app->resume_mode + 1) % 3;

    else if(event->key == InputKeyRight && app->resume_mode == ResumeDelay) {
        app->delay_ms += 250;
    }

    else if(event->key == InputKeyLeft && app->resume_mode == ResumeDelay) {
        if(app->delay_ms > 250)
            app->delay_ms -= 250;
    }

    else if(event->key == InputKeyOk) {
        app->ui_screen = UiScreenSettings;
    }
}

static void handle_profiles_input(RadioScannerApp* app, InputEvent* event) {

    if(event->key == InputKeyUp)
        app->profile = (app->profile + 1) % 2;

    else if(event->key == InputKeyDown)
        app->profile = (app->profile + 1) % 2;

    else if(event->key == InputKeyOk) {


if(app->profile == ProfilePMR) {
    app->channels = (uint32_t*)pmr446_channels;
    app->channel_count = 16;
} else {
    app->channels = (uint32_t*)frs_gmrs_channels;
    app->channel_count = 22;
}

        app->channel_index = 0;
        app->frequency = app->channels[0];
        app->signal_locked = false;
        app->hold_timer = 0;
        app->scanning = true;

        radio_scanner_apply_freq(app);

        app->ui_screen = UiScreenScanner;
    }

}

// ===================================== RADIO =================

static void radio_scanner_rx_callback(const void* d, size_t s, void* ctx) {
    UNUSED(d);
    UNUSED(s);
    UNUSED(ctx);
}

static void radio_scanner_update_rssi(RadioScannerApp* app) {
    float raw = subghz_devices_get_rssi(app->radio_device);
    app->rssi = radio_scanner_smooth_rssi(app, raw);
}

static bool radio_scanner_init_subghz(RadioScannerApp* app) {
    subghz_devices_init();

    const SubGhzDevice* dev = subghz_devices_get_by_name(SUBGHZ_DEVICE_NAME);
    if(!dev) return false;

    app->radio_device = dev;

    subghz_devices_begin(dev);
    subghz_devices_reset(dev);

    subghz_devices_load_preset(dev, FuriHalSubGhzPreset2FSKDev476Async, NULL);
    subghz_devices_set_frequency(dev, app->frequency);

    subghz_devices_start_async_rx(dev, radio_scanner_rx_callback, app);

    if(furi_hal_speaker_acquire(30)) {
        app->speaker_acquired = true;
        subghz_devices_set_async_mirror_pin(dev, &gpio_speaker);
    }

    return true;
}

// ================= SCAN =================
static void radio_scanner_apply_freq(RadioScannerApp* app) {
    subghz_devices_flush_rx(app->radio_device);
    subghz_devices_stop_async_rx(app->radio_device);

    subghz_devices_idle(app->radio_device);
    subghz_devices_set_frequency(app->radio_device, app->frequency);

    subghz_devices_start_async_rx(app->radio_device, radio_scanner_rx_callback, app);
}


static void radio_scanner_process_scanning(RadioScannerApp* app) {

if(app->signal_locked) {

    float open_threshold  = app->sensitivity;
    float close_threshold = app->sensitivity - 2;

    float rssi = radio_scanner_get_avg_rssi(app);
    app->rssi = radio_scanner_smooth_rssi(app, rssi);

    if(app->resume_mode == ResumeHold) {

        if(rssi > open_threshold) {
            return; 
        }

        if(rssi < close_threshold) {
            app->signal_locked = false;
            app->scanning = true;
            return;
        }

        return; 
    }

    if(app->resume_mode == ResumeDelay) {

        if(rssi > open_threshold) {
            app->hold_timer = app->delay_ms;
            return;
        }

        if(rssi < close_threshold) {

            if(app->hold_timer > 0) {
                app->hold_timer -= 20;
                return;
            }

            app->signal_locked = false;
            app->scanning = true;
            return;
        }

        return; 
    }
    if(app->resume_mode == ResumeLock) {
        return; 
    }
}

    // ============================ SPEED CONFIG =================
    int passes = 2;
    int settle = 15;

    switch(app->scan_speed) {
        case ScanSpeedFast:
            passes = 1;
            settle = 10;
            break;

        case ScanSpeedBalanced:
            passes = 2;
            settle = 18;
            break;

        case ScanSpeedAccurate:
            passes = 2;
            settle = 20;
            break;

        default:
            break;
    }

    // ================= MULTIPASS SCAN ==========================
float scores[128];

for(size_t i = 0; i < app->channel_count; i++) {
    scores[i] = -120.0f;
}

    for(int pass = 0; pass < passes; pass++) {

        for(size_t i = 0; i < app->channel_count; i++) {
            if(!app->scanning) return;

            app->channel_index = i;
            app->frequency = app->channels[i];

            radio_scanner_apply_freq(app);
            furi_delay_ms(settle);

            float rssi;

if(app->scan_speed == ScanSpeedFast) {
    rssi = radio_scanner_get_avg_rssi(app);
}
else {
    rssi = radio_scanner_fine_rssi(app, app->channels[i]);
}
            
            if(rssi > app->sensitivity) {

                if(app->scan_speed == ScanSpeedFast) {
                    furi_delay_ms(10);
                    float confirm = radio_scanner_get_avg_rssi(app);
                    rssi = (rssi + confirm) * 0.5f;
                }
                else if(app->scan_speed == ScanSpeedAccurate) {
                    furi_delay_ms(20);
                    float confirm = radio_scanner_get_avg_rssi(app);
                    rssi = (rssi * 0.6f) + (confirm * 0.4f);
                }
                else {
                    furi_delay_ms(15);
                    float confirm = radio_scanner_get_avg_rssi(app);
                    rssi = (rssi * 0.65f) + (confirm * 0.35f);
                }
            }

if(rssi > scores[i]) {
    scores[i] = rssi;
}

if(rssi > app->sensitivity) {
    scores[i] += 1.0f;
}
        }
    }
    
float best_score = -120.0f;
size_t best_index = 0;

// find best raw score first
for(size_t i = 0; i < app->channel_count; i++) {
    if(scores[i] > best_score) {
        best_score = scores[i];
        best_index = i;
    }
}

for(size_t i = 0; i < app->channel_count; i++) {
    if(i != best_index) {
        scores[i] -= 2.0f;
    }
}

size_t best = 0;

for(size_t i = 1; i < app->channel_count; i++) {
    if(scores[i] > scores[best]) {
        best = i;
    }
}

// ================= CONFIDENCE CHECK =================
float second_best = -120.0f;

for(size_t i = 0; i < app->channel_count; i++) {
    if(i != best && scores[i] > second_best) {
        second_best = scores[i];
    }
}

if((scores[best] - second_best) < 1.5f) {
    return; 
}

    // ================= LOCK =================
    app->channel_index = best;
    app->frequency = app->channels[best];

    radio_scanner_apply_freq(app);

    float confirm = radio_scanner_get_avg_rssi(app);
    app->rssi = radio_scanner_smooth_rssi(app, confirm);

if(confirm > app->sensitivity) {

    app->signal_locked = true;

    if(app->resume_mode == ResumeDelay) {
        app->hold_timer = app->delay_ms;
    }

    if(app->resume_mode == ResumeLock) {
        app->scanning = false;
    }

    return;
}

    furi_delay_ms(SCAN_PASS_DELAY);
}

// ================= APP =================
RadioScannerApp* radio_scanner_app_alloc() {
    RadioScannerApp* app = malloc(sizeof(RadioScannerApp));

    app->view_port = view_port_alloc();
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    
    app->ui_screen = UiScreenScanner;
    app->menu_index = 0;
    app->settings_index = 0;

    app->running = true;
    app->scanning = true;
    app->signal_locked = false;

    app->channels = (uint32_t*)pmr446_channels;
    app->channel_count = 16;
    app->channel_index = 0;
    app->frequency = app->channels[0];
    app->profile = ProfilePMR;
    app->scan_speed = ScanSpeedBalanced;

    app->rssi = RADIO_SCANNER_DEFAULT_RSSI;
    app->rssi_smoothed = app->rssi;   
    app->bars_smooth = 0.0f;   

app->sensitivity = -90.0f;

    app->resume_mode = ResumeDelay;
    app->delay_ms = 1000;
    app->hold_timer = 0;

    view_port_draw_callback_set(app->view_port, radio_scanner_draw_callback, app);
    view_port_input_callback_set(app->view_port, radio_scanner_input_callback, app->event_queue);

    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    return app;
}

void radio_scanner_app_free(RadioScannerApp* app) {
    if(app->speaker_acquired) {
        subghz_devices_set_async_mirror_pin(app->radio_device, NULL);
        furi_hal_speaker_release();
    }

    subghz_devices_stop_async_rx(app->radio_device);
    subghz_devices_idle(app->radio_device);
    subghz_devices_end(app->radio_device);

    subghz_devices_deinit();

    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);

    furi_message_queue_free(app->event_queue);
    furi_record_close(RECORD_GUI);

    free(app);
}

// ================= MAIN =================
int32_t ham_scanner_app(void* p) {
    UNUSED(p);

    RadioScannerApp* app = radio_scanner_app_alloc();

    if(!radio_scanner_init_subghz(app)) {
        radio_scanner_app_free(app);
        return 255;
    }

    InputEvent event;

    while(app->running) {

        if(app->scanning)
            radio_scanner_process_scanning(app);
        else
            radio_scanner_update_rssi(app);

if(furi_message_queue_get(app->event_queue, &event, 10) == FuriStatusOk) {

    if(event.type != InputTypeShort) {
        continue;
    }

    if(event.key == InputKeyBack) {

        if(app->ui_screen == UiScreenScanner) {
            app->running = false;
        } 
        else if(app->ui_screen == UiScreenMenu) {
            app->ui_screen = UiScreenScanner;
            app->scanning = app->scanning_before_menu;
        } 
        else if(app->ui_screen == UiScreenProfiles) {
            app->ui_screen = UiScreenMenu;
        }
        else if(app->ui_screen == UiScreenSettings) {
            app->ui_screen = UiScreenMenu;
        }
        else if(app->ui_screen == UiScreenResumeMode) {
            app->ui_screen = UiScreenSettings;
        }
        else if(app->ui_screen == UiScreenSquelch) {
            app->ui_screen = UiScreenSettings;
        }

        continue;
    }

    switch(app->ui_screen) {

        case UiScreenScanner:
            handle_scanner_input(app, &event);
            break;

        case UiScreenMenu:
            handle_menu_input(app, &event);
            break;

        case UiScreenProfiles:
            handle_profiles_input(app, &event);
            break;
                
        case UiScreenSettings:
            handle_settings_input(app, &event);
            break;
                
        case UiScreenResumeMode:
            handle_resume_mode_input(app, &event);
            break;

        case UiScreenSquelch:
            handle_squelch_input(app, &event);
            break;

        default:
            break;
    }
}

    view_port_update(app->view_port);
    furi_delay_ms(20);
}

    radio_scanner_app_free(app);
    return 0;
}
