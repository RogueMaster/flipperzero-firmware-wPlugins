#include "iso15693_data.h"

#include <furi.h>

Iso15693Data* iso15693_data_alloc() {
    Iso15693Data* instance = malloc(sizeof(Iso15693Data));
    instance->iso15693_3_data = iso15693_3_alloc();
    return instance;
}

void iso15693_data_free(Iso15693Data* instance) {
    furi_assert(instance);
    iso15693_3_free(instance->iso15693_3_data);
    free(instance);
}

void iso15693_data_reset(Iso15693Data* instance) {
    furi_assert(instance);
    iso15693_3_reset(instance->iso15693_3_data);
}

void iso15693_data_copy(Iso15693Data* target, const Iso15693Data* source) {
    furi_assert(target);
    furi_assert(source);
    iso15693_3_copy(target->iso15693_3_data, source->iso15693_3_data);
}
