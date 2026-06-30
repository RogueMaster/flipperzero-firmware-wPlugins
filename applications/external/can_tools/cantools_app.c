#include <furi.h>

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/widget.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_input.h>
#include <gui/modules/byte_input.h>
#include <dialogs/dialogs.h>

#include <storage/storage.h>
#include <stream/stream.h>
#include <stream/buffered_file_stream.h>

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>
#include <strings.h>
#include <ctype.h>

#define TAG               "dbc_app"
#define TEXT_INPUT_SIZE   32
#define VEHICLE_INFO_SIZE 64
#define MAX_SIGNALS       32
#define MAX_DBC_FILES     16

#define CANTOOLS_EVENT_TEXT_INPUT_SAVE   1U
#define CANTOOLS_EVENT_SORT_TOGGLE       2U
#define CANTOOLS_EVENT_BYTE_INPUT_SAVE   3U
#define CANTOOLS_EVENT_DECODE_MANUAL     4U
#define CANTOOLS_EVENT_DECODE_LOG        5U
#define CANTOOLS_EVENT_DECODE_CANID_SAVE 6U
#define CANTOOLS_EVENT_DECODE_DLC_SAVE   7U
#define CANTOOLS_EVENT_DBC_NAME_SAVE     8U
#define CANTOOLS_EVENT_DBC_VEHICLE_SAVE  9U
#define CANTOOLS_EVENT_DBC_CREATE        10U
#define CANTOOLS_EVENT_SIGNAL_BASE       100U
#define CANTOOLS_EVENT_DBC_FILE_BASE     200U

// Folder + file format for saved DBC files
#define DBC_USER_PATH      EXT_PATH("apps_data/can_tools")
#define DBC_EXTENSION      ".dbc"
#define DBC_VEHICLE_PREFIX "CM_ \"Vehicle: "

typedef enum {
    CantoolsScenesMainMenuScene,
    CantoolsScenesDbcFileListScene,
    CantoolsScenesDbcFileNameScene,
    CantoolsScenesVehicleInfoScene,
    CantoolsScenesDbcMakerMenuScene,
    CantoolsScenesSignalNameScene,
    CantoolsScenesCanIdScene,
    CantoolsScenesStartBitScene,
    CantoolsScenesBitLengthScene,
    CantoolsScenesEndiannessScene,
    CantoolsScenesSignedScene,
    CantoolsScenesOffsetScene,
    CantoolsScenesScalarScene,
    CantoolsScenesUnitScene,
    CantoolsScenesMinScene,
    CantoolsScenesMaxScene,
    CantoolsScenesCalculateScene,
    CantoolsScenesViewDbcListScene,
    CantoolsScenesSignalActionsScene,
    CantoolsScenesViewDbcDetailScene,
    CantoolsScenesDecodeDataScene,
    CantoolsScenesDecodeManualCanIdScene,
    CantoolsScenesDecodeManualDlcScene,
    CantoolsScenesDecodeManualDataScene,
    CantoolsScenesDecodeResultScene,
    CantoolsScenesDecodeLogScene,
    CantoolsScenesSceneCount,
} CantoolsScenesScene;

typedef enum {
    CantoolsScenesSubmenuView,
    CantoolsScenesWidgetView,
    CantoolsScenesTextInputView,
    CantoolsScenesByteInputView,
} CantoolsScenesView;

typedef enum {
    CantoolsSortByName,
    CantoolsSortByCanId,
} CantoolsSortMode;

typedef enum {
    DbcFieldSignalName,
    DbcFieldCanId,
    DbcFieldStartBit,
    DbcFieldBitLength,
    DbcFieldEndianness,
    DbcFieldSignedness,
    DbcFieldOffset,
    DbcFieldScalar,
    DbcFieldUnit,
    DbcFieldMin,
    DbcFieldMax,
    DbcFieldCount,
} DbcFieldId;

// Typical DBC signal components
typedef struct {
    bool used;
    char message_name[TEXT_INPUT_SIZE];
    char signal_name[TEXT_INPUT_SIZE];
    char can_id[TEXT_INPUT_SIZE];
    char start_bit[TEXT_INPUT_SIZE];
    char bit_length[TEXT_INPUT_SIZE];
    char endianness[TEXT_INPUT_SIZE];
    char signedness[TEXT_INPUT_SIZE];
    char offset[TEXT_INPUT_SIZE];
    char scalar[TEXT_INPUT_SIZE];
    char unit[TEXT_INPUT_SIZE];
    char min_val[TEXT_INPUT_SIZE];
    char max_val[TEXT_INPUT_SIZE];
} DbcSignal;

typedef struct {
    char name[TEXT_INPUT_SIZE];
    char path[256];
    char vehicle_info[VEHICLE_INFO_SIZE];
} DbcFile;

typedef struct App {
    SceneManager* scene_manager;
    ViewDispatcher* view_dispatcher;
    Submenu* submenu;
    Widget* widget;
    TextInput* text_input;
    ByteInput* byte_input;
    DialogsApp* dialogs;
    char* buffer; // For text input
    uint8_t buffer_size; // for text input
    DbcFieldId active_field;
    CantoolsSortMode sort_mode;
    bool editing;
    char decode_can_id_text[TEXT_INPUT_SIZE];
    char decode_dlc_text[TEXT_INPUT_SIZE];
    uint8_t decode_data_bytes[8];
    uint32_t decode_can_id;
    uint8_t decode_dlc;
    uint8_t decode_data_len;
    char dbc_file_name[TEXT_INPUT_SIZE];
    char vehicle_info[VEHICLE_INFO_SIZE];

    // Current-in-edit signal fields
    char message_name[TEXT_INPUT_SIZE];
    char signal_name[TEXT_INPUT_SIZE];
    char can_id[TEXT_INPUT_SIZE];
    char start_bit[TEXT_INPUT_SIZE];
    char bit_length[TEXT_INPUT_SIZE];
    char endianness[TEXT_INPUT_SIZE];
    char signedness[TEXT_INPUT_SIZE];
    char offset[TEXT_INPUT_SIZE];
    char scalar[TEXT_INPUT_SIZE];
    char unit[TEXT_INPUT_SIZE];
    char min_val[TEXT_INPUT_SIZE];
    char max_val[TEXT_INPUT_SIZE];

    // Saved signals (loaded from SD + new ones)
    DbcSignal signals[MAX_SIGNALS];
    uint8_t signal_count;
    int selected_signal_index;
    DbcFile dbc_files[MAX_DBC_FILES];
    uint8_t dbc_file_count;
    int active_dbc_index;
    FuriString* dbc_header;
} App;

typedef enum {
    CantoolsScenesMainMenuSceneDbcMaker,
    CantoolsScenesMainMenuSceneViewDbc,
    CantoolsScenesMainMenuSceneDecodeData,
    CantoolsScenesMainMenuSceneDbcFiles,
} CantoolsScenesMainMenuSceneIndex;

typedef enum {
    CantoolsScenesMainMenuSceneDbcMakerEvent,
    CantoolsScenesMainMenuSceneViewDbcEvent,
    CantoolsScenesMainMenuSceneDecodeDataEvent,
    CantoolsScenesMainMenuSceneDbcFilesEvent,
} CantoolsScenesMainMenuEvent;

typedef enum {
    CantoolsDbcMakerMenuGenerate = 1000,
    CantoolsDbcMakerMenuResetDefaults,
} CantoolsDbcMakerMenuAction;

typedef enum {
    CantoolsSignalActionView,
    CantoolsSignalActionEdit,
    CantoolsSignalActionDelete,
} CantoolsSignalAction;

// Substitute "_" characters with "." for decimal input support
void decimal_substitute(char* s) {
    if(!s) return;
    for(size_t i = 0; s[i]; i++) {
        if(s[i] == '_') {
            s[i] = '.';
            FURI_LOG_D(TAG, "Substituted '.' for '_' in: %s", s);
        }
    }
}

typedef bool (*DbcFieldValidator)(App* app, const char* value, FuriString* error);

typedef struct {
    const char* menu_label;
    const char* input_header;
    size_t offset;
    CantoolsScenesScene scene;
    CantoolsScenesScene next_scene;
    DbcFieldValidator validator;
} DbcFieldConfig;

static bool validate_required(const char* value, const char* label, FuriString* error) {
    if(!value || value[0] == '\0') {
        furi_string_printf(error, "%s is required.", label);
        return false;
    }
    return true;
}

static bool parse_uint32_str(const char* value, uint32_t* out) {
    if(!value || value[0] == '\0') return false;
    char* end = NULL;
    unsigned long parsed = strtoul(value, &end, 0);
    if(end == value || *end != '\0') return false;
    if(parsed > UINT32_MAX) return false;
    if(out) *out = (uint32_t)parsed;
    return true;
}

static bool parse_int32_str(const char* value, int32_t* out) {
    if(!value || value[0] == '\0') return false;
    char* end = NULL;
    long parsed = strtol(value, &end, 10);
    if(end == value || *end != '\0') return false;
    if(parsed > INT32_MAX || parsed < INT32_MIN) return false;
    if(out) *out = (int32_t)parsed;
    return true;
}

static bool parse_float_str(const char* value) {
    if(!value || value[0] == '\0') return false;
    char tmp[TEXT_INPUT_SIZE];
    strlcpy(tmp, value, sizeof(tmp));
    decimal_substitute(tmp);
    char* end = NULL;
    strtod(tmp, &end);
    return (end != tmp && *end == '\0');
}

static bool validate_signal_name(App* app, const char* value, FuriString* error) {
    UNUSED(app);
    return validate_required(value, "Signal name", error);
}

static bool validate_can_id(App* app, const char* value, FuriString* error) {
    UNUSED(app);
    if(!validate_required(value, "CAN ID", error)) return false;
    uint32_t can_id = 0;
    if(!parse_uint32_str(value, &can_id) || can_id > 0x1FFFFFFF) {
        furi_string_set(error, "CAN ID must be 0x0..0x1FFFFFFF.");
        return false;
    }
    return true;
}

static bool validate_start_bit(App* app, const char* value, FuriString* error) {
    UNUSED(app);
    int32_t start_bit = 0;
    if(!validate_required(value, "Start bit", error)) return false;
    if(!parse_int32_str(value, &start_bit) || start_bit < 0 || start_bit > 63) {
        furi_string_set(error, "Start bit must be 0..63.");
        return false;
    }
    return true;
}

static bool validate_bit_length(App* app, const char* value, FuriString* error) {
    int32_t bit_length = 0;
    int32_t start_bit = 0;
    if(!validate_required(value, "Bit length", error)) return false;
    if(!parse_int32_str(value, &bit_length) || bit_length < 1 || bit_length > 64) {
        furi_string_set(error, "Bit length must be 1..64.");
        return false;
    }
    if(parse_int32_str(app->start_bit, &start_bit)) {
        if(start_bit + bit_length > 64) {
            furi_string_set(error, "Start bit + length must be <= 64.");
            return false;
        }
    }
    return true;
}

static bool validate_endianness(App* app, const char* value, FuriString* error) {
    UNUSED(app);
    if(!validate_required(value, "Endianness", error)) return false;
    char c = value[0];
    if(!(c == '0' || c == '1' || c == 'l' || c == 'L' || c == 'b' || c == 'B' || c == 'm' ||
         c == 'M' || c == 'i' || c == 'I')) {
        furi_string_set(error, "Use 0/1, b/l (big/little).");
        return false;
    }
    return true;
}

static bool validate_signedness(App* app, const char* value, FuriString* error) {
    UNUSED(app);
    if(!validate_required(value, "Signedness", error)) return false;
    char c = value[0];
    if(!(c == 's' || c == 'S' || c == 'u' || c == 'U' || c == '+' || c == '-' || c == '0' ||
         c == '1')) {
        furi_string_set(error, "Use s/u or +/-.");
        return false;
    }
    return true;
}

static bool validate_optional_number(App* app, const char* value, FuriString* error) {
    UNUSED(app);
    if(!value || value[0] == '\0') return true;
    if(!parse_float_str(value)) {
        furi_string_set(error, "Enter a valid number.");
        return false;
    }
    return true;
}

static bool validate_optional_text(App* app, const char* value, FuriString* error) {
    UNUSED(app);
    UNUSED(value);
    UNUSED(error);
    return true;
}

static const DbcFieldConfig dbc_field_configs[DbcFieldCount] = {
    [DbcFieldSignalName] =
        {
            .menu_label = "Signal name",
            .input_header = "Signal name:",
            .offset = offsetof(App, signal_name),
            .scene = CantoolsScenesSignalNameScene,
            .next_scene = CantoolsScenesCanIdScene,
            .validator = validate_signal_name,
        },
    [DbcFieldCanId] =
        {
            .menu_label = "CAN ID",
            .input_header = "CAN ID (dec/0x):",
            .offset = offsetof(App, can_id),
            .scene = CantoolsScenesCanIdScene,
            .next_scene = CantoolsScenesStartBitScene,
            .validator = validate_can_id,
        },
    [DbcFieldStartBit] =
        {
            .menu_label = "Start bit",
            .input_header = "Start bit (0-63):",
            .offset = offsetof(App, start_bit),
            .scene = CantoolsScenesStartBitScene,
            .next_scene = CantoolsScenesBitLengthScene,
            .validator = validate_start_bit,
        },
    [DbcFieldBitLength] =
        {
            .menu_label = "Bit length",
            .input_header = "Bit length (1-64):",
            .offset = offsetof(App, bit_length),
            .scene = CantoolsScenesBitLengthScene,
            .next_scene = CantoolsScenesEndiannessScene,
            .validator = validate_bit_length,
        },
    [DbcFieldEndianness] =
        {
            .menu_label = "Endianness",
            .input_header = "Endianness (0=BE/1=LE):",
            .offset = offsetof(App, endianness),
            .scene = CantoolsScenesEndiannessScene,
            .next_scene = CantoolsScenesSignedScene,
            .validator = validate_endianness,
        },
    [DbcFieldSignedness] =
        {
            .menu_label = "Signed?",
            .input_header = "Signed? (s/u):",
            .offset = offsetof(App, signedness),
            .scene = CantoolsScenesSignedScene,
            .next_scene = CantoolsScenesOffsetScene,
            .validator = validate_signedness,
        },
    [DbcFieldOffset] =
        {
            .menu_label = "Offset",
            .input_header = "Offset (use '_' for .):",
            .offset = offsetof(App, offset),
            .scene = CantoolsScenesOffsetScene,
            .next_scene = CantoolsScenesScalarScene,
            .validator = validate_optional_number,
        },
    [DbcFieldScalar] =
        {
            .menu_label = "Scalar",
            .input_header = "Scalar (use '_' for .):",
            .offset = offsetof(App, scalar),
            .scene = CantoolsScenesScalarScene,
            .next_scene = CantoolsScenesUnitScene,
            .validator = validate_optional_number,
        },
    [DbcFieldUnit] =
        {
            .menu_label = "Unit",
            .input_header = "Unit (e.g. kph):",
            .offset = offsetof(App, unit),
            .scene = CantoolsScenesUnitScene,
            .next_scene = CantoolsScenesMinScene,
            .validator = validate_optional_text,
        },
    [DbcFieldMin] =
        {
            .menu_label = "Min",
            .input_header = "Min (use '_' for .):",
            .offset = offsetof(App, min_val),
            .scene = CantoolsScenesMinScene,
            .next_scene = CantoolsScenesMaxScene,
            .validator = validate_optional_number,
        },
    [DbcFieldMax] =
        {
            .menu_label = "Max",
            .input_header = "Max (use '_' for .):",
            .offset = offsetof(App, max_val),
            .scene = CantoolsScenesMaxScene,
            .next_scene = CantoolsScenesCalculateScene,
            .validator = validate_optional_number,
        },
};

static char* dbc_field_ptr(App* app, DbcFieldId field_id) {
    return (char*)((uint8_t*)app + dbc_field_configs[field_id].offset);
}

static void cantools_show_message(App* app, const char* header, const char* text) {
    DialogMessage* message = dialog_message_alloc();
    dialog_message_set_header(message, header, 64, 0, AlignCenter, AlignTop);
    dialog_message_set_text(message, text, 64, 24, AlignCenter, AlignTop);
    dialog_message_set_buttons(message, NULL, "OK", NULL);
    dialog_message_show(app->dialogs, message);
    dialog_message_free(message);
}

// Normalize scalar/offset/min/max fields
void calculate_values(void* context) {
    App* app = context;

    decimal_substitute(app->scalar);
    decimal_substitute(app->offset);
    decimal_substitute(app->min_val);
    decimal_substitute(app->max_val);

    FURI_LOG_D(TAG, "Normalized scalar: %s", app->scalar);
    FURI_LOG_D(TAG, "Normalized offset: %s", app->offset);
    FURI_LOG_D(TAG, "Normalized min: %s", app->min_val);
    FURI_LOG_D(TAG, "Normalized max: %s", app->max_val);
}

static void sanitize_filename(FuriString* s) {
    FuriString* clean = furi_string_alloc();
    const char* src = furi_string_get_cstr(s);
    for(size_t i = 0; src[i] != '\0'; i++) {
        char c = src[i];
        if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'z') || (c >= '0' && c <= '9') ||
           (c == '_')) {
            furi_string_push_back(clean, c);
        } else {
            furi_string_push_back(clean, '_');
        }
    }
    furi_string_set(s, clean);
    furi_string_free(clean);
}

static void dbc_build_file_path(const char* name, char* out, size_t out_size) {
    FuriString* filename = furi_string_alloc();
    FuriString* path = furi_string_alloc();

    furi_string_set_str(filename, name && name[0] ? name : "vehicle");
    sanitize_filename(filename);
    furi_string_printf(
        path, "%s/%s%s", DBC_USER_PATH, furi_string_get_cstr(filename), DBC_EXTENSION);

    strlcpy(out, furi_string_get_cstr(path), out_size);

    furi_string_free(filename);
    furi_string_free(path);
}

static void dbc_sanitize_file_name(char* name, size_t name_size) {
    FuriString* tmp = furi_string_alloc();
    furi_string_set_str(tmp, name);
    sanitize_filename(tmp);
    strlcpy(name, furi_string_get_cstr(tmp), name_size);
    furi_string_free(tmp);
}

static char dbc_endian_char(const char* endianness) {
    char endian_char = '0';
    if(endianness && strlen(endianness) > 0) {
        char c = endianness[0];
        if(c == '1' || c == 'l' || c == 'L' || c == 'i' || c == 'I') {
            endian_char = '1';
        } else {
            endian_char = '0';
        }
    }
    return endian_char;
}

static char dbc_sign_char(const char* signedness) {
    char sign_char = '+';
    if(signedness && strlen(signedness) > 0) {
        char c = signedness[0];
        if(c == 's' || c == 'S' || c == '-' || c == '1') {
            sign_char = '-';
        } else {
            sign_char = '+';
        }
    }
    return sign_char;
}

static void format_dbc_bo_line(FuriString* out, uint32_t can_id, const char* message_name) {
    const char* msg_name = (message_name && message_name[0]) ? message_name : "Message";
    furi_string_printf(out, "BO_ %lu %s: 8 VehicleBus", (unsigned long)can_id, msg_name);
}

static void format_dbc_sg_line(FuriString* out, const DbcSignal* sig) {
    const char* signal_name = (sig && sig->signal_name[0]) ? sig->signal_name : "Signal";
    const char* sb_str = (sig && sig->start_bit[0]) ? sig->start_bit : "0";
    const char* bl_str = (sig && sig->bit_length[0]) ? sig->bit_length : "8";
    const char* sc_str = (sig && sig->scalar[0]) ? sig->scalar : "1";
    const char* off_str = (sig && sig->offset[0]) ? sig->offset : "0";
    const char* min_str = (sig && sig->min_val[0]) ? sig->min_val : "0";
    const char* max_str = (sig && sig->max_val[0]) ? sig->max_val : "0";
    const char* unit_str = (sig && sig->unit[0]) ? sig->unit : "";

    char endian_char = dbc_endian_char(sig ? sig->endianness : NULL);
    char sign_char = dbc_sign_char(sig ? sig->signedness : NULL);

    furi_string_printf(
        out,
        " SG_ %s : %s|%s@%c%c (%s,%s) [%s|%s] \"%s\"  Receiver",
        signal_name,
        sb_str,
        bl_str,
        endian_char,
        sign_char,
        sc_str,
        off_str,
        min_str,
        max_str,
        unit_str);
}

static void format_dbc_entry(
    FuriString* out,
    const char* message_name,
    const char* signal_name,
    const char* can_id_str,
    const char* start_bit,
    const char* bit_length,
    const char* endianness,
    const char* signedness,
    const char* scalar,
    const char* offset,
    const char* min_val,
    const char* max_val,
    const char* unit) {
    if(!signal_name || strlen(signal_name) == 0) {
        signal_name = "Signal";
    }

    // Parse CAN ID (supports decimal or 0xHEX)
    uint32_t can_id = 0;
    if(can_id_str && strlen(can_id_str) > 0) {
        can_id = (uint32_t)strtoul(can_id_str, NULL, 0);
    }

    char endian_char = dbc_endian_char(endianness);
    char sign_char = dbc_sign_char(signedness);

    const char* msg_name = (message_name && message_name[0]) ? message_name : signal_name;

    const char* sb_str = (start_bit && strlen(start_bit) > 0) ? start_bit : "0";
    const char* bl_str = (bit_length && strlen(bit_length) > 0) ? bit_length : "8";
    const char* sc_str = (scalar && strlen(scalar) > 0) ? scalar : "1";
    const char* off_str = (offset && strlen(offset) > 0) ? offset : "0";
    const char* min_str = (min_val && strlen(min_val) > 0) ? min_val : "0";
    const char* max_str = (max_val && strlen(max_val) > 0) ? max_val : "0";
    const char* unit_str = (unit && strlen(unit) > 0) ? unit : "";

    furi_string_printf(
        out,
        "BO_ %lu %s: 8 VehicleBus\n"
        " SG_ %s : %s|%s@%c%c (%s,%s) [%s|%s] \"%s\"  Receiver",
        (unsigned long)can_id,
        msg_name,
        signal_name,
        sb_str,
        bl_str,
        endian_char,
        sign_char,
        sc_str,
        off_str,
        min_str,
        max_str,
        unit_str);
}

// SD / Storage Handling

static uint32_t dbc_parse_can_id(const DbcSignal* sig);
static void dbc_sort_signals(App* app);

static void dbc_init_folder(void) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FURI_LOG_I(TAG, "Creating DBC folder: %s", DBC_USER_PATH);
    if(storage_simply_mkdir(storage, DBC_USER_PATH)) {
        FURI_LOG_I(TAG, "DBC folder successfully created!");
    } else {
        FURI_LOG_I(TAG, "DBC folder already exists or could not be created.");
    }
    furi_record_close(RECORD_STORAGE);
}

static void dbc_clear_signals(App* app) {
    app->signal_count = 0;
    for(uint8_t i = 0; i < MAX_SIGNALS; i++) {
        app->signals[i].used = false;
        app->signals[i].signal_name[0] = '\0';
        app->signals[i].message_name[0] = '\0';
        app->signals[i].can_id[0] = '\0';
    }
}

static void dbc_clear_files(App* app) {
    app->dbc_file_count = 0;
    app->active_dbc_index = -1;
    for(uint8_t i = 0; i < MAX_DBC_FILES; i++) {
        app->dbc_files[i].name[0] = '\0';
        app->dbc_files[i].path[0] = '\0';
        app->dbc_files[i].vehicle_info[0] = '\0';
    }
}

static void dbc_set_default_header(App* app) {
    if(!app->dbc_header) {
        app->dbc_header = furi_string_alloc();
    }
    furi_string_set_str(
        app->dbc_header,
        "VERSION \"\"\n"
        "NS_ :\n"
        "    CM_\n"
        "    BA_DEF_\n"
        "    BA_\n"
        "    VAL_\n"
        "BS_:\n"
        "BU_: Vector__XXX\n");
}

static const char* dbc_trim_left(const char* s) {
    while(*s && isspace((unsigned char)*s)) {
        s++;
    }
    return s;
}

static void dbc_trim_right(char* s) {
    size_t len = strlen(s);
    while(len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r')) {
        s[len - 1] = '\0';
        len--;
    }
}

static bool dbc_parse_vehicle_comment(const char* line, char* out, size_t out_size) {
    size_t prefix_len = strlen(DBC_VEHICLE_PREFIX);
    if(strncmp(line, DBC_VEHICLE_PREFIX, prefix_len) != 0) return false;
    const char* start = line + prefix_len;
    const char* end = strchr(start, '"');
    if(!end) return false;
    size_t len = (size_t)(end - start);
    if(len >= out_size) len = out_size - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return true;
}

static bool
    dbc_parse_bo_line(const char* line, uint32_t* can_id, char* msg_name, size_t msg_size) {
    if(strncmp(line, "BO_", 3) != 0) return false;
    const char* p = line + 3;
    while(isspace((unsigned char)*p))
        p++;
    char* end = NULL;
    unsigned long parsed = strtoul(p, &end, 10);
    if(end == p) return false;
    *can_id = (uint32_t)parsed;
    p = end;
    while(isspace((unsigned char)*p))
        p++;
    const char* name_start = p;
    while(*p && *p != ':' && !isspace((unsigned char)*p))
        p++;
    if(name_start == p) return false;
    size_t len = (size_t)(p - name_start);
    if(len >= msg_size) len = msg_size - 1;
    memcpy(msg_name, name_start, len);
    msg_name[len] = '\0';
    return true;
}

static bool
    dbc_parse_sg_line(const char* line, DbcSignal* sig, uint32_t can_id, const char* msg_name) {
    if(strncmp(line, "SG_", 3) != 0) return false;
    const char* p = line + 3;
    while(isspace((unsigned char)*p))
        p++;

    const char* name_start = p;
    while(*p && !isspace((unsigned char)*p) && *p != ':')
        p++;
    if(name_start == p) return false;
    size_t name_len = (size_t)(p - name_start);
    if(name_len >= TEXT_INPUT_SIZE) name_len = TEXT_INPUT_SIZE - 1;

    while(*p && *p != ':')
        p++;
    if(*p != ':') return false;
    p++;
    while(isspace((unsigned char)*p))
        p++;

    char* end = NULL;
    long start_bit = strtol(p, &end, 10);
    if(end == p || *end != '|') return false;
    p = end + 1;
    long bit_length = strtol(p, &end, 10);
    if(end == p || *end != '@') return false;
    p = end + 1;

    char endian_char = *p++;
    char sign_char = *p++;

    while(isspace((unsigned char)*p))
        p++;
    if(*p != '(') return false;
    p++;
    double scale = strtod(p, &end);
    if(end == p || *end != ',') return false;
    p = end + 1;
    double offset = strtod(p, &end);
    if(end == p || *end != ')') return false;
    p = end + 1;

    while(isspace((unsigned char)*p))
        p++;
    if(*p != '[') return false;
    p++;
    double min_val = strtod(p, &end);
    if(end == p || *end != '|') return false;
    p = end + 1;
    double max_val = strtod(p, &end);
    if(end == p || *end != ']') return false;
    p = end + 1;

    while(isspace((unsigned char)*p))
        p++;
    char unit[TEXT_INPUT_SIZE] = {0};
    if(*p == '"') {
        p++;
        const char* unit_start = p;
        while(*p && *p != '"')
            p++;
        size_t unit_len = (size_t)(p - unit_start);
        if(unit_len >= TEXT_INPUT_SIZE) unit_len = TEXT_INPUT_SIZE - 1;
        memcpy(unit, unit_start, unit_len);
        unit[unit_len] = '\0';
    }

    memset(sig, 0, sizeof(*sig));
    sig->used = true;
    strlcpy(sig->message_name, msg_name && msg_name[0] ? msg_name : "Message", TEXT_INPUT_SIZE);
    memcpy(sig->signal_name, name_start, name_len);
    sig->signal_name[name_len] = '\0';
    snprintf(sig->can_id, sizeof(sig->can_id), "0x%lX", (unsigned long)can_id);
    snprintf(sig->start_bit, sizeof(sig->start_bit), "%ld", start_bit);
    snprintf(sig->bit_length, sizeof(sig->bit_length), "%ld", bit_length);
    sig->endianness[0] = (endian_char == '1') ? '1' : '0';
    sig->endianness[1] = '\0';
    sig->signedness[0] = (sign_char == '-') ? 's' : 'u';
    sig->signedness[1] = '\0';
    snprintf(sig->scalar, sizeof(sig->scalar), "%.6g", scale);
    snprintf(sig->offset, sizeof(sig->offset), "%.6g", offset);
    snprintf(sig->min_val, sizeof(sig->min_val), "%.6g", min_val);
    snprintf(sig->max_val, sizeof(sig->max_val), "%.6g", max_val);
    strlcpy(sig->unit, unit, sizeof(sig->unit));
    return true;
}

static bool dbc_read_vehicle_info(const char* path, char* out, size_t out_size) {
    if(!out || out_size == 0) return false;
    out[0] = '\0';
    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = buffered_file_stream_alloc(storage);
    bool found = false;
    if(!buffered_file_stream_open(stream, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        goto cleanup;
    }
    FuriString* line = furi_string_alloc();
    while(stream_read_line(stream, line)) {
        char buf[256];
        strlcpy(buf, furi_string_get_cstr(line), sizeof(buf));
        dbc_trim_right(buf);
        const char* trimmed = dbc_trim_left(buf);
        if(dbc_parse_vehicle_comment(trimmed, out, out_size)) {
            found = true;
            break;
        }
    }
    furi_string_free(line);

cleanup:
    buffered_file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
    return found;
}

static bool dbc_load_file(App* app, const char* path) {
    dbc_clear_signals(app);
    app->vehicle_info[0] = '\0';
    if(!app->dbc_header) {
        app->dbc_header = furi_string_alloc();
    }
    furi_string_reset(app->dbc_header);

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = buffered_file_stream_alloc(storage);
    bool ok = false;

    if(!buffered_file_stream_open(stream, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Could not open DBC file: %s", path);
        goto cleanup;
    }

    FuriString* line = furi_string_alloc();
    uint32_t current_can_id = 0;
    char current_msg[TEXT_INPUT_SIZE] = {0};

    while(stream_read_line(stream, line)) {
        char buf[256];
        strlcpy(buf, furi_string_get_cstr(line), sizeof(buf));
        dbc_trim_right(buf);
        const char* trimmed = dbc_trim_left(buf);
        if(trimmed[0] == '\0') {
            continue;
        }

        if(dbc_parse_vehicle_comment(trimmed, app->vehicle_info, sizeof(app->vehicle_info))) {
            continue;
        }

        if(dbc_parse_bo_line(trimmed, &current_can_id, current_msg, sizeof(current_msg))) {
            continue;
        }

        if(app->signal_count < MAX_SIGNALS) {
            DbcSignal sig;
            if(dbc_parse_sg_line(trimmed, &sig, current_can_id, current_msg)) {
                app->signals[app->signal_count++] = sig;
                continue;
            }
        } else {
            FURI_LOG_W(TAG, "MAX_SIGNALS reached, ignoring extra signals");
        }

        furi_string_cat_str(app->dbc_header, buf);
        furi_string_cat_str(app->dbc_header, "\n");
    }

    furi_string_free(line);
    ok = true;

cleanup:
    buffered_file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
    dbc_sort_signals(app);
    return ok;
}

static void dbc_scan_files(App* app) {
    char active_path[256] = {0};
    if(app->active_dbc_index >= 0 && app->active_dbc_index < app->dbc_file_count) {
        strlcpy(active_path, app->dbc_files[app->active_dbc_index].path, sizeof(active_path));
    }
    dbc_clear_files(app);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    FileInfo file_info;
    char name[128];

    if(!storage_dir_open(dir, DBC_USER_PATH)) {
        storage_file_free(dir);
        furi_record_close(RECORD_STORAGE);
        return;
    }

    while(storage_dir_read(dir, &file_info, name, sizeof(name))) {
        size_t len = strlen(name);
        size_t ext_len = strlen(DBC_EXTENSION);
        if(len <= ext_len) continue;
        if(strcmp(name + len - ext_len, DBC_EXTENSION) != 0) continue;

        if(app->dbc_file_count >= MAX_DBC_FILES) {
            FURI_LOG_W(TAG, "MAX_DBC_FILES reached, ignoring extra files");
            break;
        }

        DbcFile* file = &app->dbc_files[app->dbc_file_count++];
        snprintf(file->path, sizeof(file->path), "%s/%s", DBC_USER_PATH, name);
        size_t name_len = len - ext_len;
        if(name_len >= sizeof(file->name)) name_len = sizeof(file->name) - 1;
        memcpy(file->name, name, name_len);
        file->name[name_len] = '\0';
        dbc_read_vehicle_info(file->path, file->vehicle_info, sizeof(file->vehicle_info));
    }

    storage_dir_close(dir);
    storage_file_free(dir);
    furi_record_close(RECORD_STORAGE);

    if(active_path[0]) {
        for(uint8_t i = 0; i < app->dbc_file_count; i++) {
            if(strcmp(app->dbc_files[i].path, active_path) == 0) {
                app->active_dbc_index = i;
                break;
            }
        }
    }
}

static bool dbc_set_active_file(App* app, int index) {
    if(index < 0 || index >= app->dbc_file_count) return false;
    app->active_dbc_index = index;
    return dbc_load_file(app, app->dbc_files[index].path);
}

static bool dbc_write_file(App* app) {
    if(app->active_dbc_index < 0 || app->active_dbc_index >= app->dbc_file_count) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    Stream* stream = buffered_file_stream_alloc(storage);
    bool ok = false;
    const char* path = app->dbc_files[app->active_dbc_index].path;

    if(!buffered_file_stream_open(stream, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "Could not open DBC file for write: %s", path);
        goto cleanup;
    }

    if(app->dbc_header && furi_string_size(app->dbc_header) > 0) {
        stream_write_cstring(stream, furi_string_get_cstr(app->dbc_header));
        if(furi_string_get_cstr(app->dbc_header)[furi_string_size(app->dbc_header) - 1] != '\n') {
            stream_write_char(stream, '\n');
        }
    }

    if(app->vehicle_info[0]) {
        stream_write_format(stream, "%s%s\";\n", DBC_VEHICLE_PREFIX, app->vehicle_info);
    }

    bool written[MAX_SIGNALS] = {0};
    FuriString* line = furi_string_alloc();
    for(uint8_t i = 0; i < app->signal_count; i++) {
        if(!app->signals[i].used || written[i]) continue;
        uint32_t can_id = dbc_parse_can_id(&app->signals[i]);
        const char* msg_name = app->signals[i].message_name[0] ? app->signals[i].message_name :
                                                                 app->signals[i].signal_name;
        format_dbc_bo_line(line, can_id, msg_name);
        stream_write_cstring(stream, furi_string_get_cstr(line));
        stream_write_char(stream, '\n');

        for(uint8_t j = i; j < app->signal_count; j++) {
            if(!app->signals[j].used || written[j]) continue;
            if(dbc_parse_can_id(&app->signals[j]) != can_id) continue;
            const char* other_msg = app->signals[j].message_name[0] ?
                                        app->signals[j].message_name :
                                        app->signals[j].signal_name;
            if(strcmp(msg_name, other_msg) != 0) continue;
            format_dbc_sg_line(line, &app->signals[j]);
            stream_write_cstring(stream, furi_string_get_cstr(line));
            stream_write_char(stream, '\n');
            written[j] = true;
        }
    }
    furi_string_free(line);

    ok = true;

cleanup:
    buffered_file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
    if(ok) {
        strlcpy(
            app->dbc_files[app->active_dbc_index].vehicle_info,
            app->vehicle_info,
            sizeof(app->dbc_files[app->active_dbc_index].vehicle_info));
    }
    return ok;
}

static uint32_t dbc_parse_can_id(const DbcSignal* sig) {
    uint32_t can_id = 0;
    if(sig && sig->can_id[0]) {
        parse_uint32_str(sig->can_id, &can_id);
    }
    return can_id;
}

static void dbc_sort_signals(App* app) {
    if(app->signal_count < 2) return;
    for(uint8_t i = 0; i < app->signal_count - 1; i++) {
        for(uint8_t j = i + 1; j < app->signal_count; j++) {
            DbcSignal* a = &app->signals[i];
            DbcSignal* b = &app->signals[j];
            int cmp = 0;
            if(app->sort_mode == CantoolsSortByCanId) {
                uint32_t a_id = dbc_parse_can_id(a);
                uint32_t b_id = dbc_parse_can_id(b);
                if(a_id < b_id)
                    cmp = -1;
                else if(a_id > b_id)
                    cmp = 1;
                else
                    cmp = strcasecmp(a->signal_name, b->signal_name);
            } else {
                cmp = strcasecmp(a->signal_name, b->signal_name);
            }
            if(cmp > 0) {
                DbcSignal tmp = *a;
                *a = *b;
                *b = tmp;
            }
        }
    }
}

// Save "current" fields from App into SD, then reload all into memory
static void dbc_save_current_and_reload(App* app) {
    if(app->active_dbc_index < 0) {
        cantools_show_message(app, "No DBC file", "Select or create a DBC file first.");
        return;
    }

    if(!app->editing) {
        strlcpy(app->message_name, app->signal_name, TEXT_INPUT_SIZE);
    }

    DbcSignal tmp = {0};
    tmp.used = true;
    strlcpy(tmp.message_name, app->message_name, TEXT_INPUT_SIZE);
    if(tmp.message_name[0] == '\0') {
        strlcpy(tmp.message_name, app->signal_name, TEXT_INPUT_SIZE);
    }
    strlcpy(tmp.signal_name, app->signal_name, TEXT_INPUT_SIZE);
    strlcpy(tmp.can_id, app->can_id, TEXT_INPUT_SIZE);
    strlcpy(tmp.start_bit, app->start_bit, TEXT_INPUT_SIZE);
    strlcpy(tmp.bit_length, app->bit_length, TEXT_INPUT_SIZE);
    strlcpy(tmp.endianness, app->endianness, TEXT_INPUT_SIZE);
    strlcpy(tmp.signedness, app->signedness, TEXT_INPUT_SIZE);
    strlcpy(tmp.offset, app->offset, TEXT_INPUT_SIZE);
    strlcpy(tmp.scalar, app->scalar, TEXT_INPUT_SIZE);
    strlcpy(tmp.unit, app->unit, TEXT_INPUT_SIZE);
    strlcpy(tmp.min_val, app->min_val, TEXT_INPUT_SIZE);
    strlcpy(tmp.max_val, app->max_val, TEXT_INPUT_SIZE);

    bool updated = false;
    if(app->editing && app->selected_signal_index >= 0 &&
       app->selected_signal_index < app->signal_count) {
        app->signals[app->selected_signal_index] = tmp;
        updated = true;
    }

    if(!updated) {
        if(app->signal_count >= MAX_SIGNALS) {
            cantools_show_message(app, "Error", "Signal limit reached.");
            return;
        }
        app->signals[app->signal_count++] = tmp;
    }

    if(!dbc_write_file(app)) {
        cantools_show_message(app, "Error", "Failed to save DBC file.");
        return;
    }

    app->editing = false;
    dbc_load_file(app, app->dbc_files[app->active_dbc_index].path);

    app->selected_signal_index = -1;
    for(uint8_t i = 0; i < app->signal_count; i++) {
        if(strcmp(app->signals[i].signal_name, app->signal_name) == 0 &&
           strcmp(app->signals[i].can_id, app->can_id) == 0) {
            app->selected_signal_index = i;
        }
    }
}

static void dbc_copy_signal_to_app(App* app, const DbcSignal* sig) {
    strlcpy(app->message_name, sig->message_name, TEXT_INPUT_SIZE);
    strlcpy(app->signal_name, sig->signal_name, TEXT_INPUT_SIZE);
    strlcpy(app->can_id, sig->can_id, TEXT_INPUT_SIZE);
    strlcpy(app->start_bit, sig->start_bit, TEXT_INPUT_SIZE);
    strlcpy(app->bit_length, sig->bit_length, TEXT_INPUT_SIZE);
    strlcpy(app->endianness, sig->endianness, TEXT_INPUT_SIZE);
    strlcpy(app->signedness, sig->signedness, TEXT_INPUT_SIZE);
    strlcpy(app->offset, sig->offset, TEXT_INPUT_SIZE);
    strlcpy(app->scalar, sig->scalar, TEXT_INPUT_SIZE);
    strlcpy(app->unit, sig->unit, TEXT_INPUT_SIZE);
    strlcpy(app->min_val, sig->min_val, TEXT_INPUT_SIZE);
    strlcpy(app->max_val, sig->max_val, TEXT_INPUT_SIZE);
}

static void app_set_defaults(App* app) {
    strlcpy(app->message_name, "DI_vehicleSpeed", TEXT_INPUT_SIZE);
    strlcpy(app->signal_name, "DI_vehicleSpeed", TEXT_INPUT_SIZE);
    strlcpy(app->can_id, "0x257", TEXT_INPUT_SIZE);
    strlcpy(app->start_bit, "12", TEXT_INPUT_SIZE);
    strlcpy(app->bit_length, "12", TEXT_INPUT_SIZE);
    strlcpy(app->endianness, "1", TEXT_INPUT_SIZE);
    strlcpy(app->signedness, "u", TEXT_INPUT_SIZE);
    strlcpy(app->offset, "-40", TEXT_INPUT_SIZE);
    strlcpy(app->scalar, "0_08", TEXT_INPUT_SIZE);
    strlcpy(app->unit, "kph", TEXT_INPUT_SIZE);
    strlcpy(app->min_val, "-40", TEXT_INPUT_SIZE);
    strlcpy(app->max_val, "285", TEXT_INPUT_SIZE);
}

static void dbc_build_field_label(App* app, DbcFieldId field_id, FuriString* out) {
    const DbcFieldConfig* cfg = &dbc_field_configs[field_id];
    const char* value = dbc_field_ptr(app, field_id);
    const char* display = (value && value[0]) ? value : "<unset>";
    furi_string_printf(out, "%s: %s", cfg->menu_label, display);
}

static bool dbc_validate_field(App* app, DbcFieldId field_id, FuriString* error) {
    const DbcFieldConfig* cfg = &dbc_field_configs[field_id];
    const char* value = dbc_field_ptr(app, field_id);
    if(cfg->validator) {
        return cfg->validator(app, value, error);
    }
    return true;
}

static bool dbc_validate_all(App* app) {
    FuriString* error = furi_string_alloc();
    for(uint8_t i = 0; i < DbcFieldCount; i++) {
        if(!dbc_validate_field(app, (DbcFieldId)i, error)) {
            cantools_show_message(app, "Invalid input", furi_string_get_cstr(error));
            furi_string_free(error);
            return false;
        }
    }
    furi_string_free(error);
    return true;
}

static double parse_double_or_default(const char* value, double fallback) {
    if(!value || value[0] == '\0') return fallback;
    char tmp[TEXT_INPUT_SIZE];
    strlcpy(tmp, value, sizeof(tmp));
    decimal_substitute(tmp);
    char* end = NULL;
    double parsed = strtod(tmp, &end);
    if(end == tmp || *end != '\0') return fallback;
    return parsed;
}

static bool decode_extract_raw(
    const uint8_t* data,
    uint8_t data_len,
    uint8_t start_bit,
    uint8_t bit_length,
    bool big_endian,
    uint64_t* out_raw) {
    if(!data || !out_raw) return false;
    if(bit_length == 0 || bit_length > 64) return false;
    if(start_bit > 63) return false;
    if(!big_endian) {
        uint64_t value = 0;
        for(uint8_t i = 0; i < data_len; i++) {
            value |= ((uint64_t)data[i]) << (8U * i);
        }
        uint64_t mask = (bit_length == 64) ? UINT64_MAX : ((1ULL << bit_length) - 1ULL);
        *out_raw = (value >> start_bit) & mask;
        return true;
    }

    if(start_bit + 1 < bit_length) return false;
    uint64_t raw = 0;
    for(uint8_t i = 0; i < bit_length; i++) {
        int32_t bit_index = (int32_t)start_bit - i;
        uint8_t byte_index = (uint8_t)(bit_index / 8);
        if(byte_index >= data_len) return false;
        uint8_t bit_in_byte = 7U - (bit_index % 8);
        uint8_t bit = (data[byte_index] >> bit_in_byte) & 0x01;
        raw = (raw << 1U) | bit;
    }
    *out_raw = raw;
    return true;
}

static bool decode_signal_value(
    const DbcSignal* sig,
    const uint8_t* data,
    uint8_t data_len,
    double* out_value) {
    if(!sig || !data || !out_value) return false;
    int32_t start_bit = 0;
    int32_t bit_length = 0;
    if(!parse_int32_str(sig->start_bit, &start_bit)) return false;
    if(!parse_int32_str(sig->bit_length, &bit_length)) return false;
    if(start_bit < 0 || start_bit > 63) return false;
    if(bit_length <= 0 || bit_length > 64) return false;

    bool big_endian = false;
    if(sig->endianness[0]) {
        char c = sig->endianness[0];
        big_endian = (c == '0' || c == 'M' || c == 'm' || c == 'B' || c == 'b');
    }

    uint64_t raw = 0;
    if(!decode_extract_raw(
           data, data_len, (uint8_t)start_bit, (uint8_t)bit_length, big_endian, &raw)) {
        return false;
    }

    bool is_signed = false;
    if(sig->signedness[0]) {
        char c = sig->signedness[0];
        is_signed = (c == 's' || c == 'S' || c == '-' || c == '1');
    }

    int64_t signed_raw = (int64_t)raw;
    if(is_signed && bit_length < 64) {
        uint64_t sign_bit = 1ULL << (bit_length - 1);
        if(raw & sign_bit) {
            uint64_t mask = (1ULL << bit_length) - 1ULL;
            signed_raw = (int64_t)(raw | (~mask));
        }
    }

    double scale = parse_double_or_default(sig->scalar, 1.0);
    double offset = parse_double_or_default(sig->offset, 0.0);
    *out_value = (double)signed_raw * scale + offset;
    return true;
}

void Cantools_scenes_main_menu_callback(void* context, uint32_t index) {
    App* app = context;
    switch(index) {
    case CantoolsScenesMainMenuSceneDbcMaker:
        scene_manager_handle_custom_event(
            app->scene_manager, CantoolsScenesMainMenuSceneDbcMakerEvent);
        break;
    case CantoolsScenesMainMenuSceneViewDbc:
        scene_manager_handle_custom_event(
            app->scene_manager, CantoolsScenesMainMenuSceneViewDbcEvent);
        break;
    case CantoolsScenesMainMenuSceneDecodeData:
        scene_manager_handle_custom_event(
            app->scene_manager, CantoolsScenesMainMenuSceneDecodeDataEvent);
        break;
    case CantoolsScenesMainMenuSceneDbcFiles:
        scene_manager_handle_custom_event(
            app->scene_manager, CantoolsScenesMainMenuSceneDbcFilesEvent);
        break;
    }
}

void Cantools_scenes_main_menu_scene_on_enter(void* context) {
    App* app = context;
    app->editing = false;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "CAN Tools");

    submenu_add_item(
        app->submenu,
        "DBC Maker",
        CantoolsScenesMainMenuSceneDbcMaker,
        Cantools_scenes_main_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "View DBC",
        CantoolsScenesMainMenuSceneViewDbc,
        Cantools_scenes_main_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Decode Data",
        CantoolsScenesMainMenuSceneDecodeData,
        Cantools_scenes_main_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "DBC Files",
        CantoolsScenesMainMenuSceneDbcFiles,
        Cantools_scenes_main_menu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, CantoolsScenesSubmenuView);
}

bool Cantools_scenes_main_menu_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;
    switch(event.type) {
    case SceneManagerEventTypeCustom:
        switch(event.event) {
        case CantoolsScenesMainMenuSceneDbcMakerEvent:
            scene_manager_next_scene(app->scene_manager, CantoolsScenesDbcMakerMenuScene);
            consumed = true;
            break;
        case CantoolsScenesMainMenuSceneViewDbcEvent:
            if(app->active_dbc_index >= 0) {
                dbc_load_file(app, app->dbc_files[app->active_dbc_index].path);
            }
            scene_manager_next_scene(app->scene_manager, CantoolsScenesViewDbcListScene);
            consumed = true;
            break;
        case CantoolsScenesMainMenuSceneDecodeDataEvent:
            scene_manager_next_scene(app->scene_manager, CantoolsScenesDecodeDataScene);
            consumed = true;
            break;
        case CantoolsScenesMainMenuSceneDbcFilesEvent:
            scene_manager_next_scene(app->scene_manager, CantoolsScenesDbcFileListScene);
            consumed = true;
            break;
        }
        break;
    default:
        break;
    }
    return consumed;
}

void Cantools_scenes_main_menu_scene_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

// DBC Files

void Cantools_scenes_dbc_file_menu_callback(void* context, uint32_t index) {
    App* app = context;
    scene_manager_handle_custom_event(app->scene_manager, index);
}

void Cantools_scenes_dbc_file_list_scene_on_enter(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "DBC Files");

    dbc_scan_files(app);

    submenu_add_item(
        app->submenu,
        "Create DBC",
        CANTOOLS_EVENT_DBC_CREATE,
        Cantools_scenes_dbc_file_menu_callback,
        app);

    FuriString* label = furi_string_alloc();
    for(uint8_t i = 0; i < app->dbc_file_count; i++) {
        const char* name = app->dbc_files[i].name;
        const char* info = app->dbc_files[i].vehicle_info;
        const char* prefix = (app->active_dbc_index == (int)i) ? "* " : "";
        if(info[0]) {
            furi_string_printf(label, "%s%s - %s", prefix, name, info);
        } else {
            furi_string_printf(label, "%s%s", prefix, name);
        }
        submenu_add_item(
            app->submenu,
            furi_string_get_cstr(label),
            CANTOOLS_EVENT_DBC_FILE_BASE + i,
            Cantools_scenes_dbc_file_menu_callback,
            app);
    }
    furi_string_free(label);

    view_dispatcher_switch_to_view(app->view_dispatcher, CantoolsScenesSubmenuView);
}

bool Cantools_scenes_dbc_file_list_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == CANTOOLS_EVENT_DBC_CREATE) {
            app->dbc_file_name[0] = '\0';
            app->vehicle_info[0] = '\0';
            scene_manager_next_scene(app->scene_manager, CantoolsScenesDbcFileNameScene);
            return true;
        }
        if(event.event >= CANTOOLS_EVENT_DBC_FILE_BASE) {
            uint32_t idx = event.event - CANTOOLS_EVENT_DBC_FILE_BASE;
            if(idx < app->dbc_file_count) {
                if(dbc_set_active_file(app, (int)idx)) {
                    scene_manager_next_scene(app->scene_manager, CantoolsScenesMainMenuScene);
                } else {
                    cantools_show_message(app, "Error", "Failed to load DBC file.");
                }
                return true;
            }
        }
    }
    return false;
}

void Cantools_scenes_dbc_file_list_scene_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

static void cantools_dbc_name_input_callback(void* context) {
    App* app = context;
    scene_manager_handle_custom_event(app->scene_manager, CANTOOLS_EVENT_DBC_NAME_SAVE);
}

static void cantools_vehicle_info_input_callback(void* context) {
    App* app = context;
    scene_manager_handle_custom_event(app->scene_manager, CANTOOLS_EVENT_DBC_VEHICLE_SAVE);
}

static void cantools_simple_text_input_on_enter(
    App* app,
    const char* header,
    char* value,
    TextInputCallback callback) {
    bool clear_text = true;
    strlcpy(app->buffer, value, app->buffer_size);
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, header);
    text_input_set_result_callback(
        app->text_input, callback, app, app->buffer, app->buffer_size, clear_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, CantoolsScenesTextInputView);
}

void Cantools_scenes_dbc_file_name_scene_on_enter(void* context) {
    App* app = context;
    cantools_simple_text_input_on_enter(
        app, "DBC file name:", app->dbc_file_name, cantools_dbc_name_input_callback);
}

bool Cantools_scenes_dbc_file_name_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == CANTOOLS_EVENT_DBC_NAME_SAVE) {
        strlcpy(app->dbc_file_name, app->buffer, sizeof(app->dbc_file_name));
        if(app->dbc_file_name[0] == '\0') {
            cantools_show_message(app, "Invalid name", "Enter a file name.");
            return true;
        }
        dbc_sanitize_file_name(app->dbc_file_name, sizeof(app->dbc_file_name));
        scene_manager_next_scene(app->scene_manager, CantoolsScenesVehicleInfoScene);
        return true;
    }
    return false;
}

void Cantools_scenes_dbc_file_name_scene_on_exit(void* context) {
    UNUSED(context);
}

static bool dbc_create_file(App* app) {
    char path[256];
    dbc_build_file_path(app->dbc_file_name, path, sizeof(path));

    Storage* storage = furi_record_open(RECORD_STORAGE);
    FileInfo info;
    if(storage_common_stat(storage, path, &info) == FSE_OK) {
        furi_record_close(RECORD_STORAGE);
        cantools_show_message(app, "Error", "File already exists.");
        return false;
    }

    Stream* stream = buffered_file_stream_alloc(storage);
    bool ok = false;
    if(!buffered_file_stream_open(stream, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FURI_LOG_E(TAG, "Could not create DBC file: %s", path);
        goto cleanup;
    }

    dbc_set_default_header(app);
    stream_write_cstring(stream, furi_string_get_cstr(app->dbc_header));
    if(furi_string_get_cstr(app->dbc_header)[furi_string_size(app->dbc_header) - 1] != '\n') {
        stream_write_char(stream, '\n');
    }

    if(app->vehicle_info[0]) {
        stream_write_format(stream, "%s%s\";\n", DBC_VEHICLE_PREFIX, app->vehicle_info);
    }

    ok = true;

cleanup:
    buffered_file_stream_close(stream);
    stream_free(stream);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

void Cantools_scenes_vehicle_info_scene_on_enter(void* context) {
    App* app = context;
    cantools_simple_text_input_on_enter(
        app, "Vehicle (make/model/year):", app->vehicle_info, cantools_vehicle_info_input_callback);
}

bool Cantools_scenes_vehicle_info_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == CANTOOLS_EVENT_DBC_VEHICLE_SAVE) {
        strlcpy(app->vehicle_info, app->buffer, sizeof(app->vehicle_info));
        if(!dbc_create_file(app)) {
            return true;
        }
        dbc_scan_files(app);
        for(uint8_t i = 0; i < app->dbc_file_count; i++) {
            if(strcmp(app->dbc_files[i].name, app->dbc_file_name) == 0) {
                dbc_set_active_file(app, i);
                break;
            }
        }
        scene_manager_next_scene(app->scene_manager, CantoolsScenesDbcFileListScene);
        return true;
    }
    return false;
}

void Cantools_scenes_vehicle_info_scene_on_exit(void* context) {
    UNUSED(context);
}

void Cantools_scenes_dbc_maker_menu_callback(void* context, uint32_t index) {
    App* app = context;
    scene_manager_handle_custom_event(app->scene_manager, index);
}

void Cantools_scenes_dbc_maker_menu_scene_on_enter(void* context) {
    App* app = context;
    if(app->active_dbc_index < 0) {
        cantools_show_message(app, "No DBC file", "Select or create a DBC file first.");
        scene_manager_next_scene(app->scene_manager, CantoolsScenesDbcFileListScene);
        return;
    }
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->editing ? "Edit Signal" : "DBC Maker");

    FuriString* label = furi_string_alloc();
    for(uint8_t i = 0; i < DbcFieldCount; i++) {
        dbc_build_field_label(app, (DbcFieldId)i, label);
        submenu_add_item(
            app->submenu,
            furi_string_get_cstr(label),
            i,
            Cantools_scenes_dbc_maker_menu_callback,
            app);
    }
    furi_string_free(label);
    submenu_add_item(
        app->submenu,
        "Generate & Save",
        CantoolsDbcMakerMenuGenerate,
        Cantools_scenes_dbc_maker_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Reset defaults",
        CantoolsDbcMakerMenuResetDefaults,
        Cantools_scenes_dbc_maker_menu_callback,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, CantoolsScenesSubmenuView);
}

bool Cantools_scenes_dbc_maker_menu_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event < DbcFieldCount) {
            DbcFieldId field_id = (DbcFieldId)event.event;
            app->active_field = field_id;
            scene_manager_next_scene(app->scene_manager, dbc_field_configs[field_id].scene);
            consumed = true;
        } else if(event.event == CantoolsDbcMakerMenuGenerate) {
            scene_manager_next_scene(app->scene_manager, CantoolsScenesCalculateScene);
            consumed = true;
        } else if(event.event == CantoolsDbcMakerMenuResetDefaults) {
            app_set_defaults(app);
            calculate_values(app);
            scene_manager_next_scene(app->scene_manager, CantoolsScenesDbcMakerMenuScene);
            consumed = true;
        }
    }
    return consumed;
}

void Cantools_scenes_dbc_maker_menu_scene_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

static void cantools_text_input_callback(void* context) {
    App* app = context;
    scene_manager_handle_custom_event(app->scene_manager, CANTOOLS_EVENT_TEXT_INPUT_SAVE);
}

static void cantools_text_input_scene_on_enter(App* app, DbcFieldId field_id) {
    bool clear_text = true;
    char* value = dbc_field_ptr(app, field_id);
    strlcpy(app->buffer, value, app->buffer_size);
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, dbc_field_configs[field_id].input_header);
    text_input_set_result_callback(
        app->text_input,
        cantools_text_input_callback,
        app,
        app->buffer,
        app->buffer_size,
        clear_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, CantoolsScenesTextInputView);
}

static bool
    cantools_text_input_scene_on_event(App* app, SceneManagerEvent event, DbcFieldId field_id) {
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == CANTOOLS_EVENT_TEXT_INPUT_SAVE) {
        FuriString* error = furi_string_alloc();
        if(!dbc_field_configs[field_id].validator(app, app->buffer, error)) {
            cantools_show_message(app, "Invalid input", furi_string_get_cstr(error));
            furi_string_free(error);
            return true;
        }
        furi_string_free(error);
        strlcpy(dbc_field_ptr(app, field_id), app->buffer, TEXT_INPUT_SIZE);
        calculate_values(app);
        scene_manager_next_scene(app->scene_manager, dbc_field_configs[field_id].next_scene);
        return true;
    }
    return false;
}

#define DEFINE_FIELD_SCENE_HANDLERS(field_id, name)                                        \
    void Cantools_scenes_##name##_scene_on_enter(void* context) {                          \
        App* app = context;                                                                \
        cantools_text_input_scene_on_enter(app, field_id);                                 \
    }                                                                                      \
    bool Cantools_scenes_##name##_scene_on_event(void* context, SceneManagerEvent event) { \
        App* app = context;                                                                \
        return cantools_text_input_scene_on_event(app, event, field_id);                   \
    }                                                                                      \
    void Cantools_scenes_##name##_scene_on_exit(void* context) {                           \
        UNUSED(context);                                                                   \
    }

DEFINE_FIELD_SCENE_HANDLERS(DbcFieldSignalName, signal_name)
DEFINE_FIELD_SCENE_HANDLERS(DbcFieldCanId, can_id)
DEFINE_FIELD_SCENE_HANDLERS(DbcFieldStartBit, start_bit)
DEFINE_FIELD_SCENE_HANDLERS(DbcFieldBitLength, bit_length)
DEFINE_FIELD_SCENE_HANDLERS(DbcFieldEndianness, endianness)
DEFINE_FIELD_SCENE_HANDLERS(DbcFieldSignedness, signed)
DEFINE_FIELD_SCENE_HANDLERS(DbcFieldOffset, offset)
DEFINE_FIELD_SCENE_HANDLERS(DbcFieldScalar, scalar)
DEFINE_FIELD_SCENE_HANDLERS(DbcFieldUnit, unit)
DEFINE_FIELD_SCENE_HANDLERS(DbcFieldMin, min)
DEFINE_FIELD_SCENE_HANDLERS(DbcFieldMax, max)

void Cantools_scenes_calculate_scene_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);

    if(app->active_dbc_index < 0) {
        cantools_show_message(app, "No DBC file", "Select or create a DBC file first.");
        scene_manager_next_scene(app->scene_manager, CantoolsScenesDbcFileListScene);
        return;
    }

    if(!dbc_validate_all(app)) {
        scene_manager_next_scene(app->scene_manager, CantoolsScenesDbcMakerMenuScene);
        return;
    }

    // Normalize numeric-like fields
    calculate_values(app);

    // Persist to SD and reload in-memory list
    dbc_save_current_and_reload(app);

    FuriString* dbc = furi_string_alloc();
    format_dbc_entry(
        dbc,
        app->message_name,
        app->signal_name,
        app->can_id,
        app->start_bit,
        app->bit_length,
        app->endianness,
        app->signedness,
        app->scalar,
        app->offset,
        app->min_val,
        app->max_val,
        app->unit);

    widget_add_text_scroll_element(app->widget, 1, 1, 128, 64, furi_string_get_cstr(dbc));

    furi_string_free(dbc);

    view_dispatcher_switch_to_view(app->view_dispatcher, CantoolsScenesWidgetView);
}
bool Cantools_scenes_calculate_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}
void Cantools_scenes_calculate_scene_on_exit(void* context) {
    UNUSED(context);
}

void Cantools_scenes_view_dbc_menu_callback(void* context, uint32_t index) {
    App* app = context;
    scene_manager_handle_custom_event(app->scene_manager, index);
}

void Cantools_scenes_view_dbc_list_scene_on_enter(void* context) {
    App* app = context;

    if(app->active_dbc_index < 0) {
        cantools_show_message(app, "No DBC file", "Select or create a DBC file first.");
        scene_manager_next_scene(app->scene_manager, CantoolsScenesDbcFileListScene);
        return;
    }

    dbc_load_file(app, app->dbc_files[app->active_dbc_index].path);

    if(app->signal_count == 0) {
        widget_reset(app->widget);
        widget_add_text_scroll_element(
            app->widget,
            1,
            1,
            128,
            64,
            "No signals defined.\n\nUse DBC Maker to add\nsignals first.");
        view_dispatcher_switch_to_view(app->view_dispatcher, CantoolsScenesWidgetView);
    } else {
        submenu_reset(app->submenu);
        const char* file_name = app->dbc_files[app->active_dbc_index].name;
        if(file_name[0]) {
            submenu_set_header(app->submenu, file_name);
        } else {
            submenu_set_header(app->submenu, "Saved DBC signals");
        }
        const char* sort_label = (app->sort_mode == CantoolsSortByCanId) ? "Sort: CAN ID" :
                                                                           "Sort: Name";
        submenu_add_item(
            app->submenu,
            sort_label,
            CANTOOLS_EVENT_SORT_TOGGLE,
            Cantools_scenes_view_dbc_menu_callback,
            app);

        FuriString* label = furi_string_alloc();
        for(uint8_t i = 0; i < app->signal_count; i++) {
            const char* name = app->signals[i].signal_name;
            if(!name || strlen(name) == 0) {
                name = "(unnamed)";
            }
            furi_string_printf(label, "%s (%s)", name, app->signals[i].can_id);
            submenu_add_item(
                app->submenu,
                furi_string_get_cstr(label),
                CANTOOLS_EVENT_SIGNAL_BASE + i,
                Cantools_scenes_view_dbc_menu_callback,
                app);
        }
        furi_string_free(label);
        view_dispatcher_switch_to_view(app->view_dispatcher, CantoolsScenesSubmenuView);
    }
}

bool Cantools_scenes_view_dbc_list_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == CANTOOLS_EVENT_SORT_TOGGLE) {
            app->sort_mode = (app->sort_mode == CantoolsSortByCanId) ? CantoolsSortByName :
                                                                       CantoolsSortByCanId;
            dbc_sort_signals(app);
            scene_manager_next_scene(app->scene_manager, CantoolsScenesViewDbcListScene);
            return true;
        }
        if(event.event >= CANTOOLS_EVENT_SIGNAL_BASE) {
            uint32_t idx = event.event - CANTOOLS_EVENT_SIGNAL_BASE;
            if(idx < app->signal_count) {
                app->selected_signal_index = (int)idx;
                scene_manager_next_scene(app->scene_manager, CantoolsScenesSignalActionsScene);
                return true;
            }
        }
    }
    return false;
}
void Cantools_scenes_view_dbc_list_scene_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

void Cantools_scenes_signal_actions_menu_callback(void* context, uint32_t index) {
    App* app = context;
    scene_manager_handle_custom_event(app->scene_manager, index);
}

void Cantools_scenes_signal_actions_scene_on_enter(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Signal actions");
    submenu_add_item(
        app->submenu,
        "View details",
        CantoolsSignalActionView,
        Cantools_scenes_signal_actions_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Edit",
        CantoolsSignalActionEdit,
        Cantools_scenes_signal_actions_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Delete",
        CantoolsSignalActionDelete,
        Cantools_scenes_signal_actions_menu_callback,
        app);
    view_dispatcher_switch_to_view(app->view_dispatcher, CantoolsScenesSubmenuView);
}

bool Cantools_scenes_signal_actions_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(app->selected_signal_index < 0 || app->selected_signal_index >= app->signal_count) {
            cantools_show_message(app, "Error", "Invalid signal selection.");
            scene_manager_next_scene(app->scene_manager, CantoolsScenesViewDbcListScene);
            return true;
        }
        DbcSignal* sig = &app->signals[app->selected_signal_index];
        switch(event.event) {
        case CantoolsSignalActionView:
            scene_manager_next_scene(app->scene_manager, CantoolsScenesViewDbcDetailScene);
            return true;
        case CantoolsSignalActionEdit:
            dbc_copy_signal_to_app(app, sig);
            app->editing = true;
            scene_manager_next_scene(app->scene_manager, CantoolsScenesDbcMakerMenuScene);
            return true;
        case CantoolsSignalActionDelete: {
            DialogMessage* message = dialog_message_alloc();
            dialog_message_set_header(message, "Delete signal?", 64, 0, AlignCenter, AlignTop);
            dialog_message_set_text(message, sig->signal_name, 64, 20, AlignCenter, AlignTop);
            dialog_message_set_buttons(message, "Cancel", NULL, "Delete");
            DialogMessageButton result = dialog_message_show(app->dialogs, message);
            dialog_message_free(message);
            if(result == DialogMessageButtonRight) {
                if(app->selected_signal_index >= 0 &&
                   app->selected_signal_index < app->signal_count) {
                    for(uint8_t i = app->selected_signal_index; i + 1 < app->signal_count; i++) {
                        app->signals[i] = app->signals[i + 1];
                    }
                    app->signal_count--;
                }
                if(!dbc_write_file(app)) {
                    cantools_show_message(app, "Error", "Failed to update DBC file.");
                } else if(app->active_dbc_index >= 0) {
                    dbc_load_file(app, app->dbc_files[app->active_dbc_index].path);
                }
                app->editing = false;
                scene_manager_next_scene(app->scene_manager, CantoolsScenesViewDbcListScene);
            } else {
                scene_manager_next_scene(app->scene_manager, CantoolsScenesViewDbcListScene);
            }
            return true;
        }
        default:
            break;
        }
    }
    return false;
}

void Cantools_scenes_signal_actions_scene_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

void Cantools_scenes_view_dbc_detail_scene_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);

    if(app->selected_signal_index < 0 || app->selected_signal_index >= app->signal_count) {
        widget_add_text_scroll_element(app->widget, 1, 1, 128, 64, "Invalid signal index.");
    } else {
        DbcSignal* sig = &app->signals[app->selected_signal_index];
        if(!sig->used) {
            widget_add_text_scroll_element(app->widget, 1, 1, 128, 64, "Empty signal slot.");
        } else {
            FuriString* dbc = furi_string_alloc();
            format_dbc_entry(
                dbc,
                sig->message_name,
                sig->signal_name,
                sig->can_id,
                sig->start_bit,
                sig->bit_length,
                sig->endianness,
                sig->signedness,
                sig->scalar,
                sig->offset,
                sig->min_val,
                sig->max_val,
                sig->unit);
            widget_add_text_scroll_element(app->widget, 1, 1, 128, 64, furi_string_get_cstr(dbc));
            furi_string_free(dbc);
        }
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, CantoolsScenesWidgetView);
}
bool Cantools_scenes_view_dbc_detail_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}
void Cantools_scenes_view_dbc_detail_scene_on_exit(void* context) {
    UNUSED(context);
}

// Decoding

static void cantools_byte_input_callback(void* context) {
    App* app = context;
    scene_manager_handle_custom_event(app->scene_manager, CANTOOLS_EVENT_BYTE_INPUT_SAVE);
}

static void cantools_decode_byte_input_on_enter(
    App* app,
    const char* header,
    uint8_t* buffer,
    uint8_t size) {
    byte_input_set_header_text(app->byte_input, header);
    byte_input_set_result_callback(
        app->byte_input, cantools_byte_input_callback, NULL, app, buffer, size);
    view_dispatcher_switch_to_view(app->view_dispatcher, CantoolsScenesByteInputView);
}

static void cantools_decode_canid_input_callback(void* context) {
    App* app = context;
    scene_manager_handle_custom_event(app->scene_manager, CANTOOLS_EVENT_DECODE_CANID_SAVE);
}

static void cantools_decode_dlc_input_callback(void* context) {
    App* app = context;
    scene_manager_handle_custom_event(app->scene_manager, CANTOOLS_EVENT_DECODE_DLC_SAVE);
}

void Cantools_scenes_decode_data_scene_on_enter(void* context) {
    App* app = context;
    if(app->active_dbc_index < 0) {
        cantools_show_message(app, "No DBC file", "Select or create a DBC file first.");
        scene_manager_next_scene(app->scene_manager, CantoolsScenesDbcFileListScene);
        return;
    }
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Decode Data");
    submenu_add_item(
        app->submenu,
        "Manual frame",
        CANTOOLS_EVENT_DECODE_MANUAL,
        Cantools_scenes_view_dbc_menu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Decode log",
        CANTOOLS_EVENT_DECODE_LOG,
        Cantools_scenes_view_dbc_menu_callback,
        app);
    view_dispatcher_switch_to_view(app->view_dispatcher, CantoolsScenesSubmenuView);
}

bool Cantools_scenes_decode_data_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == CANTOOLS_EVENT_DECODE_MANUAL) {
            scene_manager_next_scene(app->scene_manager, CantoolsScenesDecodeManualCanIdScene);
            return true;
        }
        if(event.event == CANTOOLS_EVENT_DECODE_LOG) {
            scene_manager_next_scene(app->scene_manager, CantoolsScenesDecodeLogScene);
            return true;
        }
    }
    return false;
}

void Cantools_scenes_decode_data_scene_on_exit(void* context) {
    App* app = context;
    submenu_reset(app->submenu);
}

void Cantools_scenes_decode_manual_can_id_scene_on_enter(void* context) {
    App* app = context;
    if(app->decode_can_id_text[0] == '\0') {
        strlcpy(app->decode_can_id_text, "0x", sizeof(app->decode_can_id_text));
    }
    cantools_simple_text_input_on_enter(
        app, "CAN ID (dec/0x):", app->decode_can_id_text, cantools_decode_canid_input_callback);
}

bool Cantools_scenes_decode_manual_can_id_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == CANTOOLS_EVENT_DECODE_CANID_SAVE) {
        strlcpy(app->decode_can_id_text, app->buffer, sizeof(app->decode_can_id_text));
        uint32_t can_id = 0;
        if(!parse_uint32_str(app->decode_can_id_text, &can_id) || can_id > 0x1FFFFFFF) {
            cantools_show_message(app, "Invalid CAN ID", "Use 0x0..0x1FFFFFFF.");
            return true;
        }
        app->decode_can_id = can_id;
        scene_manager_next_scene(app->scene_manager, CantoolsScenesDecodeManualDlcScene);
        return true;
    }
    return false;
}

void Cantools_scenes_decode_manual_can_id_scene_on_exit(void* context) {
    UNUSED(context);
}

void Cantools_scenes_decode_manual_dlc_scene_on_enter(void* context) {
    App* app = context;
    if(app->decode_dlc_text[0] == '\0') {
        strlcpy(app->decode_dlc_text, "8", sizeof(app->decode_dlc_text));
    }
    cantools_simple_text_input_on_enter(
        app, "DLC (0-8):", app->decode_dlc_text, cantools_decode_dlc_input_callback);
}

bool Cantools_scenes_decode_manual_dlc_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == CANTOOLS_EVENT_DECODE_DLC_SAVE) {
        strlcpy(app->decode_dlc_text, app->buffer, sizeof(app->decode_dlc_text));
        int32_t dlc = 0;
        if(!parse_int32_str(app->decode_dlc_text, &dlc) || dlc < 0 || dlc > 8) {
            cantools_show_message(app, "Invalid DLC", "DLC must be 0..8.");
            return true;
        }
        app->decode_dlc = (uint8_t)dlc;
        app->decode_data_len = app->decode_dlc;
        if(app->decode_dlc == 0) {
            scene_manager_next_scene(app->scene_manager, CantoolsScenesDecodeResultScene);
        } else {
            scene_manager_next_scene(app->scene_manager, CantoolsScenesDecodeManualDataScene);
        }
        return true;
    }
    return false;
}

void Cantools_scenes_decode_manual_dlc_scene_on_exit(void* context) {
    UNUSED(context);
}

void Cantools_scenes_decode_manual_data_scene_on_enter(void* context) {
    App* app = context;
    uint8_t len = app->decode_data_len;
    if(len == 0 || len > 8) len = 8;
    cantools_decode_byte_input_on_enter(app, "Data bytes:", app->decode_data_bytes, len);
}

bool Cantools_scenes_decode_manual_data_scene_on_event(void* context, SceneManagerEvent event) {
    App* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == CANTOOLS_EVENT_BYTE_INPUT_SAVE) {
        scene_manager_next_scene(app->scene_manager, CantoolsScenesDecodeResultScene);
        return true;
    }
    return false;
}

void Cantools_scenes_decode_manual_data_scene_on_exit(void* context) {
    UNUSED(context);
}

void Cantools_scenes_decode_result_scene_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);

    if(app->active_dbc_index >= 0) {
        dbc_load_file(app, app->dbc_files[app->active_dbc_index].path);
    }

    if(app->signal_count == 0) {
        widget_add_text_scroll_element(
            app->widget, 1, 1, 128, 64, "No signals saved.\nAdd DBC signals first.");
        view_dispatcher_switch_to_view(app->view_dispatcher, CantoolsScenesWidgetView);
        return;
    }

    FuriString* result = furi_string_alloc();
    furi_string_printf(
        result, "CAN ID: 0x%lX\nDLC: %u\n\n", (unsigned long)app->decode_can_id, app->decode_dlc);

    bool matched = false;
    for(uint8_t i = 0; i < app->signal_count; i++) {
        const DbcSignal* sig = &app->signals[i];
        uint32_t sig_id = dbc_parse_can_id(sig);
        if(sig_id != app->decode_can_id) continue;

        double value = 0.0;
        if(!decode_signal_value(sig, app->decode_data_bytes, app->decode_dlc, &value)) {
            furi_string_cat_printf(result, "%s: decode error\n", sig->signal_name);
            matched = true;
            continue;
        }

        const char* unit = sig->unit[0] ? sig->unit : "";
        furi_string_cat_printf(result, "%s: %.3f %s\n", sig->signal_name, value, unit);
        matched = true;
    }

    if(!matched) {
        furi_string_cat_str(result, "No matching signals.");
    }

    widget_add_text_scroll_element(app->widget, 1, 1, 128, 64, furi_string_get_cstr(result));
    furi_string_free(result);
    view_dispatcher_switch_to_view(app->view_dispatcher, CantoolsScenesWidgetView);
}

bool Cantools_scenes_decode_result_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void Cantools_scenes_decode_result_scene_on_exit(void* context) {
    UNUSED(context);
}

void Cantools_scenes_decode_log_scene_on_enter(void* context) {
    App* app = context;
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 1, 1, 128, 64, "Decode log\n\nComing soon...");
    view_dispatcher_switch_to_view(app->view_dispatcher, CantoolsScenesWidgetView);
}

bool Cantools_scenes_decode_log_scene_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void Cantools_scenes_decode_log_scene_on_exit(void* context) {
    UNUSED(context);
}

// Scene manager handlers, lots of scenes :(

void (*const Cantools_scenes_scene_on_enter_handlers[])(void*) = {
    Cantools_scenes_main_menu_scene_on_enter, // 0
    Cantools_scenes_dbc_file_list_scene_on_enter, // 1
    Cantools_scenes_dbc_file_name_scene_on_enter, // 2
    Cantools_scenes_vehicle_info_scene_on_enter, // 3
    Cantools_scenes_dbc_maker_menu_scene_on_enter, // 4
    Cantools_scenes_signal_name_scene_on_enter, // 5
    Cantools_scenes_can_id_scene_on_enter, // 6
    Cantools_scenes_start_bit_scene_on_enter, // 7
    Cantools_scenes_bit_length_scene_on_enter, // 8
    Cantools_scenes_endianness_scene_on_enter, // 9
    Cantools_scenes_signed_scene_on_enter, // 10
    Cantools_scenes_offset_scene_on_enter, // 11
    Cantools_scenes_scalar_scene_on_enter, // 12
    Cantools_scenes_unit_scene_on_enter, // 13
    Cantools_scenes_min_scene_on_enter, // 14
    Cantools_scenes_max_scene_on_enter, // 15
    Cantools_scenes_calculate_scene_on_enter, // 16
    Cantools_scenes_view_dbc_list_scene_on_enter, // 17
    Cantools_scenes_signal_actions_scene_on_enter, // 18
    Cantools_scenes_view_dbc_detail_scene_on_enter, // 19
    Cantools_scenes_decode_data_scene_on_enter, // 20
    Cantools_scenes_decode_manual_can_id_scene_on_enter, // 21
    Cantools_scenes_decode_manual_dlc_scene_on_enter, // 22
    Cantools_scenes_decode_manual_data_scene_on_enter, // 23
    Cantools_scenes_decode_result_scene_on_enter, // 24
    Cantools_scenes_decode_log_scene_on_enter, // 25
};

bool (*const Cantools_scenes_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    Cantools_scenes_main_menu_scene_on_event,
    Cantools_scenes_dbc_file_list_scene_on_event,
    Cantools_scenes_dbc_file_name_scene_on_event,
    Cantools_scenes_vehicle_info_scene_on_event,
    Cantools_scenes_dbc_maker_menu_scene_on_event,
    Cantools_scenes_signal_name_scene_on_event,
    Cantools_scenes_can_id_scene_on_event,
    Cantools_scenes_start_bit_scene_on_event,
    Cantools_scenes_bit_length_scene_on_event,
    Cantools_scenes_endianness_scene_on_event,
    Cantools_scenes_signed_scene_on_event,
    Cantools_scenes_offset_scene_on_event,
    Cantools_scenes_scalar_scene_on_event,
    Cantools_scenes_unit_scene_on_event,
    Cantools_scenes_min_scene_on_event,
    Cantools_scenes_max_scene_on_event,
    Cantools_scenes_calculate_scene_on_event,
    Cantools_scenes_view_dbc_list_scene_on_event,
    Cantools_scenes_signal_actions_scene_on_event,
    Cantools_scenes_view_dbc_detail_scene_on_event,
    Cantools_scenes_decode_data_scene_on_event,
    Cantools_scenes_decode_manual_can_id_scene_on_event,
    Cantools_scenes_decode_manual_dlc_scene_on_event,
    Cantools_scenes_decode_manual_data_scene_on_event,
    Cantools_scenes_decode_result_scene_on_event,
    Cantools_scenes_decode_log_scene_on_event,
};

void (*const Cantools_scenes_scene_on_exit_handlers[])(void*) = {
    Cantools_scenes_main_menu_scene_on_exit,
    Cantools_scenes_dbc_file_list_scene_on_exit,
    Cantools_scenes_dbc_file_name_scene_on_exit,
    Cantools_scenes_vehicle_info_scene_on_exit,
    Cantools_scenes_dbc_maker_menu_scene_on_exit,
    Cantools_scenes_signal_name_scene_on_exit,
    Cantools_scenes_can_id_scene_on_exit,
    Cantools_scenes_start_bit_scene_on_exit,
    Cantools_scenes_bit_length_scene_on_exit,
    Cantools_scenes_endianness_scene_on_exit,
    Cantools_scenes_signed_scene_on_exit,
    Cantools_scenes_offset_scene_on_exit,
    Cantools_scenes_scalar_scene_on_exit,
    Cantools_scenes_unit_scene_on_exit,
    Cantools_scenes_min_scene_on_exit,
    Cantools_scenes_max_scene_on_exit,
    Cantools_scenes_calculate_scene_on_exit,
    Cantools_scenes_view_dbc_list_scene_on_exit,
    Cantools_scenes_signal_actions_scene_on_exit,
    Cantools_scenes_view_dbc_detail_scene_on_exit,
    Cantools_scenes_decode_data_scene_on_exit,
    Cantools_scenes_decode_manual_can_id_scene_on_exit,
    Cantools_scenes_decode_manual_dlc_scene_on_exit,
    Cantools_scenes_decode_manual_data_scene_on_exit,
    Cantools_scenes_decode_result_scene_on_exit,
    Cantools_scenes_decode_log_scene_on_exit,
};

static const SceneManagerHandlers Cantools_scenes_scene_manager_handlers = {
    .on_enter_handlers = Cantools_scenes_scene_on_enter_handlers,
    .on_event_handlers = Cantools_scenes_scene_on_event_handlers,
    .on_exit_handlers = Cantools_scenes_scene_on_exit_handlers,
    .scene_num = CantoolsScenesSceneCount,
};

static bool cantools_is_dbc_input_scene(CantoolsScenesScene scene) {
    return (scene >= CantoolsScenesSignalNameScene && scene <= CantoolsScenesCalculateScene);
}

static bool Cantools_scene_custom_callback(void* context, uint32_t custom_event) {
    furi_assert(context);
    App* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, custom_event);
}

// Back button handler - so we don't have to keep pressing back through all DBC input scenes
bool Cantools_scene_back_event_callback(void* context) {
    furi_assert(context);
    App* app = context;
    CantoolsScenesScene current =
        (CantoolsScenesScene)scene_manager_get_current_scene(app->scene_manager);
    if(cantools_is_dbc_input_scene(current)) {
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, CantoolsScenesDbcMakerMenuScene);
        return true;
    }
    return scene_manager_handle_back_event(app->scene_manager);
}

// App alloc / free / init

static App* app_alloc() {
    App* app = malloc(sizeof(App));
    app->buffer_size = TEXT_INPUT_SIZE;
    app->buffer = malloc(app->buffer_size);

    app->scene_manager = scene_manager_alloc(&Cantools_scenes_scene_manager_handlers, app);
    app->view_dispatcher = view_dispatcher_alloc();

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, Cantools_scene_custom_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, Cantools_scene_back_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CantoolsScenesSubmenuView, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CantoolsScenesWidgetView, widget_get_view(app->widget));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CantoolsScenesTextInputView, text_input_get_view(app->text_input));

    app->byte_input = byte_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, CantoolsScenesByteInputView, byte_input_get_view(app->byte_input));

    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->sort_mode = CantoolsSortByName;
    app->editing = false;
    app->active_dbc_index = -1;
    app->dbc_file_count = 0;
    app->dbc_header = furi_string_alloc();

    return app;
}

static void app_free(App* app) {
    furi_assert(app);
    view_dispatcher_remove_view(app->view_dispatcher, CantoolsScenesSubmenuView);
    view_dispatcher_remove_view(app->view_dispatcher, CantoolsScenesWidgetView);
    view_dispatcher_remove_view(app->view_dispatcher, CantoolsScenesTextInputView);
    view_dispatcher_remove_view(app->view_dispatcher, CantoolsScenesByteInputView);
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);
    submenu_free(app->submenu);
    widget_free(app->widget);
    text_input_free(app->text_input);
    byte_input_free(app->byte_input);
    if(app->dbc_header) {
        furi_string_free(app->dbc_header);
    }
    furi_record_close(RECORD_DIALOGS);
    free(app->buffer);
    free(app);
}

static void app_initialize(App* app) {
    app_set_defaults(app);

    dbc_clear_signals(app);
    app->selected_signal_index = -1;
    app->editing = false;
    app->dbc_file_name[0] = '\0';
    app->vehicle_info[0] = '\0';
    strlcpy(app->decode_can_id_text, "0x", sizeof(app->decode_can_id_text));
    strlcpy(app->decode_dlc_text, "8", sizeof(app->decode_dlc_text));
    memset(app->decode_data_bytes, 0, sizeof(app->decode_data_bytes));
    app->decode_can_id = 0;
    app->decode_dlc = 0;
    app->decode_data_len = 0;

    calculate_values(app);

    dbc_init_folder();
    dbc_scan_files(app);
    if(app->dbc_file_count > 0) {
        dbc_set_active_file(app, 0);
    } else {
        app->active_dbc_index = -1;
        dbc_set_default_header(app);
    }
}

int32_t cantools_app(void* p) {
    UNUSED(p);
    App* app = app_alloc();
    app_initialize(app);

    Gui* gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(app->view_dispatcher, gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(app->scene_manager, CantoolsScenesMainMenuScene);
    view_dispatcher_run(app->view_dispatcher);

    app_free(app);
    return 0;
}
