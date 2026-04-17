#include <furi.h>
#include <storage/storage.h>
#include <toolbox/path.h>
#include <toolbox/compress.h>
#include "png_write.h"
#include "pngle.h"

#include "file_utils.h"

#define MAX_FRAME_BUFFER_SIZE (128 * 64)

// Determines filename format string for single vs multiple frames
const char* get_filename_format_str(IEIcon* icon) {
    const char* const single_fn_fmt = "/data/%s";
    const char* const multi_fn_fmt = "/data/%s_%0*d";
    bool multi_frame = icon->frame_count > 1;
    return multi_frame ? multi_fn_fmt : single_fn_fmt;
}

// Returns the num digits of the frame count, used for printf fmt string
int get_frame_num_fmt_width(IEIcon* icon) {
    return (int)floor(log10(icon->frame_count)) + 1;
}

// *******************************************************************************
// * XBM
// *******************************************************************************
// creates a packed XBM binary buffer
uint8_t* xbm_encode(const uint8_t* data, size_t width, size_t height, size_t* size) {
    int bytes_per_row = (width + 7) / 8;
    *size = bytes_per_row * height;
    uint8_t* xbm = malloc(*size);
    memset(xbm, 0, *size);
    // const Frame* f = ie_icon_get_frame(icon, frame);
    for(size_t y = 0; y < height; ++y) {
        for(int bx = 0; bx < bytes_per_row; bx++) {
            uint8_t byte = 0;
            for(int bit = 0; bit < 8; bit++) {
                size_t x = bx * 8 + bit;
                if(x < width) {
                    if(data[y * width + x]) {
                        byte |= (1 << bit);
                    }
                }
            }
            xbm[y * bytes_per_row + bx] = byte;
        }
    }
    return xbm;
}
uint8_t* xbm_encode_from_icon(IEIcon* icon, size_t frame, size_t* size) {
    const Frame* iframe = ie_icon_get_frame(icon, frame);
    uint8_t* xbm = xbm_encode(iframe->data, icon->width, icon->height, size);
    return xbm;
}

// unpacks XBM binary buffer
uint8_t* xbm_decode(const uint8_t* xbm, int32_t w, int32_t h) {
    int num_pixels = w * h;
    int bytes_per_row = (w + 7) / 8; // XBM pads rows to full bytes
    int byte_index = 0;
    uint8_t* data = malloc(num_pixels * sizeof(uint8_t));

    FURI_LOG_I("BMX", "decoding: bytes_per_row: %d", bytes_per_row);
    for(int y = 0; y < h; y++) {
        for(int x_byte = 0; x_byte < bytes_per_row; x_byte++) {
            uint8_t byte = xbm[y * bytes_per_row + x_byte];
            // FURI_LOG_I("dec", "%02x", byte);
            for(int bit = 0; bit < 8; bit++) {
                // check if we've processed all bits for this row
                int x = x_byte * 8 + bit;
                if(x >= w) break; // and then ignore padding bits
                uint8_t pixel = (byte >> bit) & 0x01;
                data[byte_index++] = pixel;
            }
        }
    }
    return data;
}

IEIcon* xbm_decode_to_icon(const uint8_t* xbm, int32_t w, int32_t h) {
    IEIcon* icon = ie_icon_alloc(false);
    uint8_t* data = xbm_decode(xbm, w, h);
    ie_icon_reset(icon, w, h, data);
    return icon;
}

// Write the XBM bytes to the log for debugging
void log_xbm_data(const uint8_t* data, size_t size) {
    FuriString* tmp = furi_string_alloc_set_str("");
    size_t i = 0;
    while(i < size) {
        furi_string_cat_printf(tmp, " %02x", data[i]);
        ++i;
        if(i % 8 == 0) {
            furi_string_cat_str(tmp, " ");
        }
        if(i % 16 == 0) {
            FURI_LOG_I("XBM", furi_string_get_cstr(tmp));
            furi_string_set_str(tmp, "");
        }
    }
    if(furi_string_size(tmp)) {
        FURI_LOG_I("XBM", furi_string_get_cstr(tmp));
    }
    furi_string_free(tmp);
}

// Saves the icon in a "pure" XBM format (Not really sure if this useful :|)
bool xbm_file_save(IEIcon* icon) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    const char* fn_fmt = get_filename_format_str(icon);
    int pad_width = get_frame_num_fmt_width(icon);

    bool success = true;
    for(size_t f = 0; f < icon->frame_count; f++) {
        FuriString* filename =
            furi_string_alloc_printf(fn_fmt, furi_string_get_cstr(icon->name), pad_width, f);
        FuriString* base_name = furi_string_alloc_set(filename);
        furi_string_cat_str(filename, ".xbm");

        File* xbm_file = storage_file_alloc(storage);
        if(storage_file_open(
               xbm_file, furi_string_get_cstr(filename), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            const char* name = furi_string_get_cstr(base_name);

            FuriString* tmp = furi_string_alloc();
            // dimensions
            furi_string_printf(tmp, "#define %s_width %d\n", name, icon->width);
            // FURI_LOG_I("XBM", "%s", furi_string_get_cstr(tmp));
            storage_file_write(xbm_file, furi_string_get_cstr(tmp), furi_string_size(tmp));
            furi_string_printf(tmp, "#define %s_height %d\n", name, icon->height);
            // FURI_LOG_I("XBM", "%s", furi_string_get_cstr(tmp));
            storage_file_write(xbm_file, furi_string_get_cstr(tmp), furi_string_size(tmp));

            // prefix
            furi_string_printf(tmp, "static unsigned char %s_bits[] = {\n", name);
            // FURI_LOG_I("XBM", "%s", furi_string_get_cstr(tmp));
            storage_file_write(xbm_file, furi_string_get_cstr(tmp), furi_string_size(tmp));

            // raw data
            furi_string_set_str(tmp, "");
            size_t size;
            // uint8_t* data = convert_bytes_to_xbm_bits(icon, &size, f);
            uint8_t* data = xbm_encode_from_icon(icon, f, &size);
            for(size_t i = 0; i < size; ++i) {
                furi_string_cat_printf(tmp, "0x%02x", data[i]);
                if(i < size - 1) {
                    furi_string_cat_str(tmp, ",");
                }
            }
            // FURI_LOG_I("XBM", "%s", furi_string_get_cstr(tmp));
            storage_file_write(xbm_file, furi_string_get_cstr(tmp), furi_string_size(tmp));

            // suffix
            furi_string_set_str(tmp, " };");
            // FURI_LOG_I("XBM", "%s", furi_string_get_cstr(tmp));
            storage_file_write(xbm_file, furi_string_get_cstr(tmp), furi_string_size(tmp));

            free(data);

            furi_string_free(tmp);

            storage_file_close(xbm_file);
        } else {
            success = false;
            FURI_LOG_E(
                "IE", "Failed to open file for saving: %s", storage_file_get_error_desc(xbm_file));
        }

        storage_file_free(xbm_file);
        furi_string_free(filename);
        furi_string_free(base_name);
    }
    furi_record_close(RECORD_STORAGE);
    return success;
}

// *******************************************************************************
// * PNG
// *******************************************************************************

// Generates PNG byte buffer of given frame with zero compression
bool png_file_generate_binary(
    IEIcon* icon,
    size_t frame_num,
    uint8_t** png_file_data,
    size_t* png_file_size) {
    bool success = true;
    Frame* frame = ie_icon_get_frame(icon, frame_num);
    image_t* image = image_new(icon->width, icon->height);
    for(size_t i = 0; i < icon->width * icon->height; i++) {
        bool bit_on = frame->data[i];
        if(bit_on) {
            image->pixels[i] = (rgba_t){.r = 0, .g = 0, .b = 0, .a = 255};
        } else {
            image->pixels[i] = (rgba_t){.r = 255, .g = 255, .b = 255, .a = 0};
        }
    }
    image->save_to_bytes = true; // tell png_write to save as byte buffer
    *png_file_data = malloc(1024 * sizeof(uint8_t));
    image->bytes = *png_file_data;
    image->bytes_len = 1024;
    image_save(image);
    *png_file_data = image->bytes; // need to reassign in case we realloc'd
    *png_file_size = image->size;

    image_free(image);
    return success;
}

// Writes the given frame to disk
bool png_file_save_frame(IEIcon* icon, int f, FuriString* filename, Storage* storage) {
    // Whether the image is an animation or not, just save one frame
    FURI_LOG_I("PNG", "Saving %s", furi_string_get_cstr(filename));
    File* png_file = storage_file_alloc(storage);
    Frame* frame = ie_icon_get_frame(icon, f);
    bool success = true;
    if(storage_file_open(
           png_file, furi_string_get_cstr(filename), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        image_t* image = image_new(icon->width, icon->height);
        for(size_t i = 0; i < icon->width * icon->height; i++) {
            bool bit_on = frame->data[i];
            if(bit_on) {
                image->pixels[i] = (rgba_t){.r = 0, .g = 0, .b = 0, .a = 255};
            } else {
                image->pixels[i] = (rgba_t){.r = 255, .g = 255, .b = 255, .a = 0};
            }
        }
        image->file = png_file;
        image_save(image);
        image_free(image);

    } else {
        success = false;
        FURI_LOG_E(
            "IE", "Failed to open file for saving: %s", storage_file_get_error_desc(png_file));
    }
    storage_file_close(png_file);
    storage_file_free(png_file);
    return success;
}

// Saves to one or more PNG files with zero compression. For animations, a new
// directory with the image name will be created and the frames stored within.
// Addiitonally, a file called "frame_rate" will be written to that directory that
// contains the int-value of the animation frame rate
bool png_file_save(IEIcon* icon, bool current_frame_only) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    bool success = true;

    if(!current_frame_only && icon->frame_count == 0) {
        assert(false);
    }

    const char* icon_name = furi_string_get_cstr(icon->name);

    if(current_frame_only) {
        // Whether the image is an animation or not, just save one frame
        FuriString* filename = furi_string_alloc_printf("/data/%s.png", icon_name);
        FURI_LOG_I("PNG", "Saving %s", furi_string_get_cstr(filename));
        png_file_save_frame(icon, icon->current_frame, filename, storage);
        furi_string_free(filename);
    } else {
        do {
            // If we are saving a PNG animation, we need a directory to store the frames
            // If that directory already exists, delete it and its contents
            FuriString* dir = furi_string_alloc_printf("/data/%s", icon_name);
            storage_simply_remove_recursive(storage, furi_string_get_cstr(dir));

            // (Re-)make the directory
            if(!storage_simply_mkdir(storage, furi_string_get_cstr(dir))) {
                success = false;
                furi_string_free(dir);
                break;
            }
            furi_string_free(dir);

            // Write all frames as A_name_num.png in the directory
            for(size_t f = 0; f < icon->frame_count; f++) {
                FuriString* png_name =
                    furi_string_alloc_printf("/data/%s/A_%s_%d.png", icon_name, icon_name, f);
                FURI_LOG_I("PNG", "Saving %s", furi_string_get_cstr(png_name));
                png_file_save_frame(icon, f, png_name, storage);
                furi_string_free(png_name);
            }

            // Write the frame_rate file
            FuriString* fr_filename = furi_string_alloc_printf("/data/%s/frame_rate", icon_name);
            File* fr_file = storage_file_alloc(storage);
            if(storage_file_open(
                   fr_file, furi_string_get_cstr(fr_filename), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
                FuriString* rate = furi_string_alloc_printf("%d", icon->frame_rate);
                storage_file_write(fr_file, furi_string_get_cstr(rate), furi_string_size(rate));
                furi_string_free(rate);
                storage_file_close(fr_file);
            }
            storage_file_free(fr_file);
            furi_string_free(fr_filename);
        } while(false);
    }
    furi_record_close(RECORD_STORAGE);
    return success;
}

void png_init_cb(pngle_t* pngle, uint32_t width, uint32_t height) {
    IEIcon* icon = pngle_get_user_data(pngle);
    assert(icon->frames == NULL);
    assert(width > 0);
    assert(height > 0);
    ie_icon_reset(icon, width, height, NULL);
}

void png_draw_cb(pngle_t* pngle, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t rgba[4]) {
    UNUSED(pngle);
    UNUSED(w);
    UNUSED(h);
    IEIcon* icon = pngle_get_user_data(pngle);
    // FURI_LOG_W(
    //     "png",
    //     "on_draw: w,h = %ld x %ld, x,y = %ld, %ld = %d %d %d",
    //     pngle_get_width(pngle),
    //     pngle_get_height(pngle),
    //     x,
    //     y,
    //     rgba[0],
    //     rgba[1],
    //     rgba[2]);
    if(rgba[0] == 0 && rgba[1] == 0 && rgba[2] == 0) {
        // FURI_LOG_W("png", "setting pixel: %ld, %ld", x, y);
        icon->frames->data[y * pngle_get_width(pngle) + x] = 0x1;
    } else {
        icon->frames->data[y * pngle_get_width(pngle) + x] = 0x0;
    }
}
IEIcon* png_file_open(const char* icon_path) {
    FURI_LOG_I("PNG", "png_file_open: %s", icon_path);
    Storage* storage = furi_record_open(RECORD_STORAGE);

    File* png_file = storage_file_alloc(storage);

    IEIcon* icon = ie_icon_alloc(false);
    bool success = false;
    do {
        if(!storage_file_open(png_file, icon_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_E(
                "PNG", "Failed to open %s: %s", icon_path, storage_file_get_error_desc(png_file));
            break;
        }

        pngle_t* png = pngle_new();
        pngle_set_user_data(png, icon);
        pngle_set_init_callback(png, png_init_cb);
        pngle_set_draw_callback(png, png_draw_cb);

        char buffer[64] = {0};
        size_t remain = 0;
        size_t len;
        while((len = storage_file_read(png_file, buffer + remain, sizeof(buffer) - remain)) > 0) {
            int fed = pngle_feed(png, buffer, remain + len);
            if(fed < 0) {
                FURI_LOG_E("PNG", "pngle error: %s", pngle_error(png));
            }
            remain = remain + len - fed;
            if(remain > 0) {
                memmove(buffer, buffer + fed, remain);
            }
        }

        pngle_destroy(png);
        success = true;
    } while(false);

    if(!success) {
        FURI_LOG_E("PNG", "Couldn't open png file");
        ie_icon_free(icon);
        icon = NULL;
    } else {
        path_extract_filename_no_ext(icon_path, icon->name);

        FURI_LOG_I("PNG", "Opened png file");
        log_xbm_data(icon->frames->data, icon->width * icon->height);
    }

    storage_file_close(png_file);
    storage_file_free(png_file);

    furi_record_close(RECORD_STORAGE);
    return icon;
}

// *******************************************************************************
// * BMX - A compressed XBM format with width/height prepended
// *******************************************************************************

bool bmx_file_generate_binary(
    IEIcon* icon,
    size_t frame,
    uint8_t** bmx_file_data,
    size_t* bmx_file_size) {
    // create the XBM bytes
    size_t size;
    // uint8_t* xbm = convert_bytes_to_xbm_bits(icon, &size, 0);
    uint8_t* xbm = xbm_encode_from_icon(icon, frame, &size);

    // log_xbm_data(xbm, size);

    // then compress (heatshrink) them
    uint8_t* encoded_data = malloc(MAX_FRAME_BUFFER_SIZE); // is this too big?
    size_t encoded_size = 0;
    Compress* compress =
        compress_alloc(CompressTypeHeatshrink, &compress_config_heatshrink_default);
    bool encode_success =
        compress_encode(compress, xbm, size, encoded_data, MAX_FRAME_BUFFER_SIZE, &encoded_size);
    compress_free(compress);
    free(xbm);

    if(!encode_success) {
        FURI_LOG_E("BMX", "Failed to encode icon!");
        free(encoded_data);
        return false;
    }

    // generate byte data. w/h must be int32_t
    int32_t w = icon->width;
    int32_t h = icon->height;
    *bmx_file_size = sizeof(w) + sizeof(h) + encoded_size;
    *bmx_file_data = malloc(*bmx_file_size);
    memcpy(*bmx_file_data, &w, sizeof(w));
    memcpy(*bmx_file_data + sizeof(w), &h, sizeof(h));
    memcpy(*bmx_file_data + sizeof(w) + sizeof(h), encoded_data, encoded_size);

    free(encoded_data);

    return true;
}

// Currently only saving Frame 0
bool bmx_file_save(IEIcon* icon) {
    uint8_t* bmx_file_data = NULL;
    size_t bmx_file_size;

    if(!bmx_file_generate_binary(icon, 0, &bmx_file_data, &bmx_file_size)) {
        FURI_LOG_E("BMX", "Failed to generate BMX file data");
        return false;
    }

    // write file
    bool success = false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* bmx_file = storage_file_alloc(storage);
    FuriString* filename =
        furi_string_alloc_printf("/data/%s.bmx", furi_string_get_cstr(icon->name));
    if(storage_file_open(
           bmx_file, furi_string_get_cstr(filename), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(bmx_file, bmx_file_data, bmx_file_size);
        success = true;
    } else {
        FURI_LOG_E(
            "BMX", "Couldn't open BMX file for save: %s", storage_file_get_error_desc(bmx_file));
    }
    free(bmx_file_data);

    storage_file_close(bmx_file);
    storage_file_free(bmx_file);

    furi_string_free(filename);
    furi_record_close(RECORD_STORAGE);

    return success;
}

IEIcon* bmx_file_open(const char* icon_path) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* bmx_file = storage_file_alloc(storage);
    IEIcon* icon = NULL;

    do {
        if(!storage_file_open(bmx_file, icon_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            FURI_LOG_E(
                "BMX", "Failed to open %s: %s", icon_path, storage_file_get_error_desc(bmx_file));
            break;
        }
        uint64_t bmx_file_size = storage_file_size(bmx_file);
        if(bmx_file_size == 0) {
            FURI_LOG_E("BMX", "File size is zero!");
            break;
        }
        int32_t w, h;
        if(storage_file_read(bmx_file, &w, sizeof(w)) != sizeof(w)) {
            FURI_LOG_E("BMX", "BMX image width can't be read");
            break;
        }
        if(storage_file_read(bmx_file, &h, sizeof(h)) != sizeof(h)) {
            FURI_LOG_E("BMX", "BMX image height can't be read");
            break;
        }
        uint8_t* encoded_data = malloc(bmx_file_size - 2 * sizeof(int32_t));
        size_t bytes_read =
            storage_file_read(bmx_file, encoded_data, bmx_file_size - 2 * sizeof(int32_t));
        if(bytes_read != (bmx_file_size - 2 * sizeof(int32_t))) {
            FURI_LOG_E(
                "BMX", "Bytes read (%d) doesn't match file size (%lld)", bytes_read, bmx_file_size);
            free(encoded_data);
            break;
        }

        uint8_t* decoded_data = malloc(MAX_FRAME_BUFFER_SIZE);
        size_t decoded_data_size = 0;
        Compress* compress =
            compress_alloc(CompressTypeHeatshrink, &compress_config_heatshrink_default);
        bool decode_success = compress_decode(
            compress,
            encoded_data,
            bytes_read,
            decoded_data,
            MAX_FRAME_BUFFER_SIZE,
            &decoded_data_size);
        free(encoded_data);
        compress_free(compress);
        if(!decode_success) {
            FURI_LOG_E("BMX", "Failed to decode BMX data");
            free(decoded_data);
            break;
        }

        // turn the decoded xbm bytes into an icon
        icon = xbm_decode_to_icon(decoded_data, w, h);

        path_extract_filename_no_ext(icon_path, icon->name);
        free(decoded_data);
    } while(false);

    storage_file_close(bmx_file);
    storage_file_free(bmx_file);

    furi_record_close(RECORD_STORAGE);

    return icon;
}

// *******************************************************************************
// * .C source code - Flipper specific
// *******************************************************************************

// Generates the .C source code (whether single frame or multi frame) as one
// Furi string. Used by both C file saving and USB send. This is a compressed
// XBM format, not a true XBM format
FuriString* c_file_generate(IEIcon* icon, bool current_frame_only) {
    const char* name = furi_string_get_cstr(icon->name);

    FuriString* tmp = furi_string_alloc();

    int pad_width = get_frame_num_fmt_width(icon);

    // Include the Icon header
    furi_string_cat_printf(tmp, "#include <gui/icon_i.h>\n");

    Compress* compress =
        compress_alloc(CompressTypeHeatshrink, &compress_config_heatshrink_default);
    if(current_frame_only) {
        // Generate the variable for the current frame
        furi_string_cat_printf(tmp, "const uint8_t _I_%s_0[] = {", name);

        // create the XBM bytes
        size_t size;
        uint8_t* xbm = xbm_encode_from_icon(icon, icon->current_frame, &size);
        // then compress (heatshrink) them
        uint8_t* encoded_data = malloc(MAX_FRAME_BUFFER_SIZE); // is this too big?
        size_t encoded_size = 0;
        compress_encode(compress, xbm, size, encoded_data, MAX_FRAME_BUFFER_SIZE, &encoded_size);
        free(xbm);

        // convert to hex
        for(size_t i = 0; i < encoded_size; ++i) {
            furi_string_cat_printf(tmp, " 0x%02x", encoded_data[i]);
            if(i < encoded_size - 1) {
                furi_string_cat_str(tmp, ",");
            }
        }
        free(encoded_data);
        furi_string_cat_printf(tmp, "};\n");
    } else {
        // Generate each frame variable
        for(size_t f = 0; f < icon->frame_count; f++) {
            // frame 0 data start
            furi_string_cat_printf(tmp, "const uint8_t _I_%s_%0*d[] = {", name, pad_width, f);

            // create the XBM bytes
            size_t size;
            uint8_t* xbm = xbm_encode_from_icon(icon, f, &size);
            // then compress (heatshrink) them
            uint8_t* encoded_data = malloc(MAX_FRAME_BUFFER_SIZE); // is this too big?
            size_t encoded_size = 0;
            compress_encode(
                compress, xbm, size, encoded_data, MAX_FRAME_BUFFER_SIZE, &encoded_size);
            free(xbm);

            // convert to hex
            for(size_t i = 0; i < encoded_size; ++i) {
                furi_string_cat_printf(tmp, " 0x%02x", encoded_data[i]);
                if(i < encoded_size - 1) {
                    furi_string_cat_str(tmp, ",");
                }
            }
            free(encoded_data);
            furi_string_cat_printf(tmp, "};\n");
        }
    }
    compress_free(compress);

    if(current_frame_only) {
        // Generate the frame list - a list of one
        furi_string_cat_printf(tmp, "const uint8_t* const _I_%s[] = {_I_%s_0};\n", name, name);
    } else {
        // Generate the frame list
        furi_string_cat_printf(tmp, "const uint8_t* const _I_%s[] = {", name);
        for(size_t f = 0; f < icon->frame_count; f++) {
            furi_string_cat_printf(tmp, "_I_%s_%0*d", name, pad_width, f);
            if(f < icon->frame_count - 1) {
                furi_string_cat_printf(tmp, ",");
            }
        }
        furi_string_cat_printf(tmp, "};\n");
    }

    // Generate the main icon definition
    furi_string_cat_printf(
        tmp,
        "const Icon I_%s = {.width=%d,.height=%d,.frame_count=%d,.frame_rate=%d,.frames=_I_%s};\n",
        name,
        icon->width,
        icon->height,
        current_frame_only ? 1 : icon->frame_count,
        current_frame_only ? 0 : icon->frame_rate,
        name);
    // FURI_LOG_I("C", "\n\n%s\n", furi_string_get_cstr(tmp));
    return tmp;
}

// Saves the .C source code to a file of the icon (whether single frame or animation)
bool c_file_save(IEIcon* icon, bool current_frame_only) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    const char* fn_fmt = "/data/%s.c";
    bool success = true;
    FuriString* filename = furi_string_alloc_printf(fn_fmt, furi_string_get_cstr(icon->name));

    File* c_file = storage_file_alloc(storage);
    if(storage_file_open(c_file, furi_string_get_cstr(filename), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        FuriString* tmp = c_file_generate(icon, current_frame_only);
        storage_file_write(c_file, furi_string_get_cstr(tmp), furi_string_size(tmp));
        FURI_LOG_I("C", "%s", furi_string_get_cstr(tmp));
        furi_string_free(tmp);

        storage_file_close(c_file);
    } else {
        success = false;
        FURI_LOG_E("IE", "Couldn't save C file");
    }

    storage_file_free(c_file);
    furi_string_free(filename);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// *******************************************************************************
// * Generator methods - Frame 0 only
// *******************************************************************************

FuriString* generate_text_from_binary(uint8_t* data, size_t size) {
    const int bytes_per_line = 64;
    FuriString* text = furi_string_alloc();
    for(size_t i = 0; i < size; ++i) {
        furi_string_cat_printf(text, "%02x", data[i]);
        if((i + 1) % bytes_per_line == 0) {
            furi_string_cat_str(text, "\n");
        }
    }
    if(size % bytes_per_line != 0) {
        furi_string_cat_str(text, "\n");
    }
    return text;
}

#define TEXT_FILE_GENERATOR_FRAME(TYPE)                                  \
    FuriString* TYPE##_file_generate_frame(IEIcon* icon, size_t frame) { \
        uint8_t* data;                                                   \
        size_t size;                                                     \
        TYPE##_file_generate_binary(icon, frame, &data, &size);          \
        FuriString* text = generate_text_from_binary(data, size);        \
        free(data);                                                      \
        return text;                                                     \
    }

TEXT_FILE_GENERATOR_FRAME(png)
TEXT_FILE_GENERATOR_FRAME(bmx)
