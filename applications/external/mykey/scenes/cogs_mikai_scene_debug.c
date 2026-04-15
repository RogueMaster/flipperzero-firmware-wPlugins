#include "../cogs_mikai.h"
#include <storage/storage.h>
#include <machine/endian.h>
#include <furi_hal_rtc.h>

enum {
    DebugSceneEventSaveData = 1,
};

void cogs_mikai_scene_debug_on_enter(void* context) {
    COGSMyKaiApp* app = context;
    TextBox* text_box = app->text_box;
    FuriString* text = app->text_box_store;

    furi_string_reset(text);

    if(!app->mykey.is_loaded) {
        furi_string_cat(text, "No Card Loaded\n\nPlease read a card first.");
    } else {
        furi_string_cat(text, "=== DEBUG INFO ===\n\n");

        // UID
                furi_string_cat_printf(
            text, "UID: %016llX\n", (unsigned long long)app->mykey.uid);

        // lower 32 bits of UID
        furi_string_cat_printf(
            text, "UID (lower 32): 0x%08lX\n", (uint32_t)app->mykey.uid);

        // encryption key

        furi_string_cat_printf(
            text, "Encryption Key: 0x%08lX\n\n", app->mykey.encryption_key);

        // block 0x21 (credit block) analysis
        furi_string_cat(text, "--- Block 0x21 Analysis ---\n");
        uint32_t block21_raw = app->mykey.eeprom[0x21];
        furi_string_cat_printf(text, "Raw: 0x%08lX\n", block21_raw);

        // show individual bytes
        furi_string_cat_printf(text, "Bytes: [%02X %02X %02X %02X]\n",
            (uint8_t)(block21_raw & 0xFF),
            (uint8_t)((block21_raw >> 8) & 0xFF),
            (uint8_t)((block21_raw >> 16) & 0xFF),
            (uint8_t)((block21_raw >> 24) & 0xFF));

        // uint32_t block21_xor = block21_raw ^ app->mykey.encryption_key;
        // furi_string_cat_printf(text, "After XOR: 0x%08lX\n", block21_xor);

    
        // uint32_t block21_swapped = __bswap32(block21_raw);
        // furi_string_cat_printf(text, "Byte-swapped: 0x%08lX\n", block21_swapped);

        // uint32_t block21_xor_swapped = block21_swapped ^ app->mykey.encryption_key;
        // furi_string_cat_printf(text, "Swap then XOR: 0x%08lX\n\n", block21_xor_swapped);


        // furi_string_cat(text, "--- Test Combinations ---\n");

        // uint32_t test_a = block21_raw ^ app->mykey.encryption_key;
        // mykey_encode_decode_block(&test_a);
        // uint16_t credit_a = test_a & 0xFFFF;
        // furi_string_cat_printf(text, "A (Raw->XOR->Dec): %u.%02u\n", credit_a / 100, credit_a % 100);
        // uint32_t test_b = block21_swapped ^ app->mykey.encryption_key;
        // mykey_encode_decode_block(&test_b);
        // uint16_t credit_b = test_b & 0xFFFF;
        // furi_string_cat_printf(text, "B (Swap->XOR->Dec): %u.%02u\n", credit_b / 100, credit_b % 100);

        // uint32_t test_c = __bswap32(block21_raw ^ app->mykey.encryption_key);
        // mykey_encode_decode_block(&test_c);
        // uint16_t credit_c = test_c & 0xFFFF;
        // furi_string_cat_printf(text, "C (XOR->Swap->Dec): %u.%02u\n", credit_c / 100, credit_c % 100);

        // uint32_t test_d = block21_raw;
        // mykey_encode_decode_block(&test_d);
        // test_d ^= app->mykey.encryption_key;
        // uint16_t credit_d = test_d & 0xFFFF;
        // furi_string_cat_printf(text, "D (Dec->XOR): %u.%02u\n\n", credit_d / 100, credit_d % 100);

        // credit calculations
        furi_string_cat(text, "--- Credit Readings ---\n");

        // libmikai
        uint16_t credit_libmikai = mykey_get_current_credit(&app->mykey);
        furi_string_cat_printf(
            text, "libmikai (0x21): %u.%02u EUR\n", credit_libmikai / 100, credit_libmikai % 100);

        // transaction history
        uint16_t credit_history = mykey_get_credit_from_history(&app->mykey);
        if(credit_history != 0xFFFF) {
            furi_string_cat_printf(
                text,
                "History (0x34+): %u.%02u EUR\n\n",
                credit_history / 100,
                credit_history % 100);
        } else {
            furi_string_cat(text, "History: Not available\n\n");
        }

        // key blocks
        furi_string_cat(text, "--- Key Blocks ---\n");
        furi_string_cat_printf(text, "Block 0x05: 0x%08lX\n", app->mykey.eeprom[0x05]);
        furi_string_cat_printf(text, "Block 0x06: 0x%08lX\n", app->mykey.eeprom[0x06]);
        furi_string_cat_printf(text, "Block 0x07: 0x%08lX\n", app->mykey.eeprom[0x07]);
        furi_string_cat_printf(text, "Block 0x12: 0x%08lX\n", app->mykey.eeprom[0x12]);
        furi_string_cat_printf(text, "Block 0x18: 0x%08lX\n", app->mykey.eeprom[0x18]);
        furi_string_cat_printf(text, "Block 0x19: 0x%08lX\n", app->mykey.eeprom[0x19]);
        furi_string_cat_printf(text, "Block 0x21: 0x%08lX\n", app->mykey.eeprom[0x21]);
        furi_string_cat_printf(text, "Block 0x3C: 0x%08lX\n\n", app->mykey.eeprom[0x3C]);

        furi_string_cat(text, "\n--- Raw data saved in SD ---\n");
        furi_string_cat(text, "Use back to exit, or\n");
        furi_string_cat(text, "check logs for raw data");
    }

    text_box_set_text(text_box, furi_string_get_cstr(text));
    text_box_set_font(text_box, TextBoxFontText);
    text_box_set_focus(text_box, TextBoxFocusStart);


    if(app->mykey.is_loaded) {
        DateTime datetime;
        furi_hal_rtc_get_datetime(&datetime);

        FuriString* file_path = furi_string_alloc();
        furi_string_printf(
            file_path,
            "/ext/mykey_debug_%04d%02d%02d_%02d%02d%02d.txt",
            datetime.year,
            datetime.month,
            datetime.day,
            datetime.hour,
            datetime.minute,
            datetime.second);

        Storage* storage = furi_record_open(RECORD_STORAGE);
        File* file = storage_file_alloc(storage);

        if(storage_file_open(file, furi_string_get_cstr(file_path), FSAM_WRITE, FSOM_CREATE_ALWAYS)) {

            storage_file_write(file, furi_string_get_cstr(text), furi_string_size(text));

       
            storage_file_write(file, "\n\n--- Raw Data Dump ---\n", strlen("\n\n--- Raw Data Dump ---\n"));

            FuriString* line = furi_string_alloc();
            for(size_t i = 0; i < SRIX4K_BLOCKS; i++) {
                furi_string_printf(line, "Block 0x%02zX: 0x%08lX\n", i, app->mykey.eeprom[i]);
                storage_file_write(file, furi_string_get_cstr(line), furi_string_size(line));
            }
            furi_string_free(line);

            FURI_LOG_I(TAG, "Debug data saved to: %s", furi_string_get_cstr(file_path));
            storage_file_close(file);
        } else {
            FURI_LOG_E(TAG, "Failed to save debug data");
        }

        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        furi_string_free(file_path);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewTextBox);
}

bool cogs_mikai_scene_debug_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void cogs_mikai_scene_debug_on_exit(void* context) {
    COGSMyKaiApp* app = context;
    text_box_reset(app->text_box);
    furi_string_reset(app->text_box_store);
}
