#include "../passy_i.h"
#include <dolphin/dolphin.h>

#define ASN_EMIT_DEBUG 0
#include <lib/asn1/DG1.h>

#define TAG "PassySceneReadCardSuccess"
// Thank you proxmark code for your passport parsing

void passy_scene_read_success_on_enter(void* context) {
    Passy* passy = context;

    dolphin_deed(DolphinDeedNfcReadSuccess);
    notification_message(passy->notifications, &sequence_success);

    furi_string_reset(passy->text_box_store);
    FuriString* str = passy->text_box_store;
    if(passy->read_type == PassyReadDG1) {
        DG1_t* dg1 = 0;
        dg1 = calloc(1, sizeof *dg1);
        assert(dg1);
        asn_dec_rval_t rval = asn_decode(
            0,
            ATS_DER,
            &asn_DEF_DG1,
            (void**)&dg1,
            bit_buffer_get_data(passy->DG1),
            bit_buffer_get_size_bytes(passy->DG1));

        if(rval.code == RC_OK) {
            FURI_LOG_I(TAG, "ASN.1 decode success");

            char payloadDebug[384] = {0};
            memset(payloadDebug, 0, sizeof(payloadDebug));
            (&asn_DEF_DG1)
                ->op->print_struct(
                    &asn_DEF_DG1, dg1, 1, passy_print_struct_callback, payloadDebug);
            if(strlen(payloadDebug) > 0) {
                FURI_LOG_D(TAG, "DG1: %s", payloadDebug);
            } else {
                FURI_LOG_D(TAG, "Received empty Payload");
            }

            if(dg1->mrz.buf[0] == 'I' && dg1->mrz.buf[1] == 'P') {
                furi_string_cat_printf(str, "Passport card (IP)\n");
            } else if(dg1->mrz.buf[0] == 'I') {
                furi_string_cat_printf(str, "ID Card (I)\n");
            } else if(dg1->mrz.buf[0] == 'P') {
                furi_string_cat_printf(str, "Passport book (P)\n");
            } else if(dg1->mrz.buf[0] == 'A') {
                furi_string_cat_printf(str, "Residency Permit (A)\n");
            } else {
                furi_string_cat_printf(str, "Unknown (%c%c)\n", dg1->mrz.buf[0], dg1->mrz.buf[1]);
            }

            uint8_t td_variant = 0;
            if(dg1->mrz.size == 90) {
                td_variant = 1;
            } else if(dg1->mrz.size == 88) {
                td_variant = 3;
            } else {
                FURI_LOG_W(TAG, "MRZ length (%zu) is unexpected.", dg1->mrz.size);
            }

            char name[40] = {0};
            memset(name, 0, sizeof(name));
            uint8_t name_offset = td_variant == 3 ? 5 : 60;
            memcpy(name, dg1->mrz.buf + name_offset, 38);
            // Work backwards replace < at the end with \0
            for(size_t i = sizeof(name) - 1; i > 0; i--) {
                if(name[i] == '<') {
                    name[i] = '\0';
                } else {
                    break;
                }
            }
            // Work forwards replace < with space
            for(size_t i = 0; i < sizeof(name); i++) {
                if(name[i] == '<') {
                    name[i] = ' ';
                }
            }

            if(td_variant == 3) { // Passport form factor
                char* row_1 = (char*)dg1->mrz.buf + 0;
                char* row_2 = (char*)dg1->mrz.buf + 44;

                furi_string_cat_printf(str, "Issuing state: %.3s\n", row_1 + 2);
                furi_string_cat_printf(str, "Nationality: %.3s\n", row_2 + 10);
                furi_string_cat_printf(str, "Name: %s\n", name);
                furi_string_cat_printf(str, "Doc Number: %.9s\n", row_2);
                furi_string_cat_printf(str, "DoB: %.6s\n", row_2 + 13);
                furi_string_cat_printf(str, "Sex: %.1s\n", row_2 + 20);
                furi_string_cat_printf(str, "Expiry: %.6s\n", row_2 + 21);

                furi_string_cat_printf(str, "\n");
                furi_string_cat_printf(str, "Raw data:\n");
                furi_string_cat_printf(str, "%.44s\n", row_1);
                furi_string_cat_printf(str, "%.44s\n", row_2);
            } else if(td_variant == 1) { // ID form factor
                char* row_1 = (char*)dg1->mrz.buf + 0;
                char* row_2 = (char*)dg1->mrz.buf + 30;
                char* row_3 = (char*)dg1->mrz.buf + 60;

                furi_string_cat_printf(str, "Issuing state: %.3s\n", row_1 + 2);
                furi_string_cat_printf(str, "Nationality: %.3s\n", row_2 + 15);
                furi_string_cat_printf(str, "Name: %s\n", name);
                furi_string_cat_printf(str, "Doc Number: %.9s\n", row_1 + 5);
                furi_string_cat_printf(str, "DoB: %.6s\n", row_2);
                furi_string_cat_printf(str, "Sex: %.1s\n", row_2 + 7);
                furi_string_cat_printf(str, "Expiry: %.6s\n", row_2 + 8);

                furi_string_cat_printf(str, "\n");
                furi_string_cat_printf(str, "Raw data:\n");
                furi_string_cat_printf(str, "%.30s\n", row_1);
                furi_string_cat_printf(str, "%.30s\n", row_2);
                furi_string_cat_printf(str, "%.30s\n", row_3);
            }

        } else {
            FURI_LOG_E(TAG, "ASN.1 decode failed: %d.  %d consumed", rval.code, rval.consumed);
            furi_string_cat_printf(str, "%s\n", bit_buffer_get_data(passy->DG1));
        }

        free(dg1);
        dg1 = 0;

    } else if(passy->read_type == PassyReadDG2 || passy->read_type == PassyReadDG7) {
        const char* dg_type = passy->read_type == PassyReadDG2 ? "DG2" : "DG7";
        furi_string_cat_printf(
            str, "Saved to apps_data/passy/%s-%s.*\n", passy->passport_number, dg_type);
    } else {
        int dg_number = passy->read_type & 0xFF;
        furi_string_cat_printf(
            str, "Saved to apps_data/passy/%s-DG%d.bin\n", passy->passport_number, dg_number);
    }
    text_box_set_font(passy->text_box, TextBoxFontText);
    text_box_set_text(passy->text_box, furi_string_get_cstr(passy->text_box_store));
    view_dispatcher_switch_to_view(passy->view_dispatcher, PassyViewTextBox);
}

bool passy_scene_read_success_on_event(void* context, SceneManagerEvent event) {
    Passy* passy = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
    } else if(event.type == SceneManagerEventTypeBack) {
        const uint32_t possible_scenes[] = {PassySceneAdvancedMenu, PassySceneMainMenu};
        scene_manager_search_and_switch_to_previous_scene_one_of(
            passy->scene_manager, possible_scenes, COUNT_OF(possible_scenes));
        consumed = true;
    }
    return consumed;
}

void passy_scene_read_success_on_exit(void* context) {
    Passy* passy = context;

    // Clear view
    text_box_reset(passy->text_box);
}
