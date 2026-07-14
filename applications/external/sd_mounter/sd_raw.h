#pragma once
#include "sd_mounter.h"
#include "ui.h"
#include "furi_hal_sd.h"
#include "helpers/mass_storage_scsi.h"

#define TAG "SDRAW"

// (re-)Initialize the SD card
// Among other things, this will clear the read cache
// in case another card was read from
void sd_init() {
    furi_hal_sd_init(false);
}

FuriHalSdInfo* try_get_sd_info() {
    // Blink magenta while trying to scan for the SD card
    notify(&led_blink_magenta);

    FuriHalSdInfo* info = malloc(sizeof(FuriHalSdInfo));

    // Try for up to 3 seconds (30 loops of 100ms)
    for(int i = 0; i < 30; i++) {
        // Initialize the card and wait a bit before checking
        // Also power cycle the card every 10th time
        furi_hal_sd_init(i % 10 == 0);
        furi_delay_ms(100);

        // Check if card has been removed
        if(!furi_hal_sd_is_present()) {
            notify(NULL);
            return NULL;
        }

        // Try to fetch card info
        FuriStatus error = furi_hal_sd_info(info);
        if(error == FuriStatusOk) {
            notify(NULL);
            return info;
        }
    }

    // If the loop ended, we failed to find the card description
    notify(NULL);
    return NULL;
}

FuriString* get_card_desc(FuriHalSdInfo info) {
    FuriString* desc = furi_string_alloc();
    furi_string_printf(
        desc,
        "%02x %s %s v%i.%i SN#%04lx Mfg.%02i/%i",
        info.manufacturer_id,
        info.oem_id,
        info.product_name,
        info.product_revision_major,
        info.product_revision_minor,
        info.product_serial_number,
        info.manufacturing_month,
        info.manufacturing_year);
    return desc;
}
bool sd_raw_read(
    void* ctx,
    uint32_t lba,
    uint16_t count,
    uint8_t* out,
    uint32_t* out_len,
    uint32_t out_cap) {
    Context* context = ctx;

    // Promote each to a larger size before multiplying
    uint32_t length = (uint32_t)count * SCSI_BLOCK_SIZE / context->logical_block_size;
    uint64_t sector = (uint64_t)lba * SCSI_BLOCK_SIZE / context->logical_block_size;

    // Prevent length from exceeding `out_cap` bytes
    length = MIN(length, out_cap / context->logical_block_size);

    // This function reads uint32_t's, so we divide the sector and length by 4 TIMES 512 BECAUSE SOMEONE IS A DUMBASS
    FuriStatus result = furi_hal_sd_read_blocks((uint32_t*)out, sector, length);

    if(result == FuriStatusOk) {
        context->bytes_read += (*out_len) = length * context->logical_block_size;
        // FURI_LOG_T(TAG, "Read %lu bytes.", *out_len);
        return true;
    } else {
        *out_len = 0;
        return false;
    }
}

bool sd_raw_write(void* ctx, uint32_t lba, uint16_t count, uint8_t* buf, uint32_t len) {
    Context* context = ctx;

    if(len != count * SCSI_BLOCK_SIZE) {
        FURI_LOG_W(TAG, "bad write params count=%u len=%lu", count, len);
        return false;
    }

    // Promote each to a larger size before multiplying
    uint32_t length = (uint32_t)count * SCSI_BLOCK_SIZE / context->logical_block_size;
    uint64_t sector = (uint64_t)lba * SCSI_BLOCK_SIZE / context->logical_block_size;

    FuriStatus result = furi_hal_sd_write_blocks((uint32_t*)buf, sector, length);

    if(result == FuriStatusOk) {
        context->bytes_written += len;
        // FURI_LOG_T(TAG, "Wrote %lu bytes.", len);
        return true;
    } else {
        return false;
    }
}

uint32_t sd_num_blocks(void* ctx) {
    Context* context = ctx;
    return context->card_size_in_blocks;
}

void sd_eject(void* ctx) {
    Context* context = ctx;
    furi_thread_flags_set(context->thread_id, FlagEject);
}
