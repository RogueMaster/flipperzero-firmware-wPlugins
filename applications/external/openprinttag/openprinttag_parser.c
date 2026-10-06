#include "openprinttag_i.h"
#include "openprinttag_fields.h"
#include "cbor_parser.h"
#include <furi.h>

// Numeric fields may be stored as integers or floats
static bool cbor_value_to_float(const CborValue* value, float* out) {
    if(value->type == CborValueTypeFloat) {
        *out = value->value.f32;
        return true;
    }
    if(value->type == CborValueTypeUnsigned) {
        *out = (float)value->value.u64;
        return true;
    }
    return false;
}

// Whole-number fields that the specification types as "number" can also be stored as floats
static bool cbor_value_to_uint32(const CborValue* value, uint32_t* out) {
    if(value->type == CborValueTypeUnsigned) {
        *out = (uint32_t)value->value.u64;
        return true;
    }
    if(value->type == CborValueTypeFloat && value->value.f32 >= 0) {
        *out = (uint32_t)(value->value.f32 + 0.5f);
        return true;
    }
    return false;
}

// Temperatures are whole numbers, normally stored as unsigned integers
static bool cbor_value_to_int32(const CborValue* value, int32_t* out) {
    uint32_t unsigned_value;
    if(!cbor_value_to_uint32(value, &unsigned_value)) return false;
    *out = (int32_t)unsigned_value;
    return true;
}

static bool parse_meta_section(CborParser* parser, OpenPrintTagMeta* meta) {
    size_t count;
    if(!cbor_parse_map(parser, &count)) return false;

    meta->main_region_offset = 0;
    meta->main_region_size = 0;
    meta->aux_region_offset = 0;
    meta->aux_region_size = 0;

    for(size_t i = 0; i < count; i++) {
        CborValue key, value;

        if(!cbor_parse_value(parser, &key)) return false;
        if(key.type != CborValueTypeUnsigned) {
            if(!cbor_skip_contents(parser, &key) || !cbor_skip_value(parser)) return false;
            continue;
        }

        uint32_t key_id = (uint32_t)key.value.u64;

        if(!cbor_parse_value(parser, &value)) return false;

        switch(key_id) {
        case META_MAIN_REGION_OFFSET:
            if(value.type == CborValueTypeUnsigned) {
                meta->main_region_offset = (uint32_t)value.value.u64;
            }
            break;
        case META_MAIN_REGION_SIZE:
            if(value.type == CborValueTypeUnsigned) {
                meta->main_region_size = (uint32_t)value.value.u64;
            }
            break;
        case META_AUX_REGION_OFFSET:
            if(value.type == CborValueTypeUnsigned) {
                meta->aux_region_offset = (uint32_t)value.value.u64;
            }
            break;
        case META_AUX_REGION_SIZE:
            if(value.type == CborValueTypeUnsigned) {
                meta->aux_region_size = (uint32_t)value.value.u64;
            }
            break;
        default:
            break;
        }

        // Skip the elements of container values that were not read above
        if(!cbor_skip_contents(parser, &value)) return false;
    }

    return true;
}

static bool parse_main_section(CborParser* parser, OpenPrintTagMain* main) {
    size_t count;
    if(!cbor_parse_map(parser, &count)) return false;

    main->has_data = false;

    for(size_t i = 0; i < count; i++) {
        CborValue key, value;

        if(!cbor_parse_value(parser, &key)) return false;
        if(key.type != CborValueTypeUnsigned) {
            if(!cbor_skip_contents(parser, &key) || !cbor_skip_value(parser)) return false;
            continue;
        }

        uint32_t key_id = (uint32_t)key.value.u64;
        if(!cbor_parse_value(parser, &value)) return false;

        switch(key_id) {
        // Product information
        case MAIN_GTIN:
            if(value.type == CborValueTypeUnsigned) {
                main->gtin = value.value.u64;
                main->has_data = true;
            }
            break;
        case MAIN_MATERIAL_CLASS:
            if(value.type == CborValueTypeUnsigned) {
                main->material_class = (uint32_t)value.value.u64;
                main->has_data = true;
            }
            break;
        case MAIN_MATERIAL_TYPE:
            if(value.type == CborValueTypeText) {
                furi_string_set_strn(
                    main->material_type_str,
                    (const char*)value.value.text.data,
                    value.value.text.size);
                main->has_material_type_enum = false;
                main->has_data = true;
            } else if(value.type == CborValueTypeUnsigned) {
                main->material_type_enum = (uint32_t)value.value.u64;
                main->has_material_type_enum = true;
                main->has_data = true;
            }
            break;
        case MAIN_MATERIAL_NAME:
            if(value.type == CborValueTypeText) {
                furi_string_set_strn(
                    main->material_name,
                    (const char*)value.value.text.data,
                    value.value.text.size);
                main->has_data = true;
            }
            break;
        case MAIN_BRAND_NAME:
            if(value.type == CborValueTypeText) {
                furi_string_set_strn(
                    main->brand_name, (const char*)value.value.text.data, value.value.text.size);
                main->has_data = true;
            }
            break;
        case MAIN_MATERIAL_ABBREVIATION:
            if(value.type == CborValueTypeText) {
                furi_string_set_strn(
                    main->material_abbreviation,
                    (const char*)value.value.text.data,
                    value.value.text.size);
            }
            break;

        // Weights
        case MAIN_NOMINAL_NETTO_FULL_WEIGHT:
            cbor_value_to_uint32(&value, &main->nominal_netto_full_weight);
            break;
        case MAIN_ACTUAL_NETTO_FULL_WEIGHT:
            cbor_value_to_uint32(&value, &main->actual_netto_full_weight);
            break;
        case MAIN_EMPTY_CONTAINER_WEIGHT:
            cbor_value_to_uint32(&value, &main->empty_container_weight);
            break;

        // Timestamps
        case MAIN_MANUFACTURED_DATE:
            if(value.type == CborValueTypeUnsigned) {
                main->manufactured_date = value.value.u64;
            }
            break;
        case MAIN_EXPIRATION_DATE:
            if(value.type == CborValueTypeUnsigned) {
                main->expiration_date = value.value.u64;
            }
            break;

        // FFF-specific
        case MAIN_FILAMENT_DIAMETER:
            cbor_value_to_float(&value, &main->filament_diameter);
            break;
        case MAIN_NOMINAL_FULL_LENGTH:
            cbor_value_to_float(&value, &main->nominal_full_length);
            break;
        case MAIN_ACTUAL_FULL_LENGTH:
            cbor_value_to_float(&value, &main->actual_full_length);
            break;
        case MAIN_MIN_PRINT_TEMPERATURE:
            if(value.type == CborValueTypeUnsigned) {
                main->min_print_temperature = (int32_t)value.value.u64;
            }
            break;
        case MAIN_MAX_PRINT_TEMPERATURE:
            if(value.type == CborValueTypeUnsigned) {
                main->max_print_temperature = (int32_t)value.value.u64;
            }
            break;
        case MAIN_MIN_BED_TEMPERATURE:
            if(value.type == CborValueTypeUnsigned) {
                main->min_bed_temperature = (int32_t)value.value.u64;
            }
            break;
        case MAIN_MAX_BED_TEMPERATURE:
            if(value.type == CborValueTypeUnsigned) {
                main->max_bed_temperature = (int32_t)value.value.u64;
            }
            break;

        case MAIN_PREHEAT_TEMPERATURE:
            cbor_value_to_int32(&value, &main->preheat_temperature);
            break;
        case MAIN_MIN_CHAMBER_TEMPERATURE:
            cbor_value_to_int32(&value, &main->min_chamber_temperature);
            break;
        case MAIN_MAX_CHAMBER_TEMPERATURE:
            cbor_value_to_int32(&value, &main->max_chamber_temperature);
            break;
        case MAIN_CHAMBER_TEMPERATURE:
            cbor_value_to_int32(&value, &main->chamber_temperature);
            break;
        case MAIN_DRYING_TEMPERATURE:
            cbor_value_to_int32(&value, &main->drying_temperature);
            break;
        case MAIN_DRYING_TIME:
            cbor_value_to_uint32(&value, &main->drying_time);
            break;

        // Identification
        case MAIN_PRIMARY_COLOR:
            // 3 bytes (R, G, B) or 4 bytes with an alpha channel, which is not used
            if(value.type == CborValueTypeBytes && value.value.bytes.size >= 3 &&
               value.value.bytes.size <= 4) {
                memcpy(main->color, value.value.bytes.data, sizeof(main->color));
                main->has_color = true;
            }
            break;
        case MAIN_INSTANCE_UUID:
            if(value.type == CborValueTypeBytes &&
               value.value.bytes.size == sizeof(main->instance_uuid)) {
                memcpy(main->instance_uuid, value.value.bytes.data, sizeof(main->instance_uuid));
                main->has_instance_uuid = true;
            }
            break;
        case MAIN_BRAND_SPECIFIC_INSTANCE_ID:
            if(value.type == CborValueTypeText) {
                size_t length = value.value.text.size;
                if(length > sizeof(main->brand_specific_instance_id) - 1) {
                    length = sizeof(main->brand_specific_instance_id) - 1;
                }
                memcpy(main->brand_specific_instance_id, value.value.text.data, length);
                main->brand_specific_instance_id[length] = '\0';
            }
            break;

        // Material properties
        case MAIN_DENSITY:
            cbor_value_to_float(&value, &main->density);
            break;
        case MAIN_TAGS:
            // Tags is an array - not stored yet, its elements are skipped below
            break;

        default:
            // Skip unknown fields
            break;
        }

        // Skip the elements of container values that were not read above
        if(!cbor_skip_contents(parser, &value)) return false;
    }

    return main->has_data;
}

static bool parse_aux_section(CborParser* parser, OpenPrintTagAux* aux) {
    size_t count;
    if(!cbor_parse_map(parser, &count)) return false;

    aux->has_data = false;
    aux->consumed_weight = 0;
    aux->last_stir_time = 0;

    for(size_t i = 0; i < count; i++) {
        CborValue key, value;

        if(!cbor_parse_value(parser, &key)) return false;
        if(key.type != CborValueTypeUnsigned) {
            if(!cbor_skip_contents(parser, &key) || !cbor_skip_value(parser)) return false;
            continue;
        }

        uint32_t key_id = (uint32_t)key.value.u64;

        if(!cbor_parse_value(parser, &value)) return false;

        switch(key_id) {
        case AUX_CONSUMED_WEIGHT:
            if(cbor_value_to_uint32(&value, &aux->consumed_weight)) {
                aux->has_data = true;
            }
            break;
        case AUX_WORKGROUP:
            if(value.type == CborValueTypeText) {
                furi_string_set_strn(
                    aux->workgroup, (const char*)value.value.text.data, value.value.text.size);
                aux->has_data = true;
            }
            break;
        case AUX_LAST_STIR_TIME:
            if(value.type == CborValueTypeUnsigned) {
                aux->last_stir_time = value.value.u64;
                aux->has_data = true;
            }
            break;
        default:
            // Skip vendor-specific or unknown fields
            break;
        }

        // Skip the elements of container values that were not read above
        if(!cbor_skip_contents(parser, &value)) return false;
    }

    return true;
}

// Forget everything of the tag that was parsed before: fields the new tag does not have must not
// show the old tag's values
static void reset_parsed_values(OpenPrintTagData* data) {
    OpenPrintTagMain* main = &data->main;
    furi_string_reset(main->brand_name);
    furi_string_reset(main->material_name);
    furi_string_reset(main->material_type_str);
    furi_string_reset(main->material_abbreviation);
    furi_string_reset(data->aux.workgroup);

    main->material_type_enum = 0;
    main->material_class = 0;
    main->gtin = 0;
    main->nominal_netto_full_weight = 0;
    main->actual_netto_full_weight = 0;
    main->empty_container_weight = 0;
    main->manufactured_date = 0;
    main->expiration_date = 0;
    main->filament_diameter = 0;
    main->nominal_full_length = 0;
    main->actual_full_length = 0;
    main->min_print_temperature = 0;
    main->max_print_temperature = 0;
    main->min_bed_temperature = 0;
    main->max_bed_temperature = 0;
    main->density = 0;
    main->preheat_temperature = 0;
    main->min_chamber_temperature = 0;
    main->max_chamber_temperature = 0;
    main->chamber_temperature = 0;
    main->drying_temperature = 0;
    main->drying_time = 0;
    main->has_color = false;
    main->has_instance_uuid = false;
    main->brand_specific_instance_id[0] = '\0';
    main->has_data = false;
    main->has_material_type_enum = false;

    data->aux.consumed_weight = 0;
    data->aux.last_stir_time = 0;
    data->aux.has_data = false;
}

bool openprinttag_parse_cbor(OpenPrintTag* app, const uint8_t* payload, size_t size) {
    CborParser parser;
    cbor_parser_init(&parser, payload, size);

    reset_parsed_values(&app->tag_data);

    // Parse meta section
    if(!parse_meta_section(&parser, &app->tag_data.meta)) {
        FURI_LOG_E(TAG, "Failed to parse meta section");
        return false;
    }

    // The main section follows the meta section unless the meta section says otherwise
    if(app->tag_data.meta.main_region_offset > 0 && app->tag_data.meta.main_region_offset < size) {
        cbor_parser_init(
            &parser,
            payload + app->tag_data.meta.main_region_offset,
            size - app->tag_data.meta.main_region_offset);
    }

    // Parse main section
    if(!parse_main_section(&parser, &app->tag_data.main)) {
        FURI_LOG_E(TAG, "Failed to parse main section");
        return false;
    }

    // Without an explicit size the auxiliary region extends to the end of the payload
    if(app->tag_data.meta.aux_region_offset > 0 && app->tag_data.meta.aux_region_offset < size &&
       app->tag_data.meta.aux_region_size == 0) {
        app->tag_data.meta.aux_region_size = size - app->tag_data.meta.aux_region_offset;
    }

    // Parse auxiliary section if present. An offset past the payload is ignored.
    if(app->tag_data.meta.aux_region_offset > 0 && app->tag_data.meta.aux_region_offset < size) {
        cbor_parser_init(
            &parser,
            payload + app->tag_data.meta.aux_region_offset,
            size - app->tag_data.meta.aux_region_offset);
        if(!parse_aux_section(&parser, &app->tag_data.aux)) {
            FURI_LOG_W(TAG, "Failed to parse aux section");
        }
    }

    FURI_LOG_I(TAG, "Successfully parsed OpenPrintTag data");
    return true;
}
