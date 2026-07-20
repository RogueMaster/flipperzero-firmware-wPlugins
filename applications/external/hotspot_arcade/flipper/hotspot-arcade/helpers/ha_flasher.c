#include "ha_flasher.h"
#include "ha_esp_port.h"

#include <storage/storage.h>
#include "esp_loader.h" // vendored esp-serial-flasher (Apache-2.0)

// The ESP ROM bootloader always comes up at 115200; we connect there (raising the
// rate after connect is possible but not worth the extra failure mode here). The
// app's own link runs at HA_UART_BAUD — ha_uart_resume() restores it afterwards.
#define FLASH_BAUD       (115200u)
#define FLASH_BLOCK      (1024u)
#define FLASH_MAX_IMAGES (8)
#define FLASH_PATH_MAX   (256)
#define FLASH_DIR_MAX    (160) // dir + '/' + name must fit in FLASH_PATH_MAX
#define FLASH_NAME_MAX   (92)

typedef struct {
    uint32_t offset;
    char path[FLASH_PATH_MAX];
} FlashImage;

static bool read_manifest(Storage* storage, const char* path, FuriString* out) {
    File* f = storage_file_alloc(storage);
    furi_string_reset(out);
    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint8_t buf[128];
        size_t rd;
        while((rd = storage_file_read(f, buf, sizeof(buf))) > 0) {
            for(size_t i = 0; i < rd; i++)
                furi_string_push_back(out, (char)buf[i]);
            if(furi_string_size(out) > 4096) break; // manifests are tiny
        }
    }
    storage_file_close(f);
    storage_file_free(f);
    return furi_string_size(out) > 0;
}

static void dir_of(const char* path, char* out, size_t n) {
    const char* slash = strrchr(path, '/');
    size_t len = slash ? (size_t)(slash - path) : 0;
    if(len >= n) len = n - 1;
    memcpy(out, path, len);
    out[len] = '\0';
}

// Parse "<offset> <filename>" lines ('#' comments allowed). Returns image count or -1.
static int
    parse_manifest(const char* manifest_path, const char* text, FlashImage* images, int max) {
    char dir[FLASH_DIR_MAX];
    dir_of(manifest_path, dir, sizeof(dir));
    int count = 0;
    const char* p = text;
    while(*p && count < max) {
        while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            p++;
        if(!*p) break;
        if(*p == '#') {
            while(*p && *p != '\n')
                p++;
            continue;
        }
        char* end;
        uint32_t off = (uint32_t)strtoul(p, &end, 0);
        if(end == p) return -1;
        p = end;
        while(*p == ' ' || *p == '\t')
            p++;
        char fn[FLASH_NAME_MAX];
        int fi = 0;
        while(*p && *p != '\r' && *p != '\n' && fi < (int)sizeof(fn) - 1)
            fn[fi++] = *p++;
        while(fi > 0 && (fn[fi - 1] == ' ' || fn[fi - 1] == '\t'))
            fi--;
        fn[fi] = '\0';
        if(fi == 0) return -1;
        images[count].offset = off;
        snprintf(images[count].path, FLASH_PATH_MAX, "%s/%s", dir, fn);
        count++;
    }
    return count;
}

static const char* basename_of(const char* path) {
    const char* slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static esp_loader_error_t flash_image(
    Storage* storage,
    const FlashImage* img,
    HaFlashProgress cb,
    void* ctx,
    uint8_t idx,
    uint8_t cnt) {
    File* f = storage_file_alloc(storage);
    esp_loader_error_t err = ESP_LOADER_ERROR_FAIL;
    const char* name = basename_of(img->path);
    if(storage_file_open(f, img->path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(f);
        err = esp_loader_flash_start(img->offset, (uint32_t)size, FLASH_BLOCK);
        if(err == ESP_LOADER_SUCCESS) {
            static uint8_t buf[FLASH_BLOCK]; // static: keep it off the worker stack
            uint32_t done = 0;
            uint8_t lastpct = 255;
            size_t rd;
            while((rd = storage_file_read(f, buf, FLASH_BLOCK)) > 0) {
                err = esp_loader_flash_write(buf, rd);
                if(err != ESP_LOADER_SUCCESS) break;
                done += rd;
                uint8_t pct = size ? (uint8_t)((uint64_t)done * 100 / size) : 100;
                if(pct != lastpct) {
                    lastpct = pct;
                    if(cb) cb(ctx, idx, cnt, pct, name);
                }
            }
            if(err == ESP_LOADER_SUCCESS) {
                bool last = (idx + 1 == cnt);
                err = esp_loader_flash_finish(last); // reboot the ESP after the final image
            }
        }
    }
    storage_file_close(f);
    storage_file_free(f);
    return err;
}

bool ha_flasher_run(
    HaUart* uart,
    const char* manifest_path,
    HaFlashProgress cb,
    void* ctx,
    volatile bool* cancel,
    void (*on_connected)(void* ctx),
    char* err,
    size_t err_size) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FuriString* manifest = furi_string_alloc();
    static FlashImage images[FLASH_MAX_IMAGES]; // static: keep ~2 KB off the worker stack
    bool ok = false;
    int count = 0;

    if(!read_manifest(storage, manifest_path, manifest)) {
        snprintf(err, err_size, "Can't read manifest");
        goto cleanup;
    }
    count =
        parse_manifest(manifest_path, furi_string_get_cstr(manifest), images, FLASH_MAX_IMAGES);
    if(count <= 0) {
        snprintf(err, err_size, "Bad manifest");
        goto cleanup;
    }

    // Take the serial line from the session worker, at the flash baud.
    ha_uart_suspend(uart);
    ha_esp_port_start(ha_uart_serial(uart), FLASH_BAUD);

    // Poll until the board answers in download mode. The stub loader is used
    // (esptool's default) — the bare ROM path fails flashing the ESP32-S2.
    bool connected = false;
    while(!(cancel && *cancel)) {
        ha_esp_port_flush();
        esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
        args.trials = 2; // short attempts keep the poll responsive
        if(esp_loader_connect_with_stub(&args) == ESP_LOADER_SUCCESS) {
            connected = true;
            break;
        }
    }
    if(!connected) {
        snprintf(err, err_size, "Cancelled");
        goto release;
    }

    if(on_connected) on_connected(ctx);

    ok = true;
    for(int i = 0; i < count; i++) {
        esp_loader_error_t e =
            flash_image(storage, &images[i], cb, ctx, (uint8_t)i, (uint8_t)count);
        if(e != ESP_LOADER_SUCCESS) {
            snprintf(err, err_size, "Failed: %s (%d)", basename_of(images[i].path), (int)e);
            ok = false;
            break;
        }
    }

release:
    ha_esp_port_stop();
    ha_uart_resume(uart);

cleanup:
    furi_string_free(manifest);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
