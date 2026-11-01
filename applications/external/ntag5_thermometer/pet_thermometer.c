#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <notification/notification_messages.h>
#include <storage/storage.h>
#include <toolbox/saved_struct.h>

#include "body_temp_estimator.h"
#include "fdxb_sensor.h"
#include "ntag5_sensor.h"
#include "pet_animation.h"

#define SETTINGS_PATH    APP_DATA_PATH("settings")
#define SETTINGS_MAGIC   (0x50)
#define SETTINGS_VERSION (1)

typedef enum {
    ThermometerViewTagType,
    ThermometerViewLocation,
    ThermometerViewReading,
} ThermometerView;

typedef enum {
    ThermometerEventTagArrived, /**< A tag came into the field, its ID is in arrived_id */
    ThermometerEventNextFrame,
    ThermometerEventSkip,
} ThermometerEvent;

typedef enum {
    TagTypeNtag5, /**< NFC patch, NTAG 5 link with TMP117 */
    TagTypeFdxb, /**< 134 kHz FDX-B animal chip */

    TagTypeCount,
} TagType;

typedef struct {
    uint8_t tag_type;
    uint8_t location;
} ThermometerSettings;

typedef struct {
    TagType tag_type;
    BodyTempLocation location;
    TempSensorStatus status;
    bool has_value; /**< temperature and id hold the last good reading */
    float temperature; /**< Skin temperature */
    char id[TEMP_SENSOR_ID_SIZE];

    char arrived_id[TEMP_SENSOR_ID_SIZE]; /**< Tag that just came into the field */

    bool animation_active; /**< Draw the frame instead of the reading */
    uint8_t animation_width;
    uint8_t animation_height;
    char animation_name[PET_ANIMATION_NAME_SIZE + 1];
    uint8_t frame[PET_ANIMATION_FRAME_MAX];
} ThermometerReadingModel;

typedef struct {
    Gui* gui;
    NotificationApp* notifications;
    ViewDispatcher* view_dispatcher;
    Submenu* tag_type_menu;
    Submenu* location_menu;
    View* reading_view;
    Ntag5Sensor* ntag5;
    FdxbSensor* fdxb;
    ThermometerSettings settings;

    PetAnimation* animation;
    FuriTimer* animation_timer;
    uint8_t frame[PET_ANIMATION_FRAME_MAX]; /**< Read buffer, outside the model lock */

    /* Sensor worker thread state */
    bool tag_present;
    char present_id[TEMP_SENSOR_ID_SIZE];
} ThermometerApp;

typedef struct {
    const char* menu_name;
    const char* short_name; /**< Screen title, "Patch: Arm" */
    const char* location_header;
    const char* hint_first;
    const char* hint_second;
    const char* id_label;
    uint8_t decimals; /**< Shown resolution of the sensor */
} TagTypeInfo;

static const TagTypeInfo tag_types[TagTypeCount] = {
    [TagTypeNtag5] =
        {
            .menu_name = "NFC patch (NTAG 5)",
            .short_name = "Patch",
            .location_header = "Where is the patch?",
            .hint_first = "Hold the patch to the",
            .hint_second = "back, upper half",
            .id_label = "UID",
            .decimals = 2,
        },
    [TagTypeFdxb] =
        {
            .menu_name = "134 kHz chip (FDX-B)",
            .short_name = "Chip",
            .location_header = "Where is the chip?",
            .hint_first = "Hold the chip to the",
            .hint_second = "back, lower half",
            .id_label = "ID",
            .decimals = 1,
        },
};

static void thermometer_settings_load(ThermometerSettings* settings) {
    if(!saved_struct_load(
           SETTINGS_PATH, settings, sizeof(*settings), SETTINGS_MAGIC, SETTINGS_VERSION) ||
       settings->tag_type >= TagTypeCount || settings->location >= BodyTempLocationCount) {
        settings->tag_type = TagTypeNtag5;
        settings->location = BodyTempLocationArm;
    }
}

static void thermometer_settings_save(const ThermometerSettings* settings) {
    saved_struct_save(
        SETTINGS_PATH, settings, sizeof(*settings), SETTINGS_MAGIC, SETTINGS_VERSION);
}

static void thermometer_draw_centered(Canvas* canvas, const char* first, const char* second) {
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, first);
    canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, second);
}

/* Short note in the title line while a held value is shown: why it is not current */
static const char* thermometer_status_note(TempSensorStatus status) {
    switch(status) {
    case TempSensorStatusWaiting:
        return "removed";
    case TempSensorStatusNotSensor:
        return "no sensor";
    case TempSensorStatusNoPower:
        return "no power";
    case TempSensorStatusNoValue:
        return "no value";
    default:
        return NULL;
    }
}

static void thermometer_draw_title(Canvas* canvas, const ThermometerReadingModel* model) {
    const char* location = body_temp_location_name(model->location);
    const char* note = model->has_value ? thermometer_status_note(model->status) : NULL;
    char title[32];

    snprintf(title, sizeof(title), "%s: %s", tag_types[model->tag_type].short_name, location);

    canvas_set_font(canvas, FontSecondary);
    const uint16_t note_width = note ? canvas_string_width(canvas, note) : 0;
    if(note) {
        canvas_draw_str_aligned(canvas, 128, 10, AlignRight, AlignBottom, note);
    }

    canvas_set_font(canvas, FontPrimary);
    if(note && canvas_string_width(canvas, title) + note_width + 4 > 128) {
        // Long location and a note: drop the tag type
        snprintf(title, sizeof(title), "%s", location);
    }
    canvas_draw_str(canvas, 0, 10, title);
}

static void thermometer_draw_temperature(Canvas* canvas, const ThermometerReadingModel* model) {
    const TagTypeInfo* info = &tag_types[model->tag_type];
    const int decimals = info->decimals;
    char text[40];

    const float estimate = body_temp_estimate(model->temperature, model->location);

    snprintf(text, sizeof(text), "%.*f", decimals, (double)estimate);
    canvas_set_font(canvas, FontBigNumbers);
    const uint16_t width = canvas_string_width(canvas, text);
    const uint16_t x = (128 - width - 12) / 2;
    canvas_draw_str(canvas, x, 39, text);

    // The fonts have no degree sign
    canvas_draw_circle(canvas, x + width + 4, 21, 2);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, x + width + 7, 31, "C");

    canvas_set_font(canvas, FontSecondary);
    snprintf(text, sizeof(text), "Sensor %.*f C", decimals, (double)model->temperature);
    canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignBottom, text);

    if(model->id[0] != '\0') {
        snprintf(text, sizeof(text), "%s %s", info->id_label, model->id);
        canvas_draw_str_aligned(canvas, 64, 63, AlignCenter, AlignBottom, text);
    }
}

static void thermometer_draw_animation(Canvas* canvas, const ThermometerReadingModel* model) {
    const uint8_t width = model->animation_width;
    const uint8_t height = model->animation_height;

    canvas_draw_xbm(canvas, (128 - width) / 2, (64 - height) / 2, width, height, model->frame);

    if(model->animation_name[0] != '\0') {
        canvas_set_font(canvas, FontPrimary);
        const uint16_t text_width = canvas_string_width(canvas, model->animation_name);
        const uint16_t box_width = text_width + 8;
        const uint16_t box_x = (128 - box_width) / 2;

        canvas_set_color(canvas, ColorWhite);
        canvas_draw_box(canvas, box_x, 50, box_width, 14);
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_rframe(canvas, box_x, 50, box_width, 14, 3);
        canvas_draw_str_aligned(canvas, 64, 61, AlignCenter, AlignBottom, model->animation_name);
    }
}

static void thermometer_reading_draw_callback(Canvas* canvas, void* model_pointer) {
    const ThermometerReadingModel* model = model_pointer;
    const TagTypeInfo* info = &tag_types[model->tag_type];

    canvas_clear(canvas);

    if(model->animation_active) {
        thermometer_draw_animation(canvas, model);
        return;
    }

    thermometer_draw_title(canvas, model);

    if(model->has_value) {
        thermometer_draw_temperature(canvas, model);
        return;
    }

    switch(model->status) {
    case TempSensorStatusWaiting:
    case TempSensorStatusOk:
        thermometer_draw_centered(canvas, info->hint_first, info->hint_second);
        break;
    case TempSensorStatusNotSensor:
        thermometer_draw_centered(canvas, "This tag has no", "temperature sensor");
        break;
    case TempSensorStatusNoPower:
        thermometer_draw_centered(canvas, "Sensor not powered:", "energy harvesting is off");
        break;
    case TempSensorStatusNoValue:
        thermometer_draw_centered(canvas, "The sensor returned", "no measurement");
        break;
    }
}

/* Sensor worker thread */
static void thermometer_sensor_callback(const TempSensorReading* reading, void* context) {
    ThermometerApp* app = context;

    // A tag counts as arrived when it answers after the field was empty, or when another
    // tag answers. Chips without temperature have an ID too and are greeted as well.
    bool arrived = false;
    if(reading->status == TempSensorStatusWaiting) {
        app->tag_present = false;
    } else if(reading->id[0] != '\0') {
        arrived = !app->tag_present || strcmp(app->present_id, reading->id) != 0;
        app->tag_present = true;
        strlcpy(app->present_id, reading->id, sizeof(app->present_id));
    }

    with_view_model(
        app->reading_view,
        ThermometerReadingModel * model,
        {
            model->status = reading->status;
            if(reading->status == TempSensorStatusOk) {
                model->has_value = true;
                model->temperature = reading->temperature;
                strlcpy(model->id, reading->id, sizeof(model->id));
            }
            if(arrived) {
                strlcpy(model->arrived_id, reading->id, sizeof(model->arrived_id));
            }
        },
        true);

    if(reading->status == TempSensorStatusOk) {
        notification_message(app->notifications, &sequence_blink_green_10);
    }
    if(arrived) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ThermometerEventTagArrived);
    }
}

/* Timer thread */
static void thermometer_animation_timer_callback(void* context) {
    ThermometerApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, ThermometerEventNextFrame);
}

static void thermometer_animation_stop(ThermometerApp* app) {
    furi_timer_stop(app->animation_timer);
    pet_animation_close(app->animation);

    with_view_model(
        app->reading_view,
        ThermometerReadingModel * model,
        { model->animation_active = false; },
        true);
}

static void thermometer_animation_start(ThermometerApp* app) {
    char id[TEMP_SENSOR_ID_SIZE];

    if(pet_animation_is_open(app->animation)) {
        return;
    }

    with_view_model(
        app->reading_view,
        ThermometerReadingModel * model,
        { strlcpy(id, model->arrived_id, sizeof(id)); },
        false);

    if(!pet_animation_open(app->animation, id)) {
        return;
    }
    if(!pet_animation_next_frame(app->animation, app->frame)) {
        pet_animation_close(app->animation);
        return;
    }

    with_view_model(
        app->reading_view,
        ThermometerReadingModel * model,
        {
            model->animation_active = true;
            model->animation_width = pet_animation_get_width(app->animation);
            model->animation_height = pet_animation_get_height(app->animation);
            strlcpy(
                model->animation_name,
                pet_animation_get_name(app->animation),
                sizeof(model->animation_name));
            memcpy(model->frame, app->frame, sizeof(model->frame));
        },
        true);

    notification_message(app->notifications, &sequence_success);
    furi_timer_start(
        app->animation_timer, furi_ms_to_ticks(pet_animation_get_frame_ms(app->animation)));
}

static void thermometer_animation_next_frame(ThermometerApp* app) {
    if(!pet_animation_is_open(app->animation)) {
        return;
    }

    if(!pet_animation_next_frame(app->animation, app->frame)) {
        thermometer_animation_stop(app);
        return;
    }

    with_view_model(
        app->reading_view,
        ThermometerReadingModel * model,
        { memcpy(model->frame, app->frame, sizeof(model->frame)); },
        true);
}

static bool thermometer_custom_event_callback(void* context, uint32_t event) {
    ThermometerApp* app = context;

    switch(event) {
    case ThermometerEventTagArrived:
        thermometer_animation_start(app);
        return true;
    case ThermometerEventNextFrame:
        thermometer_animation_next_frame(app);
        return true;
    case ThermometerEventSkip:
        if(pet_animation_is_open(app->animation)) {
            thermometer_animation_stop(app);
        }
        return true;
    default:
        return false;
    }
}

/* OK or Back skip a running animation */
static bool thermometer_reading_input_callback(InputEvent* event, void* context) {
    ThermometerApp* app = context;
    bool active = false;

    if(event->key != InputKeyOk && event->key != InputKeyBack) {
        return false;
    }

    with_view_model(
        app->reading_view,
        ThermometerReadingModel * model,
        { active = model->animation_active; },
        false);

    if(active && event->type == InputTypeShort) {
        view_dispatcher_send_custom_event(app->view_dispatcher, ThermometerEventSkip);
    }
    return active;
}

static void thermometer_reading_enter_callback(void* context) {
    ThermometerApp* app = context;

    app->tag_present = false;

    if(app->settings.tag_type == TagTypeFdxb) {
        fdxb_sensor_start(app->fdxb, thermometer_sensor_callback, app);
    } else {
        ntag5_sensor_start(app->ntag5, thermometer_sensor_callback, app);
    }
}

static void thermometer_reading_exit_callback(void* context) {
    ThermometerApp* app = context;

    thermometer_animation_stop(app);

    if(app->settings.tag_type == TagTypeFdxb) {
        fdxb_sensor_stop(app->fdxb);
    } else {
        ntag5_sensor_stop(app->ntag5);
    }
}

static uint32_t thermometer_reading_previous_callback(void* context) {
    UNUSED(context);
    return ThermometerViewLocation;
}

static uint32_t thermometer_location_previous_callback(void* context) {
    UNUSED(context);
    return ThermometerViewTagType;
}

static void thermometer_location_callback(void* context, uint32_t index) {
    ThermometerApp* app = context;

    app->settings.location = index;
    thermometer_settings_save(&app->settings);

    with_view_model(
        app->reading_view,
        ThermometerReadingModel * model,
        {
            model->tag_type = app->settings.tag_type;
            model->location = app->settings.location;
            model->has_value = false;
            model->id[0] = '\0';
        },
        false);

    view_dispatcher_switch_to_view(app->view_dispatcher, ThermometerViewReading);
}

static void thermometer_tag_type_callback(void* context, uint32_t index) {
    ThermometerApp* app = context;

    app->settings.tag_type = index;

    submenu_set_header(app->location_menu, tag_types[index].location_header);
    submenu_set_selected_item(app->location_menu, app->settings.location);
    view_dispatcher_switch_to_view(app->view_dispatcher, ThermometerViewLocation);
}

static bool thermometer_navigation_callback(void* context) {
    ThermometerApp* app = context;
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

static ThermometerApp* thermometer_app_alloc(void) {
    ThermometerApp* app = malloc(sizeof(ThermometerApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->ntag5 = ntag5_sensor_alloc();
    app->fdxb = fdxb_sensor_alloc();
    thermometer_settings_load(&app->settings);
    app->animation = pet_animation_alloc();
    app->animation_timer =
        furi_timer_alloc(thermometer_animation_timer_callback, FuriTimerTypePeriodic, app);

    app->tag_type_menu = submenu_alloc();
    submenu_set_header(app->tag_type_menu, "Which sensor?");
    for(uint32_t type = 0; type < TagTypeCount; type++) {
        submenu_add_item(
            app->tag_type_menu,
            tag_types[type].menu_name,
            type,
            thermometer_tag_type_callback,
            app);
    }
    submenu_set_selected_item(app->tag_type_menu, app->settings.tag_type);

    app->location_menu = submenu_alloc();
    for(uint32_t location = 0; location < BodyTempLocationCount; location++) {
        submenu_add_item(
            app->location_menu,
            body_temp_location_name(location),
            location,
            thermometer_location_callback,
            app);
    }
    view_set_previous_callback(
        submenu_get_view(app->location_menu), thermometer_location_previous_callback);

    app->reading_view = view_alloc();
    view_allocate_model(app->reading_view, ViewModelTypeLocking, sizeof(ThermometerReadingModel));
    view_set_context(app->reading_view, app);
    view_set_draw_callback(app->reading_view, thermometer_reading_draw_callback);
    view_set_input_callback(app->reading_view, thermometer_reading_input_callback);
    view_set_enter_callback(app->reading_view, thermometer_reading_enter_callback);
    view_set_exit_callback(app->reading_view, thermometer_reading_exit_callback);
    view_set_previous_callback(app->reading_view, thermometer_reading_previous_callback);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_add_view(
        app->view_dispatcher, ThermometerViewTagType, submenu_get_view(app->tag_type_menu));
    view_dispatcher_add_view(
        app->view_dispatcher, ThermometerViewLocation, submenu_get_view(app->location_menu));
    view_dispatcher_add_view(app->view_dispatcher, ThermometerViewReading, app->reading_view);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, thermometer_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, thermometer_navigation_callback);

    return app;
}

static void thermometer_app_free(ThermometerApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, ThermometerViewTagType);
    view_dispatcher_remove_view(app->view_dispatcher, ThermometerViewLocation);
    view_dispatcher_remove_view(app->view_dispatcher, ThermometerViewReading);
    view_dispatcher_free(app->view_dispatcher);

    view_free(app->reading_view);
    submenu_free(app->location_menu);
    submenu_free(app->tag_type_menu);
    furi_timer_free(app->animation_timer);
    pet_animation_free(app->animation);
    fdxb_sensor_free(app->fdxb);
    ntag5_sensor_free(app->ntag5);

    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t pet_thermometer_app(void* p) {
    UNUSED(p);

    ThermometerApp* app = thermometer_app_alloc();

    view_dispatcher_switch_to_view(app->view_dispatcher, ThermometerViewTagType);
    view_dispatcher_run(app->view_dispatcher);

    thermometer_app_free(app);

    return 0;
}
