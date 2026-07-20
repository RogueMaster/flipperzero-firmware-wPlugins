/**
 * @file walkie_talkie.c
 * @brief Walkie-Talkie application for Flipper Zero.
 *
 * @date 09-16-2024
 * @author Coolshrimp
 */

#include <furi.h>                     // Furi API (Application Programming Interface) for Flipper Zero 
#include <furi_hal.h>                  // HAL API (Hardware Abstraction Layer) for Furi API 
#include <stdint.h>                    // Standard integer types
#include <gui/gui.h>                    // GUI
#include <gui/view.h>                   // View API         
#include <gui/view_dispatcher.h>        // View dispatcher
#include <input/input.h>                // Input events
#include <gui/icon_i.h>                 // Icons Library
#include <notification/notification.h>  // Notification service
#include <notification/notification_messages.h>  // Notification messages

#include "walki_talki_icons.h"            // Icons (if any) auto imported from images folder .png files scaled to size.

#define BACKLIGHT_ALWAYS_ON               // Define to keep the backlight always on
#define NUM_CHANNELS 22                   // Total number of channels

typedef enum {
    MyEventTypeKey, // A key was pressed.
    MyEventTypeDone, // The user is done with this app.
} MyEventType;

typedef struct {
    MyEventType type; // The reason for this event.
    InputEvent input; // This data is specific to keypress data.
} MyEvent;

FuriMessageQueue* queue;   // Message queue

typedef struct {
    uint8_t current_channel; // The current channel              
    bool mute_status;      // The mute status     
    NotificationApp* notifications;  // Add notification service to the app struct
} WalkieTalkieApp;

// FRS (Family Radio Service) channel frequencies in Hz
const uint32_t frs_frequencies[NUM_CHANNELS] = {
    462562500, 462587500, 462612500, 462637500, 462662500,
    462687500, 462712500, 467562500, 467587500, 467612500,
    467637500, 467662500, 467687500, 467712500, 462550000,
    462575000, 462600000, 462625000, 462650000, 462675000,
    462700000, 462725000
};

// Draw the main Walkie-Talkie screen
static void my_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);
    WalkieTalkieApp* app = (WalkieTalkieApp*)context;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 36, 10, "Walkie-Talkie");

    //canvas_draw_icon(canvas, 5, 5, &I_walkietalkie);   // Walki-Talki Logo  

    // Draw Up/Dwn Arrow icons
    canvas_draw_icon(canvas, 57, 15, &I_ButtonUpInv);
    canvas_draw_icon_ex(canvas, 56, 43, &I_ButtonUpInv, IconRotation180);

    canvas_set_font(canvas, FontBigNumbers);

    // Display the current channel
    char channel_str[4];
    snprintf(channel_str, sizeof(channel_str), "%02d", app->current_channel + 1);
    canvas_draw_str(canvas, 49, 37, channel_str);

    canvas_set_font(canvas, FontSecondary);

    // Display the current frequency in MHz
    double frequency_mhz = frs_frequencies[app->current_channel] / 1000000.0;
    char frequency_str[32];
    snprintf(frequency_str, sizeof(frequency_str), "Frequency: %.4f MHz", frequency_mhz);
    canvas_draw_str(canvas, 4, 60, frequency_str);

    // Display mute status icon
    if(app->mute_status) {
        //canvas_draw_icon(canvas, 10, 2, &I_volume_muted);
    } else {
        canvas_draw_icon(canvas, 10, 2, &I_volume_normal);
    }
}

// Handle inputs for the main screen
static void my_input_callback(InputEvent* input_event, void* context) {
    UNUSED(context);
    WalkieTalkieApp* app = (WalkieTalkieApp*)context;

    if(input_event->type == InputTypeShort) {
        if(input_event->key == InputKeyUp) {
            app->current_channel = (app->current_channel == 0) ? NUM_CHANNELS - 1 : app->current_channel - 1;
        } else if(input_event->key == InputKeyDown) {            
            app->current_channel = (app->current_channel + 1) % NUM_CHANNELS;
        } else if(input_event->key == InputKeyOk) {
            app->mute_status = !app->mute_status;
        } else if(input_event->key == InputKeyBack) {
            MyEvent event;
            event.type = MyEventTypeDone;
            furi_message_queue_put(queue, &event, FuriWaitForever);
        }
    }
}

// Main function
int32_t walkie_talkie_main(void* p) {
    UNUSED(p); // Suppress unused variable warning
    queue = furi_message_queue_alloc(8, sizeof(MyEvent)); // Allocate a message queue

    WalkieTalkieApp* app = (WalkieTalkieApp*)malloc(sizeof(WalkieTalkieApp)); // Allocate memory for the app struct
    app->current_channel = 0; // Set the current channel to 0
    app->mute_status = false; // Set the mute status to false

    // Initialize notification service
    app->notifications = furi_record_open(RECORD_NOTIFICATION);

    #ifdef BACKLIGHT_ALWAYS_ON 
        notification_message(app->notifications, &sequence_display_backlight_enforce_on); 
    #endif
    
    ViewPort* view_port = view_port_alloc(); // Allocate a view port
    view_port_draw_callback_set(view_port, my_draw_callback, app); // Set the draw callback
    view_port_input_callback_set(view_port, my_input_callback, app); // Set the input callback

    view_port_set_orientation(view_port, ViewPortOrientationHorizontal); // USB/D-pad on bottom

    Gui* gui = furi_record_open(RECORD_GUI); // Open the GUI record
    gui_add_view_port(gui, view_port, GuiLayerFullscreen); // Add the view port to the GUI

    MyEvent event; // Event struct
    bool keep_processing = true; // Keep processing 
    
    while(keep_processing) { // Main loop
        if(furi_message_queue_get(queue, &event, FuriWaitForever) == FuriStatusOk) {
            if(event.type == MyEventTypeDone) {
                keep_processing = false;
            }
        }
        view_port_update(view_port); // Update the view port
        furi_delay_ms(10); // Delay for 10 ms
    }

    furi_message_queue_free(queue); // Free the message queue
    view_port_enabled_set(view_port, false); // Disable the view port
    gui_remove_view_port(gui, view_port); // Remove the view port from the GUI    
    furi_record_close(RECORD_GUI); 
    view_port_free(view_port); // Free the view port

    #ifdef BACKLIGHT_ALWAYS_ON
        notification_message(app->notifications, &sequence_display_backlight_enforce_auto);
    #endif
    
    // Close notification service
    furi_record_close(RECORD_NOTIFICATION);
    free(app);

    return 0;
}
