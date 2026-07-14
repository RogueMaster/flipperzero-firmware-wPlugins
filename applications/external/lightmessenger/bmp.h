/**
 *  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
 *  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
 *  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
 *    + Tixlegeek
 */
#ifndef _BMP_STRUCT_H
#define _BMP_STRUCT_H

#include <stdint.h>
#include <storage/storage.h>
/**
  Simple BitMap header
*/
typedef enum {
    BMP_OK = 0,
    BMP_ERR_INVALID,
    BMP_ERR_TOO_LARGE,
    BMP_ERR_BAD_FORMAT,
    BMP_ERR_INTERNAL,
    BMP_ERR_FILESYSTEM,
    BMP_ERR_WRITE,
    BMP_ERR_MEMORY,
} BMP_err;

#define BMP_FILE_MAX_SIZE 500
#pragma pack(push, 1)
typedef struct {
    uint16_t bfType; // Specifies the file type; must be "BM" to indicate a BMP file.
    uint32_t bfSize; // Specifies the size, in bytes, of the bitmap file.
    uint16_t bfReserved1; // Reserved; must be set to zero.
    uint16_t bfReserved2; // Reserved; must be set to zero.
    uint32_t bfOffBits; // Specifies the offset, in bytes, from the beginning of the
        // BMPFileHeader structure to the bitmap bits.
} BMPFileHeader;

typedef struct {
    uint32_t biSize; // Specifies the number of bytes required by the structure.
    int32_t biWidth; // Specifies the width of the image, in pixels.
    int32_t biHeight; // Specifies the height of the image, in pixels.
    uint16_t biPlanes; // Specifies the number of planes for the target device; must be set to 1.
    uint16_t biBitCount; // Specifies the number of bits per pixel (bpp), which determines the
        // number of colors that can be represented. For a 1bpp image, this would be set to 1.
    uint32_t biCompression; // Specifies the type of compression for the bitmap. For 1bpp images,
        // this is typically BI_RGB, indicating no compression.
    uint32_t
        biSizeImage; // Specifies the size, in bytes, of the image. This can be set to 0 for BI_RGB bitmaps.
    int32_t
        biXPelsPerMeter; // Specifies the horizontal resolution, in pixels per meter, of the target device.
    int32_t
        biYPelsPerMeter; // Specifies the vertical resolution, in pixels per meter, of the target device.
    uint32_t
        biClrUsed; // Specifies the number of colors used in the bitmap. For 1bpp images, this would typically be 2.
    uint32_t
        biClrImportant; // Specifies the number of important colors used, or 0 when every color is important.
} BMPInfoHeader;
#pragma pack(pop)

//  Generic Bitmap structure
typedef struct {
    uint8_t width;
    uint8_t height;
    uint8_t** array;
    void* next_bitmap;
} bitmapMatrix;

typedef struct {
    BMPFileHeader file_header;
    BMPInfoHeader info_header;
    unsigned char* data; // Actual RGB data
} BMPImage;

void bitmapMatrix_free(bitmapMatrix* bitMatrix);
int bmp_read_file_header(File* bitmapFile, BMPFileHeader* file_header);
int bmp_read_info_header(File* bitmapFile, BMPInfoHeader* info_header);
int bmp_load(const char* path, BMPImage* bitmap);
bitmapMatrix* bmp_to_bitmapMatrix(const char* path);
int bmp_load_header(const char* path, BMPImage* bitmap);
int bmp_header_check_1bpp(const char* path);
int bmp_write(const char* path, bitmapMatrix* bitmap);
#endif /* end of include guard: _BMP_STRUCT_H */
