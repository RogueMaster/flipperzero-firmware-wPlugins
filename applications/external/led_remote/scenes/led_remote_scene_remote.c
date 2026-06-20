#include "../led_remote.h"
#include "../helpers/led_remote_ir.h"

#define IR_SEND_COOLDOWN_TICKS (furi_ms_to_ticks(50))

// Compound-literal helpers — GCC gives static storage duration at file scope.
#define LED_R(v) (&(const NotificationMessage){.type = NotificationMessageTypeLedRed,   .data.led.value = (v)})
#define LED_G(v) (&(const NotificationMessage){.type = NotificationMessageTypeLedGreen, .data.led.value = (v)})
#define LED_B(v) (&(const NotificationMessage){.type = NotificationMessageTypeLedBlue,  .data.led.value = (v)})
#define LED_OFF  &message_red_0, &message_green_0, &message_blue_0

#define BLINK4(r, g, b) \
    r, g, b, &message_delay_50, LED_OFF, &message_delay_50, \
    r, g, b, &message_delay_50, LED_OFF, &message_delay_50, \
    NULL

static const NotificationSequence seq_blink_white       = { BLINK4(LED_R(255), LED_G(255), LED_B(255)) };
static const NotificationSequence seq_blink_lime        = { BLINK4(LED_R(100), LED_G(255), LED_B(0))   };
static const NotificationSequence seq_blink_orange      = { BLINK4(LED_R(255), LED_G(80),  LED_B(0))   };
static const NotificationSequence seq_blink_amber       = { BLINK4(LED_R(255), LED_G(140), LED_B(0))   };
static const NotificationSequence seq_blink_sky_blue    = { BLINK4(LED_R(0),   LED_G(150), LED_B(255)) };
static const NotificationSequence seq_blink_teal        = { BLINK4(LED_R(0),   LED_G(128), LED_B(128)) };
static const NotificationSequence seq_blink_dark_purple = { BLINK4(LED_R(80),  LED_G(0),   LED_B(80))  };
static const NotificationSequence seq_blink_indigo      = { BLINK4(LED_R(60),  LED_G(0),   LED_B(200)) };
static const NotificationSequence seq_blink_pink        = { BLINK4(LED_R(255), LED_G(60),  LED_B(120)) };

static const NotificationSequence* const BTN_BLINK_18K[18] = {
    &seq_blink_white,               // ON
    &seq_blink_white,               // OFF
    &seq_blink_white,               // Timer
    &sequence_blink_start_red,      // Red
    &sequence_blink_start_green,    // Green
    &sequence_blink_start_blue,     // Blue
    &seq_blink_orange,              // Orange
    &seq_blink_lime,                // Lime
    &sequence_blink_start_cyan,     // Cyan
    &sequence_blink_start_yellow,   // Yellow
    &seq_blink_white,               // Multi
    &sequence_blink_start_magenta,  // Purple
    &seq_blink_white,               // Bright+
    &sequence_blink_start_blue,     // Music
    &seq_blink_white,               // Bright-
    &seq_blink_white,               // Strobe
    &seq_blink_white,               // Fade
    &seq_blink_white,               // Flash
};

static const NotificationSequence* const BTN_BLINK_24K[24] = {
    &seq_blink_white,               // B+
    &seq_blink_white,               // B-
    &seq_blink_white,               // OFF
    &seq_blink_white,               // ON
    &sequence_blink_start_red,      // R
    &sequence_blink_start_green,    // G
    &sequence_blink_start_blue,     // B
    &seq_blink_white,               // W
    &sequence_blink_start_red,      // R+ (orange-red)
    &seq_blink_lime,                // LM (lime)
    &seq_blink_sky_blue,            // SB (sky blue)
    &seq_blink_white,               // FL (flash)
    &seq_blink_orange,              // OR (orange)
    &sequence_blink_start_cyan,     // CY (cyan)
    &sequence_blink_start_magenta,  // PU (purple)
    &seq_blink_white,               // ST (strobe)
    &seq_blink_amber,               // AM (amber)
    &seq_blink_teal,                // TE (teal)
    &seq_blink_dark_purple,         // DP (dark purple)
    &seq_blink_white,               // FD (fade)
    &sequence_blink_start_yellow,   // YL (yellow)
    &seq_blink_indigo,              // IN (indigo)
    &seq_blink_pink,                // PK (pink)
    &seq_blink_white,               // SM (smooth)
};

static uint32_t last_send_tick = 0;

static void led_remote_scene_remote_send_callback(LedButton button, void* context) {
    LedRemoteApp* app = context;

    if(button >= app->button_count) return;
    if(!app->learned[button]) return;

    uint32_t now = furi_get_tick();
    if(last_send_tick != 0 && (now - last_send_tick) < IR_SEND_COOLDOWN_TICKS) return;

    const NotificationSequence* seq =
        (app->remote_type == LedRemoteType18Keys)
            ? BTN_BLINK_18K[button]
            : BTN_BLINK_24K[button];

    notification_message(app->notifications, seq);
    led_remote_ir_send(app, button);
    notification_message(app->notifications, &sequence_blink_stop);

    last_send_tick = furi_get_tick();
}

void led_remote_scene_remote_on_enter(void* context) {
    LedRemoteApp* app = context;

    last_send_tick = 0;

    led_remote_view_remote_set_callback(
        app->view_remote, led_remote_scene_remote_send_callback, app);
    led_remote_view_remote_set_learned(app->view_remote, app->learned);
    led_remote_view_remote_set_profile_name(app->view_remote, app->profile_name);
    led_remote_view_remote_set_layout(app->view_remote, app->layout);
    led_remote_view_remote_set_remote_params(
        app->view_remote,
        app->button_count,
        app->grid_cols,
        app->button_labels_short);

    view_dispatcher_switch_to_view(app->view_dispatcher, LedRemoteViewIdRemote);
}

bool led_remote_scene_remote_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void led_remote_scene_remote_on_exit(void* context) {
    UNUSED(context);
}
