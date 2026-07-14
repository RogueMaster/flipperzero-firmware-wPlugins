#include "pbm.h"

#include <storage/storage.h>

unsigned int _pbm_read_number(File* file) {
    unsigned int result = 0;

    bool read_valid_data = false;
    bool comment = false;
    char buf;
    for(;;) {
        if(storage_file_read(file, &buf, sizeof(buf)) == sizeof(buf)) {
            // If it's a comment, skip
            if(buf == '#') {
                comment = true;
            } else {
                if(isspace(buf)) {
                    if(read_valid_data) {
                        // Parsed the number successfully
                        break;
                    }
                    if(buf == '\r' || buf == '\n') {
                        comment = false;
                    }
                    continue;
                }
                if(comment) {
                    continue;
                }

                if(!(buf >= '0' && buf <= '9')) {
                    return 0;
                }

                read_valid_data = true;
                result = (result * 10) + (buf - '0');
            }
        }
    }
    return result;
}

Pbm* pbm_load_file(Storage* storage, const char* path) {
    Pbm* result = NULL;

    File* file = storage_file_alloc(storage);
    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char magic[2];
        if(storage_file_read(file, magic, sizeof(magic)) == sizeof(magic) && magic[0] == 'P' &&
           magic[1] == '4') {
            const unsigned int width = _pbm_read_number(file);
            const unsigned int height = _pbm_read_number(file);
            const size_t buf_size = ((width + 7) >> 3) * height;

            result = malloc(sizeof(*result) + buf_size);
            if(storage_file_read(file, result->bitmap, buf_size) != buf_size) {
                free(result);
                result = NULL;
            }

            // PBM stores pixels from the most signifcant bit to the least significant bit,
            // so we need to reverse the order. Thankfully an intrinsic to do that exists.
            for(uint8_t *pix = result->bitmap, *pix_end = result->bitmap + buf_size;
                pix != pix_end;
                ++pix) {
                *pix = __RBIT(*pix) >> 24;
            }

            result->width = (uint16_t)width;
            result->height = (uint16_t)height;
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return result;
}

void pbm_free(Pbm* pbm) {
    free(pbm);
}

uint16_t pbm_get_pitch(Pbm* pbm) {
    return (pbm->width + 7) & ~7;
}
