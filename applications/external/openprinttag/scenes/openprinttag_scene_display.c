#include "../openprinttag_i.h"
#include "../material_types.h"

void openprinttag_scene_display_on_enter(void* context) {
    OpenPrintTag* app = context;
    Widget* widget = app->widget;

    FuriString* temp_str = furi_string_alloc();

    // Display tag information
    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(temp_str));

    // Build display string
    furi_string_cat_printf(temp_str, "OpenPrintTag Data\n\n");

    if(app->tag_data.main.has_data) {
        // Brand and material identification
        if(!furi_string_empty(app->tag_data.main.brand_name)) {
            furi_string_cat_printf(
                temp_str, "Brand: %s\n", furi_string_get_cstr(app->tag_data.main.brand_name));
        }

        if(!furi_string_empty(app->tag_data.main.material_name)) {
            furi_string_cat_printf(
                temp_str,
                "Material: %s\n",
                furi_string_get_cstr(app->tag_data.main.material_name));
        }

        // Material class (FFF/SLA)
        const char* class_abbr = material_class_get_abbr(app->tag_data.main.material_class);
        furi_string_cat_printf(temp_str, "Class: %s\n", class_abbr);

        // Material type (PLA, PETG, etc.)
        if(app->tag_data.main.has_material_type_enum) {
            const char* type_abbr = material_type_get_abbr(app->tag_data.main.material_type_enum);
            furi_string_cat_printf(temp_str, "Type: %s\n", type_abbr);
        } else if(!furi_string_empty(app->tag_data.main.material_type_str)) {
            furi_string_cat_printf(
                temp_str,
                "Type: %s\n",
                furi_string_get_cstr(app->tag_data.main.material_type_str));
        }

        if(app->tag_data.main.gtin > 0) {
            furi_string_cat_printf(temp_str, "GTIN: %llu\n", app->tag_data.main.gtin);
        }

        // Weights
        if(app->tag_data.main.actual_netto_full_weight > 0) {
            furi_string_cat_printf(
                temp_str, "Weight: %lu g\n", app->tag_data.main.actual_netto_full_weight);
        }

        // FFF-specific
        if(app->tag_data.main.filament_diameter > 0) {
            furi_string_cat_printf(
                temp_str, "Diameter: %.2f mm\n", (double)app->tag_data.main.filament_diameter);
        }

        if(app->tag_data.main.actual_full_length > 0) {
            furi_string_cat_printf(
                temp_str,
                "Length: %.1f m\n",
                (double)(app->tag_data.main.actual_full_length / 1000.0f));
        }

        // Print temperatures
        if(app->tag_data.main.min_print_temperature > 0) {
            if(app->tag_data.main.max_print_temperature > 0) {
                furi_string_cat_printf(
                    temp_str,
                    "Nozzle: %ld-%ld°C\n",
                    app->tag_data.main.min_print_temperature,
                    app->tag_data.main.max_print_temperature);
            } else {
                furi_string_cat_printf(
                    temp_str, "Nozzle: %ld°C+\n", app->tag_data.main.min_print_temperature);
            }
        }

        if(app->tag_data.main.min_bed_temperature > 0) {
            if(app->tag_data.main.max_bed_temperature > 0) {
                furi_string_cat_printf(
                    temp_str,
                    "Bed: %ld-%ld°C\n",
                    app->tag_data.main.min_bed_temperature,
                    app->tag_data.main.max_bed_temperature);
            } else {
                furi_string_cat_printf(
                    temp_str, "Bed: %ld°C+\n", app->tag_data.main.min_bed_temperature);
            }
        }
    } else {
        furi_string_cat_printf(temp_str, "No main data found\n");
    }

    if(app->tag_data.aux.has_data) {
        furi_string_cat_printf(temp_str, "\n--- Usage Data ---\n");

        if(app->tag_data.aux.consumed_weight > 0) {
            furi_string_cat_printf(
                temp_str, "Consumed: %lu g\n", app->tag_data.aux.consumed_weight);

            // Calculate remaining if we have full weight
            if(app->tag_data.main.actual_netto_full_weight > 0) {
                uint32_t remaining = app->tag_data.main.actual_netto_full_weight -
                                     app->tag_data.aux.consumed_weight;
                furi_string_cat_printf(temp_str, "Remaining: %lu g\n", remaining);
            }
        }

        if(!furi_string_empty(app->tag_data.aux.workgroup)) {
            furi_string_cat_printf(
                temp_str, "Workgroup: %s\n", furi_string_get_cstr(app->tag_data.aux.workgroup));
        }

        if(app->tag_data.aux.last_stir_time > 0) {
            furi_string_cat_printf(
                temp_str, "Last stir: %llu\n", app->tag_data.aux.last_stir_time);
        }
    }

    // Update widget with the built string
    widget_reset(widget);
    widget_add_text_scroll_element(widget, 0, 0, 128, 64, furi_string_get_cstr(temp_str));

    furi_string_free(temp_str);

    view_dispatcher_switch_to_view(app->view_dispatcher, OpenPrintTagViewWidget);
}

bool openprinttag_scene_display_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void openprinttag_scene_display_on_exit(void* context) {
    OpenPrintTag* app = context;
    widget_reset(app->widget);
}
