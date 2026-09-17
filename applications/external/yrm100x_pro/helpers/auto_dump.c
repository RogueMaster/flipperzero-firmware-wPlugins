#include "auto_dump.h"
#include "../app.h"
#include "saved_epc_functions.h"
#include "debug_log.h"
#include <furi/core/memmgr.h>

#define UHF_AUTO_DUMP_FILE_TYPE     "YRM100X_PRO UHF Full Dump"
#define UHF_AUTO_DUMP_VERSION       3U
#define UHF_AUTO_DUMP_EXT           ".uhf"
#define UHF_AUTO_DUMP_MIN_FREE_HEAP 4096U

static const char* uhf_dump_profile_name(uint8_t profile) {
    return profile == UHF_READER_TAG_PROFILE_MONZA4QT ? "Impinj Monza 4QT" : "Generic";
}

static bool uhf_auto_dump_write_file(UHFReaderApp* App, UHFTag* tag, const char* display_name) {
    if(!App || !tag || !tag->epc || tag->epc->size == 0) return false;

    const size_t free_heap = memmgr_get_free_heap();
    if(free_heap < UHF_AUTO_DUMP_MIN_FREE_HEAP) {
        UHF_E(
            "SAVE",
            "SKIP low heap free=%lu required=%u",
            (unsigned long)free_heap,
            (unsigned int)UHF_AUTO_DUMP_MIN_FREE_HEAP);
        uhf_debug_flush();
        return false;
    }

    UHF_I(
        "SAVE",
        "BEGIN base=%s complete=%d profile=%s match=%d epc=%u tid=%u user=%u reserved=%u",
        display_name ? display_name : "?",
        tag->full_dump_complete ? 1 : 0,
        uhf_dump_profile_name(tag->read_profile),
        tag->profile_match ? 1 : 0,
        (unsigned int)tag->epc->size,
        (unsigned int)tag->tid->size,
        (unsigned int)tag->user->size,
        (unsigned int)tag->reserved->size);
    uhf_debug_hex(UHFDebugInfo, "SAVE", "EPC", tag->epc->data, tag->epc->size);

    if(!storage_simply_mkdir(App->TagStorage, UHF_AUTO_DUMP_DIR)) {
        UHF_E("SAVE", "mkdir dump dir failed");
        uhf_debug_flush();
        return false;
    }

    FuriString* next_name = furi_string_alloc();
    FuriString* full_path = furi_string_alloc();

    storage_get_next_filename(
        App->TagStorage, UHF_AUTO_DUMP_DIR, display_name, UHF_AUTO_DUMP_EXT, next_name, 64);

    furi_string_printf(
        full_path,
        "%s/%s%s",
        UHF_AUTO_DUMP_DIR,
        furi_string_get_cstr(next_name),
        UHF_AUTO_DUMP_EXT);

    UHF_I("SAVE", "path=%s", furi_string_get_cstr(full_path));
    uhf_debug_flush();

    FlipperFormat* ff = flipper_format_file_alloc(App->TagStorage);
    bool ok = false;

    do {
        if(!flipper_format_file_open_new(ff, furi_string_get_cstr(full_path))) break;
        if(!flipper_format_write_header_cstr(ff, UHF_AUTO_DUMP_FILE_TYPE, UHF_AUTO_DUMP_VERSION))
            break;

        uint32_t epc_len = (uint32_t)tag->epc->size;
        uint32_t tid_len = (uint32_t)tag->tid->size;
        uint32_t res_len = (uint32_t)tag->reserved->size;
        uint32_t user_len = (uint32_t)tag->user->size;
        uint32_t pc = tag->epc->pc;
        uint32_t crc = tag->epc->crc;
        int32_t rssi = tag->epc->rssi;
        bool complete = tag->full_dump_complete;

        bool finalized = tag->dump_finalized;
        if(!flipper_format_write_bool(ff, "Dump Finalized", &finalized, 1)) break;
        if(!flipper_format_write_bool(ff, "Full Dump Complete", &complete, 1)) break;

        if(!flipper_format_write_string_cstr(
               ff, "Read Profile", uhf_dump_profile_name(tag->read_profile)))
            break;
        if(!flipper_format_write_bool(ff, "Profile Match", &tag->profile_match, 1)) break;

        if(!flipper_format_write_string_cstr(
               ff, "TID Status", uhf_tag_bank_status_name(tag->tid_status)))
            break;
        if(!flipper_format_write_string_cstr(
               ff, "User Status", uhf_tag_bank_status_name(tag->user_status)))
            break;
        if(!flipper_format_write_string_cstr(
               ff, "Reserved Status", uhf_tag_bank_status_name(tag->reserved_status)))
            break;

        if(!flipper_format_write_uint32(ff, "EPC Bytes", &epc_len, 1)) break;
        if(!flipper_format_write_hex(ff, "EPC", tag->epc->data, (uint16_t)tag->epc->size)) break;
        if(!flipper_format_write_uint32(ff, "PC", &pc, 1)) break;
        if(!flipper_format_write_uint32(ff, "CRC", &crc, 1)) break;
        if(!flipper_format_write_int32(ff, "RSSI dBm", &rssi, 1)) break;

        if(!flipper_format_write_uint32(ff, "TID Bytes", &tid_len, 1)) break;
        if(tid_len > 0 &&
           !flipper_format_write_hex(ff, "TID", tag->tid->data, (uint16_t)tag->tid->size)) {
            break;
        }

        if(tag->read_profile == UHF_READER_TAG_PROFILE_MONZA4QT) {
            uint32_t monza_public_len = (uint32_t)tag->monza4qt_epc_public_size;
            bool pc_umi = (tag->epc->pc & 0x0400U) != 0U;

            if(!flipper_format_write_bool(ff, "Monza4QT PC UMI", &pc_umi, 1)) break;
            if(!flipper_format_write_string_cstr(
                   ff,
                   "Monza4QT EPC Public Status",
                   uhf_tag_bank_status_name(tag->monza4qt_epc_public_status)))
                break;
            if(!flipper_format_write_uint32(ff, "Monza4QT EPC Public Bytes", &monza_public_len, 1))
                break;
            if(monza_public_len > 0 && !flipper_format_write_hex(
                                           ff,
                                           "Monza4QT EPC Public",
                                           tag->monza4qt_epc_public,
                                           (uint16_t)tag->monza4qt_epc_public_size)) {
                break;
            }
        }

        if(!flipper_format_write_uint32(ff, "Reserved Bytes", &res_len, 1)) break;
        if(res_len > 0 &&
           !flipper_format_write_hex(
               ff, "Reserved", tag->reserved->data, (uint16_t)tag->reserved->size)) {
            break;
        }

        if(!flipper_format_write_uint32(ff, "User Bytes", &user_len, 1)) break;
        if(user_len > 0 &&
           !flipper_format_write_hex(ff, "User", tag->user->data, (uint16_t)tag->user->size)) {
            break;
        }

        ok = true;
    } while(false);

    flipper_format_file_close(ff);
    flipper_format_free(ff);

    if(ok) {
        char* epc = convertToHexString(tag->epc->data, tag->epc->size);
        char* tid = convertToHexString(tag->tid->data, tag->tid->size);
        char* res = convertToHexString(tag->reserved->data, tag->reserved->size);
        char* user = convertToHexString(tag->user->data, tag->user->size);
        char* pc = uint16_to_hex_string(tag->epc->pc);
        char* crc = uint16_to_hex_string(tag->epc->crc);

        if(epc && tid && res && user && pc && crc) {
            UHF_I("SAVE", "SAVED_INDEX BEGIN name=%s", furi_string_get_cstr(next_name));
            uhf_debug_flush();

            save_uhf_tag_to_file(
                App, furi_string_get_cstr(next_name), epc, tid, res, user, pc, crc);

            UHF_I("SAVE", "SAVED_INDEX END name=%s", furi_string_get_cstr(next_name));
            uhf_debug_flush();
        }

        free(epc);
        free(tid);
        free(res);
        free(user);
        free(pc);
        free(crc);
    } else {
        storage_simply_remove(App->TagStorage, furi_string_get_cstr(full_path));
    }

    UHF_I("SAVE", "END ok=%d name=%s", ok ? 1 : 0, furi_string_get_cstr(next_name));
    uhf_debug_flush();

    furi_string_free(full_path);
    furi_string_free(next_name);
    return ok;
}

bool uhf_auto_dump_save_tag(UHFReaderApp* App, UHFTag* tag) {
    if(!App || !tag || !tag->epc || tag->epc->size == 0) return false;
    if(tag->auto_dump_saved) return true;

    char* epc = convertToHexString(tag->epc->data, tag->epc->size);
    if(!epc) return false;

    size_t epc_len = strlen(epc);
    const char* tail = epc_len > 4 ? epc + epc_len - 4 : epc;

    DateTime now = {0};
    furi_hal_rtc_get_datetime(&now);

    // FT/PT_DDMMYY-HHMMSS_EPC4, for example FT_280816-230301_ABCD.
    char base_name[40];
    snprintf(
        base_name,
        sizeof(base_name),
        "%s_%02u%02u%02u-%02u%02u%02u_%s",
        tag->full_dump_complete ? "FT" : "PT",
        (unsigned int)now.day,
        (unsigned int)now.month,
        (unsigned int)(now.year % 100U),
        (unsigned int)now.hour,
        (unsigned int)now.minute,
        (unsigned int)now.second,
        tail);

    bool ok = uhf_auto_dump_write_file(App, tag, base_name);
    if(ok) tag->auto_dump_saved = true;

    free(epc);
    return ok;
}

size_t uhf_auto_dump_save_all(UHFReaderApp* App) {
    if(!App || !App->YRM100XWorker || !App->YRM100XWorker->uhf_tag_wrapper) return 0;

    UHFTagWrapper* wrapper = App->YRM100XWorker->uhf_tag_wrapper;
    size_t saved = 0;

    for(size_t i = 0; i < wrapper->tag_count; i++) {
        UHFTag* tag = wrapper->tags[i];
        if(!tag || !tag->epc || tag->epc->size == 0) continue;
        if(uhf_auto_dump_save_tag(App, tag)) saved++;
    }

    return saved;
}

bool uhf_delete_all_saved_data(UHFReaderApp* App) {
    if(!App || !App->TagStorage) return false;

    bool ok = true;

    if(!storage_simply_remove_recursive(App->TagStorage, UHF_AUTO_DUMP_DIR)) ok = false;
    if(!storage_simply_mkdir(App->TagStorage, UHF_AUTO_DUMP_DIR)) ok = false;

    flipper_format_file_close(App->EpcFile);
    flipper_format_file_close(App->EpcIndexFile);

    if(!storage_simply_remove(App->TagStorage, APP_DATA_PATH("Saved_EPCs.txt"))) ok = false;
    if(!storage_simply_remove(App->TagStorage, APP_DATA_PATH("Index_File.txt"))) ok = false;

    if(!flipper_format_file_open_new(App->EpcIndexFile, APP_DATA_PATH("Index_File.txt"))) {
        ok = false;
    } else {
        if(!flipper_format_write_string_cstr(App->EpcIndexFile, "Number of Tags", "0")) ok = false;
        flipper_format_file_close(App->EpcIndexFile);
    }

    App->NumberOfSavedTags = 0;
    App->SavedPage = 0;
    uhf_saved_update_count_labels(App);
    if(App->SubmenuSaved) {
        uhf_reader_saved_menu_rebuild(App);
    }

    return ok;
}
