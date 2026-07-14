#ifndef PNG_H
#define PNG_H

#include <stdio.h>
#include <storage/storage.h>

typedef struct {
    unsigned char r, g, b, a;
} rgba_t;

#define BYTES_GROW_SIZE 512
typedef struct {
    unsigned int width;
    unsigned int height;
    rgba_t* pixels;

    bool save_to_bytes;
    File* file;
    uint8_t* bytes;
    size_t bytes_len;
    size_t size;

    unsigned int crc;
    unsigned int s1;
    unsigned int s2;
} image_t;

typedef rgba_t palette_t[256];

typedef enum {
    MODE_TRUECOLOUR_WITH_ALPHA = 6,
    MODE_GREYSCALE_WITH_ALPHA = 4,
    MODE_TRUECOLOUR = 2,
    MODE_GREYSCALE = 0,
    MODE_INDEXED = 3
} image_mode_t;

image_t* image_new(unsigned int width, unsigned int height) {
    image_t* image = malloc(sizeof(image_t));
    image->width = width;
    image->height = height;
    image->pixels = malloc(sizeof(rgba_t) * width * height);

    image->save_to_bytes = false;
    image->file = NULL;
    image->bytes = NULL;
    image->bytes_len = 0;
    image->size = 0;
    return image;
}

void image_free(image_t* image) {
    free(image->pixels);
    free(image);
}

static inline rgba_t lerp(rgba_t p, rgba_t q, float t) {
    return (rgba_t){
        p.r + (q.r - p.r) * t, p.g + (q.g - p.g) * t, p.b + (q.b - p.b) * t, p.a + (q.a - p.a) * t};
}

unsigned int crc_table[256];
int crc_table_computed = 0;

static inline void make_crc_table() {
    for(int n = 0; n < 256; n++) {
        unsigned int c = (unsigned int)n;
        for(int k = 0; k < 8; k++)
            if(c & 1)
                c = 0xedb88320L ^ (c >> 1);
            else
                c = c >> 1;
        crc_table[n] = c;
    }
    crc_table_computed = 1;
}

void check_bytes_size(image_t* image, size_t num_bytes_to_write) {
    if(!image->save_to_bytes) return;
    if(image->size + num_bytes_to_write > image->bytes_len) {
        image->bytes = realloc(image->bytes, image->bytes_len + BYTES_GROW_SIZE);
        image->bytes_len += BYTES_GROW_SIZE;
    }
}

static inline void image_write_header(image_t* image) {
    if(image->save_to_bytes) {
        check_bytes_size(image, 8);
        memcpy(image->bytes + image->size, "\x89PNG\r\n\x1a\n", 8);
        image->size += 8;
    } else {
        storage_file_write(image->file, "\x89PNG\r\n\x1a\n", 8);
    }
}

static inline void image_write_uchar(image_t* image, unsigned char c) {
    if(image->save_to_bytes) {
        check_bytes_size(image, 1);
        memcpy(image->bytes + image->size, &c, 1);
        image->size += 1;
    } else {
        storage_file_write(image->file, &c, 1);
    }
}

static inline void image_update_crc(image_t* image, unsigned char c) {
    if(!crc_table_computed) make_crc_table();
    image->crc = crc_table[(image->crc ^ c) & 0xff] ^ (image->crc >> 8);
}

static inline void image_write_uchar_crc(image_t* image, unsigned char c) {
    image_write_uchar(image, c);
    image_update_crc(image, c);
}

static inline void image_write_uint(image_t* image, unsigned int u) {
    image_write_uchar(image, (u >> 24) & 0xff);
    image_write_uchar(image, (u >> 16) & 0xff);
    image_write_uchar(image, (u >> 8) & 0xff);
    image_write_uchar(image, u & 0xff);
}

static inline void image_write_uint_crc(image_t* image, unsigned int u) {
    image_write_uchar_crc(image, (u >> 24) & 0xff);
    image_write_uchar_crc(image, (u >> 16) & 0xff);
    image_write_uchar_crc(image, (u >> 8) & 0xff);
    image_write_uchar_crc(image, u & 0xff);
}

static inline void image_write_chunk_start(image_t* image, unsigned int size, char* name) {
    image->crc = 0xffffffff;
    image_write_uint(image, size);
    image_write_uchar_crc(image, name[0]);
    image_write_uchar_crc(image, name[1]);
    image_write_uchar_crc(image, name[2]);
    image_write_uchar_crc(image, name[3]);
}

static inline void image_write_chunk_end(image_t* image) {
    image->crc = ~image->crc;
    char chunk[4] = {
        (image->crc >> 24) & 0xff,
        (image->crc >> 16) & 0xff,
        (image->crc >> 8) & 0xff,
        (image->crc) & 0xff};
    if(image->save_to_bytes) {
        check_bytes_size(image, 4);
        memcpy(image->bytes + image->size, chunk, 4);
        image->size += 4;
    } else {
        storage_file_write(image->file, chunk, 4);
    }
}

static inline void image_write_ihdr_chunk(image_t* image) {
    image_write_chunk_start(image, 13, "IHDR");
    image_write_uint_crc(image, image->width);
    image_write_uint_crc(image, image->height);
    image_write_uchar_crc(image, 8);
    image_write_uchar_crc(image, MODE_TRUECOLOUR_WITH_ALPHA);
    image_write_uchar_crc(image, 0);
    image_write_uchar_crc(image, 0);
    image_write_uchar_crc(image, 0);
    image_write_chunk_end(image);
}

static inline void image_write_ushort_crc_reverse(image_t* image, unsigned short s) {
    image_write_uchar_crc(image, s & 0xff);
    image_write_uchar_crc(image, (s >> 8) & 0xff);
}

static inline void image_write_uchar_adler(image_t* image, unsigned char c) {
    image_write_uchar_crc(image, c);
    image->s1 = (image->s1 + c) % 65521;
    image->s2 = (image->s2 + image->s1) % 65521;
}

static inline void image_write_uint_adler(image_t* image, unsigned int u) {
    image_write_uchar_adler(image, (u >> 24) & 0xff);
    image_write_uchar_adler(image, (u >> 16) & 0xff);
    image_write_uchar_adler(image, (u >> 8) & 0xff);
    image_write_uchar_adler(image, u & 0xff);
}

static inline void image_write_idat_chunk(image_t* image) {
    image_write_chunk_start(image, 2 + image->height * (6 + 4 * image->width) + 4, "IDAT");
    image_write_uchar_crc(image, 0x78);
    image_write_uchar_crc(image, 0xda);

    image->s1 = 1;
    image->s2 = 0;

    for(unsigned int y = 0; y < image->height; y++) {
        if(y == image->height - 1)
            image_write_uchar_crc(image, 1);
        else
            image_write_uchar_crc(image, 0);
        unsigned short s = 1 + 4 * image->width;

        image_write_ushort_crc_reverse(image, s);
        image_write_ushort_crc_reverse(image, ~s);

        image_write_uchar_adler(image, 0);

        for(unsigned int x = 0; x < image->width; x++) {
            rgba_t rgba = image->pixels[y * image->width + x];

            image_write_uchar_adler(image, rgba.r);
            image_write_uchar_adler(image, rgba.g);
            image_write_uchar_adler(image, rgba.b);
            image_write_uchar_adler(image, rgba.a);
        }
    }

    image_write_uint_crc(image, (image->s2 << 16) | image->s1);
    image_write_chunk_end(image);
}

static inline void image_write_iend_chunk(image_t* image) {
    image_write_chunk_start(image, 0, "IEND");
    image_write_chunk_end(image);
}

void image_save(image_t* image) {
    assert(image->file != NULL || image->bytes != NULL);
    image_write_header(image);
    image_write_ihdr_chunk(image);
    image_write_idat_chunk(image);
    image_write_iend_chunk(image);
}

#endif
