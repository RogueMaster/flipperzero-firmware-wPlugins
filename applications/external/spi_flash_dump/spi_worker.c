/* spi_worker.c — Bit-banged SPI driver and background read/verify worker thread.
 * Handles GPIO init, JEDEC ID detection, chip read-to-file, and verify-against-file.
 * Uses PA4 (CS), PB3 (CLK), PA7 (MOSI), PA6 (MISO) on the external header. */

#include "spi_worker.h"
#include <toolbox/crc32_calc.h>

/* ------------------------------------------------------------------ */
/*  Pin assignments – external GPIO header                            */
/*  CLK  = PB3 (pin 5)                                               */
/*  MISO = PA6 (pin 6)                                               */
/*  MOSI = PA7 (pin 7)                                               */
/*  CS   = PA4 (pin 4)                                               */
/* ------------------------------------------------------------------ */
#define SPI_PIN_CLK  (&gpio_ext_pb3)
#define SPI_PIN_MISO (&gpio_ext_pa6)
#define SPI_PIN_MOSI (&gpio_ext_pa7)
#define SPI_PIN_CS   (&gpio_ext_pa4)

/* ------------------------------------------------------------------ */
/*  JEDEC ID database – 32 common SPI NOR flash parts                 */
/* ------------------------------------------------------------------ */

const SpiFlashChipInfo spi_flash_db[] = {
    /* ----- Winbond (0xEF) ----- */
    {0xEF, {0x40, 0x14}, "Winbond", "W25Q80BV", 1 * 1024 * 1024},
    {0xEF, {0x40, 0x15}, "Winbond", "W25Q16JV", 2 * 1024 * 1024},
    {0xEF, {0x40, 0x16}, "Winbond", "W25Q32JV", 4 * 1024 * 1024},
    {0xEF, {0x40, 0x17}, "Winbond", "W25Q64JV", 8 * 1024 * 1024},
    {0xEF, {0x40, 0x18}, "Winbond", "W25Q128JV", 16 * 1024 * 1024},
    {0xEF, {0x40, 0x19}, "Winbond", "W25Q256JV", 32 * 1024 * 1024},
    {0xEF, {0x70, 0x18}, "Winbond", "W25Q128JW", 16 * 1024 * 1024},
    {0xEF, {0x60, 0x17}, "Winbond", "W25Q64JW", 8 * 1024 * 1024},

    /* ----- Macronix (0xC2) ----- */
    {0xC2, {0x20, 0x14}, "Macronix", "MX25L8006E", 1 * 1024 * 1024},
    {0xC2, {0x20, 0x15}, "Macronix", "MX25L1606E", 2 * 1024 * 1024},
    {0xC2, {0x20, 0x16}, "Macronix", "MX25L3233F", 4 * 1024 * 1024},
    {0xC2, {0x20, 0x17}, "Macronix", "MX25L6433F", 8 * 1024 * 1024},
    {0xC2, {0x20, 0x18}, "Macronix", "MX25L12835F", 16 * 1024 * 1024},
    {0xC2, {0x20, 0x19}, "Macronix", "MX25L25645G", 32 * 1024 * 1024},

    /* ----- GigaDevice (0xC8) ----- */
    {0xC8, {0x40, 0x14}, "GigaDevice", "GD25Q80C", 1 * 1024 * 1024},
    {0xC8, {0x40, 0x15}, "GigaDevice", "GD25Q16C", 2 * 1024 * 1024},
    {0xC8, {0x40, 0x16}, "GigaDevice", "GD25Q32C", 4 * 1024 * 1024},
    {0xC8, {0x40, 0x17}, "GigaDevice", "GD25Q64C", 8 * 1024 * 1024},
    {0xC8, {0x40, 0x18}, "GigaDevice", "GD25Q128C", 16 * 1024 * 1024},

    /* ----- Adesto / Atmel (0x1F) ----- */
    {0x1F, {0x86, 0x01}, "Adesto", "AT25SF081", 1 * 1024 * 1024},
    {0x1F, {0x86, 0x02}, "Adesto", "AT25SF161", 2 * 1024 * 1024},
    {0x1F, {0x87, 0x01}, "Adesto", "AT25SF321", 4 * 1024 * 1024},

    /* ----- ISSI (0x9D) ----- */
    {0x9D, {0x60, 0x16}, "ISSI", "IS25LP032", 4 * 1024 * 1024},
    {0x9D, {0x60, 0x17}, "ISSI", "IS25LP064", 8 * 1024 * 1024},
    {0x9D, {0x60, 0x18}, "ISSI", "IS25LP128", 16 * 1024 * 1024},

    /* ----- SST / Microchip (0xBF) ----- */
    {0xBF, {0x25, 0x8D}, "SST", "SST25VF040B", 512 * 1024},
    {0xBF, {0x25, 0x8E}, "SST", "SST25VF080B", 1 * 1024 * 1024},
    {0xBF, {0x25, 0x41}, "SST", "SST25VF016B", 2 * 1024 * 1024},
    {0xBF, {0x25, 0x4A}, "SST", "SST25VF032B", 4 * 1024 * 1024},

    /* ----- Spansion / Cypress / Infineon (0x01) ----- */
    {0x01, {0x02, 0x15}, "Spansion", "S25FL116K", 2 * 1024 * 1024},
    {0x01, {0x02, 0x16}, "Spansion", "S25FL132K", 4 * 1024 * 1024},
    {0x01, {0x02, 0x17}, "Spansion", "S25FL164K", 8 * 1024 * 1024},
};

const size_t SPI_FLASH_DB_COUNT = sizeof(spi_flash_db) / sizeof(spi_flash_db[0]);

/* ------------------------------------------------------------------ */
/*  Worker private struct                                             */
/* ------------------------------------------------------------------ */
typedef enum {
    WorkerOpRead,
    WorkerOpVerify,
} WorkerOp;

struct SpiWorker {
    FuriThread* thread;
    volatile bool running;
    bool result;

    /* Operation parameters (set before thread start) */
    WorkerOp op;
    const SpiFlashChipInfo* chip;
    char path[128];
    uint8_t read_cmd;
    uint32_t delay_us;
    SpiProgressCallback cb;
    void* cb_ctx;
    /* verify-specific */
    uint32_t* match_out;
    uint32_t* mismatch_out;
    /* CRC32 of dump file (computed in worker thread after successful read) */
    uint32_t crc32;
    bool crc32_valid;
};

/* ------------------------------------------------------------------ */
/*  GPIO init / deinit                                                */
/* ------------------------------------------------------------------ */

void spi_worker_gpio_init(void) {
    /* CLK, MOSI, CS – output push-pull, no pull, very-high speed */
    furi_hal_gpio_init(SPI_PIN_CLK, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_init(SPI_PIN_MOSI, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);
    furi_hal_gpio_init(SPI_PIN_CS, GpioModeOutputPushPull, GpioPullNo, GpioSpeedVeryHigh);

    /* MISO – input with pull-up */
    furi_hal_gpio_init(SPI_PIN_MISO, GpioModeInput, GpioPullUp, GpioSpeedVeryHigh);

    /* Idle state: CLK low, CS high, MOSI low */
    furi_hal_gpio_write(SPI_PIN_CLK, false);
    furi_hal_gpio_write(SPI_PIN_CS, true);
    furi_hal_gpio_write(SPI_PIN_MOSI, false);
}

void spi_worker_gpio_deinit(void) {
    /* Return all pins to safe analog / input state */
    furi_hal_gpio_init(SPI_PIN_CLK, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(SPI_PIN_MOSI, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(SPI_PIN_CS, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(SPI_PIN_MISO, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

/* ------------------------------------------------------------------ */
/*  Low-level SPI helpers                                             */
/* ------------------------------------------------------------------ */

void spi_cs_low(void) {
    furi_hal_gpio_write(SPI_PIN_CS, false);
}

void spi_cs_high(void) {
    furi_hal_gpio_write(SPI_PIN_CS, true);
}

uint8_t spi_transfer_byte(uint8_t tx, uint32_t clock_delay) {
    uint8_t rx = 0;

    for(int8_t bit = 7; bit >= 0; bit--) {
        /* Set MOSI before rising edge */
        furi_hal_gpio_write(SPI_PIN_MOSI, (tx >> bit) & 1);

        if(clock_delay) furi_delay_us(clock_delay);

        /* Rising edge – slave latches MOSI, drives MISO */
        furi_hal_gpio_write(SPI_PIN_CLK, true);

        if(clock_delay) furi_delay_us(clock_delay);

        /* Sample MISO on (or just after) rising edge */
        if(furi_hal_gpio_read(SPI_PIN_MISO)) {
            rx |= (1 << bit);
        }

        /* Falling edge */
        furi_hal_gpio_write(SPI_PIN_CLK, false);
    }

    return rx;
}

/* ------------------------------------------------------------------ */
/*  4-byte address mode helpers                                       */
/* ------------------------------------------------------------------ */

/** Enter 4-byte address mode (cmd 0xB7). Required for chips > 16 MB. */
static void enter_4byte_mode(uint32_t delay) {
    spi_cs_low();
    spi_transfer_byte(CMD_ENTER_4BYTE, delay);
    spi_cs_high();
}

/** Exit 4-byte address mode (cmd 0xE9). */
static void exit_4byte_mode(uint32_t delay) {
    spi_cs_low();
    spi_transfer_byte(CMD_EXIT_4BYTE, delay);
    spi_cs_high();
}

/** Send a 24-bit or 32-bit address depending on `four_byte`. */
static void spi_send_address(uint32_t address, bool four_byte, uint32_t delay) {
    if(four_byte) {
        spi_transfer_byte((address >> 24) & 0xFF, delay);
    }
    spi_transfer_byte((address >> 16) & 0xFF, delay);
    spi_transfer_byte((address >> 8) & 0xFF, delay);
    spi_transfer_byte(address & 0xFF, delay);
}

/* ------------------------------------------------------------------ */
/*  Chip-level operations                                             */
/* ------------------------------------------------------------------ */

uint8_t read_status_register(uint32_t delay) {
    spi_cs_low();
    spi_transfer_byte(CMD_READ_STATUS1, delay);
    uint8_t sr = spi_transfer_byte(0xFF, delay);
    spi_cs_high();
    return sr;
}

uint8_t read_status_register2(uint32_t delay) {
    spi_cs_low();
    spi_transfer_byte(CMD_READ_STATUS2, delay);
    uint8_t sr = spi_transfer_byte(0xFF, delay);
    spi_cs_high();
    return sr;
}

void chip_release_power_down(uint32_t delay) {
    spi_cs_low();
    spi_transfer_byte(CMD_RELEASE_PWRDOWN, delay);
    /* Three dummy bytes + 1 device-id byte per datasheet; we don't need
       the device ID here, but clocking them out is harmless. */
    spi_transfer_byte(0xFF, delay);
    spi_transfer_byte(0xFF, delay);
    spi_transfer_byte(0xFF, delay);
    spi_transfer_byte(0xFF, delay);
    spi_cs_high();
    /* tRES1 is typically ~3 us; give it 100 us to be safe */
    furi_delay_us(100);
}

bool chip_detect(SpiFlashChipInfo* info_out, uint32_t delay) {
    /* Wake chip in case it is in power-down */
    chip_release_power_down(delay);

    /* Read JEDEC ID */
    spi_cs_low();
    spi_transfer_byte(CMD_READ_JEDEC_ID, delay);
    uint8_t mfr = spi_transfer_byte(0xFF, delay);
    uint8_t id_hi = spi_transfer_byte(0xFF, delay);
    uint8_t id_lo = spi_transfer_byte(0xFF, delay);
    spi_cs_high();

    /* Always populate the raw IDs */
    info_out->manufacturer_id = mfr;
    info_out->device_id[0] = id_hi;
    info_out->device_id[1] = id_lo;
    info_out->manufacturer_name = "Unknown";
    info_out->part_name = "Unknown";
    info_out->size_bytes = 0;

    /* All-FF or all-00 means nothing is connected */
    if((mfr == 0xFF && id_hi == 0xFF && id_lo == 0xFF) ||
       (mfr == 0x00 && id_hi == 0x00 && id_lo == 0x00)) {
        return false;
    }

    /* Look up in database */
    for(size_t i = 0; i < SPI_FLASH_DB_COUNT; i++) {
        const SpiFlashChipInfo* db = &spi_flash_db[i];
        if(db->manufacturer_id == mfr && db->device_id[0] == id_hi && db->device_id[1] == id_lo) {
            info_out->manufacturer_name = db->manufacturer_name;
            info_out->part_name = db->part_name;
            info_out->size_bytes = db->size_bytes;
            return true;
        }
    }

    /* Valid JEDEC response but chip not in database – estimate size from
       the common "capacity" encoding where id_lo = log2(bytes). */
    if(id_lo >= 0x10 && id_lo <= 0x19) {
        info_out->size_bytes = 1UL << id_lo;
    }

    return false;
}

/* ------------------------------------------------------------------ */
/*  Chip read – full dump to SD card file                             */
/* ------------------------------------------------------------------ */

bool chip_read(
    const SpiFlashChipInfo* chip,
    const char* path,
    uint8_t read_cmd,
    uint32_t delay,
    SpiProgressCallback cb,
    void* cb_ctx) {
    if(chip->size_bytes == 0) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);

    /* Ensure output directory exists */
    storage_simply_mkdir(storage, SPI_DUMP_DIR);

    File* file = storage_file_alloc(storage);
    if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    uint8_t buf[SPI_FLASH_PAGE_SIZE];
    uint32_t address = 0;
    uint32_t total = chip->size_bytes;
    bool success = true;
    bool four_byte = (total > SPI_3BYTE_ADDR_MAX);

    if(four_byte) enter_4byte_mode(delay);

    while(address < total) {
        uint32_t chunk = SPI_FLASH_PAGE_SIZE;
        if(address + chunk > total) chunk = total - address;

        spi_cs_low();
        spi_transfer_byte(read_cmd, delay);
        spi_send_address(address, four_byte, delay);

        /* Fast-Read (0x0B) requires a dummy byte */
        if(read_cmd == CMD_FAST_READ) {
            spi_transfer_byte(0xFF, delay);
        }

        for(uint32_t i = 0; i < chunk; i++) {
            buf[i] = spi_transfer_byte(0xFF, delay);
        }
        spi_cs_high();

        if(storage_file_write(file, buf, (uint16_t)chunk) != chunk) {
            success = false;
            break;
        }

        address += chunk;
        if(cb) cb(address, total, cb_ctx);
    }

    if(four_byte) exit_4byte_mode(delay);

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

/* ------------------------------------------------------------------ */
/*  Chip verify – re-read and compare                                 */
/* ------------------------------------------------------------------ */

bool chip_verify(
    const SpiFlashChipInfo* chip,
    const char* path,
    uint8_t read_cmd,
    uint32_t delay,
    SpiProgressCallback cb,
    void* cb_ctx,
    uint32_t* match_out,
    uint32_t* mismatch_out) {
    if(chip->size_bytes == 0) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    uint8_t spi_buf[SPI_FLASH_PAGE_SIZE];
    uint8_t file_buf[SPI_FLASH_PAGE_SIZE];
    uint32_t address = 0;
    uint32_t total = chip->size_bytes;
    uint32_t match = 0;
    uint32_t mismatch = 0;
    bool four_byte = (total > SPI_3BYTE_ADDR_MAX);

    if(four_byte) enter_4byte_mode(delay);

    while(address < total) {
        uint32_t chunk = SPI_FLASH_PAGE_SIZE;
        if(address + chunk > total) chunk = total - address;

        /* Read from SPI */
        spi_cs_low();
        spi_transfer_byte(read_cmd, delay);
        spi_send_address(address, four_byte, delay);
        if(read_cmd == CMD_FAST_READ) {
            spi_transfer_byte(0xFF, delay);
        }
        for(uint32_t i = 0; i < chunk; i++) {
            spi_buf[i] = spi_transfer_byte(0xFF, delay);
        }
        spi_cs_high();

        /* Read from file */
        uint16_t read_cnt = storage_file_read(file, file_buf, (uint16_t)chunk);
        if(read_cnt != chunk) {
            /* File shorter than chip – count remaining as mismatches */
            mismatch += (total - address);
            break;
        }

        /* Compare */
        for(uint32_t i = 0; i < chunk; i++) {
            if(spi_buf[i] == file_buf[i]) {
                match++;
            } else {
                mismatch++;
            }
        }

        /* Update counters incrementally so the progress callback can display them */
        if(match_out) *match_out = match;
        if(mismatch_out) *mismatch_out = mismatch;

        address += chunk;
        if(cb) cb(address, total, cb_ctx);
    }

    if(four_byte) exit_4byte_mode(delay);

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(match_out) *match_out = match;
    if(mismatch_out) *mismatch_out = mismatch;

    return (mismatch == 0);
}

/* ------------------------------------------------------------------ */
/*  Worker thread entry point                                         */
/* ------------------------------------------------------------------ */

static int32_t spi_worker_thread(void* ctx) {
    SpiWorker* w = ctx;

    if(w->op == WorkerOpRead) {
        w->result = chip_read(w->chip, w->path, w->read_cmd, w->delay_us, w->cb, w->cb_ctx);

        /* Compute CRC32 here in the worker thread so the timer daemon is not
           blocked re-reading the entire dump file from SD card. */
        if(w->result) {
            Storage* storage = furi_record_open(RECORD_STORAGE);
            File* crc_file = storage_file_alloc(storage);
            if(storage_file_open(crc_file, w->path, FSAM_READ, FSOM_OPEN_EXISTING)) {
                w->crc32 = crc32_calc_file(crc_file, NULL, NULL);
                w->crc32_valid = true;
                storage_file_close(crc_file);
            }
            storage_file_free(crc_file);
            furi_record_close(RECORD_STORAGE);
        }
    } else {
        w->result = chip_verify(
            w->chip,
            w->path,
            w->read_cmd,
            w->delay_us,
            w->cb,
            w->cb_ctx,
            w->match_out,
            w->mismatch_out);
    }

    w->running = false;
    return 0;
}

/* ------------------------------------------------------------------ */
/*  Worker alloc / free                                               */
/* ------------------------------------------------------------------ */

SpiWorker* spi_worker_alloc(void) {
    SpiWorker* w = malloc(sizeof(SpiWorker));
    furi_assert(w);
    memset(w, 0, sizeof(SpiWorker));
    w->thread = furi_thread_alloc_ex("SpiWorker", 4096, spi_worker_thread, w);
    return w;
}

void spi_worker_free(SpiWorker* w) {
    if(!w) return;
    if(w->running) {
        spi_worker_wait(w);
    }
    furi_thread_free(w->thread);
    free(w);
}

/* ------------------------------------------------------------------ */
/*  Start read / verify in background thread                          */
/* ------------------------------------------------------------------ */

void spi_worker_start_read(
    SpiWorker* w,
    const SpiFlashChipInfo* chip,
    const char* path,
    uint8_t read_cmd,
    uint32_t delay,
    SpiProgressCallback cb,
    void* cb_ctx) {
    furi_assert(!w->running);

    w->op = WorkerOpRead;
    w->chip = chip;
    strncpy(w->path, path, sizeof(w->path) - 1);
    w->read_cmd = read_cmd;
    w->delay_us = delay;
    w->cb = cb;
    w->cb_ctx = cb_ctx;
    w->result = false;
    w->crc32 = 0;
    w->crc32_valid = false;
    w->running = true;

    furi_thread_join(w->thread);
    furi_thread_start(w->thread);
}

void spi_worker_start_verify(
    SpiWorker* w,
    const SpiFlashChipInfo* chip,
    const char* path,
    uint8_t read_cmd,
    uint32_t delay,
    SpiProgressCallback cb,
    void* cb_ctx,
    uint32_t* match_out,
    uint32_t* mismatch_out) {
    furi_assert(!w->running);

    w->op = WorkerOpVerify;
    w->chip = chip;
    strncpy(w->path, path, sizeof(w->path) - 1);
    w->read_cmd = read_cmd;
    w->delay_us = delay;
    w->cb = cb;
    w->cb_ctx = cb_ctx;
    w->match_out = match_out;
    w->mismatch_out = mismatch_out;
    w->result = false;
    w->running = true;

    furi_thread_join(w->thread);
    furi_thread_start(w->thread);
}

bool spi_worker_is_running(SpiWorker* w) {
    return w->running;
}

void spi_worker_wait(SpiWorker* w) {
    furi_thread_join(w->thread);
}

bool spi_worker_get_result(SpiWorker* w) {
    return w->result;
}

uint32_t spi_worker_get_crc32(SpiWorker* w) {
    return w->crc32;
}

bool spi_worker_has_crc32(SpiWorker* w) {
    return w->crc32_valid;
}
