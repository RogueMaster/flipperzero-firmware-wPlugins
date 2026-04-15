#include "../cogs_mikai.h"
#include <machine/endian.h>

void cogs_mikai_scene_info_on_enter(void* context) {
    COGSMyKaiApp* app = context;
    TextBox* text_box = app->text_box;
    FuriString* text = app->text_box_store;

    furi_string_reset(text);

    if(!app->mykey.is_loaded) {
        furi_string_cat(text, "No Card Loaded\n\nPlease read a card first.");
    } else {

        furi_string_cat_printf(text, "Serial: %08lX\n", (uint32_t)app->mykey.eeprom[0x07]);

        // vendor ID - calculated from blocks 0x18 and 0x19
        uint32_t block18 = app->mykey.eeprom[0x18];
        uint32_t block19 = app->mykey.eeprom[0x19];
        mykey_encode_decode_block(&block18);
        mykey_encode_decode_block(&block19);
        uint64_t vendor = (((uint64_t)block18 << 16) | (block19 & 0x0000FFFF)) + 1;
  
        furi_string_cat_printf(text, "Vendor: %llX\n", vendor);

        // current credit
        furi_string_cat_printf(
            text,
            "Credit: %u.%02u EUR\n",
            app->mykey.current_credit / 100,
            app->mykey.current_credit % 100);

        // card status
        furi_string_cat_printf(text, "Status: %s\n", app->mykey.is_reset ? "Reset" : "Active");

        // operation count (block 0x12, lower 24 bits)
        uint32_t op_count = app->mykey.eeprom[0x12] & 0x00FFFFFF;
        furi_string_cat_printf(text, "Operations: %lu\n", (unsigned long)op_count);

        // UID
        furi_string_cat_printf(
            text,
            "UID: %08lX%08lX\n",
            (uint32_t)(app->mykey.uid >> 32),
            (uint32_t)(app->mykey.uid & 0xFFFFFFFF));

        // parse and display full transaction history
        uint32_t block3C = app->mykey.eeprom[0x3C];
        if(block3C != 0xFFFFFFFF) {
            block3C ^= app->mykey.eeprom[0x07];
            uint32_t starting_offset =
                ((block3C & 0x30000000) >> 28) | ((block3C & 0x00100000) >> 18);

            if(starting_offset < 8) {
                // first, find how many transactions exist by going forward from starting_offset
                int num_transactions = 0;
                for(int i = 0; i < 8; i++) {
                    uint32_t txn_block = app->mykey.eeprom[0x34 + ((starting_offset + i) % 8)];
                    if(txn_block == 0xFFFFFFFF) {
                        break;
                    }
                    num_transactions++;
                }

                if(num_transactions > 0) {
                    furi_string_cat(text, "\n=== Transaction History ===\n");
                    furi_string_cat(text, "(Newest first)\n\n");

                    // display transactions in reverse order (newest first)
                    for(int i = num_transactions - 1; i >= 0; i--) {
                        uint32_t txn_block = app->mykey.eeprom[0x34 + ((starting_offset + i) % 8)];

                        // extract transaction fields directly from big-endian block
                        uint8_t day = txn_block >> 27;
                        uint8_t month = (txn_block >> 23) & 0xF;
                        uint16_t year = 2000 + ((txn_block >> 16) & 0x7F);
                        uint16_t credit = txn_block & 0xFFFF;

                        furi_string_cat_printf(
                            text,
                            "%d. %02d/%02d/%04d - %d.%02d EUR\n",
                            num_transactions - i,
                            day,
                            month,
                            year,
                            credit / 100,
                            credit % 100);
                    }
                } else {
                    furi_string_cat(text, "\nNo transaction history\n");
                }
            } else {
                furi_string_cat(text, "\nTransaction history:\n");
                furi_string_cat(text, "Invalid offset\n");
            }
        } else {
            furi_string_cat(text, "\nTransaction history:\n");
            furi_string_cat(text, "Not available\n");
        }
    }

    text_box_set_text(text_box, furi_string_get_cstr(text));
    text_box_set_font(text_box, TextBoxFontText);
    text_box_set_focus(text_box, TextBoxFocusStart);

    view_dispatcher_switch_to_view(app->view_dispatcher, COGSMyKaiViewTextBox);
}

bool cogs_mikai_scene_info_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void cogs_mikai_scene_info_on_exit(void* context) {
    COGSMyKaiApp* app = context;
    text_box_reset(app->text_box);
    furi_string_reset(app->text_box_store);
}
