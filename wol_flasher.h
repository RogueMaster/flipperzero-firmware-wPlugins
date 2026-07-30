#pragma once

#include <furi.h>
#include <stdbool.h>

/**
 * ESP32-S2 flasher built on esp-serial-flasher, talking to the dev board over
 * the same USART the WoL protocol uses.
 *
 * The board has to be put into its ROM bootloader by hand before connecting
 * (hold BOOT, tap RESET, release BOOT): the Flipper header carries no DTR/RTS,
 * so there is nothing to toggle from this side.
 *
 * All calls are blocking and belong on a worker thread.
 */
typedef struct WolFlasher WolFlasher;

typedef enum {
    WolFlasherOk,
    /** USART is held by something else. */
    WolFlasherErrBusy,
    /** No SLIP answer: not in bootloader mode, or not connected. */
    WolFlasherErrNoBoard,
    /** Connected, but it is not an ESP32-S2. */
    WolFlasherErrWrongChip,
    WolFlasherErrFile,
    WolFlasherErrFlash,
    WolFlasherErrVerify,
    WolFlasherErrCancelled,
} WolFlasherResult;

typedef enum {
    WolFlasherStageConnect,
    WolFlasherStageErase,
    WolFlasherStageWrite,
    WolFlasherStageRead,
    WolFlasherStageVerify,
} WolFlasherStage;

typedef void (*WolFlasherProgressCallback)(
    void* context,
    WolFlasherStage stage,
    uint8_t percent);

typedef struct {
    const char* path;
    uint32_t address;
} WolFlasherImage;

WolFlasher* wol_flasher_alloc(volatile bool* cancel);
void wol_flasher_free(WolFlasher* flasher);

void wol_flasher_set_progress_callback(
    WolFlasher* flasher,
    WolFlasherProgressCallback callback,
    void* context);

/** Take the USART, sync with the ROM loader, upload the stub, detect flash. */
WolFlasherResult wol_flasher_connect(WolFlasher* flasher);
void wol_flasher_disconnect(WolFlasher* flasher);

uint32_t wol_flasher_get_flash_size(const WolFlasher* flasher);
const char* wol_flasher_get_chip_name(const WolFlasher* flasher);
bool wol_flasher_is_stub_running(const WolFlasher* flasher);
uint32_t wol_flasher_get_transmission_rate(const WolFlasher* flasher);

/** Dump the entire flash chip to path. */
WolFlasherResult wol_flasher_backup(WolFlasher* flasher, const char* path);

/** Write images in order, reporting one combined progress over all of them. */
WolFlasherResult
    wol_flasher_write_images(WolFlasher* flasher, const WolFlasherImage* images, size_t count);

const char* wol_flasher_result_text(WolFlasherResult result);
