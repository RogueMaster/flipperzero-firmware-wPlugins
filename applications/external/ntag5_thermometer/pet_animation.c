#include "pet_animation.h"

#include <furi.h>
#include <storage/storage.h>

#define TAG "PetAnimation"

#define PET_ANIMATION_MAGIC        "PETA"
#define PET_ANIMATION_VERSION      (1)
#define PET_ANIMATION_HEADER_SIZE  (32)
#define PET_ANIMATION_FRAME_MS_MIN (20)
#define PET_ANIMATION_FRAME_MS_MAX (10000)

struct PetAnimation {
    Storage* storage;
    File* file;
    bool open;

    uint8_t width;
    uint8_t height;
    uint8_t frame_count;
    uint16_t frame_ms;
    uint8_t loops;
    char name[PET_ANIMATION_NAME_SIZE + 1];

    size_t frame_size;
    uint8_t frame_index; /**< Next frame to read */
    uint8_t loop_index;
};

PetAnimation* pet_animation_alloc(void) {
    PetAnimation* instance = malloc(sizeof(PetAnimation));

    instance->storage = furi_record_open(RECORD_STORAGE);
    instance->file = storage_file_alloc(instance->storage);
    storage_simply_mkdir(instance->storage, PET_ANIMATION_DIRECTORY);

    return instance;
}

void pet_animation_free(PetAnimation* instance) {
    furi_check(instance);

    pet_animation_close(instance);
    storage_file_free(instance->file);
    furi_record_close(RECORD_STORAGE);

    free(instance);
}

static bool pet_animation_read_header(PetAnimation* instance) {
    uint8_t header[PET_ANIMATION_HEADER_SIZE];

    if(storage_file_read(instance->file, header, sizeof(header)) != sizeof(header)) {
        return false;
    }
    if(memcmp(header, PET_ANIMATION_MAGIC, 4) != 0 || header[4] != PET_ANIMATION_VERSION) {
        return false;
    }

    instance->width = header[5];
    instance->height = header[6];
    instance->frame_count = header[7];
    instance->frame_ms = header[8] | (header[9] << 8);
    instance->loops = header[10];
    memcpy(instance->name, &header[12], PET_ANIMATION_NAME_SIZE);
    instance->name[PET_ANIMATION_NAME_SIZE] = '\0';

    if(instance->width == 0 || instance->width > PET_ANIMATION_WIDTH_MAX ||
       instance->height == 0 || instance->height > PET_ANIMATION_HEIGHT_MAX ||
       instance->frame_count == 0 || instance->loops == 0 ||
       instance->frame_ms < PET_ANIMATION_FRAME_MS_MIN ||
       instance->frame_ms > PET_ANIMATION_FRAME_MS_MAX) {
        return false;
    }

    instance->frame_size = (instance->width + 7) / 8 * instance->height;
    const uint64_t expected =
        PET_ANIMATION_HEADER_SIZE + (uint64_t)instance->frame_size * instance->frame_count;
    return storage_file_size(instance->file) >= expected;
}

bool pet_animation_open(PetAnimation* instance, const char* id) {
    furi_check(instance);
    furi_check(id);

    pet_animation_close(instance);

    FuriString* path = furi_string_alloc_printf("%s/%s.anim", PET_ANIMATION_DIRECTORY, id);
    bool result = false;

    if(storage_file_open(
           instance->file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
        result = pet_animation_read_header(instance);
        if(!result) {
            FURI_LOG_W(TAG, "Invalid animation %s", furi_string_get_cstr(path));
            storage_file_close(instance->file);
        }
    } else {
        // Tells users the file name to use
        FURI_LOG_I(TAG, "No animation %s", furi_string_get_cstr(path));
        storage_file_close(instance->file);
    }

    if(result) {
        instance->open = true;
        instance->frame_index = 0;
        instance->loop_index = 0;
        FURI_LOG_I(
            TAG,
            "%s: %ux%u, %u frames",
            furi_string_get_cstr(path),
            instance->width,
            instance->height,
            instance->frame_count);
    }

    furi_string_free(path);
    return result;
}

void pet_animation_close(PetAnimation* instance) {
    furi_check(instance);

    if(instance->open) {
        storage_file_close(instance->file);
        instance->open = false;
    }
}

bool pet_animation_is_open(const PetAnimation* instance) {
    furi_check(instance);
    return instance->open;
}

bool pet_animation_next_frame(PetAnimation* instance, uint8_t* frame) {
    furi_check(instance);
    furi_check(frame);

    if(!instance->open) {
        return false;
    }

    if(instance->frame_index == instance->frame_count) {
        instance->frame_index = 0;
        instance->loop_index++;
        if(instance->loop_index == instance->loops ||
           !storage_file_seek(instance->file, PET_ANIMATION_HEADER_SIZE, true)) {
            return false;
        }
    }

    if(storage_file_read(instance->file, frame, instance->frame_size) != instance->frame_size) {
        return false;
    }

    instance->frame_index++;
    return true;
}

uint8_t pet_animation_get_width(const PetAnimation* instance) {
    furi_check(instance);
    return instance->width;
}

uint8_t pet_animation_get_height(const PetAnimation* instance) {
    furi_check(instance);
    return instance->height;
}

uint32_t pet_animation_get_frame_ms(const PetAnimation* instance) {
    furi_check(instance);
    return instance->frame_ms;
}

const char* pet_animation_get_name(const PetAnimation* instance) {
    furi_check(instance);
    return instance->name;
}
