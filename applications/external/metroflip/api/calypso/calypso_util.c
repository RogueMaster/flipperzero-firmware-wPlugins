#include "metroflip_i.h"
#include <stdlib.h>
#include <string.h>
#include "calypso_util.h"
#include "../metroflip/metroflip_api.h"
#define CALYPSO_MAX_FILE_COUNT (10U)
#define CALYPSO_MAX_FILE_SIZE  (30U)
CalypsoElement make_calypso_final_element(
    const char* key,
    int size,
    const char* label,
    CalypsoFinalType final_type) {
    CalypsoElement final_element = {};

    final_element.type = CALYPSO_ELEMENT_TYPE_FINAL;
    final_element.final = malloc(sizeof(CalypsoFinalElement));
    final_element.final->size = size;
    final_element.final->final_type = final_type;
    strncpy(final_element.final->key, key, 36);
    strncpy(final_element.final->label, label, 64);

    return final_element;
}

uint8_t* read_calypso_data(FlipperFormat* format, const char* app_id, const char* file_id) {
    bool found = false;
    uint8_t* byte_array_buffer = (uint8_t*)malloc(29);
    FuriString* entry_preamble = furi_string_alloc();
    // Read the travel history entries
    for(uint8_t i = 0; i < 1; i++) {
        furi_string_printf(entry_preamble, "AID %s FID %s", app_id, file_id);
        // For every line in the flipper format file
        // We read the entire line's hex and store it in the byte_array_buffer
        if(!flipper_format_read_hex(
               format, furi_string_get_cstr(entry_preamble), byte_array_buffer, 29)) {
            FURI_LOG_W(
                "calypso",
                "app %s file %s not found. payload: %s",
                app_id,
                file_id,
                furi_string_get_cstr(entry_preamble));
            found = false;
            break;
        } else {
            FURI_LOG_I("calypso", "FOUND %s", furi_string_get_cstr(entry_preamble));
            found = true;
        }
    }
    furi_string_free(entry_preamble);
    if(found) {
        FURI_LOG_I("calypso", "25th byte: %02X", byte_array_buffer[25]);
        return byte_array_buffer;
    } else {
        return NULL;
    }
}
CalypsoElement make_calypso_bitmap_element(const char* key, int size, CalypsoElement* elements) {
    CalypsoElement bitmap_element = {};

    bitmap_element.type = CALYPSO_ELEMENT_TYPE_BITMAP;
    bitmap_element.bitmap = malloc(sizeof(CalypsoBitmapElement));
    bitmap_element.bitmap->size = size;
    bitmap_element.bitmap->elements = malloc(size * sizeof(CalypsoElement));
    for(int i = 0; i < size; i++) {
        bitmap_element.bitmap->elements[i] = elements[i];
    }
    strncpy(bitmap_element.bitmap->key, key, 36);

    return bitmap_element;
}

CalypsoElement
    make_calypso_container_element(const char* key, int size, CalypsoElement* elements) {
    CalypsoElement container_element = {};

    container_element.type = CALYPSO_ELEMENT_TYPE_CONTAINER;
    container_element.container = malloc(sizeof(CalypsoContainerElement));
    container_element.container->size = size;
    container_element.container->elements = malloc(size * sizeof(CalypsoElement));
    for(int i = 0; i < size; i++) {
        container_element.container->elements[i] = elements[i];
    }
    strncpy(container_element.container->key, key, 36);

    return container_element;
}

CalypsoElement make_calypso_repeater_element(const char* key, int size, CalypsoElement element) {
    CalypsoElement repeater_element = {};

    repeater_element.type = CALYPSO_ELEMENT_TYPE_REPEATER;
    repeater_element.repeater = malloc(sizeof(CalypsoRepeaterElement));
    repeater_element.repeater->size = size;
    repeater_element.repeater->element = element;
    strncpy(repeater_element.repeater->key, key, 36);

    return repeater_element;
}

void free_calypso_element(CalypsoElement* element) {
    if(element->type == CALYPSO_ELEMENT_TYPE_FINAL) {
        free(element->final);
    } else if(element->type == CALYPSO_ELEMENT_TYPE_BITMAP) {
        for(int i = 0; i < element->bitmap->size; i++) {
            free_calypso_element(&element->bitmap->elements[i]);
        }
        free(element->bitmap->elements);
        free(element->bitmap);
    } else if(element->type == CALYPSO_ELEMENT_TYPE_CONTAINER) {
        for(int i = 0; i < element->container->size; i++) {
            free_calypso_element(&element->container->elements[i]);
        }
        free(element->container->elements);
        free(element->container);
    } else if(element->type == CALYPSO_ELEMENT_TYPE_REPEATER) {
        free_calypso_element(&element->repeater->element);
        free(element->repeater);
    }
}

void free_calypso_structure(CalypsoApp* structure) {
    for(int i = 0; i < structure->container->size; i++) {
        free_calypso_element(&structure->container->elements[i]);
    }
    free(structure->container->elements);
    free(structure->container);
    free(structure);
}

int* get_bitmap_positions(const char* binary_string, int* count) {
    int length = strlen(binary_string);
    int* positions = malloc(length * sizeof(int));
    int pos_index = 0;

    for(int i = 0; i < length; i++) {
        if(binary_string[length - 1 - i] == '1') {
            positions[pos_index++] = i;
        }
    }

    *count = pos_index;
    return positions;
}

int is_bit_present(int* positions, int count, int bit) {
    for(int i = 0; i < count; i++) {
        if(positions[i] == bit) {
            return 1;
        }
    }
    return 0;
}

bool is_calypso_subnode_present(
    const char* binary_string,
    const char* key,
    CalypsoBitmapElement* bitmap) {
    char bit_slice[bitmap->size + 1];
    strncpy(bit_slice, binary_string, bitmap->size);
    bit_slice[bitmap->size] = '\0';
    int count = 0;
    int* positions = get_bitmap_positions(bit_slice, &count);
    int offset = bitmap->size;
    for(int i = 0; i < count; i++) {
        CalypsoElement* element = &bitmap->elements[positions[i]];
        if(element->type == CALYPSO_ELEMENT_TYPE_FINAL) {
            if(strcmp(element->final->key, key) == 0) {
                free(positions);
                return true;
            }
            offset += element->final->size;
        } else {
            if(strcmp(element->bitmap->key, key) == 0) {
                free(positions);
                return true;
            }
            int sub_binary_string_size = element->bitmap->size;
            char bit_slice[sub_binary_string_size + 1];
            strncpy(bit_slice, binary_string, sub_binary_string_size);
            bit_slice[sub_binary_string_size] = '\0';
            if(is_calypso_subnode_present(binary_string + offset, key, element->bitmap)) {
                free(positions);
                return true;
            }
            offset += element->bitmap->size;
        }
    }
    free(positions);
    return false;
}

bool is_calypso_node_present(const char* binary_string, const char* key, CalypsoApp* structure) {
    int offset = 0;
    for(int i = 0; i < structure->container->size; i++) {
        if(structure->container->elements[i].type == CALYPSO_ELEMENT_TYPE_FINAL) {
            if(strcmp(structure->container->elements[i].final->key, key) == 0) {
                return true;
            }
            offset += structure->container->elements[i].final->size;
        } else {
            if(strcmp(structure->container->elements[i].bitmap->key, key) == 0) {
                return true;
            }
            int sub_binary_string_size = structure->container->elements[i].bitmap->size;
            char bit_slice[sub_binary_string_size + 1];
            strncpy(bit_slice, binary_string, sub_binary_string_size);
            bit_slice[sub_binary_string_size] = '\0';
            if(is_calypso_subnode_present(
                   binary_string + offset, key, structure->container->elements[i].bitmap)) {
                return true;
            }
            offset += structure->container->elements[i].bitmap->size;
        }
    }
    return false;
}

int get_calypso_subnode_offset(
    const char* binary_string,
    const char* key,
    CalypsoElement* elem,
    bool* found) {
    // recursive function to get the offset of a subnode in a calypso binary string
    if(elem->type == CALYPSO_ELEMENT_TYPE_FINAL) {
        if(strcmp(elem->final->key, key) == 0) {
            *found = true;
            return 0;
        }
        return elem->final->size;
    } else if(elem->type == CALYPSO_ELEMENT_TYPE_BITMAP) {
        CalypsoBitmapElement* bitmap = elem->bitmap;

        char bit_slice[bitmap->size + 1];
        strncpy(bit_slice, binary_string, bitmap->size);
        bit_slice[bitmap->size] = '\0';

        int count = 0;
        int* positions = get_bitmap_positions(bit_slice, &count);
        bool f = false;

        int count_offset = bitmap->size;
        for(int i = 0; i < count; i++) {
            CalypsoElement element = bitmap->elements[positions[i]];
            count_offset +=
                get_calypso_subnode_offset(binary_string + count_offset, key, &element, &f);
            if(f) {
                *found = true;
                free(positions);
                return count_offset;
            }
        }

        free(positions);
        return count_offset;
    } else if(elem->type == CALYPSO_ELEMENT_TYPE_CONTAINER) {
        // same as bitmap but without bitmap at the beginning
        CalypsoContainerElement* container = elem->container;

        int count_offset = 0;
        bool f = false;
        for(int i = 0; i < container->size; i++) {
            CalypsoElement element = container->elements[i];
            count_offset +=
                get_calypso_subnode_offset(binary_string + count_offset, key, &element, &f);
            if(f) {
                *found = true;
                return count_offset;
            }
        }

        return count_offset;
    } else if(elem->type == CALYPSO_ELEMENT_TYPE_REPEATER) {
        // same as bitmap but instead of a bitmap, we have the count of how many times to repeat the inner element
        CalypsoRepeaterElement* repeater = elem->repeater;

        char bit_slice[repeater->size + 1];
        strncpy(bit_slice, binary_string, repeater->size);
        bit_slice[repeater->size] = '\0';

        int C = bit_slice_to_dec(bit_slice, 0, repeater->size - 1);

        int count_offset = repeater->size;
        bool f = false;
        for(int i = 0; i < C; i++) {
            count_offset += get_calypso_subnode_offset(
                binary_string + count_offset, key, &repeater->element, &f);
            if(f) {
                *found = true;
                return count_offset;
            }
        }
    }
    return 0;
}

int get_calypso_node_offset(const char* binary_string, const char* key, CalypsoApp* structure) {
    CalypsoElement* element = malloc(sizeof(CalypsoElement));
    element->type = CALYPSO_ELEMENT_TYPE_CONTAINER;
    element->container = structure->container;
    bool found;
    int offset = get_calypso_subnode_offset(binary_string, key, element, &found);
    if(!found) {
        FURI_LOG_E("Metroflip:Scene:Calypso", "Key %s not found in calypso structure", key);
    }
    free(element);
    return offset;
}

int get_calypso_subnode_size(const char* key, CalypsoElement* element) {
    if(element->type == CALYPSO_ELEMENT_TYPE_FINAL) {
        if(strcmp(element->final->key, key) == 0) {
            return element->final->size;
        }
    } else if(element->type == CALYPSO_ELEMENT_TYPE_BITMAP) {
        if(strcmp(element->bitmap->key, key) == 0) {
            return element->bitmap->size;
        }
        for(int i = 0; i < element->bitmap->size; i++) {
            int size = get_calypso_subnode_size(key, &element->bitmap->elements[i]);
            if(size != 0) {
                return size;
            }
        }
    } else if(element->type == CALYPSO_ELEMENT_TYPE_CONTAINER) {
        if(strcmp(element->container->key, key) == 0) {
            return element->container->size;
        }
        for(int i = 0; i < element->container->size; i++) {
            int size = get_calypso_subnode_size(key, &element->container->elements[i]);
            if(size != 0) {
                return size;
            }
        }
    } else if(element->type == CALYPSO_ELEMENT_TYPE_REPEATER) {
        if(strcmp(element->repeater->key, key) == 0) {
            return element->repeater->size;
        }
        int size = get_calypso_subnode_size(key, &element->repeater->element);
        if(size != 0) {
            return size;
        }
    }
    return 0;
}

int get_calypso_node_size(const char* key, CalypsoApp* structure) {
    CalypsoElement* element = malloc(sizeof(CalypsoElement));
    element->type = CALYPSO_ELEMENT_TYPE_CONTAINER;
    element->container = structure->container;
    int count = get_calypso_subnode_size(key, element);
    free(element);
    return count;
}

CALYPSO_CARD_TYPE guess_card_type(int country_num, int network_num) {
    switch(country_num) {
    case 56:
        switch(network_num) {
        case 1:
            return CALYPSO_CARD_MOBIB;
        default:
            return CALYPSO_CARD_UNKNOWN;
        }
    case 124:
        switch(network_num) {
        case 1:
            return CALYPSO_CARD_OPUS;
        default:
            return CALYPSO_CARD_UNKNOWN;
        }
    case 131:
        return CALYPSO_CARD_VIVA;
    case 250:
        switch(network_num) {
        case 0:
            return CALYPSO_CARD_PASSPASS;
        case 64:
            return CALYPSO_CARD_TAM; // Montpellier
        case 149:
            return CALYPSO_CARD_TRANSPOLE; // Lille
        case 502:
            return CALYPSO_CARD_OURA;
        case 901:
            return CALYPSO_CARD_NAVIGO;
        case 908:
            return CALYPSO_CARD_KORRIGO;
        case 916:
            return CALYPSO_CARD_TISSEO;
        case 920:
            return CALYPSO_CARD_ENVIBUS;
        case 921:
            return CALYPSO_CARD_GIRONDE;
        default:
            return CALYPSO_CARD_UNKNOWN;
        }
    case 376:
        return CALYPSO_CARD_RAVKAV;
    default:
        return CALYPSO_CARD_UNKNOWN;
    }
}

const char* get_country_string(int country_num) {
    switch(country_num) {
    case 56:
        return "Belgium";
    case 124:
        return "Canada";
    case 131:
        return "Portugal";
    case 250:
        return "France";
    case 376:
        return "Israel";
    default: {
        char* country = malloc(4 * sizeof(char));
        snprintf(country, 4, "%d", country_num);
        return country;
    }
    }
}

const char* get_network_string(CALYPSO_CARD_TYPE card_type) {
    switch(card_type) {
    case CALYPSO_CARD_MOBIB:
        return "Mobib";
    case CALYPSO_CARD_OPUS:
        return "Opus";
    case CALYPSO_CARD_VIVA:
        return "Viva";
    case CALYPSO_CARD_PASSPASS:
        return "PassPass";
    case CALYPSO_CARD_TAM:
        return "TAM";
    case CALYPSO_CARD_OURA:
        return "Oura";
    case CALYPSO_CARD_NAVIGO:
        return "IDFM";
    case CALYPSO_CARD_KORRIGO:
        return "KorriGo";
    case CALYPSO_CARD_TISSEO:
        return "Tisseo";
    case CALYPSO_CARD_ENVIBUS:
        return "Envibus";
    case CALYPSO_CARD_GIRONDE:
        return "Gironde";
    case CALYPSO_CARD_RAVKAV:
        return "Rav-Kav";
    default:
        return "Unknown";
    }
}
