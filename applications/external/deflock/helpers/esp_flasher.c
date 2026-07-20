// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 ReconGrunt and FlipDeFlock contributors
#include "esp_flasher.h"

#include <furi.h>
#include <furi_hal.h>
#include <expansion/expansion.h>
#include <string.h>
#include <stdarg.h>

#include <esp_loader.h>
#include <esp_loader_io.h>

#define FLASH_BAUD   115200
#define FLASH_RX_BUF 2048 /* UART RX stream buffer; reads drain it continuously */
#define FLASH_BLOCK  1024 /* flash_write payload */
#define BACKUP_CHUNK 2048 /* flash_read chunk (smaller = less peak RAM on a tight heap) */

struct EspFlasher {
    FuriHalSerialHandle* serial;
    FuriStreamBuffer* rx;
    EspFlasherLog log;
    void* ctx;
};

/* The esp-serial-flasher port hooks are global free functions, so the active
 * instance + timer deadline live in file statics. One flasher at a time. */
static EspFlasher* s_active = NULL;
static uint32_t s_deadline = 0;
static volatile bool s_abort = false;

void esp_flasher_abort(void) {
    s_abort = true;
}

static void esp_flasher_logf(EspFlasher* f, const char* fmt, ...) {
    if(!f || !f->log) return;
    char buf[96];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    f->log(f->ctx, buf);
}

/* ---- esp-serial-flasher port (loader_port_*) ---- */

esp_loader_error_t loader_port_read(uint8_t* data, uint16_t size, uint32_t timeout) {
    if(!s_active) return ESP_LOADER_ERROR_FAIL;
    // Stream buffers can return early with a partial count; keep reading until we
    // have the whole frame (or a receive times out), else SLIP desyncs.
    size_t got = 0;
    while(got < size) {
        size_t r = furi_stream_buffer_receive(s_active->rx, data + got, size - got, timeout);
        if(r == 0) return ESP_LOADER_ERROR_TIMEOUT;
        got += r;
    }
    return ESP_LOADER_SUCCESS;
}

esp_loader_error_t loader_port_write(const uint8_t* data, uint16_t size, uint32_t timeout) {
    UNUSED(timeout);
    if(!s_active) return ESP_LOADER_ERROR_FAIL;
    furi_hal_serial_tx(s_active->serial, data, size);
    return ESP_LOADER_SUCCESS;
}

void loader_port_delay_ms(uint32_t ms) {
    furi_delay_ms(ms);
}

void loader_port_start_timer(uint32_t ms) {
    s_deadline = furi_get_tick() + ms;
}

uint32_t loader_port_remaining_time(void) {
    uint32_t now = furi_get_tick();
    return (s_deadline > now) ? (s_deadline - now) : 0;
}

/* Manual bootloader entry: the user resets the board into download mode. */
void loader_port_reset_target(void) {
}

void loader_port_enter_bootloader(void) {
}

esp_loader_error_t loader_port_change_transmission_rate(uint32_t rate) {
    if(s_active) {
        // Drain the TX FIFO/shift register before switching the divisor, or the
        // in-flight CHANGE_BAUDRATE ack gets mangled mid-byte and desyncs.
        furi_hal_serial_tx_wait_complete(s_active->serial);
        furi_hal_serial_set_br(s_active->serial, rate);
    }
    return ESP_LOADER_SUCCESS;
}

void loader_port_debug_print(const char* str) {
    if(s_active && s_active->log) s_active->log(s_active->ctx, str);
}

void loader_port_spi_set_cs(uint32_t level) {
    UNUSED(level);
}

/* ---- UART RX into the stream buffer ---- */

static void esp_flasher_rx_irq(FuriHalSerialHandle* handle, FuriHalSerialRxEvent ev, void* ctx) {
    EspFlasher* f = ctx;
    if(ev == FuriHalSerialRxEventData) {
        uint8_t b = furi_hal_serial_async_rx(handle);
        furi_stream_buffer_send(f->rx, &b, 1, 0);
    }
}

EspFlasher* esp_flasher_alloc(FuriHalSerialId ch, EspFlasherLog log_cb, void* ctx) {
    EspFlasher* f = malloc(sizeof(EspFlasher));
    if(!f) return NULL; // heap critically low; caller already handles a NULL link
    f->log = log_cb;
    f->ctx = ctx;
    f->rx = furi_stream_buffer_alloc(FLASH_RX_BUF, 1);

    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_disable(expansion);
    furi_record_close(RECORD_EXPANSION);

    f->serial = furi_hal_serial_control_acquire(ch);
    if(!f->serial) {
        furi_stream_buffer_free(f->rx);
        free(f);
        Expansion* exp = furi_record_open(RECORD_EXPANSION);
        expansion_enable(exp);
        furi_record_close(RECORD_EXPANSION);
        return NULL;
    }
    furi_hal_serial_init(f->serial, FLASH_BAUD);
    furi_hal_serial_async_rx_start(f->serial, esp_flasher_rx_irq, f, false);

    s_abort = false;
    s_active = f;
    return f;
}

void esp_flasher_free(EspFlasher* f) {
    if(!f) return;
    s_active = NULL;
    // Order matters: stop RX (no more IRQs touching f->rx) BEFORE freeing f->rx.
    furi_hal_serial_async_rx_stop(f->serial);
    furi_hal_serial_deinit(f->serial);
    furi_hal_serial_control_release(f->serial);
    furi_stream_buffer_free(f->rx);
    free(f);

    Expansion* expansion = furi_record_open(RECORD_EXPANSION);
    expansion_enable(expansion);
    furi_record_close(RECORD_EXPANSION);
}

bool esp_flasher_connect(EspFlasher* f, uint32_t fast_baud) {
    // Talk to the raw ESP32 ROM loader (esp_loader_connect, NO stub) -- the same
    // approach as the proven 0xchocolate ESP Flasher. We never upload the stub,
    // so the ROM's "software loader is resident / overlapping address range"
    // error cannot happen, and there is no stub MD5 transfer to corrupt. Writes
    // use the ROM flash commands; reads (backup) use the ROM's 64-byte read path.
    //
    // Manual bootloader entry (hold BOOT, tap RESET) is fiddly, so retry the SYNC
    // several times with a generous per-SYNC timeout and a pause between tries so
    // the user can re-tap RESET.
    const int attempts = 5;
    esp_loader_error_t err = ESP_LOADER_ERROR_TIMEOUT;
    for(int i = 1; i <= attempts; i++) {
        if(s_abort) {
            esp_flasher_logf(f, "Aborted.");
            return false;
        }
        esp_loader_connect_args_t args = ESP_LOADER_CONNECT_DEFAULT();
        args.sync_timeout = 500; // ms per SYNC (default 100) -- wait longer
        args.trials = 10; // SYNC frames per attempt
        esp_flasher_logf(f, "Connecting %d/%d...", i, attempts);
        err = esp_loader_connect(&args);
        if(err == ESP_LOADER_SUCCESS) break;
        esp_flasher_logf(f, "  no sync (%d).", (int)err);
        if(i < attempts) {
            esp_flasher_logf(f, "  hold BOOT, tap RESET.");
            furi_delay_ms(1500); // give the user a moment to re-enter bootloader
        }
    }
    if(err != ESP_LOADER_SUCCESS) {
        esp_flasher_logf(f, "Connect failed. Power-cycle the");
        esp_flasher_logf(f, "ESP, re-enter bootloader, retry.");
        return false;
    }
    esp_flasher_logf(f, "Connected (ROM loader).");

    if(fast_baud) {
        // The plain rate change only tells the ESP; the library does NOT re-rate
        // the host here, so we switch the Flipper UART ourselves to match (same
        // as the 0xchocolate flasher).
        err = esp_loader_change_transmission_rate(fast_baud);
        if(err == ESP_LOADER_SUCCESS) {
            furi_hal_serial_set_br(f->serial, fast_baud);
            esp_flasher_logf(f, "Speed -> %lu baud", (unsigned long)fast_baud);
        } else {
            esp_flasher_logf(f, "Fast baud failed (%d).", (int)err);
            esp_flasher_logf(f, "Re-enter bootloader, use Safe.");
            return false; // link may be desynced; bail rather than risk a bad flash
        }
    }
    return true;
}

bool esp_flasher_flash_file(EspFlasher* f, Storage* storage, const char* path, uint32_t addr) {
    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        esp_flasher_logf(f, "Cannot open bin.");
        storage_file_free(file);
        return false;
    }
    uint32_t size = (uint32_t)storage_file_size(file);
    // flash_start requires a 4-byte-aligned image size; the final short block is
    // padded with 0xFF (erased-flash value) so the device and MD5 agree.
    uint32_t img = (size + 3u) & ~3u;
    esp_flasher_logf(f, "Erasing + writing %luKB...", (unsigned long)(img / 1024));

    esp_loader_error_t err = esp_loader_flash_start(addr, img, FLASH_BLOCK);
    if(err != ESP_LOADER_SUCCESS) {
        esp_flasher_logf(f, "flash_start failed (%d).", (int)err);
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }

    uint8_t* buf = malloc(FLASH_BLOCK);
    if(!buf) {
        esp_flasher_logf(f, "Out of RAM.");
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }
    uint32_t done = 0;
    int last_pct = -1;
    bool ok = true;
    while(done < img) {
        if(s_abort) {
            esp_flasher_logf(f, "Aborted.");
            ok = false;
            break;
        }
        uint32_t want = (img - done < FLASH_BLOCK) ? (img - done) : FLASH_BLOCK;
        // Real file bytes still expected in this block. `img` is `size` rounded up
        // to 4 bytes, so only the true final block may legitimately read short --
        // by the 1-3 alignment bytes, which we 0xFF-pad below. A short read of the
        // REAL bytes means a truncated file or an SD glitch mid-stream; padding it
        // as EOF would shift every later block and corrupt the image (and MD5
        // verify checks the bytes we streamed, so it would still report "OK").
        uint32_t real = (size > done) ? (size - done) : 0;
        if(real > want) real = want;
        uint16_t n = storage_file_read(file, buf, (uint16_t)want);
        if(n != real) {
            esp_flasher_logf(
                f,
                "SD read error @%lu (%u/%lu).",
                (unsigned long)done,
                (unsigned)n,
                (unsigned long)real);
            ok = false;
            break;
        }
        if(want > n) memset(buf + n, 0xFF, want - n); // 4-byte alignment tail padding only
        err = esp_loader_flash_write(buf, want);
        if(err != ESP_LOADER_SUCCESS) {
            esp_flasher_logf(f, "write failed @%lu (%d).", (unsigned long)done, (int)err);
            ok = false;
            break;
        }
        done += want;
        int pct = (int)((uint64_t)done * 100 / img);
        if(pct != last_pct && pct % 10 == 0) {
            esp_flasher_logf(f, "  %d%%", pct);
            last_pct = pct;
        }
    }
    free(buf);
    storage_file_close(file);
    storage_file_free(file);

    if(ok) {
        // Verify the on-chip image FIRST, while the chip is still cleanly in
        // flash-download mode. The esp-serial-flasher examples verify before any
        // finish/reset, and for good reason: the ESP32 *ROM* loader answers
        // FLASH_END with COMMAND_FAILED even on a perfectly good flash, and that
        // failed command can leave the link unable to answer the MD5 query -- so
        // verifying *after* FLASH_END times out (error 2). Verify first.
        err = esp_loader_flash_verify(); // ROM SPI_FLASH_MD5 of the actual flash
        if(err == ESP_LOADER_SUCCESS) {
            esp_flasher_logf(f, "Verified OK.");
        } else if(err == ESP_LOADER_ERROR_INVALID_MD5) {
            // Definitive: what's on the chip does NOT match the image we sent.
            esp_flasher_logf(f, "VERIFY MISMATCH (bad flash)!");
            esp_flasher_logf(f, "Turn off Fast, reflash.");
            ok = false;
        } else {
            // The ROM didn't answer the MD5 query (some ESP32 ROMs don't support
            // SPI_FLASH_MD5 over UART; shows up as TIMEOUT/2). Every data block
            // was already written and acked, so this is "written but unverified",
            // NOT a failure -- let the user reset and functionally test it.
            esp_flasher_logf(f, "Wrote OK; MD5 n/a (%d).", (int)err);
            esp_flasher_logf(f, "Can't auto-verify on this");
            esp_flasher_logf(f, "ROM -- reset ESP + test it.");
        }
    }
    if(ok) {
        // Best-effort leave-flash-mode. On the ESP32 ROM this FLASH_END often
        // answers COMMAND_FAILED even though the image is fine, so its result is
        // cosmetic and ignored here -- the verify above already decided pass/fail.
        esp_loader_flash_finish(false);
        esp_flasher_logf(f, "Done. Reset ESP to run.");
    }
    return ok;
}

bool esp_flasher_backup(EspFlasher* f, Storage* storage, const char* out_path) {
    uint32_t size = 0;
    esp_loader_error_t err = esp_loader_flash_detect_size(&size);
    if(err != ESP_LOADER_SUCCESS || size == 0) {
        esp_flasher_logf(f, "Flash size unknown (%d).", (int)err);
        return false;
    }
    esp_flasher_logf(f, "Backing up %luKB...", (unsigned long)(size / 1024));

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, out_path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        esp_flasher_logf(f, "Cannot create backup file.");
        storage_file_free(file);
        return false;
    }

    uint8_t* buf = malloc(BACKUP_CHUNK);
    if(!buf) {
        esp_flasher_logf(f, "Out of RAM.");
        storage_file_close(file);
        storage_file_free(file);
        return false;
    }
    uint32_t addr = 0;
    int last_pct = -1;
    bool ok = true;
    while(addr < size) {
        if(s_abort) {
            esp_flasher_logf(f, "Aborted.");
            ok = false;
            break;
        }
        uint32_t n = (size - addr < BACKUP_CHUNK) ? (size - addr) : BACKUP_CHUNK;
        // Retry a chunk on a transient error -- notably INVALID_MD5 (4) from line
        // noise, common at fast baud. Re-reading the same address usually
        // succeeds; only give up after several tries.
        err = ESP_LOADER_ERROR_FAIL;
        for(int r = 0; r < 5 && !s_abort; r++) {
            err = esp_loader_flash_read(buf, addr, n);
            if(err == ESP_LOADER_SUCCESS) break;
            esp_flasher_logf(f, "  retry @%lu (%d)", (unsigned long)addr, (int)err);
            furi_delay_ms(50);
        }
        if(err != ESP_LOADER_SUCCESS) {
            esp_flasher_logf(f, "read failed @%lu (%d).", (unsigned long)addr, (int)err);
            esp_flasher_logf(f, "Use Safe speed; check wiring.");
            ok = false;
            break;
        }
        if(storage_file_write(file, buf, n) != n) {
            esp_flasher_logf(f, "SD write failed.");
            ok = false;
            break;
        }
        addr += n;
        int pct = (int)((uint64_t)addr * 100 / size);
        if(pct != last_pct && pct % 10 == 0) {
            esp_flasher_logf(f, "  %d%%", pct);
            last_pct = pct;
        }
    }
    free(buf);
    storage_file_close(file);
    storage_file_free(file);

    if(ok) {
        esp_flasher_logf(f, "Backup saved.");
    } else {
        // Don't leave a truncated image around that could be flashed later.
        storage_simply_remove(storage, out_path);
        esp_flasher_logf(f, "Partial backup deleted.");
    }
    return ok;
}
