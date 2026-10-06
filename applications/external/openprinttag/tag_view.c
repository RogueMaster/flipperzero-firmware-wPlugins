#include "tag_view.h"
#include "openprinttag_i.h"
#include "material_types.h"

#include <furi.h>
#include <gui/canvas.h>
#include <gui/elements.h>
#include <input/input.h>
#include <stdarg.h>

typedef enum {
    TagViewPageSummary,
    TagViewPageDetails,
    TagViewPageIdentification,
    TagViewPageCount,
} TagViewPage;

// The two scrolling pages are lists of label/value rows
#define TAG_VIEW_LISTS        (2U)
#define TAG_VIEW_MAX_ROWS     (16U)
#define TAG_VIEW_VISIBLE_ROWS (4U)

#define TAG_VIEW_ROW_TOP    (14U)
#define TAG_VIEW_ROW_HEIGHT (12U)

typedef struct {
    char label[14];
    char value[64];
} TagViewRow;

typedef struct {
    // Summary page
    char title[64];
    char brand[32];
    char chip[8]; // Material type, shown in a box
    bool has_weight;
    uint32_t remaining; // Grams left
    uint32_t total; // Grams of a full spool
    uint8_t percent; // Remaining, 0-100
    char nozzle[24]; // Temperature ranges for the footer
    char bed[24];

    // Details and identification pages
    TagViewRow rows[TAG_VIEW_LISTS][TAG_VIEW_MAX_ROWS];
    uint8_t row_count[TAG_VIEW_LISTS];

    uint8_t page;
    uint8_t scroll[TAG_VIEW_LISTS];
} TagViewModel;

struct TagView {
    View* view;
};

// ---- Text helpers -----------------------------------------------------------------------------

// printf with floats is not relied on, the decimals are printed from integers
static void format_fixed(char* out, size_t size, float value, uint8_t decimals) {
    const uint32_t scale = decimals == 1 ? 10 : 100;
    const uint32_t scaled = (uint32_t)(value * (float)scale + 0.5f);

    if(decimals == 1) {
        snprintf(out, size, "%lu.%lu", scaled / scale, scaled % scale);
    } else {
        snprintf(out, size, "%lu.%02lu", scaled / scale, scaled % scale);
    }
}

// "220-240", "220+" or "220", empty if there is no temperature
static void format_range(char* out, size_t size, int32_t min, int32_t max) {
    if(min > 0 && max > 0 && min != max) {
        snprintf(out, size, "%ld-%ld", min, max);
    } else if(max > 0 && min <= 0) {
        snprintf(out, size, "%ld", max);
    } else if(min > 0 && max <= 0) {
        snprintf(out, size, "%ld+", min);
    } else if(min > 0) {
        snprintf(out, size, "%ld", min);
    } else {
        out[0] = '\0';
    }
}

// YYYY-MM-DD from a UNIX timestamp (civil-from-days algorithm)
static void format_date(char* out, size_t size, uint64_t timestamp) {
    int64_t days = (int64_t)(timestamp / 86400U) + 719468;
    const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const uint32_t day_of_era = (uint32_t)(days - era * 146097);
    const uint32_t year_of_era =
        (day_of_era - day_of_era / 1460 + day_of_era / 36524 - day_of_era / 146096) / 365;
    int64_t year = (int64_t)year_of_era + era * 400;
    const uint32_t day_of_year =
        day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    const uint32_t mp = (5 * day_of_year + 2) / 153;
    const uint32_t day = day_of_year - (153 * mp + 2) / 5 + 1;
    const uint32_t month = mp < 10 ? mp + 3 : mp - 9;
    if(month <= 2) year++;

    snprintf(out, size, "%04ld-%02lu-%02lu", (long)year, day, month);
}

static void add_row(TagViewModel* model, uint8_t list, const char* label, const char* format, ...)
    __attribute__((format(printf, 4, 5)));

static void
    add_row(TagViewModel* model, uint8_t list, const char* label, const char* format, ...) {
    uint8_t* count = &model->row_count[list];
    if(*count >= TAG_VIEW_MAX_ROWS) return;

    TagViewRow* row = &model->rows[list][*count];
    snprintf(row->label, sizeof(row->label), "%.13s", label);

    va_list args;
    va_start(args, format);
    vsnprintf(row->value, sizeof(row->value), format, args);
    va_end(args);

    (*count)++;
}

// ---- Building the model from the tag data -----------------------------------------------------

static void build_model(TagViewModel* model, const OpenPrintTagData* data, const uint8_t* uid) {
    const OpenPrintTagMain* main = &data->main;
    char text[24];

    // Material type: the enum is the normal form, a text is accepted too
    const char* type_abbreviation = NULL;
    const char* type_name = NULL;
    if(main->has_material_type_enum) {
        type_abbreviation = material_type_get_abbr(main->material_type_enum);
        type_name = material_type_get_name(main->material_type_enum);
        if(strcmp(type_abbreviation, "Unknown") == 0) {
            type_abbreviation = NULL;
            type_name = NULL;
        }
    } else if(!furi_string_empty(main->material_type_str)) {
        type_abbreviation = furi_string_get_cstr(main->material_type_str);
        type_name = type_abbreviation;
    }

    // Header
    if(!furi_string_empty(main->material_name)) {
        snprintf(
            model->title, sizeof(model->title), "%.63s", furi_string_get_cstr(main->material_name));
    } else if(type_name) {
        snprintf(model->title, sizeof(model->title), "%.63s", type_name);
    } else {
        snprintf(model->title, sizeof(model->title), "OpenPrintTag");
    }
    snprintf(model->brand, sizeof(model->brand), "%.31s", furi_string_get_cstr(main->brand_name));

    if(!furi_string_empty(main->material_abbreviation)) {
        snprintf(
            model->chip,
            sizeof(model->chip),
            "%.7s",
            furi_string_get_cstr(main->material_abbreviation));
    } else if(type_abbreviation) {
        snprintf(model->chip, sizeof(model->chip), "%.7s", type_abbreviation);
    }

    // Weight left
    model->total = main->actual_netto_full_weight ? main->actual_netto_full_weight :
                                                    main->nominal_netto_full_weight;
    const uint32_t consumed = data->aux.consumed_weight;
    model->has_weight = model->total > 0;
    model->remaining = model->total > consumed ? model->total - consumed : 0;
    model->percent =
        model->total ?
            (uint8_t)(((uint64_t)model->remaining * 100 + model->total / 2) / model->total) :
            0;

    format_range(
        model->nozzle,
        sizeof(model->nozzle),
        main->min_print_temperature,
        main->max_print_temperature);
    format_range(
        model->bed, sizeof(model->bed), main->min_bed_temperature, main->max_bed_temperature);

    // Details page
    const uint8_t details = 0;
    if(model->nozzle[0]) add_row(model, details, "Nozzle", "%s C", model->nozzle);
    if(model->bed[0]) add_row(model, details, "Bed", "%s C", model->bed);

    char chamber[24];
    if(main->min_chamber_temperature > 0 || main->max_chamber_temperature > 0) {
        format_range(
            chamber, sizeof(chamber), main->min_chamber_temperature, main->max_chamber_temperature);
        add_row(model, details, "Chamber", "%s C", chamber);
    } else if(main->chamber_temperature > 0) {
        add_row(model, details, "Chamber", "%ld C", main->chamber_temperature);
    }
    if(main->preheat_temperature > 0) {
        add_row(model, details, "Preheat", "%ld C", main->preheat_temperature);
    }
    if(main->nominal_netto_full_weight) {
        add_row(model, details, "Nominal", "%lu g", main->nominal_netto_full_weight);
    }
    if(main->actual_netto_full_weight) {
        add_row(model, details, "Actual", "%lu g", main->actual_netto_full_weight);
    }
    if(data->aux.has_data) {
        add_row(model, details, "Consumed", "%lu g", consumed);
    }
    if(main->empty_container_weight) {
        add_row(model, details, "Spool", "%lu g", main->empty_container_weight);
    }
    if(main->nominal_full_length > 0) {
        format_fixed(text, sizeof(text), main->nominal_full_length / 1000.0f, 1);
        add_row(model, details, "Nom. length", "%s m", text);
    }
    if(main->actual_full_length > 0) {
        format_fixed(text, sizeof(text), main->actual_full_length / 1000.0f, 1);
        add_row(model, details, "Act. length", "%s m", text);
    }
    if(main->filament_diameter > 0) {
        format_fixed(text, sizeof(text), main->filament_diameter, 2);
        add_row(model, details, "Diameter", "%s mm", text);
    }
    if(main->density > 0) {
        format_fixed(text, sizeof(text), main->density, 2);
        add_row(model, details, "Density", "%s g/cm3", text);
    }
    if(main->drying_temperature > 0) {
        if(main->drying_time >= 60 && main->drying_time % 60 == 0) {
            add_row(
                model,
                details,
                "Drying",
                "%ld C, %lu h",
                main->drying_temperature,
                main->drying_time / 60);
        } else if(main->drying_time > 0) {
            add_row(
                model,
                details,
                "Drying",
                "%ld C, %lu min",
                main->drying_temperature,
                main->drying_time);
        } else {
            add_row(model, details, "Drying", "%ld C", main->drying_temperature);
        }
    }
    if(main->has_color) {
        add_row(
            model,
            details,
            "Color",
            "#%02X%02X%02X",
            main->color[0],
            main->color[1],
            main->color[2]);
    }
    if(model->row_count[details] == 0) {
        add_row(model, details, "No data", "-");
    }

    // Identification page
    const uint8_t identification = 1;
    if(model->brand[0]) add_row(model, identification, "Brand", "%s", model->brand);
    if(!furi_string_empty(main->material_name)) {
        add_row(
            model, identification, "Material", "%s", furi_string_get_cstr(main->material_name));
    }
    if(type_abbreviation) add_row(model, identification, "Type", "%s", type_abbreviation);
    add_row(model, identification, "Class", "%s", material_class_get_name(main->material_class));
    if(main->gtin) {
        add_row(model, identification, "GTIN", "%llu", main->gtin);
    }
    if(main->brand_specific_instance_id[0]) {
        add_row(model, identification, "Instance", "%s", main->brand_specific_instance_id);
    }
    if(main->has_instance_uuid) {
        const uint8_t* u = main->instance_uuid;
        add_row(
            model,
            identification,
            "UUID",
            "%02X%02X%02X%02X..%02X%02X",
            u[0],
            u[1],
            u[2],
            u[3],
            u[14],
            u[15]);
    }
    if(main->manufactured_date) {
        format_date(text, sizeof(text), main->manufactured_date);
        add_row(model, identification, "Made", "%s", text);
    }
    if(main->expiration_date) {
        format_date(text, sizeof(text), main->expiration_date);
        add_row(model, identification, "Expires", "%s", text);
    }
    if(uid) {
        add_row(
            model,
            identification,
            "UID",
            "%02X%02X%02X%02X%02X%02X%02X%02X",
            uid[0],
            uid[1],
            uid[2],
            uid[3],
            uid[4],
            uid[5],
            uid[6],
            uid[7]);
    }
}

void openprinttag_tag_view_set_data(
    TagView* tag_view,
    const OpenPrintTagData* data,
    const uint8_t* uid) {
    furi_check(tag_view);
    furi_check(data);

    with_view_model(
        tag_view->view,
        TagViewModel * model,
        {
            memset(model, 0, sizeof(TagViewModel));
            build_model(model, data, uid);
        },
        true);
}

// ---- Drawing ----------------------------------------------------------------------------------

// Draws the text with the current font, cut with ".." if it is wider than max_width
static void draw_fitted(
    Canvas* canvas,
    int32_t x,
    int32_t y,
    const char* text,
    uint16_t max_width,
    bool right_aligned) {
    char shown[72];
    snprintf(shown, sizeof(shown), "%.69s", text);

    if(canvas_string_width(canvas, shown) > max_width) {
        size_t length = strlen(shown);
        while(length > 0) {
            snprintf(shown, sizeof(shown), "%.*s..", (int)length, text);
            if(canvas_string_width(canvas, shown) <= max_width) break;
            length--;
        }
    }

    const int32_t draw_x = right_aligned ? x - canvas_string_width(canvas, shown) : x;
    canvas_draw_str(canvas, draw_x, y, shown);
}

// Filled square for the page that is shown, a dot for the others
static void draw_page_dots(Canvas* canvas, uint8_t page, int32_t center_x, int32_t y) {
    for(uint8_t i = 0; i < TagViewPageCount; i++) {
        const int32_t x = center_x + ((int32_t)i - 1) * 5;
        if(i == page) {
            canvas_draw_box(canvas, x - 1, y - 1, 3, 3);
        } else {
            canvas_draw_dot(canvas, x, y);
        }
    }
}

// Small funnel: the nozzle
static void draw_nozzle_icon(Canvas* canvas, int32_t x, int32_t y) {
    canvas_draw_box(canvas, x, y, 7, 3);
    canvas_draw_line(canvas, x + 1, y + 3, x + 3, y + 6);
    canvas_draw_line(canvas, x + 5, y + 3, x + 3, y + 6);
}

// Wavy heat lines over a plate: the bed
static void draw_bed_icon(Canvas* canvas, int32_t x, int32_t y) {
    canvas_draw_line(canvas, x + 1, y, x + 1, y + 2);
    canvas_draw_line(canvas, x + 3, y + 1, x + 3, y + 3);
    canvas_draw_line(canvas, x + 5, y, x + 5, y + 2);
    canvas_draw_box(canvas, x, y + 5, 7, 2);
}

static void draw_summary(Canvas* canvas, const TagViewModel* model) {
    // Title and brand
    canvas_set_font(canvas, FontPrimary);
    draw_fitted(canvas, 2, 10, model->title, 124, false);

    canvas_set_font(canvas, FontSecondary);
    uint16_t brand_width = 124;
    if(model->chip[0]) {
        // The material type sits in a box at the right end of the brand line
        const uint16_t chip_width = canvas_string_width(canvas, model->chip) + 6;
        const int32_t chip_x = 126 - chip_width;
        canvas_draw_rbox(canvas, chip_x, 13, chip_width, 10, 2);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, chip_x + 3, 21, model->chip);
        canvas_set_color(canvas, ColorBlack);
        brand_width = chip_x - 6;
    }
    draw_fitted(canvas, 2, 21, model->brand, brand_width, false);

    // Weight left, the number is big, with a bar below
    if(model->has_weight) {
        char text[24];

        snprintf(text, sizeof(text), "%lu", model->remaining);
        canvas_set_font(canvas, FontBigNumbers);
        canvas_draw_str(canvas, 2, 42, text);
        const uint16_t number_width = canvas_string_width(canvas, text);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 2 + number_width + 3, 42, "g");

        canvas_set_font(canvas, FontSecondary);
        snprintf(text, sizeof(text), "of %lu g", model->total);
        draw_fitted(canvas, 126, 31, text, 60, true);
        canvas_set_font(canvas, FontPrimary);
        snprintf(text, sizeof(text), "%u%%", model->percent);
        draw_fitted(canvas, 126, 42, text, 40, true);

        canvas_draw_rframe(canvas, 2, 46, 124, 7, 2);
        const uint16_t fill = (uint16_t)(120U * model->percent / 100U);
        if(fill > 0) canvas_draw_box(canvas, 4, 48, fill, 3);
    } else {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 38, AlignCenter, AlignCenter, "No weight data");
    }

    // Footer: nozzle and bed ranges, page dots in the middle
    canvas_set_font(canvas, FontSecondary);
    if(model->nozzle[0]) {
        draw_nozzle_icon(canvas, 2, 55);
        draw_fitted(canvas, 12, 62, model->nozzle, 44, false);
    }
    if(model->bed[0]) {
        draw_bed_icon(canvas, 74, 55);
        draw_fitted(canvas, 84, 62, model->bed, 42, false);
    }
    draw_page_dots(canvas, TagViewPageSummary, 64, 60);
}

static void draw_list(Canvas* canvas, const TagViewModel* model, uint8_t page) {
    const uint8_t list = page - 1;
    const uint8_t count = model->row_count[list];
    const uint8_t scroll = model->scroll[list];
    const bool scrollable = count > TAG_VIEW_VISIBLE_ROWS;
    const int32_t right = scrollable ? 121 : 126;

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 9, page == TagViewPageDetails ? "Details" : "Identification");
    draw_page_dots(canvas, page, 112, 5);
    canvas_draw_line(canvas, 0, 12, 127, 12);

    for(uint8_t i = 0; i < TAG_VIEW_VISIBLE_ROWS && scroll + i < count; i++) {
        const TagViewRow* row = &model->rows[list][scroll + i];
        const int32_t top = TAG_VIEW_ROW_TOP + i * TAG_VIEW_ROW_HEIGHT;
        const int32_t baseline = top + 9;

        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, baseline, row->label);
        const uint16_t label_width = canvas_string_width(canvas, row->label);

        // The bold font first, the smaller one when the value does not fit, then it is cut
        const uint16_t value_width = right - 2 - label_width - 4;
        canvas_set_font(canvas, FontPrimary);
        if(canvas_string_width(canvas, row->value) > value_width) {
            canvas_set_font(canvas, FontSecondary);
        }
        draw_fitted(canvas, right, baseline, row->value, value_width, true);

        // Dotted line between the rows
        for(int32_t x = 2; x <= right; x += 3) {
            canvas_draw_dot(canvas, x, top + 11);
        }
    }

    if(scrollable) {
        elements_scrollbar_pos(canvas, 127, TAG_VIEW_ROW_TOP, 48, scroll, count);
    }
}

static void tag_view_draw_callback(Canvas* canvas, void* _model) {
    const TagViewModel* model = _model;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(model->page == TagViewPageSummary) {
        draw_summary(canvas, model);
    } else {
        draw_list(canvas, model, model->page);
    }
}

// ---- Input ------------------------------------------------------------------------------------

static bool tag_view_input_callback(InputEvent* event, void* context) {
    TagView* tag_view = context;

    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    bool consumed = true;

    switch(event->key) {
    case InputKeyLeft:
    case InputKeyRight:
        if(event->type != InputTypeShort) break;
        with_view_model(
            tag_view->view,
            TagViewModel * model,
            {
                model->page = event->key == InputKeyRight ?
                                  (model->page + 1) % TagViewPageCount :
                                  (model->page + TagViewPageCount - 1) % TagViewPageCount;
            },
            true);
        break;

    case InputKeyUp:
    case InputKeyDown:
        with_view_model(
            tag_view->view,
            TagViewModel * model,
            {
                if(model->page != TagViewPageSummary) {
                    const uint8_t list = model->page - 1;
                    const uint8_t count = model->row_count[list];
                    uint8_t* scroll = &model->scroll[list];
                    if(event->key == InputKeyUp && *scroll > 0) {
                        (*scroll)--;
                    } else if(event->key == InputKeyDown && *scroll + TAG_VIEW_VISIBLE_ROWS < count) {
                        (*scroll)++;
                    }
                }
            },
            true);
        break;

    default:
        // BACK and OK are left to the scene
        consumed = false;
        break;
    }

    return consumed;
}

// ---- Lifecycle --------------------------------------------------------------------------------

TagView* tag_view_alloc(void) {
    TagView* tag_view = malloc(sizeof(TagView));

    tag_view->view = view_alloc();
    view_set_context(tag_view->view, tag_view);
    view_allocate_model(tag_view->view, ViewModelTypeLocking, sizeof(TagViewModel));
    view_set_draw_callback(tag_view->view, tag_view_draw_callback);
    view_set_input_callback(tag_view->view, tag_view_input_callback);

    with_view_model(
        tag_view->view, TagViewModel * model, { memset(model, 0, sizeof(TagViewModel)); }, false);

    return tag_view;
}

void tag_view_free(TagView* tag_view) {
    furi_check(tag_view);
    view_free(tag_view->view);
    free(tag_view);
}

View* tag_view_get_view(TagView* tag_view) {
    furi_check(tag_view);
    return tag_view->view;
}
