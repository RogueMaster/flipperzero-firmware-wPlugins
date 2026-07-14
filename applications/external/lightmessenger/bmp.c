/**
*  ▌  ▞▚ ▛▚ ▌  ▞▚ ▟  Copyright© 2025 LAB401 GPLv3
*  ▌  ▛▜ ▛▚ ▙▙ ▌▐ ▐  This program is free software
*  ▀▀ ▘▝ ▀▘  ▘ ▝▘ ▀▘ See LICENSE.txt - lab401.com
*    + Tixlegeek
*/
/*
  Tiny handcrafted 1bpp bitmap library
*/
#include "bmp.h"
#include <inttypes.h>
static const char* TAG = "BITMAP";

/*static void bmp_debug(BMPFileHeader* fileHeader, BMPInfoHeader* infoHeader) {
    FURI_LOG_I(TAG, "fileHeader:");
    FURI_LOG_I(TAG, ".....Tag: %04x", fileHeader->bfType);
    FURI_LOG_I(TAG, ".....Size: %ld", fileHeader->bfSize);
    FURI_LOG_I(TAG, ".....Offset: %ld", fileHeader->bfOffBits);
    FURI_LOG_I(TAG, "infoHeader:");
    FURI_LOG_I(TAG, ".....Width: %ld", infoHeader->biWidth);
    FURI_LOG_I(TAG, ".....Height: %ld", infoHeader->biHeight);
    FURI_LOG_I(TAG, ".....Image size: %ld", infoHeader->biSizeImage);
}*/

/**
 * Frees the memory occupied by the given bit array.
 *
 * @param bitMatrix The text bit array to be freed.
 * @param height The height of the bit array.
 */
void bitmapMatrix_free(bitmapMatrix* bitMatrix) {
    furi_assert(bitMatrix);
    for(uint8_t i = 0; i < bitMatrix->height - 1; i++) {
        free(bitMatrix->array[i]);
    }
    free(bitMatrix->array);
    free(bitMatrix);
}

/**
* @brief Reads BMP file header.
*
* Reads the BMPFileHeader from a bitmap file. If the read byte count is incorrect,
* logs an error and returns 1.
*
* @param bitmapFile Pointer to the file from which to read.
* @param file_header Pointer to store the read file header.
* @return int 0 on success, 1 on failure.
*/
int bmp_read_file_header(File* bitmapFile, BMPFileHeader* file_header) {
    size_t bytes = storage_file_read(bitmapFile, file_header, sizeof(BMPFileHeader));
    if(bytes != sizeof(BMPFileHeader)) {
        FURI_LOG_E(TAG, "Could not read bitmap file's info header.");
        return BMP_ERR_INTERNAL;
    }
    return BMP_OK;
}

/**
* @brief Reads BMP information header.
*
* Attempts to read the BMPInfoHeader from a bitmap file. Logs error and returns 1 if
* the read size does not match BMPInfoHeader's size.
*
* @param bitmapFile Pointer to the file from which to read.
* @param info_header Pointer to store the read information header.
* @return int 0 on success, 1 on failure.
*/
int bmp_read_info_header(File* bitmapFile, BMPInfoHeader* info_header) {
    size_t bytes = storage_file_read(bitmapFile, info_header, sizeof(BMPInfoHeader));
    if(bytes != sizeof(BMPInfoHeader)) {
        FURI_LOG_E(TAG, "Could not read bitmap file's header.");
        return BMP_ERR_INTERNAL;
    }
    return BMP_OK;
}

uint32_t swap_uint32(uint32_t val) {
    return (val << 24) | ((val << 8) & 0x00FF0000) | ((val >> 8) & 0x0000FF00) | (val >> 24);
}

/**
* Reads a 1bpp BMP file into a BMPImage structure.
* Assumes BMPImage is not initialized and will allocate memory for the image data.
* Caller is responsible for freeing BMPImage.data.
*
* @param filename Path to the BMP file.
* @param bitmap Pointer to BMPImage structure to fill.
* @return 0 on success, non-zero on error.
*/
int bmp_load(const char* path, BMPImage* bitmap) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* bitmapFile = storage_file_alloc(storage);
    int err = BMP_OK;
    if(!storage_file_open(bitmapFile, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        err = BMP_ERR_INTERNAL;
        FURI_LOG_E(TAG, "could not open %s", path);
        goto fail;
    }

    storage_file_seek(bitmapFile, 0, true);
    err = bmp_read_file_header(bitmapFile, &(bitmap->file_header));
    if(err != BMP_OK) {
        FURI_LOG_E(TAG, "bmp_read_file_header error");
        goto fail;
    }

    if(bitmap->file_header.bfType != 0x4d42) {
        FURI_LOG_E(TAG, "type must be bitmap");
        err = BMP_ERR_INVALID;
        goto fail;
    }
    err = bmp_read_info_header(bitmapFile, &(bitmap->info_header));
    if(err != BMP_OK) {
        FURI_LOG_E(TAG, "bmp_read_info_header error");
        goto fail;
    }
    //bmp_debug(&(bitmap->file_header), &(bitmap->info_header));

    if(bitmap->file_header.bfSize > BMP_FILE_MAX_SIZE) {
        FURI_LOG_E(TAG, "File too large");
        err = BMP_ERR_TOO_LARGE;
        goto fail;
    }
    // Verify that it is a 1bpp BMP
    if(bitmap->info_header.biBitCount != 1) {
        err = BMP_ERR_BAD_FORMAT;
        goto fail;
    }

    // Calculate the size of the image data
    int rowSize = (bitmap->info_header.biWidth + 7) / 8; // Number of bytes per row
    int paddedRowSize = ((rowSize + 3) / 4) * 4; // Align to 4 bytes
    size_t dataSize = paddedRowSize * bitmap->info_header.biHeight;

    // Allocate memory for image data
    bitmap->data = (unsigned char*)malloc(dataSize);
    if(!bitmap->data) {
        err = BMP_ERR_INTERNAL;
        FURI_LOG_E(TAG, "could not allocate %d o", dataSize);

        goto fail;
    }

    // Seek to the start of the image data
    storage_file_seek(bitmapFile, bitmap->file_header.bfOffBits, true);

    // Read image data
    size_t bytes = storage_file_read(bitmapFile, bitmap->data, dataSize);
    if(bytes != dataSize) {
        free(bitmap->data);
        err = BMP_ERR_INTERNAL;
        FURI_LOG_E(TAG, "could not read %d o (got %d)", dataSize, bytes);
        goto fail;
    }

fail:
    storage_file_close(bitmapFile);
    furi_record_close(RECORD_STORAGE);
    return err;
}

/**
 * Creates a bit array representation of the given image.
 *
 * @param path The path of a bitmap file.
 * @return A pointer to the created image bit array.
 */
bitmapMatrix* bmp_to_bitmapMatrix(const char* path) {
    furi_assert(path);
    // Assuming the bitmap's header have been verified.
    BMPImage image;
    uint16_t row_padded;
    bitmapMatrix* bitMatrix = malloc(sizeof(bitmapMatrix));
    if(!bitMatrix) {
        bitmapMatrix_free(bitMatrix);
        return NULL;
    }

    int err = bmp_load(path, &image);
    if(err != BMP_OK) {
        bitmapMatrix_free(bitMatrix);
        return NULL;
    }

    bitMatrix->width = image.info_header.biWidth;
    bitMatrix->height = image.info_header.biHeight;

    bitMatrix->array = malloc(image.info_header.biHeight * sizeof(uint8_t*));

    row_padded = (image.info_header.biWidth + 7) / 8;
    row_padded = (row_padded + 3) & (~3);

    for(int h = 0; h < image.info_header.biHeight; h++) {
        bitMatrix->array[h] = malloc(image.info_header.biWidth * sizeof(uint8_t));
        bitMatrix->height = h;

        if(!bitMatrix->array[h]) {
            bitmapMatrix_free(bitMatrix);
            return NULL;
        }
        for(int w = 0; w < image.info_header.biWidth; w++) {
            int byteIndex = (image.info_header.biHeight - h - 1) * row_padded + w / 8;
            int bitIndex = 7 - w % 8;
            unsigned char pixel = image.data[byteIndex] & (1 << bitIndex);
            bitMatrix->array[h][w] = pixel ? 0x00 : 0xff;
        }
    }
    bitMatrix->height++;
    free(image.data);

    return bitMatrix;
}

/**
* @brief Loads BMP headers from a file.
*
* Opens a bitmap file, reads its file and information headers, and verifies the file type.
* Logs errors and performs cleanup on failure.
*
* @param path File path to open.
* @param bitmap Pointer to BMPImage to store headers.
* @return int 0 if both headers are successfully loaded, 1 otherwise.
*/
int bmp_load_header(const char* path, BMPImage* bitmap) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* bitmapFile = storage_file_alloc(storage);
    int err = BMP_OK;
    if(!storage_file_open(bitmapFile, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        err = BMP_ERR_INTERNAL;
        goto fail;
    }
    storage_file_seek(bitmapFile, 0, true);
    err = bmp_read_file_header(bitmapFile, &(bitmap->file_header));
    if(err != BMP_OK) goto fail;

    if(bitmap->file_header.bfType != 0x4d42) {
        FURI_LOG_E(TAG, "type must be bitmap");
        err = BMP_ERR_INVALID;
        goto fail;
    }
    err = bmp_read_info_header(bitmapFile, &(bitmap->info_header));
    if(err != BMP_OK) goto fail;

fail:
    storage_file_close(bitmapFile);
    furi_record_close(RECORD_STORAGE);
    return err;
}

int bmp_header_check_1bpp(const char* path) {
    BMPImage image;
    int err = bmp_load_header(path, &image);
    //bmp_debug(&image.file_header, &image.info_header);

    if(err != BMP_OK) {
        return err;
    }
    if((image.info_header.biHeight != 16) || (image.info_header.biWidth > 300)) {
        FURI_LOG_E(
            TAG,
            "%ldx%ld height must be 16 and width <= 32",
            image.info_header.biWidth,
            image.info_header.biHeight);
        return BMP_ERR_BAD_FORMAT;
    }
    if(image.info_header.biBitCount != 1) {
        FURI_LOG_E(TAG, "bits_per_pixel must be 1 %d given", image.info_header.biBitCount);
        return BMP_ERR_BAD_FORMAT;
    }
    if(image.info_header.biCompression != 0) {
        FURI_LOG_E(TAG, "compression must be 0 ,%ld given", image.info_header.biCompression);
        return BMP_ERR_BAD_FORMAT;
    }
    return BMP_OK;
}

int bmp_write(const char* path, bitmapMatrix* bitmap) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* bitmapFile = storage_file_alloc(storage);
    uint8_t width = bitmap->width;
    uint8_t height = bitmap->height;
    int err = BMP_OK;
    if(!storage_file_open(bitmapFile, path, FSAM_WRITE, FSOM_OPEN_ALWAYS)) {
        err = BMP_ERR_FILESYSTEM;
        goto fail;
    }
    size_t rowSize = (width + 7) / 8; // Compute row size in bytes for 1bpp
    size_t paddingSize = (rowSize % 4) ? (4 - (rowSize % 4)) : 0;
    size_t paddedRowSize = rowSize + paddingSize;
    size_t pixelArraySize = paddedRowSize * height;
    size_t paletteSize = 2 * 4; // 2 colors, 4 bytes each

    BMPFileHeader fileHeader;
    BMPInfoHeader infoHeader;

    // Initialize BITMAPFILEHEADER
    fileHeader.bfType = 0x4D42; // 'BM'
    fileHeader.bfSize = 54 + paletteSize + pixelArraySize; // Headers + palette + pixel data
    fileHeader.bfReserved1 = 0;
    fileHeader.bfReserved2 = 0;
    fileHeader.bfOffBits = 54 + paletteSize; // Pixel data offset, accounting for palette

    // Initialize BITMAPINFOHEADER
    infoHeader.biSize = sizeof(BMPInfoHeader);
    infoHeader.biWidth = width;
    infoHeader.biHeight = height;
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 1; // 1bpp
    infoHeader.biCompression = 0; // BI_RGB, no compression
    infoHeader.biSizeImage = pixelArraySize;
    infoHeader.biXPelsPerMeter = 0;
    infoHeader.biYPelsPerMeter = 0;
    infoHeader.biClrUsed = 2; // Using 2 colors
    infoHeader.biClrImportant = 2; // Both colors are important

    // Write headers
    if(storage_file_write(bitmapFile, &fileHeader, sizeof(BMPFileHeader)) !=
       sizeof(BMPFileHeader)) {
        err = BMP_ERR_WRITE;
        goto fail;
    }
    if(storage_file_write(bitmapFile, &infoHeader, sizeof(BMPInfoHeader)) !=
       sizeof(BMPInfoHeader)) {
        err = BMP_ERR_WRITE;
        goto fail;
    }
    //bmp_debug(&fileHeader, &infoHeader);
    // Write the color palette (black and white)
    uint32_t palette[2] = {0x00000000, 0x00FFFFFF}; // Black (0x00RRGGBB), White (0x00RRGGBB)
    if(storage_file_write(bitmapFile, palette, sizeof(palette)) != sizeof(palette)) {
        err = BMP_ERR_WRITE;
        goto fail;
    }
    unsigned char padding[4] = {
        0}; // Assuming the maximum padding size is 4 bytes, which is true for BMP files
    unsigned char* bmpRowData = malloc(rowSize);
    if(!bmpRowData) {
        err = BMP_ERR_MEMORY;
        goto fail;
    }

    for(int h = 0; h < height; h++) {
        memset(bmpRowData, 0, rowSize); // Clear row data
        for(int w = 0; w < width; w++) {
            int byteIndex = w / 8;
            int bitIndex = 7 - (w % 8);
            if(bitmap->array[height - h - 1][w] == 0x00) { // Note the inversion of height
                bmpRowData[byteIndex] |= (1 << bitIndex);
            }

            // Print ASCII representation
            printf("%s", (bitmap->array[height - h - 1][w] == 0xFF) ? "██" : "  ");
        }
        printf("\n"); // Newline after each row

        // Write the row data
        if(storage_file_write(bitmapFile, bmpRowData, rowSize) != rowSize) {
            err = BMP_ERR_WRITE;
            goto fail;
        }
        // Write padding if necessary
        if(paddingSize > 0) {
            if(storage_file_write(bitmapFile, padding, paddingSize) != paddingSize) {
                err = BMP_ERR_WRITE;
                goto fail;
            }
        }
    }

    free(bmpRowData); // Free the allocated memory for bmpRowData

fail:

    storage_file_close(bitmapFile);
    furi_record_close(RECORD_STORAGE);
    return err;
}
