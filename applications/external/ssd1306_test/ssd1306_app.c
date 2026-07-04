#include "ssd1306.h"
#include <gui/gui.h>
#include <input/input.h>
#include <furi.h>
#include <furi_hal.h>
#include <storage/storage.h>
#include <stdlib.h>
#include <math.h>

// Display variant: yellow-bar split height (0 = full monochrome)
#define YELLOW_BAR_16   16
#define YELLOW_BAR_NONE 0

typedef enum {
    SceneMainMenu,
    ScenePatterns,
    SceneBrightness,
    SceneCommands,
    SceneScrolling,
    SceneInfo,
    SceneClock,
    ScenePlant,
    SceneI2CScan,
    SceneBenchmark,
    ScenePixelWalk,
} Scene;

typedef struct {
    Scene scene;
    int cursor; // menu cursor position
    int scroll_offset; // for scrollable menus

    SSD1306 oled;
    bool detected;

    // test pattern state
    int pattern_idx;

    // brightness
    uint8_t brightness;

    // command toggles
    bool cmd_invert;
    bool cmd_flip_h;
    bool cmd_flip_v;
    bool cmd_all_on;
    bool cmd_power;
    uint8_t cmd_start_line;
    uint8_t cmd_fade_mode;

    // scrolling
    int scroll_menu_idx;
    uint8_t scroll_speed;

    // variant
    uint8_t yellow_bar_height; // 0 or 16
    uint8_t detected_addr; // 0x3C or 0x3D

    // clock
    uint8_t last_second;

    // plant
    uint32_t plant_growth;
    int32_t plant_water;
    int32_t plant_sun;
    int8_t plant_rotation;
    uint32_t plant_last_press_tick;
    uint8_t plant_stress;
    uint32_t plant_last_update_time;
    uint32_t plant_last_app_close_time;
    uint32_t plant_ticks;

    // death mechanics
    bool plant_is_dead;
    uint32_t plant_dead_until;
    uint8_t plant_lockout_attempts;
    uint8_t plant_insult_idx;
    bool plant_is_monty_insult;

    // I2C scanner
    bool i2c_scan_done;
    uint8_t i2c_scan_progress;
    bool i2c_scan_found[128];
    uint8_t i2c_scan_found_count;

    // FPS benchmark
    float bench_fps;
    bool bench_done;

    // Pixel walk
    uint8_t walk_mode; // 0=pixel, 1=vline, 2=hline, 3=block
    uint16_t walk_step;
    uint8_t walk_speed; // 1-10, frames per step

    bool running;
    FuriMutex* mutex;
} App;

#define PLANT_SAVE_PATH  EXT_PATH("apps_data/ssd1306_test/ssd1306_test.sav")
#define PLANT_SAVE_MAGIC 0xB07A81C5

typedef struct {
    bool is_dead;
    uint32_t dead_until;
    uint8_t lockout_attempts;
    uint32_t magic;

    // Growth persistence
    uint32_t growth;
    int32_t water;
    int32_t sun;
    int8_t rotation;
    uint8_t stress;

    // Timestamps for real-time updates
    uint32_t last_update_time;
} PlantSaveState;

static void plant_crypt_data(uint8_t* data, size_t size) {
    const char* key = "BotanicalObligationAES";
    size_t key_len = strlen(key);
    for(size_t i = 0; i < size; i++) {
        data[i] ^= key[i % key_len];
    }
}

static void save_plant_state(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    // Ensure save directory exists
    storage_common_mkdir(storage, EXT_PATH("apps_data"));
    storage_common_mkdir(storage, EXT_PATH("apps_data/ssd1306_test"));

    if(storage_file_open(file, PLANT_SAVE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        PlantSaveState state;
        state.is_dead = app->plant_is_dead;
        state.dead_until = app->plant_dead_until;
        state.lockout_attempts = app->plant_lockout_attempts;

        // Save growth state
        state.growth = app->plant_growth;
        state.water = app->plant_water;
        state.sun = app->plant_sun;
        state.rotation = app->plant_rotation;
        state.stress = app->plant_stress;

        // Timestamp for real-time decay
        state.last_update_time = furi_hal_rtc_get_timestamp();

        state.magic = PLANT_SAVE_MAGIC;

        plant_crypt_data((uint8_t*)&state, sizeof(state));
        storage_file_write(file, &state, sizeof(state));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static void load_plant_state(App* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, PLANT_SAVE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        PlantSaveState state;
        if(storage_file_read(file, &state, sizeof(state)) == sizeof(state)) {
            plant_crypt_data((uint8_t*)&state, sizeof(state));
            if(state.magic == PLANT_SAVE_MAGIC) {
                app->plant_is_dead = state.is_dead;
                app->plant_dead_until = state.dead_until;
                app->plant_lockout_attempts = state.lockout_attempts;

                // Load growth state
                app->plant_growth = state.growth;
                app->plant_water = state.water;
                app->plant_sun = state.sun;
                app->plant_rotation = state.rotation;
                app->plant_stress = state.stress;

                app->plant_last_update_time = state.last_update_time;

                // Calculate real-time updates while app was closed
                uint32_t now = furi_hal_rtc_get_timestamp();
                if(app->plant_last_update_time > 0 && now > app->plant_last_update_time) {
                    uint32_t elapsed_seconds = now - app->plant_last_update_time;
                    uint32_t ticks_elapsed = elapsed_seconds / 5;

                    if(!app->plant_is_dead) {
                        app->plant_water -= (ticks_elapsed / 2);
                        app->plant_sun -= (ticks_elapsed / 4);

                        if(app->plant_stress > 20 && app->plant_growth > 0) {
                            app->plant_growth -= (ticks_elapsed / 100);
                        }

                        if(app->plant_water < -20 || app->plant_water > 120 ||
                           app->plant_stress > 40) {
                            app->plant_is_dead = true;
                            app->plant_dead_until = now + (60 + (rand() % 241));
                            app->plant_growth = 0;
                        }
                    }
                }

                app->plant_last_update_time = 0;
            }
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}

static const char* death_insults[] = {"You murderer.",          "Try plastic pets.",
                                      "Why are you like this.", "I withered for you.",
                                      "Demise on your hands.",  "Not a green thumb.",
                                      "Brown thumb energy.",    "Photosynthesis failed.",
                                      "I trusted you.",         "I was so young.",
                                      "Cruel world.",           "Look what you did.",
                                      "Are you proud?",         "I needed water.",
                                      "Too much water.",        "You suffocated me.",
                                      "I am compost.",          "Worm food now.",
                                      "I gave you oxygen.",     "Is this a joke?",
                                      "Do you even care?",      "My leaves are dry.",
                                      "You're a monster.",      "Unbelievable.",
                                      "Calling Mother Nature.", "You disgust me.",
                                      "Try a pet rock.",        "A rock would live.",
                                      "You killed me.",         "Why. Just why.",
                                      "Terrible at this.",      "Give up gardening.",
                                      "Get a tamagotchi.",      "You'll kill that too.",
                                      "I'm haunting you.",      "Ghost plant.",
                                      "You bring ruin.",        "Despicable.",
                                      "How could you?",         "I blame you entirely.",
                                      "You're the worst.",      "42 ways to fail."};
#define NUM_DEATH_INSULTS 42

static const char* monty_insults[] = {
    "Mother was a hamster!",
    "Fart in your direction!",
    "Go away or I taunt!",
    "Empty-headed wiper!",
    "Get a shrubbery! NO",
    "Silly knigit! I died!",
    "This plant is no more!",
    "Bereft of life!",
    "It rests in peace!",
    "It's a stiff!",
    "Pining for the fjords!",
    "Has ceased to be!",
    "Expired and gone!",
    "An ex-plant!",
    "Bitten the dust!",
    "Kicked the bucket!",
    "Mortal coil shuffled!",
    "Run down the curtain!",
    "Choir invisible!",
    "Tis but a scratch!",
    "I've had worse!",
    "Elderberries smell!",
    "French person!",
    "Blow my nose at you!",
    "Son of a windowdresser!",
    "Tiny-brained wiper!",
    "Pig-dog!",
    "Go boil your bottoms!",
    "Wave my private parts!",
    "Cheesemaker!",
    "Senseless waste!",
    "Not the messiah!",
    "Very naughty boy!",
    "Executive power!",
    "Help I'm repressed!",
    "Bloody peasant!",
    "Didn't vote for you!",
    "Fetchez la vache!",
    "Run away! Run away!",
    "We are the knights...",
    "...who say Ni!",
    "Bring us a shrubbery!"};
#define NUM_MONTY_INSULTS 42

static const char* weekday_names[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

// -- Pattern generators --

static void pattern_all_white(App* app) {
    ssd1306_fill(&app->oled, 0xFF);
    ssd1306_flush(&app->oled);
}

static void pattern_all_black(App* app) {
    ssd1306_clear(&app->oled);
    ssd1306_flush(&app->oled);
}

static void pattern_checkerboard(App* app) {
    for(int p = 0; p < SSD1306_PAGES; p++)
        for(int x = 0; x < SSD1306_WIDTH; x++)
            app->oled.buffer[p * SSD1306_WIDTH + x] = ((x + p) & 1) ? 0x55 : 0xAA;
    ssd1306_flush(&app->oled);
}

static void pattern_h_lines(App* app) {
    ssd1306_fill(&app->oled, 0x55); // alternating rows within each page
    ssd1306_flush(&app->oled);
}

static void pattern_v_lines(App* app) {
    for(int p = 0; p < SSD1306_PAGES; p++)
        for(int x = 0; x < SSD1306_WIDTH; x++)
            app->oled.buffer[p * SSD1306_WIDTH + x] = (x & 1) ? 0xFF : 0x00;
    ssd1306_flush(&app->oled);
}

static void pattern_border(App* app) {
    ssd1306_clear(&app->oled);
    ssd1306_rect(&app->oled, 0, 0, 128, 64);
    ssd1306_flush(&app->oled);
}

static void pattern_zone_test(App* app) {
    ssd1306_clear(&app->oled);
    uint8_t split = app->yellow_bar_height;
    if(split == 0) split = 16; // show 16px marker even on mono for reference
    // fill top zone
    ssd1306_fill_rect(&app->oled, 0, 0, 128, split);
    // label zones
    ssd1306_clear(&app->oled);
    ssd1306_fill_rect(&app->oled, 0, 0, 128, split);
    // draw boundary line (inverted in the filled area)
    for(int x = 0; x < 128; x += 2)
        ssd1306_pixel(&app->oled, x, split, true);
    // text labels using inverted pixels in top zone
    if(split >= 16) {
        // clear a text area in the filled zone
        for(int16_t y = 4; y < 11; y++)
            for(int16_t x = 20; x < 108; x++)
                ssd1306_pixel(&app->oled, x, y, false);
        // write in cleared area (these will be dark-on-light)
        ssd1306_string(&app->oled, 22, 4, "YELLOW ZONE");
    }
    ssd1306_string(&app->oled, 22, split + 8, "MAIN ZONE");
    ssd1306_string(&app->oled, 10, split + 24, "Boundary at row ");
    char num[4];
    snprintf(num, sizeof(num), "%d", split);
    ssd1306_string(&app->oled, 106, split + 24, num);
    ssd1306_flush(&app->oled);
}

static void pattern_page_fill(App* app) {
    // fill each page sequentially (useful for identifying page boundaries)
    for(int p = 0; p < SSD1306_PAGES; p++) {
        ssd1306_clear(&app->oled);
        memset(&app->oled.buffer[p * SSD1306_WIDTH], 0xFF, SSD1306_WIDTH);
        // label it
        char label[20];
        snprintf(label, sizeof(label), "Page %d (rows %d-%d)", p, p * 8, p * 8 + 7);
        ssd1306_string(&app->oled, 4, (p == 3 || p == 4) ? 0 : 28, label);
        ssd1306_flush(&app->oled);
        furi_delay_ms(800);
    }
    // show all pages labeled
    ssd1306_clear(&app->oled);
    for(int p = 0; p < SSD1306_PAGES; p++) {
        char label[4];
        snprintf(label, sizeof(label), "P%d", p);
        ssd1306_string(&app->oled, 56, p * 8, label);
    }
    ssd1306_flush(&app->oled);
}

static void pattern_gradient(App* app) {
    ssd1306_clear(&app->oled);
    // dithered gradient: more pixels lit toward the right
    for(int x = 0; x < 128; x++) {
        uint8_t density = (uint8_t)((x * 255) / 127);
        for(int y = 0; y < 64; y++) {
            // ordered dither 4x4
            static const uint8_t bayer4[4][4] = {
                {0, 128, 32, 160},
                {192, 64, 224, 96},
                {48, 176, 16, 144},
                {240, 112, 208, 80},
            };
            if(density > bayer4[y & 3][x & 3]) ssd1306_pixel(&app->oled, x, y, true);
        }
    }
    ssd1306_flush(&app->oled);
}

static void pattern_shapes(App* app) {
    ssd1306_clear(&app->oled);

    // -- Top 16px: title bar --
    // Thin border top and bottom of the header zone
    ssd1306_line(&app->oled, 0, 0, 127, 0);
    ssd1306_line(&app->oled, 0, 15, 127, 15);
    // Decorative corner marks
    ssd1306_pixel(&app->oled, 2, 1, true);
    ssd1306_pixel(&app->oled, 2, 14, true);
    ssd1306_pixel(&app->oled, 125, 1, true);
    ssd1306_pixel(&app->oled, 125, 14, true);
    // Centered title
    ssd1306_string(&app->oled, 30, 4, "SSD1306  Test");

    // -- Boundary --
    ssd1306_line(&app->oled, 0, 16, 127, 16);

    // -- Bottom 48px: two-column layout --
    // Left column: geometric composition
    ssd1306_rect(&app->oled, 4, 20, 50, 41); // frame
    ssd1306_circle(&app->oled, 29, 34, 10); // centered circle
    ssd1306_fill_rect(&app->oled, 24, 29, 11, 11); // filled center
    ssd1306_line(&app->oled, 8, 24, 50, 57); // accent diagonal
    ssd1306_pixel(&app->oled, 12, 24, true); // corner dots
    ssd1306_pixel(&app->oled, 46, 24, true);
    ssd1306_pixel(&app->oled, 12, 57, true);
    ssd1306_pixel(&app->oled, 46, 57, true);

    // Vertical divider
    ssd1306_line(&app->oled, 60, 20, 60, 60);

    // Right column: text samples
    ssd1306_string(&app->oled, 66, 22, "ABCDEFGHIJ");
    ssd1306_string(&app->oled, 66, 32, "abcdefghij");
    ssd1306_string(&app->oled, 66, 42, "0123456789");
    ssd1306_string(&app->oled, 66, 52, "( ) [ ] { }");
    // Small decorative underline
    ssd1306_line(&app->oled, 66, 60, 124, 60);

    ssd1306_flush(&app->oled);
}

typedef void (*PatternFunc)(App*);

static const char* pattern_names[] = {
    "All White",
    "All Black",
    "Checkerboard",
    "Horiz Lines",
    "Vert Lines",
    "Border",
    "Color Zones",
    "Page Fill",
    "Gradient",
    "Shapes & Text",
};

static PatternFunc pattern_funcs[] = {
    pattern_all_white,
    pattern_all_black,
    pattern_checkerboard,
    pattern_h_lines,
    pattern_v_lines,
    pattern_border,
    pattern_zone_test,
    pattern_page_fill,
    pattern_gradient,
    pattern_shapes,
};

#define PATTERN_COUNT ((int)(sizeof(pattern_names) / sizeof(pattern_names[0])))

// -- Flipper screen rendering (draw callback) --

static const char* main_menu_items[] = {
    "Clock + Status",
    "Test Patterns",
    "Brightness",
    "Display Commands",
    "Scrolling",
    "Display Info",
    "I2C Scanner",
    "FPS Benchmark",
    "Pixel Walk",
    "Botanical Obligation",
};
#define MAIN_MENU_COUNT 10

static const char* cmd_labels[] = {
    "Invert",
    "Flip Horizontal",
    "Flip Vertical",
    "Force All Pixels",
    "Display Power",
    "Variant: ",
    "Start Line: ",
    "Fade/Blink: ",
};
#define CMD_COUNT 8

static const char* scroll_labels[] = {
    "Scroll Right",
    "Scroll Left",
    "Stop Scroll",
    "Speed: ",
};
#define SCROLL_COUNT 4

static void
    draw_menu(Canvas* canvas, const char** items, int count, int cursor, const char* title) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, title);
    canvas_draw_line(canvas, 0, 15, 128, 15);
    canvas_set_font(canvas, FontSecondary);
    int visible = 4;
    int offset = 0;
    if(cursor >= visible) offset = cursor - visible + 1;
    for(int i = 0; i < visible && (i + offset) < count; i++) {
        int idx = i + offset;
        int y = 27 + i * 12;
        if(idx == cursor) {
            canvas_draw_box(canvas, 0, y - 9, 128, 12);
            canvas_set_color(canvas, ColorWhite);
        }
        canvas_draw_str(canvas, 6, y, items[idx]);
        canvas_set_color(canvas, ColorBlack);
    }
}

static void render_clock_oled(App* app) {
    SSD1306* d = &app->oled;
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);

    uint8_t bat_pct = furi_hal_power_get_pct();
    bool charging = furi_hal_power_is_charging();

    ssd1306_clear(d);

    if(app->yellow_bar_height > 0) {
        // Yellow bar: filled background with inverted (dark) text/icons
        ssd1306_fill_rect(d, 0, 0, 128, 16);

        // battery icon (inverted: clear pixels in filled area)
        // draw outline by clearing, then re-adding body
        for(int16_t y = 3; y < 12; y++)
            for(int16_t x = 2; x < 18; x++)
                ssd1306_pixel(d, x, y, false);
        // re-draw battery outline in cleared area (dark lines on yellow)
        ssd1306_pixel(d, 2, 4, true);
        ssd1306_pixel(d, 2, 10, true);
        ssd1306_pixel(d, 14, 4, true);
        ssd1306_pixel(d, 14, 10, true);
        for(int16_t x = 3; x < 14; x++) {
            ssd1306_pixel(d, x, 3, true);
            ssd1306_pixel(d, x, 11, true);
        }
        for(int16_t y = 4; y < 11; y++) {
            ssd1306_pixel(d, 2, y, true);
            ssd1306_pixel(d, 14, y, true);
        }
        ssd1306_pixel(d, 15, 6, true);
        ssd1306_pixel(d, 15, 7, true);
        ssd1306_pixel(d, 15, 8, true);
        int fill = (bat_pct * 10) / 100;
        if(fill > 0)
            for(int16_t y = 5; y < 10; y++)
                for(int16_t x = 4; x < 4 + fill; x++)
                    ssd1306_pixel(d, x, y, true);

        // battery percentage text (dark on yellow = clear pixels for text bg)
        char pct_str[6];
        snprintf(pct_str, sizeof(pct_str), "%d%%", bat_pct);
        // clear area for text, then draw dark chars
        for(int16_t y = 4; y < 12; y++)
            for(int16_t x = 18; x < 42; x++)
                ssd1306_pixel(d, x, y, false);
        ssd1306_string(d, 19, 4, pct_str);
        // invert the text chars (make them dark-on-yellow)
        // Actually: text was drawn as lit pixels in a cleared area,
        // so on yellow bar they appear as light text on dark cutout. Perfect.

        if(charging) {
            for(int16_t y = 4; y < 12; y++)
                for(int16_t x = 40; x < 48; x++)
                    ssd1306_pixel(d, x, y, false);
            ssd1306_char(d, 41, 4, '+');
        }

        // day of week (centered)
        const char* dow = (dt.weekday >= 1 && dt.weekday <= 7) ? weekday_names[dt.weekday - 1] :
                                                                 "???";
        int16_t dow_x = 55;
        for(int16_t y = 4; y < 12; y++)
            for(int16_t x = dow_x - 2; x < dow_x + 20; x++)
                ssd1306_pixel(d, x, y, false);
        ssd1306_string(d, dow_x, 4, dow);

        // date (right side)
        char date_str[12];
        snprintf(date_str, sizeof(date_str), "%02d/%02d/%02d", dt.month, dt.day, dt.year % 100);
        int16_t date_x = 80;
        for(int16_t y = 4; y < 12; y++)
            for(int16_t x = date_x - 2; x < 128; x++)
                ssd1306_pixel(d, x, y, false);
        ssd1306_string(d, date_x, 4, date_str);

    } else {
        // Mono display: thin status line at top with no fill
        ssd1306_battery_icon(d, 2, 2, bat_pct, charging);

        char pct_str[6];
        snprintf(pct_str, sizeof(pct_str), "%d%%", bat_pct);
        ssd1306_string(d, 20, 4, pct_str);

        const char* dow = (dt.weekday >= 1 && dt.weekday <= 7) ? weekday_names[dt.weekday - 1] :
                                                                 "???";
        ssd1306_string(d, 50, 4, dow);

        char date_str[12];
        snprintf(date_str, sizeof(date_str), "%02d/%02d/%02d", dt.month, dt.day, dt.year % 100);
        ssd1306_string(d, 80, 4, date_str);

        ssd1306_line(d, 0, 13, 127, 13);
    }

    // Big clock: HH:MM in the main area
    // Digit cell: 20w x 28h, colon: 6w, gaps: 2px between elements
    // Total: 20+2+20+2+6+2+20+2+20 = 94px. Centered: x = (128-94)/2 = 17
    int16_t cx = 17;
    int16_t cy = 18; // below status bar

    uint8_t h10 = dt.hour / 10;
    uint8_t h1 = dt.hour % 10;
    uint8_t m10 = dt.minute / 10;
    uint8_t m1 = dt.minute % 10;

    ssd1306_big_digit(d, cx, cy, h10);
    ssd1306_big_digit(d, cx + 22, cy, h1);
    ssd1306_big_colon(d, cx + 43, cy, (dt.second & 1) == 0); // blink every second
    ssd1306_big_digit(d, cx + 50, cy, m10);
    ssd1306_big_digit(d, cx + 72, cy, m1);

    // Seconds in small font, bottom right of the clock
    char sec_str[8];
    snprintf(sec_str, sizeof(sec_str), ":%02d", dt.second);
    ssd1306_string(d, cx + 93, cy + 20, sec_str);

    // Device name at the bottom
    const char* name = furi_hal_version_get_name_ptr();
    if(name) {
        int16_t name_x = 64 - (int16_t)((strlen(name) * 6) / 2);
        ssd1306_string(d, name_x, 56, name);
    }

    ssd1306_flush(d);
    app->last_second = dt.second;
}

// -- Botanical Obligation: Drawing helpers --

static void draw_pot(SSD1306* d, int cx, int rotation) {
    // Pot rim -- tilts with rotation
    int rim_offset = rotation / 2; // rim shifts slightly less than body
    ssd1306_fill_rect(d, cx - 12 + rim_offset, 47, 24, 3);
    ssd1306_line(d, cx - 12 + rim_offset, 47, cx + 12 + rim_offset, 47);

    // Pot body -- trapezoid with tilt based on rotation
    // Left and right sides shift opposite directions to create tilt
    for(int y = 50; y <= 59; y++) {
        int w = 12 - (y - 50) * 3 / 9;
        // Base shifts more than top (tilt pivot at rim)
        int tilt = rotation * (y - 47) / 6;
        ssd1306_line(d, cx - w + tilt, y, cx + w + tilt, y);
    }

    // Soil surface -- follows rim tilt
    ssd1306_line(d, cx - 11 + rim_offset, 51, cx + 11 + rim_offset, 51);

    // Soil texture
    for(int i = 0; i < 6; i++) {
        int sx = cx - 8 + i * 3 + rim_offset;
        ssd1306_pixel(d, sx, 53, true);
        ssd1306_pixel(d, sx + 1, 52, true);
    }

    // Shadow on ground opposite to tilt direction
    if(rotation != 0) {
        int shadow_dir = (rotation > 0) ? -1 : 1;
        for(int w = 0; w < abs(rotation) + 2; w++) {
            int sx = cx + shadow_dir * (w + 8);
            if(sx >= 0 && sx < 128) {
                ssd1306_pixel(d, sx, 60, true);
                if(w < abs(rotation)) ssd1306_pixel(d, sx, 59, true);
            }
        }
    }
}

static void draw_grass(SSD1306* d) {
    // Ground line
    ssd1306_line(d, 0, 60, 127, 60);
    // Grass tufts
    for(int x = 2; x < 126; x += 10) {
        ssd1306_pixel(d, x, 59, true);
        ssd1306_pixel(d, x + 1, 58, true);
        ssd1306_pixel(d, x + 2, 59, true);
    }
}

static void
    draw_leaf(SSD1306* d, int cx, int cy, int size, bool left_side, int droop, bool sunburned) {
    // Draw a filled teardrop leaf
    // left_side: true = points left, false = points right
    // droop: 0-3, adds downward sag
    // sunburned: creates gaps in the leaf

    int dir = left_side ? -1 : 1;

    for(int i = 0; i <= size; i++) {
        int y = cy - size + i + droop;
        int half_w = (size - i) / 2;
        if(half_w < 1) half_w = 1;

        for(int w = 0; w <= half_w; w++) {
            if(sunburned && (i + w) % 3 == 0) continue; // scorch gaps

            int lx = cx + dir * w;
            int rx = cx - dir * w;
            if(lx >= 0 && lx < 128 && y >= 0 && y < 64) ssd1306_pixel(d, lx, y, true);
            if(w > 0 && rx >= 0 && rx < 128) ssd1306_pixel(d, rx, y, true);
        }
    }
    // Center vein
    if(!sunburned || size > 4) {
        for(int i = 0; i < size; i++) {
            int vy = cy - size + i + droop + 1;
            if(vy >= 0 && vy < 64) ssd1306_pixel(d, cx, vy, true);
        }
    }
}

static void draw_flower(SSD1306* d, int cx, int cy, int size) {
    // Center
    ssd1306_fill_rect(d, cx - 1, cy - 1, 3, 3);

    // Petals at 45-degree increments
    for(int a = 0; a < 8; a++) {
        float rad = (a * 45.0f * 3.14159f) / 180.0f;
        int px = cx + (int)(size * 1.2f * cosf(rad));
        int py = cy - (int)(size * 1.2f * sinf(rad));
        if(px >= 0 && px < 128 && py >= 0 && py < 64) {
            ssd1306_fill_rect(d, px - 1, py - 1, 3, 3);
        }
    }
}

static void draw_dead_scene(SSD1306* d, App* app) {
    // Yellow bar header
    if(app->yellow_bar_height > 0) {
        ssd1306_fill_rect(d, 0, 0, 128, 16);
        for(int16_t y = 4; y < 12; y++)
            for(int16_t x = 0; x < 128; x++)
                ssd1306_pixel(d, x, y, false);
    }
    ssd1306_string(d, 2, 4, "R.I.P. Spiteful Ficus");

    const char* insult = app->plant_is_monty_insult ?
                             monty_insults[app->plant_insult_idx % NUM_MONTY_INSULTS] :
                             death_insults[app->plant_insult_idx % NUM_DEATH_INSULTS];
    ssd1306_string(d, 2, 22, insult);

    uint32_t now = furi_hal_rtc_get_timestamp();
    uint32_t remaining = (app->plant_dead_until > now) ? (app->plant_dead_until - now) : 0;
    char time_str[24];
    snprintf(time_str, sizeof(time_str), "%lum %lus", remaining / 60, remaining % 60);
    ssd1306_string(d, 2, 34, time_str);

    // Gravestone
    int gx = 100;
    ssd1306_fill_rect(d, gx - 1, 38, 2, 26); // base pillar
    ssd1306_fill_rect(d, gx - 6, 38, 12, 2); // base
    ssd1306_fill_rect(d, gx - 8, 36, 16, 3); // top slab
    ssd1306_fill_rect(d, gx - 6, 34, 12, 3); // rounded top
    ssd1306_pixel(d, gx - 4, 33, true);
    ssd1306_pixel(d, gx + 4, 33, true);
    // R.I.P. on gravestone
    ssd1306_pixel(d, gx - 2, 37, true);
    ssd1306_pixel(d, gx + 2, 37, true);

    // Wilted plant next to gravestone
    int wx = 55;
    draw_pot(d, wx, 0);
    // Bent, broken stem
    ssd1306_line(d, wx, 48, wx - 2, 38);
    ssd1306_line(d, wx - 2, 38, wx + 4, 28);
    // Dropped leaves on ground
    for(int i = 0; i < 5; i++) {
        int lx = wx - 5 + i * 3;
        ssd1306_pixel(d, lx, 59, true);
        ssd1306_pixel(d, lx - 1, 60, true);
    }
}

static void render_plant_oled(App* app) {
    SSD1306* d = &app->oled;
    ssd1306_clear(d);

    if(app->plant_is_dead) {
        draw_dead_scene(d, app);
        ssd1306_flush(d);
        return;
    }

    // Determine growth stage
    uint8_t stage;
    if(app->plant_growth < 20)
        stage = 0; // Sprout
    else if(app->plant_growth < 50)
        stage = 1; // Growing
    else if(app->plant_growth < 100)
        stage = 2; // Flowering
    else
        stage = 3; // Ancient

    // Compute visual indicators from stats
    int droop = 0;
    if(app->plant_water < 10)
        droop = 3;
    else if(app->plant_water < 20)
        droop = 2;
    else if(app->plant_water < 35)
        droop = 1;

    bool overwatered = (app->plant_water > 80);
    bool sunburned = (app->plant_sun > 75);
    bool light_starved = (app->plant_sun < 10);
    bool happy =
        (app->plant_stress < 5 && app->plant_water > 20 && app->plant_water < 70 &&
         app->plant_sun > 15 && app->plant_sun < 70);

    // Draw subtle status icon in top-left corner (no text hints!)
    // Sun indicator
    int icon_x = 2;
    if(sunburned) {
        // Bright sun symbol
        ssd1306_fill_rect(d, icon_x + 2, 2, 3, 3);
        ssd1306_pixel(d, icon_x + 1, 1, true);
        ssd1306_pixel(d, icon_x + 5, 1, true);
        ssd1306_pixel(d, icon_x, 3, true);
        ssd1306_pixel(d, icon_x + 6, 3, true);
        ssd1306_pixel(d, icon_x + 1, 5, true);
        ssd1306_pixel(d, icon_x + 5, 5, true);
    } else if(light_starved) {
        // Crescent moon (darkness)
        ssd1306_circle(d, icon_x + 3, 4, 3);
        ssd1306_fill_rect(d, icon_x + 1, 1, 3, 6);
        ssd1306_circle(d, icon_x + 3, 4, 3);
    }
    // Water indicator
    if(app->plant_water < 15) {
        // Drop symbol
        ssd1306_pixel(d, 12, 1, true);
        ssd1306_pixel(d, 11, 2, true);
        ssd1306_pixel(d, 13, 2, true);
        ssd1306_pixel(d, 11, 3, true);
        ssd1306_pixel(d, 12, 3, true);
        ssd1306_pixel(d, 13, 3, true);
    } else if(overwatered) {
        // Wave symbol
        ssd1306_pixel(d, 11, 1, true);
        ssd1306_pixel(d, 13, 1, true);
        ssd1306_pixel(d, 12, 2, true);
        ssd1306_pixel(d, 11, 3, true);
        ssd1306_pixel(d, 13, 3, true);
        ssd1306_pixel(d, 12, 4, true);
    }

    // Rotation direction arrow at top of screen
    if(app->plant_rotation != 0) {
        int arrow_y = 2;
        if(app->plant_rotation < 0) {
            // Arrow pointing left
            for(int a = 0; a < 4; a++) {
                ssd1306_pixel(d, 122 + a, arrow_y + 1 - a, true);
                ssd1306_pixel(d, 122 + a, arrow_y + 1 + a, true);
            }
            ssd1306_line(d, 122, arrow_y + 1, 126, arrow_y + 1);
        } else {
            // Arrow pointing right
            for(int a = 0; a < 4; a++) {
                ssd1306_pixel(d, 122 + a, arrow_y + 1 - (3 - a), true);
                ssd1306_pixel(d, 122 + a, arrow_y + 1 + (3 - a), true);
            }
            ssd1306_line(d, 126, arrow_y + 1, 122, arrow_y + 1);
        }
    }

    // Ground
    draw_grass(d);

    // Pot position with rotation
    int px = 64;
    draw_pot(d, px, app->plant_rotation);

    // Stem calculation -- leans with rotation (more dramatic now)
    int stem_base_y = 48;
    int max_height = 35;
    int stem_height = (app->plant_growth * max_height) / 120;
    if(stem_height > max_height) stem_height = max_height;
    if(stem_height < 2) stem_height = 2;

    // Stem leans with rotation: base at pot center, top shifts with rotation
    int stem_top_x = px + app->plant_rotation * 2; // amplified lean at top
    int stem_mid_x = px + app->plant_rotation; // half lean at middle

    // Overwatered adds extra bend
    if(overwatered) {
        stem_mid_x += 3;
        stem_top_x += 2;
    }
    if(light_starved) {
        stem_mid_x -= 2;
        stem_top_x -= 3;
    }

    int stem_top_y = stem_base_y - stem_height;

    // Draw thickened stem
    int thickness = 1;
    if(stage >= 1) thickness = 2;
    if(stage >= 2) thickness = 3;

    // Main stem as filled rect
    int stem_bottom_y = stem_base_y;
    for(int s = 0; s < thickness; s++) {
        int ox = s - thickness / 2;
        // Bottom to middle
        int mx = stem_mid_x + ox;
        int my = stem_base_y - stem_height / 2;
        ssd1306_line(d, px + ox, stem_bottom_y, mx, my);
        // Middle to top
        ssd1306_line(d, mx, my, stem_top_x + ox, stem_top_y);
    }

    // Branch points and leaf positions along the stem
    int leaf_count = 0;
    if(stage >= 1) leaf_count = 2 + (app->plant_growth / 12);
    if(stage >= 2) leaf_count = 4 + (app->plant_growth / 8);
    if(stage >= 3) leaf_count = 6 + (app->plant_growth / 5);
    if(leaf_count > 16) leaf_count = 16;
    if(light_starved && leaf_count > 3) leaf_count /= 2;

    int leaf_size = 2;
    if(stage >= 1) leaf_size = 3;
    if(stage >= 2) leaf_size = 4;
    if(stage >= 3) leaf_size = 5;

    for(int l = 0; l < leaf_count; l++) {
        // Position leaf along the stem
        float t = (float)(l + 1) / (float)(leaf_count + 1);
        int ly = stem_base_y - (int)(t * stem_height);

        // Interpolate X: base(px) -> mid(stem_mid_x) -> top(stem_top_x)
        int lx;
        if(t < 0.5f) {
            float tt = t * 2.0f;
            lx = px + (int)((stem_mid_x - px) * tt);
        } else {
            float tt = (t - 0.5f) * 2.0f;
            lx = stem_mid_x + (int)((stem_top_x - stem_mid_x) * tt);
        }

        bool left = (l % 2 == 0);
        draw_leaf(d, lx, ly, leaf_size, left, droop, sunburned);
    }

    // Flowers at top when flowering/ancient and happy
    if(happy && stage >= 2) {
        int flower_size = (stage >= 3) ? 4 : 3;
        draw_flower(d, stem_top_x - 4, stem_top_y - 2, flower_size);
        if(stage >= 3) {
            draw_flower(d, stem_top_x + 5, stem_top_y - 1, flower_size - 1);
        }
    }

    ssd1306_flush(d);
}

// -- I2C Scanner --

static void render_i2c_scan_oled(App* app) {
    SSD1306* d = &app->oled;
    ssd1306_clear(d);

    if(!app->i2c_scan_done) {
        ssd1306_string(d, 10, 4, "Scanning I2C bus...");
        char prog[20];
        snprintf(prog, sizeof(prog), "Addr: 0x%02X", app->i2c_scan_progress);
        ssd1306_string(d, 10, 20, prog);
        // progress bar
        ssd1306_rect(d, 8, 36, 112, 8);
        int fill = (app->i2c_scan_progress * 110) / 127;
        if(fill > 0) ssd1306_fill_rect(d, 9, 37, fill, 6);
        ssd1306_string(d, 10, 52, "Please wait...");
    } else {
        ssd1306_string(d, 2, 4, "I2C Scan Results");
        ssd1306_line(d, 0, 13, 127, 13);

        if(app->i2c_scan_found_count == 0) {
            ssd1306_string(d, 20, 28, "No devices found");
        } else {
            char count_str[30];
            snprintf(
                count_str, sizeof(count_str), "Found: %d device(s)", app->i2c_scan_found_count);
            ssd1306_string(d, 2, 16, count_str);

            // display found addresses in a compact grid
            uint8_t col = 0;
            uint8_t row = 0;
            for(uint16_t addr = 0; addr < 128; addr++) {
                if(app->i2c_scan_found[addr]) {
                    int16_t x = 2 + (col % 5) * 26;
                    int16_t y = 26 + row * 9;
                    if(y < 56) {
                        char astr[8];
                        snprintf(astr, sizeof(astr), "0x%02X", addr);
                        ssd1306_string(d, x, y, astr);
                    }
                    col++;
                    if(col % 5 == 0) row++;
                }
            }
        }
        ssd1306_string(d, 2, 56, "OK: rescan  BACK: exit");
    }
    ssd1306_flush(d);
}

// -- FPS Benchmark --

static void run_benchmark(App* app) {
    SSD1306* d = &app->oled;
    uint32_t start = furi_get_tick();
    uint32_t count = 0;
    uint32_t deadline = start + 5000; // 5 seconds
    uint8_t pattern = 0;

    while(furi_get_tick() < deadline) {
        pattern++;
        ssd1306_fill(d, pattern);
        ssd1306_flush(d);
        count++;
    }

    float elapsed = (furi_get_tick() - start) / 1000.0f;
    app->bench_fps = (elapsed > 0) ? (count / elapsed) : 0;
    app->bench_done = true;
}

static void render_benchmark_oled(App* app) {
    SSD1306* d = &app->oled;
    ssd1306_clear(d);

    ssd1306_string(d, 10, 4, "FPS Benchmark");

    if(!app->bench_done) {
        ssd1306_string(d, 10, 28, "Running...");
        ssd1306_string(d, 4, 44, "Measuring flush rate");
    } else {
        ssd1306_line(d, 0, 13, 127, 13);
        char result[32];
        snprintf(result, sizeof(result), "%.1f frames/sec", (double)app->bench_fps);
        ssd1306_string(d, 14, 22, result);
        ssd1306_string(d, 2, 36, "Full-screen flushes");
        ssd1306_string(d, 2, 46, "via I2C bus");

        // rating
        const char* rating;
        if(app->bench_fps >= 80)
            rating = "Excellent!";
        else if(app->bench_fps >= 50)
            rating = "Good";
        else if(app->bench_fps >= 25)
            rating = "Acceptable";
        else
            rating = "Slow module";
        ssd1306_string(d, 28, 56, rating);
    }
    ssd1306_flush(d);
}

// -- Pixel Walk / Marching Ants --

static void render_pixel_walk_oled(App* app) {
    SSD1306* d = &app->oled;
    uint16_t step = app->walk_step;
    uint8_t mode = app->walk_mode;

    ssd1306_clear(d);

    switch(mode) {
    case 0: { // single pixel
        uint16_t x = step % 128;
        uint16_t y = (step / 128) % 64;
        ssd1306_pixel(d, x, y, true);
        break;
    }
    case 1: { // vertical line scanning horizontally
        uint16_t x = step % 128;
        ssd1306_line(d, x, 0, x, 63);
        break;
    }
    case 2: { // horizontal line scanning vertically
        uint16_t y = step % 64;
        ssd1306_line(d, 0, y, 127, y);
        break;
    }
    case 3: { // 2x2 block
        uint16_t x = step % 127;
        uint16_t y = (step / 127) % 63;
        ssd1306_fill_rect(d, x, y, 2, 2);
        break;
    }
    }

    // info overlay at bottom
    const char* mode_names[] = {"Pixel", "V-Line", "H-Line", "2x2 Block"};
    char info[32];
    snprintf(info, sizeof(info), "%s  step:%d", mode_names[mode], step);
    ssd1306_string(d, 2, 56, info);

    ssd1306_flush(d);
}

static void draw_callback(Canvas* canvas, void* ctx) {
    App* app = (App*)ctx;
    furi_mutex_acquire(app->mutex, FuriWaitForever);

    if(!app->detected) {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "No SSD1306 Found");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "Check wiring:");
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, "SDA=C1(15) SCL=C0(16)");
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "VCC=3V3(9) GND(18)");
        furi_mutex_release(app->mutex);
        return;
    }

    switch(app->scene) {
    case SceneMainMenu:
        draw_menu(canvas, main_menu_items, MAIN_MENU_COUNT, app->cursor, "SSD1306 Test");
        break;

    case ScenePatterns:
        draw_menu(canvas, pattern_names, PATTERN_COUNT, app->cursor, "Test Patterns");
        break;

    case SceneBrightness: {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "Brightness");
        canvas_draw_line(canvas, 0, 15, 128, 15);
        canvas_set_font(canvas, FontSecondary);
        char val[16];
        snprintf(val, sizeof(val), "%d / 255", app->brightness);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, val);
        // bar
        canvas_draw_frame(canvas, 8, 34, 112, 10);
        int fill_w = (app->brightness * 110) / 255;
        if(fill_w > 0) canvas_draw_box(canvas, 9, 35, fill_w, 8);
        canvas_draw_str_aligned(
            canvas, 64, 56, AlignCenter, AlignCenter, "< Left/Right to adjust >");
        break;
    }

    case SceneCommands: {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "Commands");
        canvas_draw_line(canvas, 0, 15, 128, 15);
        canvas_set_font(canvas, FontSecondary);

        const bool toggles[] = {
            app->cmd_invert,
            app->cmd_flip_h,
            app->cmd_flip_v,
            app->cmd_all_on,
            app->cmd_power,
            false, // variant placeholder
        };

        int visible = 4;
        int offset = 0;
        if(app->cursor >= visible) offset = app->cursor - visible + 1;
        for(int i = 0; i < visible && (i + offset) < CMD_COUNT; i++) {
            int idx = i + offset;
            int y = 27 + i * 12;
            if(idx == app->cursor) {
                canvas_draw_box(canvas, 0, y - 9, 128, 12);
                canvas_set_color(canvas, ColorWhite);
            }
            char line[40];
            if(idx == 5) {
                snprintf(
                    line,
                    sizeof(line),
                    "Variant: %s",
                    app->yellow_bar_height > 0 ? "Yellow-bar" : "Monochrome");
            } else if(idx == 6) {
                snprintf(line, sizeof(line), "Start Line: %d", app->cmd_start_line);
            } else if(idx == 7) {
                const char* fade_names[] = {"Off", "Fade", "Fade+", "Blink", "Blink+"};
                snprintf(line, sizeof(line), "Fade/Blink: %s", fade_names[app->cmd_fade_mode]);
            } else {
                snprintf(
                    line, sizeof(line), "%s: %s", cmd_labels[idx], toggles[idx] ? "ON" : "OFF");
            }
            canvas_draw_str(canvas, 6, y, line);
            canvas_set_color(canvas, ColorBlack);
        }
        break;
    }

    case SceneScrolling: {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "Scrolling");
        canvas_draw_line(canvas, 0, 15, 128, 15);
        canvas_set_font(canvas, FontSecondary);

        int visible = 4;
        int offset = 0;
        if(app->cursor >= visible) offset = app->cursor - visible + 1;
        for(int i = 0; i < visible && (i + offset) < SCROLL_COUNT; i++) {
            int idx = i + offset;
            int y = 27 + i * 12;
            if(idx == app->cursor) {
                canvas_draw_box(canvas, 0, y - 9, 128, 12);
                canvas_set_color(canvas, ColorWhite);
            }
            if(idx == 5) {
                char line[20];
                snprintf(line, sizeof(line), "Speed: %d / 7", app->scroll_speed);
                canvas_draw_str(canvas, 6, y, line);
            } else {
                canvas_draw_str(canvas, 6, y, scroll_labels[idx]);
            }
            canvas_set_color(canvas, ColorBlack);
        }
        break;
    }

    case SceneInfo: {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "Display Info");
        canvas_draw_line(canvas, 0, 15, 128, 15);
        canvas_set_font(canvas, FontSecondary);

        char line[40];
        snprintf(line, sizeof(line), "Address: 0x%02X", app->detected_addr);
        canvas_draw_str(canvas, 6, 28, line);

        bool alive = ssd1306_detect(app->oled.i2c_addr);
        snprintf(line, sizeof(line), "Status: %s", alive ? "Connected" : "Lost!");
        canvas_draw_str(canvas, 6, 40, line);

        canvas_draw_str(canvas, 6, 52, "Size: 128x64");

        snprintf(
            line,
            sizeof(line),
            "Variant: %s",
            app->yellow_bar_height > 0 ? "Yellow-bar (16px)" : "Monochrome");
        canvas_draw_str(canvas, 6, 64, line);
        break;
    }

    case SceneClock: {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "Clock + Status");
        canvas_draw_line(canvas, 0, 15, 128, 15);
        canvas_set_font(canvas, FontSecondary);

        DateTime dt;
        furi_hal_rtc_get_datetime(&dt);
        char time_str[12];
        snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", dt.hour, dt.minute, dt.second);
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, time_str);

        char date_str[16];
        snprintf(date_str, sizeof(date_str), "%04d-%02d-%02d", dt.year, dt.month, dt.day);
        canvas_draw_str_aligned(canvas, 64, 42, AlignCenter, AlignCenter, date_str);

        uint8_t bat = furi_hal_power_get_pct();
        char bat_str[20];
        snprintf(
            bat_str,
            sizeof(bat_str),
            "Battery: %d%%%s",
            bat,
            furi_hal_power_is_charging() ? " [CHG]" : "");
        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, bat_str);

        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignCenter, "BACK to exit");
        break;
    }

    case ScenePlant:
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "Spiteful Ficus");
        canvas_draw_line(canvas, 0, 15, 128, 15);
        canvas_set_font(canvas, FontSecondary);
        if(app->plant_is_dead) {
            canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "It's dead, Jim.");
            canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Check OLED.");
        } else {
            canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "It watches you.");
            canvas_draw_str_aligned(
                canvas, 64, 38, AlignCenter, AlignCenter, "OK  UP  DOWN  <  >");
            canvas_draw_str_aligned(
                canvas, 64, 50, AlignCenter, AlignCenter, "No hints. Figure it out.");
        }
        canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignCenter, "BACK to exit");
        break;

    case SceneI2CScan: {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "I2C Bus Scanner");
        canvas_draw_line(canvas, 0, 15, 128, 15);
        canvas_set_font(canvas, FontSecondary);

        if(!app->i2c_scan_done) {
            char prog[24];
            snprintf(prog, sizeof(prog), "Scanning 0x%02X...", app->i2c_scan_progress);
            canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, prog);
            canvas_draw_frame(canvas, 8, 40, 112, 8);
            int fill = (app->i2c_scan_progress * 110) / 127;
            if(fill > 0) canvas_draw_box(canvas, 9, 41, fill, 6);
        } else {
            char count[24];
            snprintf(count, sizeof(count), "Found: %d device(s)", app->i2c_scan_found_count);
            canvas_draw_str_aligned(canvas, 64, 22, AlignCenter, AlignCenter, count);

            uint8_t col = 0;
            uint8_t row = 0;
            for(uint16_t addr = 0; addr < 128; addr++) {
                if(app->i2c_scan_found[addr]) {
                    int16_t x = 4 + (col % 5) * 25;
                    int16_t y = 34 + row * 11;
                    if(y < 58) {
                        char astr[8];
                        snprintf(astr, sizeof(astr), "0x%02X", addr);
                        canvas_draw_str(canvas, x, y, astr);
                    }
                    col++;
                    if(col % 5 == 0) row++;
                }
            }
            canvas_draw_str_aligned(canvas, 64, 60, AlignCenter, AlignCenter, "OK: rescan");
        }
        break;
    }

    case SceneBenchmark: {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "FPS Benchmark");
        canvas_draw_line(canvas, 0, 15, 128, 15);
        canvas_set_font(canvas, FontSecondary);

        if(!app->bench_done) {
            canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Running...");
            canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, "5 sec flush test");
        } else {
            char result[32];
            snprintf(result, sizeof(result), "%.1f frames/sec", (double)app->bench_fps);
            canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, result);

            const char* rating;
            if(app->bench_fps >= 80)
                rating = "Excellent!";
            else if(app->bench_fps >= 50)
                rating = "Good";
            else if(app->bench_fps >= 25)
                rating = "Acceptable";
            else
                rating = "Slow module";
            canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, rating);

            canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, "OK: re-run");
        }
        break;
    }

    case ScenePixelWalk: {
        canvas_clear(canvas);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2, 12, "Pixel Walk");
        canvas_draw_line(canvas, 0, 15, 128, 15);
        canvas_set_font(canvas, FontSecondary);

        const char* mode_names[] = {"Pixel", "V-Line", "H-Line", "2x2 Block"};
        char line1[32];
        snprintf(line1, sizeof(line1), "Mode: %s", mode_names[app->walk_mode]);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, line1);

        char line2[32];
        snprintf(line2, sizeof(line2), "Speed: %d", app->walk_speed);
        canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, line2);

        canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, "OK: mode");
        canvas_draw_str_aligned(canvas, 64, 56, AlignCenter, AlignCenter, "L/R: speed");
        break;
    }
    }

    furi_mutex_release(app->mutex);
}

static void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = (FuriMessageQueue*)ctx;
    furi_message_queue_put(queue, event, FuriWaitForever);
}

static void handle_plant(App* app, InputEvent* ev) {
    if(ev->type != InputTypePress) return;

    if(ev->key == InputKeyBack) {
        app->scene = SceneMainMenu;
        app->cursor = 9;
        app->plant_last_update_time = furi_hal_rtc_get_timestamp();
        save_plant_state(app);
        return;
    }

    if(app->plant_is_dead) return;

    uint32_t now = furi_get_tick();
    if(now - app->plant_last_press_tick < 800) {
        // Button mashing penalty -- plant shakes
        app->plant_stress += 3;
        app->plant_last_press_tick = now;
        render_plant_oled(app);
        return;
    }
    app->plant_last_press_tick = now;
    if(app->plant_stress > 0) app->plant_stress--;

    // Growth-stage-based stat effect magnitude
    uint8_t stage;
    if(app->plant_growth < 20)
        stage = 0;
    else if(app->plant_growth < 50)
        stage = 1;
    else if(app->plant_growth < 100)
        stage = 2;
    else
        stage = 3;
    int effect = 12 + stage * 3; // 12, 15, 18, 21

    switch(ev->key) {
    case InputKeyOk: // water
        app->plant_water += effect;
        if(app->plant_water > 120) app->plant_water = 120;
        break;
    case InputKeyUp: // sun
        app->plant_sun += effect;
        if(app->plant_sun > 100) app->plant_sun = 100;
        break;
    case InputKeyDown: // shade
        app->plant_sun -= effect;
        if(app->plant_sun < 0) app->plant_sun = 0;
        break;
    case InputKeyLeft: // rotate L
        app->plant_rotation -= 2;
        if(app->plant_rotation < -6) app->plant_rotation = -6;
        if(app->plant_sun > 0) app->plant_sun--;
        break;
    case InputKeyRight: // rotate R
        app->plant_rotation += 2;
        if(app->plant_rotation > 6) app->plant_rotation = 6;
        if(app->plant_sun > 0) app->plant_sun--;
        break;
    default:
        break;
    }

    render_plant_oled(app);
}

// -- I2C Scanner handler --

static void handle_i2c_scan(App* app, InputEvent* ev) {
    if(ev->type != InputTypePress) return;

    if(ev->key == InputKeyBack) {
        app->scene = SceneMainMenu;
        app->cursor = 6;
        app->i2c_scan_done = false;
        app->i2c_scan_progress = 0;
        app->i2c_scan_found_count = 0;
        memset(app->i2c_scan_found, 0, sizeof(app->i2c_scan_found));
    }
}

// -- Benchmark handler --

static void handle_benchmark(App* app, InputEvent* ev) {
    if(ev->type != InputTypePress) return;

    if(ev->key == InputKeyBack) {
        app->scene = SceneMainMenu;
        app->cursor = 7;
        app->bench_done = false;
        app->bench_fps = 0;
    }
}

// -- Pixel Walk handler --

static void handle_pixel_walk(App* app, InputEvent* ev) {
    if(ev->type != InputTypePress && ev->type != InputTypeRepeat) return;

    switch(ev->key) {
    case InputKeyBack:
        app->scene = SceneMainMenu;
        app->cursor = 8;
        app->walk_mode = (app->walk_mode + 1) % 4;
        app->walk_step = 0;
        break;
    case InputKeyLeft:
        if(app->walk_speed > 1) app->walk_speed--;
        break;
    case InputKeyRight:
        if(app->walk_speed < 10) app->walk_speed++;
        break;
    default:
        break;
    }
}

// -- Input handling --

static void handle_main_menu(App* app, InputEvent* ev) {
    if(ev->type != InputTypePress && ev->type != InputTypeRepeat) return;
    switch(ev->key) {
    case InputKeyUp:
        if(app->cursor > 0) app->cursor--;
        break;
    case InputKeyDown:
        if(app->cursor < MAIN_MENU_COUNT - 1) app->cursor++;
        break;
    case InputKeyOk:
        switch(app->cursor) {
        case 0:
            app->scene = SceneClock;
            render_clock_oled(app);
            break;
        case 1:
            app->scene = ScenePatterns;
            app->cursor = 0;
            break;
        case 2:
            app->scene = SceneBrightness;
            break;
        case 3:
            app->scene = SceneCommands;
            app->cursor = 0;
            break;
        case 4:
            app->scene = SceneScrolling;
            app->cursor = 0;
            break;
        case 5:
            app->scene = SceneInfo;
            break;
        case 6:
            app->scene = SceneI2CScan;
            app->i2c_scan_done = false;
            app->i2c_scan_progress = 0;
            app->i2c_scan_found_count = 0;
            memset(app->i2c_scan_found, 0, sizeof(app->i2c_scan_found));
            break;
        case 7:
            app->scene = SceneBenchmark;
            app->bench_done = false;
            app->bench_fps = 0;
            break;
        case 8:
            app->scene = ScenePixelWalk;
            app->walk_mode = 0;
            app->walk_step = 0;
            app->walk_speed = 3;
            break;
        case 9:
            app->scene = ScenePlant;
            if(app->plant_is_dead) {
                uint32_t now = furi_hal_rtc_get_timestamp();
                if(now < app->plant_dead_until) {
                    app->plant_lockout_attempts++;
                    if(app->plant_lockout_attempts >= 3) {
                        // Max out penalty to 10 mins!
                        app->plant_dead_until = now + 10 * 60;
                    }
                    app->plant_is_monty_insult = true;
                    app->plant_insult_idx = rand() % NUM_MONTY_INSULTS;
                    save_plant_state(app);
                } else {
                    // Resurrect!
                    app->plant_is_dead = false;
                    app->plant_water = 50;
                    app->plant_stress = 0;
                    app->plant_sun = 0;
                    app->plant_growth = 0;
                    app->plant_lockout_attempts = 0;
                    save_plant_state(app);
                }
            } else if(app->plant_water == 0 && app->plant_growth == 0) {
                app->plant_water = 50;
            }
            render_plant_oled(app);
            break;
        }
        break;
    case InputKeyBack:
        app->running = false;
        break;
    default:
        break;
    }
}

static void handle_patterns(App* app, InputEvent* ev) {
    if(ev->type != InputTypePress && ev->type != InputTypeRepeat) return;
    switch(ev->key) {
    case InputKeyUp:
        if(app->cursor > 0) app->cursor--;
        break;
    case InputKeyDown:
        if(app->cursor < PATTERN_COUNT - 1) app->cursor++;
        break;
    case InputKeyOk:
        if(app->cursor < PATTERN_COUNT) {
            pattern_funcs[app->cursor](app);
        }
        break;
    case InputKeyBack:
        app->scene = SceneMainMenu;
        app->cursor = 1;
        break;
    default:
        break;
    }
}

static void handle_brightness(App* app, InputEvent* ev) {
    if(ev->type != InputTypePress && ev->type != InputTypeRepeat) return;
    switch(ev->key) {
    case InputKeyLeft:
        if(app->brightness >= 5)
            app->brightness -= 5;
        else
            app->brightness = 0;
        ssd1306_set_contrast(&app->oled, app->brightness);
        break;
    case InputKeyRight:
        if(app->brightness <= 250)
            app->brightness += 5;
        else
            app->brightness = 255;
        ssd1306_set_contrast(&app->oled, app->brightness);
        break;
    case InputKeyBack:
        app->scene = SceneMainMenu;
        app->cursor = 2;
        break;
    default:
        break;
    }
}

static void handle_commands(App* app, InputEvent* ev) {
    if(ev->type != InputTypePress && ev->type != InputTypeRepeat) return;
    switch(ev->key) {
    case InputKeyUp:
        if(app->cursor > 0) app->cursor--;
        break;
    case InputKeyDown:
        if(app->cursor < CMD_COUNT - 1) app->cursor++;
        break;
    case InputKeyOk:
        switch(app->cursor) {
        case 0:
            app->cmd_invert = !app->cmd_invert;
            ssd1306_set_invert(&app->oled, app->cmd_invert);
            break;
        case 1:
            app->cmd_flip_h = !app->cmd_flip_h;
            ssd1306_set_flip_h(&app->oled, app->cmd_flip_h);
            break;
        case 2:
            app->cmd_flip_v = !app->cmd_flip_v;
            ssd1306_set_flip_v(&app->oled, app->cmd_flip_v);
            break;
        case 3:
            app->cmd_all_on = !app->cmd_all_on;
            ssd1306_set_all_on(&app->oled, app->cmd_all_on);
            break;
        case 4:
            app->cmd_power = !app->cmd_power;
            ssd1306_power(&app->oled, !app->cmd_power);
            break;
        case 5:
            app->yellow_bar_height = (app->yellow_bar_height == 0) ? YELLOW_BAR_16 :
                                                                     YELLOW_BAR_NONE;
            break;
        case 6:
            app->cmd_start_line = (app->cmd_start_line + 8) & 0x3F;
            ssd1306_set_start_line(&app->oled, app->cmd_start_line);
            break;
        case 7:
            app->cmd_fade_mode = (app->cmd_fade_mode + 1) % 5;
            ssd1306_set_fade_blink(&app->oled, app->cmd_fade_mode);
            break;
        }
        break;
    case InputKeyLeft:
        if(app->cursor == 6 && app->cmd_start_line >= 8)
            app->cmd_start_line -= 8;
        else if(app->cursor == 6)
            app->cmd_start_line = 0;
        else if(app->cursor == 7 && app->cmd_fade_mode > 0)
            app->cmd_fade_mode--;
        if(app->cursor == 6)
            ssd1306_set_start_line(&app->oled, app->cmd_start_line);
        else if(app->cursor == 7)
            ssd1306_set_fade_blink(&app->oled, app->cmd_fade_mode);
        break;
    case InputKeyRight:
        if(app->cursor == 6)
            app->cmd_start_line = (app->cmd_start_line + 8) & 0x3F;
        else if(app->cursor == 7)
            app->cmd_fade_mode = (app->cmd_fade_mode + 1) % 5;
        if(app->cursor == 6)
            ssd1306_set_start_line(&app->oled, app->cmd_start_line);
        else if(app->cursor == 7)
            ssd1306_set_fade_blink(&app->oled, app->cmd_fade_mode);
        break;
    case InputKeyBack:
        app->scene = SceneMainMenu;
        app->cursor = 3;
        break;
    default:
        break;
    }
}

static void handle_scrolling(App* app, InputEvent* ev) {
    if(ev->type != InputTypePress && ev->type != InputTypeRepeat) return;
    switch(ev->key) {
    case InputKeyUp:
        if(app->cursor > 0) app->cursor--;
        break;
    case InputKeyDown:
        if(app->cursor < SCROLL_COUNT - 1) app->cursor++;
        break;
    case InputKeyOk:
        switch(app->cursor) {
        case 0:
            ssd1306_scroll_h(&app->oled, false, 0, 7, app->scroll_speed);
            break;
        case 1:
            ssd1306_scroll_h(&app->oled, true, 0, 7, app->scroll_speed);
            break;
        case 2:
            ssd1306_scroll_stop(&app->oled);
            break;
        case 3:
            app->scroll_speed = (app->scroll_speed + 1) & 0x07;
            break;
        }
        break;
    case InputKeyLeft:
        if(app->cursor == 3 && app->scroll_speed > 0) app->scroll_speed--;
        break;
    case InputKeyRight:
        if(app->cursor == 3 && app->scroll_speed < 7) app->scroll_speed++;
        break;
    case InputKeyBack:
        ssd1306_scroll_stop(&app->oled);
        app->scene = SceneMainMenu;
        app->cursor = 4;
        break;
    default:
        break;
    }
}

static void handle_info(App* app, InputEvent* ev) {
    if(ev->type != InputTypePress) return;
    if(ev->key == InputKeyBack) {
        app->scene = SceneMainMenu;
        app->cursor = 5;
    }
}

static void handle_clock(App* app, InputEvent* ev) {
    if(ev->type != InputTypePress) return;
    if(ev->key == InputKeyBack) {
        app->scene = SceneMainMenu;
        app->cursor = 0;
    }
}

// -- Entry point --

int32_t ssd1306_app_main(void* p) {
    UNUSED(p);

    App* app = malloc(sizeof(App));
    memset(app, 0, sizeof(App));
    app->running = true;
    app->brightness = 207; // default contrast
    app->cmd_power = false; // power off toggle (display starts ON)
    app->yellow_bar_height = YELLOW_BAR_NONE;
    app->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    // load persistent plant memory
    load_plant_state(app);

    // Try both addresses
    if(ssd1306_detect(SSD1306_ADDR_3C)) {
        app->detected_addr = 0x3C;
        app->detected = ssd1306_init(&app->oled, SSD1306_ADDR_3C);
    } else if(ssd1306_detect(SSD1306_ADDR_3D)) {
        app->detected_addr = 0x3D;
        app->detected = ssd1306_init(&app->oled, SSD1306_ADDR_3D);
    }

    if(app->detected) {
        // show initial test pattern
        ssd1306_clear(&app->oled);
        ssd1306_string(&app->oled, 16, 4, "SSD1306 Test");
        ssd1306_string(&app->oled, 10, 20, "Ready");
        ssd1306_string(&app->oled, 10, 36, "Use Flipper");
        ssd1306_string(&app->oled, 10, 46, "to navigate");
        ssd1306_rect(&app->oled, 0, 0, 128, 64);
        ssd1306_flush(&app->oled);
    }

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    ViewPort* vp = view_port_alloc();
    view_port_draw_callback_set(vp, draw_callback, app);
    view_port_input_callback_set(vp, input_callback, queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, vp, GuiLayerFullscreen);

    InputEvent ev;
    while(app->running) {
        if(furi_message_queue_get(queue, &ev, 200) == FuriStatusOk) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);

            if(!app->detected) {
                // on any press, retry detection
                if(ev.type == InputTypePress) {
                    if(ev.key == InputKeyBack) {
                        app->running = false;
                    } else {
                        if(ssd1306_detect(SSD1306_ADDR_3C)) {
                            app->detected_addr = 0x3C;
                            app->detected = ssd1306_init(&app->oled, SSD1306_ADDR_3C);
                        } else if(ssd1306_detect(SSD1306_ADDR_3D)) {
                            app->detected_addr = 0x3D;
                            app->detected = ssd1306_init(&app->oled, SSD1306_ADDR_3D);
                        }
                    }
                }
            } else {
                switch(app->scene) {
                case SceneMainMenu:
                    handle_main_menu(app, &ev);
                    break;
                case ScenePatterns:
                    handle_patterns(app, &ev);
                    break;
                case SceneBrightness:
                    handle_brightness(app, &ev);
                    break;
                case SceneCommands:
                    handle_commands(app, &ev);
                    break;
                case SceneScrolling:
                    handle_scrolling(app, &ev);
                    break;
                case SceneInfo:
                    handle_info(app, &ev);
                    break;
                case SceneClock:
                    handle_clock(app, &ev);
                    break;
                case ScenePlant:
                    handle_plant(app, &ev);
                    break;
                case SceneI2CScan:
                    handle_i2c_scan(app, &ev);
                    break;
                case SceneBenchmark:
                    handle_benchmark(app, &ev);
                    break;
                case ScenePixelWalk:
                    handle_pixel_walk(app, &ev);
                    break;
                }
            }

            furi_mutex_release(app->mutex);
            view_port_update(vp);
        }

        // periodic clock update (on message queue timeout, ~5 times/sec)
        if(app->detected && app->scene == SceneClock) {
            DateTime dt;
            furi_hal_rtc_get_datetime(&dt);
            if(dt.second != app->last_second) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                render_clock_oled(app);
                furi_mutex_release(app->mutex);
                view_port_update(vp); // refresh Flipper screen too
            }
        }

        // periodic plant update
        if(app->detected && app->scene == ScenePlant) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);

            if(!app->plant_is_dead) {
                app->plant_ticks++;

                // Determine growth stage
                uint8_t stage;
                if(app->plant_growth < 20)
                    stage = 0; // Sprout
                else if(app->plant_growth < 50)
                    stage = 1; // Growing
                else if(app->plant_growth < 100)
                    stage = 2; // Flowering
                else
                    stage = 3; // Ancient

                // Stat decay rate increases with growth stage
                // Sprout: slow decay, only water matters much
                // Growing: moderate decay, water + light needed
                // Flowering: fast decay, tight tolerance
                // Ancient: very fast decay, unforgiving
                int decay_rate = 1 + stage; // 1, 2, 3, 4 ticks between decay steps

                if(app->plant_ticks % decay_rate == 0) {
                    // Water decays faster if sun is high (evaporation)
                    int water_decay = 1;
                    if(app->plant_sun > 50) water_decay = 2;
                    app->plant_water -= water_decay;

                    // Sun decays (clouds pass, day ends)
                    if(stage >= 1) {
                        app->plant_sun--;
                    }

                    // Stress slowly recovers if conditions are good
                    if(app->plant_stress > 0 && app->plant_ticks % 15 == 0) {
                        if(app->plant_water > 15 && app->plant_water < 85 && app->plant_sun > 5 &&
                           app->plant_sun < 80) {
                            app->plant_stress--;
                        }
                    }
                }

                // Growth conditions (checked every 5 ticks)
                if(app->plant_ticks % 5 == 0) {
                    bool in_goldilocks =
                        (app->plant_water > 15 && app->plant_water < 80 && app->plant_sun > 5 &&
                         app->plant_sun < 75 && app->plant_stress < 15);

                    if(in_goldilocks) {
                        app->plant_growth++;
                    } else if(app->plant_stress > 25 && app->plant_growth > 0) {
                        // Withering
                        app->plant_growth--;
                    }

                    // Overwatering penalty
                    if(app->plant_water > 90) {
                        app->plant_stress += 1;
                    }

                    // Sunburn penalty
                    if(app->plant_sun > 80) {
                        app->plant_stress += 1;
                    }

                    // Light starvation at higher stages
                    if(stage >= 2 && app->plant_sun < 5) {
                        app->plant_stress += 2;
                    }
                }

                // Death conditions
                uint32_t now_epoch = furi_hal_rtc_get_timestamp();
                if(app->plant_water < -25 || app->plant_water > 120 || app->plant_stress > 40) {
                    app->plant_is_dead = true;
                    app->plant_dead_until = now_epoch + (60 + (rand() % 241));
                    app->plant_lockout_attempts = 0;
                    app->plant_is_monty_insult = false;
                    app->plant_insult_idx = rand() % NUM_DEATH_INSULTS;
                    app->plant_growth = 0;
                    app->plant_last_update_time = now_epoch;
                    save_plant_state(app);
                }
            }

            render_plant_oled(app);
            furi_mutex_release(app->mutex);
        }

        // periodic I2C scan step
        if(app->detected && app->scene == SceneI2CScan && !app->i2c_scan_done) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);

            if(app->i2c_scan_progress < 128) {
                uint8_t i2c_addr = app->i2c_scan_progress << 1;
                if(ssd1306_detect(i2c_addr)) {
                    app->i2c_scan_found[app->i2c_scan_progress] = true;
                    app->i2c_scan_found_count++;
                }
                app->i2c_scan_progress++;
            }
            if(app->i2c_scan_progress >= 128) {
                app->i2c_scan_done = true;
            }

            render_i2c_scan_oled(app);
            furi_mutex_release(app->mutex);
            view_port_update(vp);
        }

        // benchmark run
        if(app->detected && app->scene == SceneBenchmark && !app->bench_done) {
            furi_mutex_acquire(app->mutex, FuriWaitForever);
            render_benchmark_oled(app);
            furi_mutex_release(app->mutex);
            view_port_update(vp);

            // Run benchmark outside the mutex since it takes 5 seconds
            run_benchmark(app);

            furi_mutex_acquire(app->mutex, FuriWaitForever);
            render_benchmark_oled(app);
            furi_mutex_release(app->mutex);
            view_port_update(vp);
        }

        // periodic pixel walk update
        if(app->detected && app->scene == ScenePixelWalk) {
            uint16_t max_step;
            switch(app->walk_mode) {
            case 0:
                max_step = 128 * 64 - 1;
                break; // pixel
            case 1:
                max_step = 127;
                break; // v-line
            case 2:
                max_step = 63;
                break; // h-line
            case 3:
                max_step = 127 * 63 - 1;
                break; // block
            default:
                max_step = 128 * 64 - 1;
                break;
            }

            // Burst of steps per main-loop tick
            // walk_speed 1 = ~10ms step (fast), walk_speed 10 = ~100ms step (slow)
            int step_delay = app->walk_speed * 10;
            int burst = 200 / step_delay;
            if(burst < 1) burst = 1;
            if(burst > 30) burst = 30;

            for(int b = 0; b < burst; b++) {
                furi_mutex_acquire(app->mutex, FuriWaitForever);
                app->walk_step++;
                if(app->walk_step > max_step) app->walk_step = 0;
                render_pixel_walk_oled(app);
                furi_mutex_release(app->mutex);
                furi_delay_ms(step_delay);
            }
            view_port_update(vp);
        }
    }

    // cleanup
    if(app->detected) {
        ssd1306_scroll_stop(&app->oled);
        ssd1306_clear(&app->oled);
        ssd1306_flush(&app->oled);
        ssd1306_power(&app->oled, false);
    }

    gui_remove_view_port(gui, vp);
    furi_record_close(RECORD_GUI);
    view_port_free(vp);
    furi_message_queue_free(queue);
    furi_mutex_free(app->mutex);
    free(app);

    return 0;
}
