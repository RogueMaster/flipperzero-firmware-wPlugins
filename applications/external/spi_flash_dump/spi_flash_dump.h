#pragma once

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <gui/view.h>
#include <storage/storage.h>
#include <notification/notification_messages.h>
#include <toolbox/stream/file_stream.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/*  SPI Flash command defines                                         */
/* ------------------------------------------------------------------ */
#define CMD_READ_JEDEC_ID   0x9F
#define CMD_READ_DATA       0x03
#define CMD_FAST_READ       0x0B
#define CMD_READ_STATUS1    0x05
#define CMD_READ_STATUS2    0x35
#define CMD_RELEASE_PWRDOWN 0xAB
#define CMD_ENTER_4BYTE     0xB7
#define CMD_EXIT_4BYTE      0xE9

/* ------------------------------------------------------------------ */
/*  Address mode threshold – chips > 16 MB need 4-byte addressing     */
/* ------------------------------------------------------------------ */
#define SPI_3BYTE_ADDR_MAX (16UL * 1024 * 1024)

/* ------------------------------------------------------------------ */
/*  Read page size                                                    */
/* ------------------------------------------------------------------ */
#define SPI_FLASH_PAGE_SIZE 256

/* ------------------------------------------------------------------ */
/*  Hex viewer preview size                                           */
/* ------------------------------------------------------------------ */
#define HEX_PREVIEW_SIZE 4096

/* ------------------------------------------------------------------ */
/*  Dump output directory                                             */
/* ------------------------------------------------------------------ */
#define SPI_DUMP_DIR "/ext/spi_dumps"

/* ------------------------------------------------------------------ */
/*  View IDs                                                          */
/* ------------------------------------------------------------------ */
typedef enum {
    SpiFlashDumpViewWiringGuide,
    SpiFlashDumpViewChipInfo,
    SpiFlashDumpViewReadProgress,
    SpiFlashDumpViewVerifyProgress,
    SpiFlashDumpViewHexPreview,
    SpiFlashDumpViewSettings,
} SpiFlashDumpView;

/* ------------------------------------------------------------------ */
/*  SPI speed options                                                 */
/* ------------------------------------------------------------------ */
typedef enum {
    SpiSpeedSlow = 0, /* ~50 kHz  – delay 10 us */
    SpiSpeedMedium, /* ~250 kHz – delay 2 us  */
    SpiSpeedFast, /* ~1 MHz   – delay 0 us  */
    SpiSpeedCount,
} SpiSpeed;

/* ------------------------------------------------------------------ */
/*  Read command options                                              */
/* ------------------------------------------------------------------ */
typedef enum {
    SpiReadCmdNormal = 0, /* 0x03 */
    SpiReadCmdFast, /* 0x0B */
    SpiReadCmdCount,
} SpiReadCmd;

/* ------------------------------------------------------------------ */
/*  Chip info                                                         */
/* ------------------------------------------------------------------ */
typedef struct {
    uint8_t manufacturer_id;
    uint8_t device_id[2];
    const char* manufacturer_name;
    const char* part_name;
    uint32_t size_bytes;
} SpiFlashChipInfo;

/* ------------------------------------------------------------------ */
/*  JEDEC ID database – 32 common SPI NOR flash parts                 */
/* ------------------------------------------------------------------ */
/* Each entry: { mfr_id, {dev_id_hi, dev_id_lo}, mfr_name, part, size } */

extern const SpiFlashChipInfo spi_flash_db[];
extern const size_t SPI_FLASH_DB_COUNT;

/* ------------------------------------------------------------------ */
/*  Worker state / status                                             */
/* ------------------------------------------------------------------ */
typedef enum {
    SpiWorkerStateIdle,
    SpiWorkerStateReading,
    SpiWorkerStateVerifying,
    SpiWorkerStateDone,
    SpiWorkerStateError,
} SpiWorkerState;

/* ------------------------------------------------------------------ */
/*  Forward declarations                                              */
/* ------------------------------------------------------------------ */
typedef struct SpiWorker SpiWorker;
typedef struct HexViewer HexViewer;

/* ------------------------------------------------------------------ */
/*  Progress callback                                                 */
/* ------------------------------------------------------------------ */
typedef void (*SpiProgressCallback)(uint32_t bytes_done, uint32_t total_bytes, void* context);

/* ------------------------------------------------------------------ */
/*  Main application state                                            */
/* ------------------------------------------------------------------ */
typedef struct {
    /* Gui / views */
    Gui* gui;
    ViewDispatcher* view_dispatcher;
    Widget* wiring_guide;
    Widget* chip_info_widget;
    View* read_progress_view;
    View* verify_progress_view;
    HexViewer* hex_viewer;
    VariableItemList* settings_list;
    NotificationApp* notifications;

    /* SPI worker */
    SpiWorker* worker;

    /* Detected chip info (copy from db or raw) */
    bool chip_detected;
    SpiFlashChipInfo chip;
    uint8_t status_reg1;
    uint8_t status_reg2;

    /* Settings */
    SpiSpeed spi_speed;
    bool verify_after_read;
    SpiReadCmd read_cmd;

    /* Dump file path */
    char dump_path[128];

    /* Progress state (read by custom views) */
    uint32_t progress_bytes;
    uint32_t progress_total;
    uint32_t progress_start_tick;
    uint32_t verify_match;
    uint32_t verify_mismatch;
    SpiWorkerState worker_state;

    /* Guard flag: true while a worker thread is active (not yet joined) */
    volatile bool worker_running;
} SpiFlashDumpApp;
