#include "view_clone_banks.h"
#include "../helpers/saved_epc_functions.h"

static const char* const CLONE_ON_OFF[] = {"OFF", "ON"};

// ─── helpers ─────────────────────────────────────────────────────────────────

static void clone_set_reserved_value(UHFReaderApp* App, bool enabled) {
    size_t source_bytes = App->CloneSourceTag->reserved->size;

    if(source_bytes < 4U) {
        variable_item_set_current_value_text(App->CloneResItem, "N/A");
        return;
    }

    char label[18];

    if(enabled) {
        if(source_bytes > 8U) {
            snprintf(label, sizeof(label), "ON 8/%uB", (unsigned int)source_bytes);
        } else if(source_bytes >= 8U) {
            snprintf(label, sizeof(label), "ON 8B");
        } else {
            snprintf(label, sizeof(label), "ON Kill4B");
        }
    } else {
        if(source_bytes > 8U) {
            snprintf(label, sizeof(label), "OFF 8/%uB", (unsigned int)source_bytes);
        } else {
            snprintf(label, sizeof(label), "OFF");
        }
    }

    variable_item_set_current_value_text(App->CloneResItem, label);
}

static void clone_update_start_item(UHFReaderApp* App) {
    const char* text = "---";

    if(App->CloneMask) {
        text = "OK >";
    }

    variable_item_set_current_value_text(App->CloneStartItem, text);
}

// Build a deep copy of the source tag into App->CloneSourceTag.
// For ActionFromLive the data comes from the ViewEpc model (hex strings).
// For ActionFromSaved it comes from Saved_EPCs.txt at SelectedTagIndex.
static void build_clone_source_tag(UHFReaderApp* App) {
    uhf_tag_reset(App->CloneSourceTag);

    if(App->ActionContext == ActionFromLive) {
        FuriString* Epc = furi_string_alloc();
        FuriString* Tid = furi_string_alloc();
        FuriString* Res = furi_string_alloc();
        FuriString* Mem = furi_string_alloc();
        FuriString* Pc = furi_string_alloc();
        FuriString* Crc = furi_string_alloc();

        with_view_model(
            App->ViewEpc,
            UHFRFIDTagModel * Live,
            {
                furi_string_set(Epc, Live->Epc);
                furi_string_set(Tid, Live->Tid);
                furi_string_set(Res, Live->Reserved);
                furi_string_set(Mem, Live->User);
                furi_string_set(Pc, Live->Pc);
                furi_string_set(Crc, Live->Crc);
            },
            false);

        // EPC
        uint8_t epc_bytes[EPC_MAX_BANK_SIZE] = {0};
        size_t epc_len = 0;
        hex_string_to_bytes(furi_string_get_cstr(Epc), epc_bytes, sizeof(epc_bytes), &epc_len);
        if(epc_len > 0) uhf_tag_set_epc(App->CloneSourceTag, epc_bytes, epc_len);

        uint16_t pc_arr[4] = {0};
        size_t pc_len = 0;
        hex_string_to_uint16(furi_string_get_cstr(Pc), pc_arr, COUNT_OF(pc_arr), &pc_len);
        if(pc_len > 0) uhf_tag_set_epc_pc(App->CloneSourceTag, pc_arr[0]);

        uint16_t crc_arr[4] = {0};
        size_t crc_len = 0;
        hex_string_to_uint16(furi_string_get_cstr(Crc), crc_arr, COUNT_OF(crc_arr), &crc_len);
        if(crc_len > 0) uhf_tag_set_epc_crc(App->CloneSourceTag, crc_arr[0]);

        // TID
        uint8_t tid_bytes[TID_MAX_BANK_SIZE] = {0};
        size_t tid_len = 0;
        hex_string_to_bytes(furi_string_get_cstr(Tid), tid_bytes, sizeof(tid_bytes), &tid_len);
        if(tid_len > 0) uhf_tag_set_tid(App->CloneSourceTag, tid_bytes, tid_len);

        // User
        uint8_t user_bytes[USER_MAX_BANK_SIZE] = {0};
        size_t user_len = 0;
        hex_string_to_bytes(furi_string_get_cstr(Mem), user_bytes, sizeof(user_bytes), &user_len);
        if(user_len > 0) uhf_tag_set_user(App->CloneSourceTag, user_bytes, user_len);

        // Reserved (first 4 bytes = kill pwd, next 4 = access pwd)
        uint8_t res_bytes[RESERVED_MAX_BANK_SIZE] = {0};
        size_t res_len = 0;
        hex_string_to_bytes(furi_string_get_cstr(Res), res_bytes, sizeof(res_bytes), &res_len);
        if(res_len >= 4) uhf_tag_set_kill_pwd(App->CloneSourceTag, res_bytes, 4);
        if(res_len >= 8) uhf_tag_set_access_pwd(App->CloneSourceTag, res_bytes + 4, 4);
        App->CloneSourceTag->reserved->size = res_len;

        furi_string_free(Epc);
        furi_string_free(Tid);
        furi_string_free(Res);
        furi_string_free(Mem);
        furi_string_free(Pc);
        furi_string_free(Crc);

    } else {
        // ActionFromSaved — read from Saved_EPCs.txt
        FuriString* TempStr = furi_string_alloc();
        FuriString* TempTag = furi_string_alloc();

        if(flipper_format_file_open_existing(App->EpcFile, APP_DATA_PATH("Saved_EPCs.txt"))) {
            furi_string_printf(TempStr, "Tag%ld", App->SelectedTagIndex);
            if(flipper_format_read_string(App->EpcFile, furi_string_get_cstr(TempStr), TempTag)) {
                const char* s = furi_string_get_cstr(TempTag);

                char* epc_str = extract_epc(s);
                char* tid_str = extract_tid(s);
                char* res_str = extract_res(s);
                char* mem_str = extract_mem(s);
                char* pc_str = extract_pc(s);
                char* crc_str = extract_crc(s);

                if(epc_str) {
                    uint8_t b[EPC_MAX_BANK_SIZE] = {0};
                    size_t l = 0;
                    hex_string_to_bytes(epc_str, b, sizeof(b), &l);
                    if(l > 0) uhf_tag_set_epc(App->CloneSourceTag, b, l);
                    free(epc_str);
                }
                if(pc_str) {
                    uint16_t arr[4] = {0};
                    size_t l = 0;
                    hex_string_to_uint16(pc_str, arr, COUNT_OF(arr), &l);
                    if(l > 0) uhf_tag_set_epc_pc(App->CloneSourceTag, arr[0]);
                    free(pc_str);
                }
                if(crc_str) {
                    uint16_t arr[4] = {0};
                    size_t l = 0;
                    hex_string_to_uint16(crc_str, arr, COUNT_OF(arr), &l);
                    if(l > 0) uhf_tag_set_epc_crc(App->CloneSourceTag, arr[0]);
                    free(crc_str);
                }
                if(tid_str) {
                    uint8_t b[TID_MAX_BANK_SIZE] = {0};
                    size_t l = 0;
                    hex_string_to_bytes(tid_str, b, sizeof(b), &l);
                    if(l > 0) uhf_tag_set_tid(App->CloneSourceTag, b, l);
                    free(tid_str);
                }
                if(mem_str) {
                    uint8_t b[USER_MAX_BANK_SIZE] = {0};
                    size_t l = 0;
                    hex_string_to_bytes(mem_str, b, sizeof(b), &l);
                    if(l > 0) uhf_tag_set_user(App->CloneSourceTag, b, l);
                    free(mem_str);
                }
                if(res_str) {
                    uint8_t b[RESERVED_MAX_BANK_SIZE] = {0};
                    size_t l = 0;
                    hex_string_to_bytes(res_str, b, sizeof(b), &l);
                    if(l >= 4) uhf_tag_set_kill_pwd(App->CloneSourceTag, b, 4);
                    if(l >= 8) uhf_tag_set_access_pwd(App->CloneSourceTag, b + 4, 4);
                    App->CloneSourceTag->reserved->size = l;
                    free(res_str);
                }
            }
            flipper_format_file_close(App->EpcFile);
        }

        furi_string_free(TempStr);
        furi_string_free(TempTag);
    }
}

// ─── VariableItem change callbacks ───────────────────────────────────────────

static void clone_epc_changed(VariableItem* item) {
    UHFReaderApp* App = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);

    // Clone_PC3400 requires the Saved 96-bit EPC/PC pair. EPC cannot be disabled.
    if(App->ClonePc3400Mode) {
        variable_item_set_current_value_index(item, 1);
        variable_item_set_current_value_text(item, "FORCED");
        App->CloneMask |= WRITE_EPC;
        App->CloneMask |= UHF_CLONE_FORCE_PC3400;
        clone_update_start_item(App);
        return;
    }

    // Reject ON if no EPC data
    if(idx == 1 && App->CloneSourceTag->epc->size == 0) {
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, "N/A");
        return;
    }
    variable_item_set_current_value_text(item, CLONE_ON_OFF[idx]);
    if(idx)
        App->CloneMask |= WRITE_EPC;
    else
        App->CloneMask &= ~(uint16_t)WRITE_EPC;
    clone_update_start_item(App);
}

static void clone_tid_changed(VariableItem* item) {
    UHFReaderApp* App = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx == 1 && App->CloneSourceTag->tid->size == 0) {
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, "N/A");
        return;
    }
    variable_item_set_current_value_text(item, CLONE_ON_OFF[idx]);
    if(idx)
        App->CloneMask |= WRITE_TID;
    else
        App->CloneMask &= ~(uint16_t)WRITE_TID;
    clone_update_start_item(App);
}

static void clone_user_changed(VariableItem* item) {
    UHFReaderApp* App = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    bool source_has_user = App->CloneSourceTag->user->size > 0;

    if(App->ClonePc3400Mode) {
        /*
         * Dedicated mode deliberately differs from the source User bank. The
         * target TID model selects a marker learned by Test UMI Auto; the rest
         * of User is copied/zeroed using the capacity-aware path.
         */
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, "Auto TID");
        App->CloneMask |= UHF_CLONE_FORCE_PC3400;
        App->CloneMask &= ~(uint16_t)UHF_CLONE_FORCE_USER_ZERO;

        if(source_has_user) {
            App->CloneMask |= WRITE_USER;
        } else {
            App->CloneMask &= ~(uint16_t)WRITE_USER;
            App->CloneMask |= UHF_CLONE_FORCE_USER_ZERO;
        }

        UHF_I(
            "CLONE3400",
            "User selector AUTO-TID source_bytes=%u registry_entries=%u mask=0x%04X",
            (unsigned int)App->CloneSourceTag->user->size,
            (unsigned int)App->UmiMarkerRegistry.count,
            (unsigned int)App->CloneMask);
        uhf_debug_flush();

        clone_update_start_item(App);
        return;
    }

    if(source_has_user) {
        App->CloneMask &= ~(uint16_t)UHF_CLONE_FORCE_USER_ZERO;
        variable_item_set_current_value_text(item, CLONE_ON_OFF[idx]);

        if(idx)
            App->CloneMask |= WRITE_USER;
        else
            App->CloneMask &= ~(uint16_t)WRITE_USER;
    } else {
        /*
         * Source has no User bytes:
         *   N/A    = do not touch target User
         *   FORCED = discover target User capacity and zero-fill it
         */
        App->CloneMask &= ~(uint16_t)WRITE_USER;

        if(idx) {
            App->CloneMask |= UHF_CLONE_FORCE_USER_ZERO;
            variable_item_set_current_value_text(item, "FORCED");
        } else {
            App->CloneMask &= ~(uint16_t)UHF_CLONE_FORCE_USER_ZERO;
            variable_item_set_current_value_text(item, "N/A");
        }
    }

    UHF_I(
        "CLONE",
        "User selector source_bytes=%u index=%u force_zero=%d mask=0x%04X",
        (unsigned int)App->CloneSourceTag->user->size,
        (unsigned int)idx,
        (App->CloneMask & UHF_CLONE_FORCE_USER_ZERO) ? 1 : 0,
        (unsigned int)App->CloneMask);
    uhf_debug_flush();

    clone_update_start_item(App);
}

static void clone_res_changed(VariableItem* item) {
    UHFReaderApp* App = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    if(idx == 1 && App->CloneSourceTag->reserved->size < 4) {
        variable_item_set_current_value_index(item, 0);
        variable_item_set_current_value_text(item, "N/A");
        return;
    }
    clone_set_reserved_value(App, idx != 0U);
    if(idx)
        App->CloneMask |= WRITE_RFU;
    else
        App->CloneMask &= ~(uint16_t)WRITE_RFU;

    UHF_I(
        "CLONE",
        "Reserved PW selector source=%u enabled=%u max_standard_write=%u",
        (unsigned int)App->CloneSourceTag->reserved->size,
        (unsigned int)idx,
        (unsigned int)(App->CloneSourceTag->reserved->size >= 8U ? 8U :
                       App->CloneSourceTag->reserved->size >= 4U ? 4U :
                                                                   0U));
    uhf_debug_flush();

    clone_update_start_item(App);
}

static void clone_build_menu_items(UHFReaderApp* App) {
    variable_item_list_reset(App->VariableItemListClone);

    App->CloneEpcItem =
        variable_item_list_add(App->VariableItemListClone, "EPC", 2, clone_epc_changed, App);

    App->CloneTidItem = variable_item_list_add(
        App->VariableItemListClone,
        App->ClonePc3400Mode ? "TID" : "TID (RW tag only)",
        2,
        clone_tid_changed,
        App);

    if(App->ClonePc3400Mode) {
        /* The target TID model chooses the tested marker at write time. */
        App->CloneUserItem =
            variable_item_list_add(App->VariableItemListClone, "User PC3400", 1, NULL, App);
    } else {
        App->CloneUserItem =
            variable_item_list_add(App->VariableItemListClone, "User", 2, clone_user_changed, App);
    }

    App->CloneResItem = variable_item_list_add(
        App->VariableItemListClone, "Reserved[!]", 2, clone_res_changed, App);

    /* Brackets plus the explicit OK value make this action read as a button. */
    App->CloneStartItem =
        variable_item_list_add(App->VariableItemListClone, "[ START CLONE ]", 1, NULL, App);
    variable_item_set_current_value_text(App->CloneStartItem, "---");

    variable_item_list_set_selected_item(App->VariableItemListClone, CLONE_BANKS_IDX_EPC);
}

// ─── Enter callback (item selection) ─────────────────────────────────────────

static uint32_t clone_pc3400_info_back(void* context) {
    UNUSED(context);
    return UHFReaderViewCloneBanks;
}

static void clone_pc3400_info_result(DialogExResult result, void* context) {
    if(result != DialogExResultCenter) return;

    UHFReaderApp* App = context;
    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewCloneBanks);
}

static void clone_pc3400_show_info(UHFReaderApp* App) {
    if(!App->ClonePc3400InfoDialog) {
        App->ClonePc3400InfoDialog = dialog_ex_alloc();
        if(!App->ClonePc3400InfoDialog) {
            uhf_notify_error(App);
            return;
        }

        dialog_ex_set_header(
            App->ClonePc3400InfoDialog, "Why PC3400?", 64, 8, AlignCenter, AlignCenter);
        dialog_ex_set_text(
            App->ClonePc3400InfoDialog,
            "Target TID selects a\ntested User marker.\nUnknown: run UMI test.",
            64,
            30,
            AlignCenter,
            AlignCenter);
        dialog_ex_set_center_button_text(App->ClonePc3400InfoDialog, "Back");
        dialog_ex_set_context(App->ClonePc3400InfoDialog, App);
        dialog_ex_set_result_callback(App->ClonePc3400InfoDialog, clone_pc3400_info_result);
        view_set_previous_callback(
            dialog_ex_get_view(App->ClonePc3400InfoDialog), clone_pc3400_info_back);
        view_dispatcher_add_view(
            App->ViewDispatcher,
            UHFReaderViewClonePc3400Info,
            dialog_ex_get_view(App->ClonePc3400InfoDialog));
    }

    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewClonePc3400Info);
}

static void clone_banks_item_enter(void* context, uint32_t index) {
    UHFReaderApp* App = context;

    if(index == CLONE_BANKS_IDX_USER && App->ClonePc3400Mode) {
        clone_pc3400_show_info(App);
        return;
    }

    if(index != CLONE_BANKS_IDX_START) return;
    if(!App->CloneMask) return;
    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewClone);
}

// ─── Navigation ──────────────────────────────────────────────────────────────

uint32_t uhf_reader_navigation_clone_banks_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewTagAction;
}

// ─── View enter callback ──────────────────────────────────────────────────────

void view_clone_banks_enter_callback(void* context) {
    UHFReaderApp* App = context;
    // Deep-copy the source tag at entry time (safe snapshot, worker can't mutate it)
    build_clone_source_tag(App);

    // Reset selection mask
    App->CloneMask = 0;

    bool epc_avail = (App->CloneSourceTag->epc->size > 0);
    bool tid_avail = (App->CloneSourceTag->tid->size > 0);
    bool user_avail = (App->CloneSourceTag->user->size > 0);
    bool res_avail = (App->CloneSourceTag->reserved->size >= 4);

    if(App->ClonePc3400Mode && App->CloneSourceTag->epc->pc != UHF_CLONE_PC3400_SOURCE_PC) {
        UHF_W(
            "CLONE3400",
            "BANKS source PC mismatch got=0x%04X expected=0x%04X -> disable mode",
            (unsigned int)App->CloneSourceTag->epc->pc,
            (unsigned int)UHF_CLONE_PC3400_SOURCE_PC);
        App->ClonePc3400Mode = false;
    }

    /* Labels differ between normal Clone and the dedicated PC3400 workflow. */
    clone_build_menu_items(App);

    // EPC: default ON when data is present. Dedicated 3400 mode forces EPC ON.
    variable_item_set_current_value_index(App->CloneEpcItem, epc_avail ? 1 : 0);
    if(App->ClonePc3400Mode && epc_avail) {
        variable_item_set_current_value_text(App->CloneEpcItem, "FORCED");
        App->CloneMask |= WRITE_EPC;
        App->CloneMask |= UHF_CLONE_FORCE_PC3400;
    } else {
        variable_item_set_current_value_text(App->CloneEpcItem, epc_avail ? "ON" : "N/A");
        if(epc_avail) App->CloneMask |= WRITE_EPC;
    }

    // TID: default ON when present. YRM100X_PRO is explicitly used with
    // rewritable-TID test tags; incompatible target tags will simply reject it.
    variable_item_set_current_value_index(App->CloneTidItem, tid_avail ? 1 : 0);
    variable_item_set_current_value_text(App->CloneTidItem, tid_avail ? "ON" : "N/A");
    if(tid_avail) App->CloneMask |= WRITE_TID;

    // User: dedicated PC3400 mode always requires a model-matched marker.
    if(App->ClonePc3400Mode) {
        variable_item_set_current_value_index(App->CloneUserItem, 0);
        variable_item_set_current_value_text(App->CloneUserItem, "Auto TID");
        App->CloneMask |= UHF_CLONE_FORCE_PC3400;

        if(user_avail) {
            App->CloneMask |= WRITE_USER;
            App->CloneMask &= ~(uint16_t)UHF_CLONE_FORCE_USER_ZERO;
        } else {
            App->CloneMask &= ~(uint16_t)WRITE_USER;
            App->CloneMask |= UHF_CLONE_FORCE_USER_ZERO;
        }

        UHF_I(
            "CLONE3400",
            "BANKS READY source_pc=0x%04X source_user=%u marker=auto-by-TID "
            "registry_entries=%u mask=0x%04X",
            (unsigned int)App->CloneSourceTag->epc->pc,
            (unsigned int)App->CloneSourceTag->user->size,
            (unsigned int)App->UmiMarkerRegistry.count,
            (unsigned int)App->CloneMask);
        uhf_debug_flush();
    } else {
        // Normal clone: source data -> ON; no source data -> N/A/FORCED zero.
        variable_item_set_current_value_index(App->CloneUserItem, user_avail ? 1 : 0);
        variable_item_set_current_value_text(App->CloneUserItem, user_avail ? "ON" : "N/A");
        if(user_avail) {
            App->CloneMask |= WRITE_USER;
        } else {
            App->CloneMask &= ~(uint16_t)WRITE_USER;
            App->CloneMask &= ~(uint16_t)UHF_CLONE_FORCE_USER_ZERO;
        }
    }

    // Reserved PW stays OFF; target capacity is measured after target scan.
    variable_item_set_current_value_index(App->CloneResItem, 0);
    if(res_avail)
        clone_set_reserved_value(App, false);
    else
        variable_item_set_current_value_text(App->CloneResItem, "N/A");

    clone_update_start_item(App);
}

// ─── Alloc / Free ─────────────────────────────────────────────────────────────

void view_clone_banks_alloc(UHFReaderApp* App) {
    App->CloneSourceTag = uhf_tag_alloc();
    App->CloneMask = 0;
    App->ClonePc3400InfoDialog = NULL;

    App->VariableItemListClone = variable_item_list_alloc();

    variable_item_list_set_enter_callback(App->VariableItemListClone, clone_banks_item_enter, App);

    View* vil_view = variable_item_list_get_view(App->VariableItemListClone);
    view_set_previous_callback(vil_view, uhf_reader_navigation_clone_banks_callback);

    view_dispatcher_add_view(App->ViewDispatcher, UHFReaderViewCloneBanks, vil_view);
}

void view_clone_banks_free(UHFReaderApp* App) {
    if(App->ClonePc3400InfoDialog) {
        view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewClonePc3400Info);
        dialog_ex_free(App->ClonePc3400InfoDialog);
        App->ClonePc3400InfoDialog = NULL;
    }

    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewCloneBanks);
    variable_item_list_free(App->VariableItemListClone);
    App->VariableItemListClone = NULL;

    uhf_tag_free(App->CloneSourceTag);
    App->CloneSourceTag = NULL;
}
