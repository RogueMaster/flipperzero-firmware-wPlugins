#include "openprinttag_i.h"
#include "openprinttag_fields.h"
#include "cbor_parser.h"
#include <furi.h>

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
            cbor_skip_value(parser);
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
            cbor_skip_value(parser);
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
            if(value.type == CborValueTypeUnsigned) {
                main->nominal_netto_full_weight = (uint32_t)value.value.u64;
            }
            break;
        case MAIN_ACTUAL_NETTO_FULL_WEIGHT:
            if(value.type == CborValueTypeUnsigned) {
                main->actual_netto_full_weight = (uint32_t)value.value.u64;
            }
            break;
        case MAIN_EMPTY_CONTAINER_WEIGHT:
            if(value.type == CborValueTypeUnsigned) {
                main->empty_container_weight = (uint32_t)value.value.u64;
            }
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
            if(value.type == CborValueTypeUnsigned) {
                main->filament_diameter = (float)value.value.u64;
            }
            break;
        case MAIN_NOMINAL_FULL_LENGTH:
            if(value.type == CborValueTypeUnsigned) {
                main->nominal_full_length = (float)value.value.u64;
            }
            break;
        case MAIN_ACTUAL_FULL_LENGTH:
            if(value.type == CborValueTypeUnsigned) {
                main->actual_full_length = (float)value.value.u64;
            }
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

        // Material properties
        case MAIN_DENSITY:
            if(value.type == CborValueTypeUnsigned) {
                main->density = (float)value.value.u64;
            }
            break;
        case MAIN_TAGS:
            // Tags is an array - store for later parsing
            if(value.type == CborValueTypeArray) {
                // Skip for now - would need to parse array
                cbor_skip_value(parser);
            }
            break;

        default:
            // Skip unknown fields
            break;
        }
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
            cbor_skip_value(parser);
            continue;
        }

        uint32_t key_id = (uint32_t)key.value.u64;

        if(!cbor_parse_value(parser, &value)) return false;

        switch(key_id) {
        case AUX_CONSUMED_WEIGHT:
            if(value.type == CborValueTypeUnsigned) {
                aux->consumed_weight = (uint32_t)value.value.u64;
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
    }

    return true;
}

bool openprinttag_parse_cbor(OpenPrintTag* app, const uint8_t* payload, size_t size) {
    CborParser parser;
    cbor_parser_init(&parser, payload, size);

    // Parse meta section
    if(!parse_meta_section(&parser, &app->tag_data.meta)) {
        FURI_LOG_E(TAG, "Failed to parse meta section");
        return false;
    }

    // Parse main section
    if(!parse_main_section(&parser, &app->tag_data.main)) {
        FURI_LOG_E(TAG, "Failed to parse main section");
        return false;
    }

    // Parse auxiliary section if present
    if(app->tag_data.meta.aux_region_offset > 0) {
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
