#include "ha_flasher.h"
#include "ha_esp_port.h"

#include <storage/storage.h>
#include "esp_loader.h" // vendored esp_serial_flasher (Apache-2.0)

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

// `stage` reports which call failed, so a flash error says where it died rather
// than just handing back a number.
static esp_loader_error_t flash_image(
    Storage* storage,
    const FlashImage* img,
    HaFlashProgress cb,
    void* ctx,
    uint8_t idx,
    uint8_t cnt,
    bool rom,
    const char** stage) {
    File* f = storage_file_alloc(storage);
    esp_loader_error_t err = ESP_LOADER_ERROR_FAIL;
    const char* name = basename_of(img->path);
    if(stage) *stage = "open";
    if(storage_file_open(f, img->path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        uint64_t size = storage_file_size(f);
        // flash_start is the first command after connect and, on the ROM path,
        // the one that also detects flash size and sets SPI params. A single
        // timeout there is often transient, so give it a couple of tries —
        // writes already retry inside the library, this one did not.
        if(stage) *stage = "start";
        for(int attempt = 0; attempt < 3; attempt++) {
            err = esp_loader_flash_start(img->offset, (uint32_t)size, FLASH_BLOCK);
            if(err == ESP_LOADER_SUCCESS || err != ESP_LOADER_ERROR_TIMEOUT) break;
        }
        if(err == ESP_LOADER_SUCCESS) {
            if(stage) *stage = "write";
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
            // FLASH_END is skipped entirely on the ROM path: the C5 answers it
            // with a failure status in both forms (stay-in-loader and reboot).
            // Nothing is lost by dropping it — every FLASH_DATA block is already
            // committed, and the next image's FLASH_BEGIN re-arms the sequence.
            // The reboot it would have done is a DTR pulse in ha_flasher_run.
            // The stub path is untouched, as it has always worked on S2/WROOM.
            if(err == ESP_LOADER_SUCCESS && !rom) {
                bool last = (idx + 1 == cnt);
                if(stage) *stage = "finish";
                err = esp_loader_flash_finish(last);
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
    bool auto_boot,
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
    // Let the port abort its waits as soon as Back is pressed, so the scene's
    // on_exit join returns promptly instead of sitting through a library timeout.
    ha_esp_port_set_cancel(cancel);

    // Pulse the reset lines once (not per attempt) when the board supports it.
    if(auto_boot && !(cancel && *cancel)) ha_esp_port_enter_bootloader();

    // Poll until the board answers in download mode. The stub loader is used
    // (esptool's default) — the bare ROM path fails flashing the ESP32-S2.
    bool connected = false;
    bool used_rom = false; // connected over the plain ROM protocol, not the stub
    while(!(cancel && *cancel)) {
        ha_esp_port_flush();
        esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
        args.trials = 2; // short attempts keep the poll responsive
        esp_loader_error_t err_connect = esp_loader_connect_with_stub(&args);
        if(err_connect == ESP_LOADER_SUCCESS) {
            connected = true;
            break;
        }
        // The library refuses the stub for chips it has no stub blob for (the
        // C5 and P4), but only after it has synced and read the chip id. Those
        // flash fine over the plain ROM protocol — slower, no stub upload — so
        // retry that way rather than sitting in the poll forever.
        if(err_connect == ESP_LOADER_ERROR_UNSUPPORTED_CHIP && !(cancel && *cancel)) {
            ha_esp_port_flush();
            esp_loader_connect_args_t rom_args = ESP_LOADER_CONNECT_DEFAULT();
            rom_args.trials = 2;
            if(esp_loader_connect(&rom_args) == ESP_LOADER_SUCCESS) {
                connected = true;
                used_rom = true;
                break;
            }
        }
    }
    if(!connected) {
        snprintf(err, err_size, "Cancelled");
        goto release;
    }

    if(on_connected) on_connected(ctx);

    ok = true;
    for(int i = 0; i < count; i++) {
        const char* stage = "?";
        esp_loader_error_t e = flash_image(
            storage, &images[i], cb, ctx, (uint8_t)i, (uint8_t)count, used_rom, &stage);
        if(e != ESP_LOADER_SUCCESS) {
            // Name the failing stage and the connect route: the ROM path runs
            // flash-size detection and SPI setup that the stub path never does,
            // so knowing which of the two was used narrows a failure a lot.
            snprintf(
                err,
                err_size,
                "%s img%d %s\nerr %d (%s)",
                stage,
                i + 1,
                used_rom ? "ROM" : "stub",
                (int)e,
                basename_of(images[i].path));
            ok = false;
            break;
        }
    }
    // The ROM path never sent the FLASH_END that reboots the chip, so do it in
    // hardware. Harmless on boards whose reset isn't on these pins; the done
    // screen still tells the user to tap RESET.
    if(ok && used_rom) ha_esp_port_reset_target();

release:
    ha_esp_port_set_cancel(NULL);
    ha_esp_port_stop();
    ha_uart_resume(uart);

cleanup:
    furi_string_free(manifest);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
