#include "../fire_string.h"
#include "furi_hal_random.h"

#define ALPH_ROWS 2
#define NUMS_ROWS 1
#define SYMB_ROWS 4
#define HEX_ROWS  2
#define BIN_ROWS  1
#define COLS      2

void build_string_generator_widget(FireString* app);

#define DEFAULT_DELAY          250 // arbitrary delay for string gen animation speed
#define TEXT_SCROLL_CHAR_LIMIT 250 // limits text displayed in widget scroll element
#define ANIMATION_LEN_LIMIT    25 // limits how many characters are animated
static uint32_t delay_ms = DEFAULT_DELAY;
static uint16_t fire_string_len = 0;
static char word_delimiter = '-';

// -- Character list builder --------------------------------------------------

static void get_char_list(FireString* app) {
    FURI_LOG_T(TAG, "get_char_list");

    if(app->dict->char_list != NULL) {
        furi_string_free(app->dict->char_list);
        app->dict->char_list = NULL;
    }
    app->dict->char_list = furi_string_alloc();

    // Alphabet: Uppercase (65-90) and Lowercase (97-122)
    const uint8_t alphabet_ranges[ALPH_ROWS][COLS] = {
        {65, 90}, // Uppercase A-Z
        {97, 122} // Lowercase a-z
    };

    // Numbers: Digits (48-57)
    const uint8_t numbers_ranges[NUMS_ROWS][COLS] = {
        {48, 57} // Digits 0-9
    };

    // Symbols: Multiple ranges
    const uint8_t symbols_ranges[SYMB_ROWS][COLS] = {
        {33, 47}, // Exclamation mark to Slash (! to /)
        {58, 64}, // Colon to At Symbol (: to @)
        {91, 96}, // Opening Bracket to Backtick ([ to `)
        {123, 126} // Opening Curly Brace to Tilde ({ to ~)
    };

    // Hex: 0-9 (48-57) A-F (65-70)
    const uint8_t hex_range[HEX_ROWS][COLS] = {
        {48, 57}, // Zero to Nine (0 to 9)
        {65, 70}}; // Uppercase A-F

    // Binary: (48-49)
    const uint8_t binary_range[BIN_ROWS][COLS] = {
        {48, 49} // Zero to One (0 to 1)
    };

    switch(app->settings->str_type) {
    case(StrType_AlphaNumSymb):
        for(uint8_t i = 0; i < ALPH_ROWS; i++)
            for(uint8_t j = alphabet_ranges[i][0]; j <= alphabet_ranges[i][COLS - 1]; j++)
                furi_string_push_back(app->dict->char_list, (char)j);
        for(uint8_t i = 0; i < NUMS_ROWS; i++)
            for(uint8_t j = numbers_ranges[i][0]; j <= numbers_ranges[i][COLS - 1]; j++)
                furi_string_push_back(app->dict->char_list, (char)j);
        for(uint8_t i = 0; i < SYMB_ROWS; i++)
            for(uint8_t j = symbols_ranges[i][0]; j <= symbols_ranges[i][COLS - 1]; j++)
                furi_string_push_back(app->dict->char_list, (char)j);
        break;
    case(StrType_AlphaNum):
        for(uint8_t i = 0; i < ALPH_ROWS; i++)
            for(uint8_t j = alphabet_ranges[i][0]; j <= alphabet_ranges[i][COLS - 1]; j++)
                furi_string_push_back(app->dict->char_list, (char)j);
        for(uint8_t i = 0; i < NUMS_ROWS; i++)
            for(uint8_t j = numbers_ranges[i][0]; j <= numbers_ranges[i][COLS - 1]; j++)
                furi_string_push_back(app->dict->char_list, (char)j);
        break;
    case(StrType_Alpha):
        for(uint8_t i = 0; i < ALPH_ROWS; i++)
            for(uint8_t j = alphabet_ranges[i][0]; j <= alphabet_ranges[i][COLS - 1]; j++)
                furi_string_push_back(app->dict->char_list, (char)j);
        break;
    case(StrType_Symb):
        for(uint8_t i = 0; i < SYMB_ROWS; i++)
            for(uint8_t j = symbols_ranges[i][0]; j <= symbols_ranges[i][COLS - 1]; j++)
                furi_string_push_back(app->dict->char_list, (char)j);
        break;
    case(StrType_Num):
        for(uint8_t i = 0; i < NUMS_ROWS; i++)
            for(uint8_t j = numbers_ranges[i][0]; j <= numbers_ranges[i][COLS - 1]; j++)
                furi_string_push_back(app->dict->char_list, (char)j);
        break;
    case(StrType_Hex):
        for(uint8_t i = 0; i < HEX_ROWS; i++)
            for(uint8_t j = hex_range[i][0]; j <= hex_range[i][COLS - 1]; j++)
                furi_string_push_back(app->dict->char_list, (char)j);
        break;
    case(StrType_Bin):
        for(uint8_t i = 0; i < BIN_ROWS; i++)
            for(uint8_t j = binary_range[i][0]; j <= binary_range[i][COLS - 1]; j++)
                furi_string_push_back(app->dict->char_list, (char)j);
        break;
    }
}

// -- String assembly ---------------------------------------------------------

static inline void string_builder(FireString* app, FuriString* str) {
    if(fire_string_len == 0) {
        furi_string_set(app->fire_string, str);
    } else {
        furi_string_push_back(app->fire_string, word_delimiter);

        // char-by-char append is intentional: furi_string_cat causes performance
        // degradation when called consecutively and rapidly on large strings.
        size_t str_len = furi_string_size(str);
        for(size_t i = 0; i < str_len; i++) {
            furi_string_push_back(app->fire_string, furi_string_get_char(str, i));
        }
    }
    fire_string_len++;
}

// -- String length -----------------------------------------------------------

// Returns character count, or word count for word lists
static inline size_t get_str_len(FireString* app) {
    FURI_LOG_T(TAG, "get_str_len");

    if(app->settings->str_type == StrType_Words) {
        size_t string_size = furi_string_size(app->fire_string);
        if(string_size == 0) return 0;

        size_t word_count = 1;
        for(size_t i = 0; i < string_size; i++) {
            if(furi_string_get_char(app->fire_string, i) == word_delimiter) word_count++;
        }
        return word_count;
    }

    return furi_string_size(app->fire_string);
}

// -- Haptic feedback ---------------------------------------------------------

static void vibro(FireString* app) {
    if(fire_string_len >= app->settings->str_len && !app->settings->file_loaded) {
        furi_hal_vibro_on(true);
        furi_delay_ms(30);
        furi_hal_vibro_on(false);
    }
}

// -- RNG helpers -------------------------------------------------------------

// Returns a random word pointer (save=false) or appends to fire_string (save=true)
const char* get_rnd_word(FireString* app, bool save) {
    uint16_t rnd_buffer;
    do {
        rnd_buffer = furi_hal_random_get() & 0xFFF; // 12-bit mask
    } while(rnd_buffer >= app->dict->len);

    if(save) {
        string_builder(app, app->dict->word_list[rnd_buffer]);
        return NULL;
    }
    return furi_string_get_cstr(app->dict->word_list[rnd_buffer]);
}

// Returns a random char (save=false) or appends to fire_string (save=true)
char get_rnd_char(FireString* app, bool save) {
    uint8_t rnd_byte;
    do {
        rnd_byte = 0;
        furi_hal_random_fill_buf(&rnd_byte, sizeof(rnd_byte));
        rnd_byte &= 0x7F; // 7-bit mask
    } while(rnd_byte > app->dict->len - 1);

    char result = furi_string_get_char(app->dict->char_list, rnd_byte);
    if(save) {
        furi_string_push_back(app->fire_string, result);
        fire_string_len++;
        return '\0';
    }
    return result;
}

// -- Bulk fill ---------------------------------------------------------------

// Fills fire_string to str_len in one shot (used once animation phase ends)
static void get_random_str(FireString* app) {
    FURI_LOG_T(TAG, "get_random_str");

    uint16_t remaining = app->settings->str_len - fire_string_len;
    bool is_wordlist = app->settings->str_type == StrType_Words;

    for(uint16_t i = 0; i < remaining; i++) {
        if(is_wordlist) {
            get_rnd_word(app, true);
        } else {
            get_rnd_char(app, true);
        }
    }
}

// -- Button callback ---------------------------------------------------------

static void string_generator_btn_callback(GuiButtonType result, InputType type, void* context) {
    FURI_LOG_T(TAG, "string_generator_btn_callback");
    furi_assert(context);

    FireString* app = context;
    if(type != InputTypeShort) return;

    switch(result) {
    case GuiButtonTypeRight:
        scene_manager_next_scene(app->scene_manager, FireStringScene_GenerateStepTwo);
        break;
    case GuiButtonTypeLeft:
        scene_manager_next_scene(app->scene_manager, FireStringScene_Settings);
        break;
    case GuiButtonTypeCenter:
        furi_string_reset(app->fire_string);
        furi_string_reserve(app->fire_string, STR_RESERVE_LEN);
        fire_string_len = 0;
        delay_ms = DEFAULT_DELAY;
        app->settings->file_loaded = false;
        if(app->settings->use_ir) build_string_generator_widget(app);
        break;
    default:
        break;
    }
}

// -- Widget ------------------------------------------------------------------

void build_string_generator_widget(FireString* app) {
    FURI_LOG_T(TAG, "build_string_generator_widget");

    widget_reset(app->widget);

    // Limit size of displayed string to avoid memory issues in the scroll element
    if(furi_string_size(app->fire_string) > TEXT_SCROLL_CHAR_LIMIT) {
        FuriString* short_fire_string = furi_string_alloc();
        furi_string_set_strn(
            short_fire_string, furi_string_get_cstr(app->fire_string), TEXT_SCROLL_CHAR_LIMIT);
        furi_string_cat_str(short_fire_string, "...");
        widget_add_text_scroll_element(
            app->widget, 0, 1, 128, 45, furi_string_get_cstr(short_fire_string));
        furi_string_free(short_fire_string);
        short_fire_string = NULL;
    } else {
        widget_add_text_scroll_element(
            app->widget, 0, 1, 128, 45, furi_string_get_cstr(app->fire_string));
    }

    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Config", string_generator_btn_callback, app);

    if(fire_string_len > 0) {
        widget_add_button_element(
            app->widget, GuiButtonTypeCenter, "Reset", string_generator_btn_callback, app);
    }

    if(fire_string_len >= app->settings->str_len || app->settings->file_loaded) {
        widget_add_button_element(
            app->widget, GuiButtonTypeRight, "Next", string_generator_btn_callback, app);
    } else {
        FuriString* progress = furi_string_alloc();
        furi_string_printf(progress, "%u/%lu", fire_string_len, app->settings->str_len);
        widget_add_string_element(
            app->widget,
            85,
            56,
            AlignLeft,
            AlignTop,
            FontSecondary,
            furi_string_get_cstr(progress));
        furi_string_free(progress);
    }
}

// -- Infrared RNG ------------------------------------------------------------

static void ir_received_callback(void* context, InfraredWorkerSignal* signal) {
    FURI_LOG_T(TAG, "ir_received_callback");
    furi_assert(context);

    FireString* app = context;
    if(!app->ir_worker) return;

    const uint32_t* timings;
    size_t timings_size;
    infrared_worker_get_raw_signal(signal, &timings, &timings_size);

    bool is_wordlist = app->settings->str_type == StrType_Words;
    size_t i = 0;

    while(fire_string_len < app->settings->str_len && i < timings_size) {
        uint32_t index = timings[i] % app->dict->len;
        if(is_wordlist) {
            string_builder(app, app->dict->word_list[index]);
        } else {
            furi_string_push_back(
                app->fire_string, furi_string_get_char(app->dict->char_list, index));
            fire_string_len++;
        }
        i++;
    }

    build_string_generator_widget(app);
}

static void infrared_rx_start(FireString* app) {
    FURI_LOG_T(TAG, "infrared_rx_start");
    furi_check(app);

    app->ir_worker = infrared_worker_alloc();
    infrared_worker_rx_enable_blink_on_receiving(app->ir_worker, true);
    infrared_worker_rx_set_received_signal_callback(app->ir_worker, ir_received_callback, app);
    infrared_worker_rx_start(app->ir_worker);
}

static void infrared_rx_stop(FireString* app) {
    FURI_LOG_T(TAG, "infrared_rx_stop");
    furi_check(app);

    infrared_worker_rx_stop(app->ir_worker);
    infrared_worker_free(app->ir_worker);
    app->ir_worker = NULL;
}

// -- Scene lifecycle ---------------------------------------------------------

void fire_string_scene_on_enter_string_generator(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_enter_string_generator");
    furi_check(context);

    FireString* app = context;
    delay_ms = DEFAULT_DELAY;

    view_dispatcher_switch_to_view(app->view_dispatcher, FireStringView_Widget);

    if(app->settings->str_type != StrType_Words) {
        if(app->dict->char_list == NULL) {
            get_char_list(app);
        }
        app->dict->len = furi_string_size(app->dict->char_list);
    } else { // set word delimiter
        switch(app->settings->delimiter) {
        case DelimType_Dash:
            word_delimiter = '-';
            break;
        case DelimType_Plus:
            word_delimiter = '+';
            break;
        case DelimType_Comma:
            word_delimiter = ',';
            break;
        case DelimType_SemiColon:
            word_delimiter = ';';
            break;
        case DelimType_ForwardSlash:
            word_delimiter = '/';
            break;
        case DelimType_VerticalBar:
            word_delimiter = '|';
            break;
        case DelimType_Space:
            word_delimiter = ' ';
            break;
        default:
            word_delimiter = '-';
        }
    }

    if(app->fire_string == NULL) {
        app->fire_string = furi_string_alloc();
        furi_string_reserve(app->fire_string, STR_RESERVE_LEN);
    }

    fire_string_len = get_str_len(app);
    build_string_generator_widget(app);
}

bool fire_string_scene_on_event_string_generator(void* context, SceneManagerEvent event) {
    furi_check(context);

    FireString* app = context;
    bool consumed = false;

    switch(event.type) {
    case SceneManagerEventTypeCustom:
        break;

    case SceneManagerEventTypeBack:
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, FireStringScene_MainMenu);
        consumed = true;
        break;

    case SceneManagerEventTypeTick: {
        bool needs_more = fire_string_len < app->settings->str_len;
        bool not_from_file = !app->settings->file_loaded;

        // Manage IR worker lifecycle
        if(needs_more && !app->ir_worker && app->settings->use_ir && not_from_file)
            infrared_rx_start(app);
        if(!needs_more && app->ir_worker) {
            infrared_rx_stop(app);
            vibro(app);
        }

        // Animated automatic generation (non-IR, non-file)
        if(needs_more && !app->settings->use_ir && not_from_file) {
            if(fire_string_len > ANIMATION_LEN_LIMIT) {
                // Animation phase complete — fill remainder instantly
                get_random_str(app);
            } else {
                // Animated phase: one token per tick with exponentially decaying delay
                if(app->settings->str_type == StrType_Words) {
                    get_rnd_word(app, true);
                } else {
                    get_rnd_char(app, true);
                }
                furi_delay_ms(delay_ms);
                delay_ms = (delay_ms > 1) ? delay_ms * 2 / 3 : 0;
            }
            build_string_generator_widget(app);
            vibro(app);
        }
        break;
    }
    }

    return consumed;
}

void fire_string_scene_on_exit_string_generator(void* context) {
    FURI_LOG_T(TAG, "fire_string_scene_on_exit_string_generator");
    furi_check(context);

    FireString* app = context;

    if(app->ir_worker != NULL) infrared_rx_stop(app);

    widget_reset(app->widget);
}
