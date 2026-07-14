#include <furi.h>
#include "furi_hal_usb.h"
#include "storage/storage.h"
#include <core/string.h>

#include "sd_mounter.h"
#include "helpers/mass_storage_usb.h"
#include "sd_raw.h"
#include "ui.h"
#include "sd_mounter_icons.h"

#ifdef TAG
#undef TAG
#endif
#define TAG "SDMounter"

Storage* storage;
FuriThreadId* storage_thread;

bool find_storage_thread();
void do_mass_storage(FuriHalSdInfo card_info);
int32_t sd_mounter_app(void* p);
void cleanup();

bool find_storage_thread() {
    FuriThreadList* threads = furi_thread_list_alloc();
    furi_thread_enumerate(threads);

    FuriThread* result = NULL;

    for(size_t i = 0; i < furi_thread_list_size(threads); i++) {
        const FuriThreadListItem* item = furi_thread_list_get_at(threads, i);

        if(strcmp(item->app_id, "storage") == 0) {
            if(result == NULL) {
                result = item->thread;
            } else {
                show_error_and_wait(
                    "More than one storage thread!\nPlease report this error to the\napplication developer.",
                    &I_Error_62x31,
                    62);
                return false;
            }
        }

        if(strcmp(item->name, "MassStorageUsb") == 0 ||
           strcmp(item->name, "SD Card Mounter") == 0) {
            FURI_LOG_E("DBG", "%-20s -> ID %p", item->name, furi_thread_get_id(item->thread));
        }
    }

    if(result != NULL) {
        storage_thread = furi_thread_get_id(result);
        return storage_thread != NULL;
    }

    return false;
}

// This function handles the actual USB mass storage interface.
// It is called when a card is connected.
// On return, the user will be prompted to insert a new card.
void do_mass_storage(FuriHalSdInfo card_info) {
    const char* card_desc = furi_string_get_cstr(get_card_desc(card_info));
    show("SD Card ready\nConnect computer");
    furi_delay_ms(500);

    // Initialize Mass Storage USB stuff
    Context ctx = {
        .thread_id = furi_thread_get_current_id(),
        .card_size_in_blocks = card_info.capacity / SCSI_BLOCK_SIZE,
        .logical_block_size = card_info.logical_block_size,
        .bytes_read = 0,
        .bytes_written = 0,
    };
    SCSIDeviceFunc fn = {
        .ctx = &ctx,
        .read = sd_raw_read,
        .write = sd_raw_write,
        .num_blocks = sd_num_blocks,
        .eject = sd_eject,
    };
    furi_hal_usb_unlock();

    MassStorageUsb* usb = mass_storage_usb_start(card_desc, fn);
    if(usb == NULL) {
        show_error_and_wait(
            "Unable to connect to computer.\nPlease try again.",
            &I_SDQuestion_35x43,
            35); // TODO: Diff icon?
        return;
    }

    find_storage_thread();

    uint64_t last_read = 0;

    // Wait for the card to be ejected
    FuriString* message = furi_string_alloc();
    while(1) {
        // Update bytes counter
        furi_string_printf(
            message,
            "Read: %llu kB\n"
            "Written: %llu kB\n"
            "\n"
            "Press Back to eject.",
            ctx.bytes_read / 1024,
            ctx.bytes_written / 1024);
        update_existing_popup(furi_string_get_cstr(message));

        if(last_read != ctx.bytes_read) {
            last_read = ctx.bytes_read;
            notify(NULL);
            furi_delay_ms(2);
            notify(&led_green);
            furi_delay_ms(2);
        }

        // Quit if the card is removed
        if(!furi_hal_sd_is_present()) break;

        // Stop if the card is ejected (from the computer) or the back button is pressed
        uint32_t flags =
            furi_thread_flags_wait(FlagEject | FlagBackButtonPressed, FuriFlagNoClear, 50);
        if(flags != FuriFlagErrorTimeout) break;
    }

    update_existing_popup("Ejecting card, please wait...");

    // Cleanup
    mass_storage_usb_stop(usb);
}

int32_t sd_mounter_app(void* p) {
    UNUSED(p);

    ui_init();

    /*
    * Initialize Storage
    * Unmounts SD card and suspends the Storage thred
    */
    storage = furi_record_open(RECORD_STORAGE);
    FS_Error error = storage_sd_unmount(storage);
    switch(error) {
    case FSE_OK: // Success
    case FSE_NOT_READY: // Already unmounted
        break;

    case FSE_DENIED: // Can't unmount, open files
        show_error_and_wait(
            "Can't unmount SD\ncard. Please close all\nopen files.",
            &I_SDQuestion_35x43,
            35); // TODO: Diff icon?
        cleanup();
        return -1;

    default:
        show_error_and_wait(
            "Can't unmount SD\ncard due to an\nunknown error",
            &I_SDQuestion_35x43,
            35); // TODO: Diff icon?
        cleanup();
        return -1;
    }

    if(!find_storage_thread()) {
        show_error_and_wait(
            "Unable to find storage thread!\nPlease report this error to the\napplication developer.",
            &I_Error_62x31,
            62);
        cleanup();
        return -1;
    }

    // Call `storage_sd_status()` to block until the service's queue is empty, then immediately suspend.
    // This ensures that the service will not be in the middle of anything when we suspend it.
    storage_sd_status(storage);
    furi_thread_suspend(storage_thread);

    /*
    * Identify SD Card
    * Stores infomation about the current SD card, to check whether it has been reinserted
    */

    FuriHalSdInfo* target_card_info = NULL;
    show("Scanning card, please wait...\nDO NOT REMOVE CARD!");
    target_card_info = try_get_sd_info();
    if(target_card_info == NULL) {
        show_error_and_wait(
            "Error getting SD card\ninformation!", &I_SDQuestion_35x43, 35); // TODO: Diff icon?
        cleanup();
        return -1;
    }
    FuriString* original_card_desc = get_card_desc(*target_card_info);

    FURI_LOG_D(TAG, "Original card info: '%s'", furi_string_get_cstr(original_card_desc));

    /*
    * Main Loop
    * This loop runs until the user presses the Back button
    */
    FuriHalSdInfo* card_info = NULL;
    FuriString* card_desc = NULL;
    while(!back_button_was_pressed(true)) {
        show("Insert an SD card or\npress Back to exit.");
        // Wait for the card to be inserted or the back button to be pressed
        notify(&led_blink_cyan);
        while(!furi_hal_sd_is_present() && !back_button_was_pressed(false))
            furi_thread_yield();

        // If the back button was pressed, attempt to quit the app
        if(back_button_was_pressed(true)) break;

        show("Scanning card, please wait...\nDO NOT REMOVE CARD!");

        // Try to fetch the card info
        card_info = try_get_sd_info();

        // Fetching the card info can take a while so check again if the back button was pressed
        if(back_button_was_pressed(true)) break;

        if(card_info == NULL) {
            show("Failed to get card info!\nRemove card and try again.");
            // Wait for card to be removed before continuing
            notify(&led_red);
            while(furi_hal_sd_is_present())
                furi_thread_yield();
            continue;
        }

        card_desc = get_card_desc(*card_info);
        notify(&led_green);

        // Clear SD card cache and start USB mass storage interface
        FURI_LOG_D(TAG, "Card info: '%s'", furi_string_get_cstr(card_desc));
        sd_init();
        do_mass_storage(*card_info);

        // Card ejected, wait for it to be removed
        show("SD Card ejected,\nplease remove it.\nPress Back to exit.");
        notify(&led_blink_yellow_slow);
        while(furi_hal_sd_is_present() && !back_button_was_pressed(false))
            furi_thread_yield();
    };

    /*
    * Wait for user to re-insert Flipper's original SD card before exiting
    */
    card_info = NULL;
    card_desc = NULL;
    notify(&led_blink_cyan);
    while(1) {
        show("Please reinsert the\nFlipper's original SD\ncard to exit this app.");
        if(furi_hal_sd_is_present()) {
            show("Scanning card, please wait...\nDO NOT REMOVE CARD!");
            card_info = try_get_sd_info();
            if(card_info != NULL) {
                card_desc = get_card_desc(*card_info);
            }

            // If the card is the correct one, exit the loop
            if(card_info != NULL && card_desc != NULL) {
                if(furi_string_equal(card_desc, original_card_desc)) {
                    notify(&led_green);
                    break;
                }
            }

            // Otherwise, show a message and wait for the card to be removed
            notify(&led_red);
            show("Wrong SD card. Reinsert\nthe Flipper's original SD\ncard to exit this app.");
            while(furi_hal_sd_is_present())
                furi_thread_yield();
            notify(&led_blink_cyan);
        }
    }

    notify(NULL);
    show("Flipper card detected.\nGoodbye!");
    furi_delay_ms(500);

    cleanup();
    return 0;
}

void cleanup() {
    /*
    * Cleanup
    * Resume storage thread and remount the SD card, then clean up UI resources
    */

    // Re-init SD card to ensure it is ready for use by the system
    sd_init();

    // If the storage thread was found, resume it
    if(storage_thread != NULL) {
        furi_thread_resume(storage_thread);
    }

    // Make sure the SD card is mounted before exiting
    storage_sd_mount(storage);

    if(storage != NULL) {
        furi_record_close(RECORD_STORAGE);
    }

    ui_cleanup();
}
