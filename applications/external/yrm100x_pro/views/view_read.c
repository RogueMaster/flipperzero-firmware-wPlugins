#include "view_read.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <furi/core/memmgr.h>

#define UHF_FOREVER_HISTORY_MAX 10U

/*
 * The read-screen redraw runs every 100 ms. Keep EPC/PC/CRC formatting
 * allocation-free so USB/RPC activity cannot amplify heap fragmentation.
 */
static void read_hex_to_buf(char* out, size_t out_size, const uint8_t* data, size_t data_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!data || data_size == 0U) return;

    size_t max_bytes = (out_size - 1U) / 2U;
    if(data_size > max_bytes) data_size = max_bytes;

    for(size_t i = 0; i < data_size; i++) {
        snprintf(out + i * 2U, out_size - i * 2U, "%02X", data[i]);
    }
}

static void read_u16_to_buf(char* out, size_t out_size, uint16_t value) {
    if(!out || out_size == 0U) return;
    snprintf(out, out_size, "%04X", (unsigned int)value);
}

static void read_trim_forever_history(UHFTagWrapper* wrapper) {
    while(wrapper && wrapper->tag_count > UHF_FOREVER_HISTORY_MAX) {
        uhf_tag_free(wrapper->tags[0]);
        memmove(
            &wrapper->tags[0],
            &wrapper->tags[1],
            (wrapper->tag_count - 1U) * sizeof(wrapper->tags[0]));
        wrapper->tag_count--;
        wrapper->tags[wrapper->tag_count] = NULL;
    }
}

/**
 * @brief      Converts hex bytearray to string
 * @details    This function is used to convert a bytearray to a char* for use with views.
 * @param      array    The bytearray -Byte array of hexadecimal values
 * @param      length  The length of the array - size_t length value for the byte array
 * @return     char* - the string representation of the hexadecimal array.
*/
char* convert_to_hex_string(uint8_t* array, size_t length) {
    if(array == NULL || length == 0) {
        char* empty = (char*)malloc(1);
        if(empty) empty[0] = '\0';
        return empty;
    }

    FuriString* temp_str = furi_string_alloc();
    if(!temp_str) return NULL;

    for(size_t i = 0; i < length; i++) {
        furi_string_cat_printf(temp_str, "%02X", array[i]);
    }

    const char* furi_str = furi_string_get_cstr(temp_str);
    size_t str_len = strlen(furi_str);
    char* str = (char*)malloc(str_len + 1U);

    if(str) {
        memcpy(str, furi_str, str_len);
        str[str_len] = '\0';
    }

    furi_string_free(temp_str);
    return str;
}

static void uhf_reader_load_wrapper_tag(UHFReaderApp* App, size_t zero_index) {
    if(!App || !App->YRM100XWorker || !App->YRM100XWorker->uhf_tag_wrapper) return;

    UHFTagWrapper* wrapper = App->YRM100XWorker->uhf_tag_wrapper;
    if(zero_index >= wrapper->tag_count) return;

    UHFTag* tag = wrapper->tags[zero_index];
    if(!tag || !tag->epc || tag->epc->size == 0) return;

    char* epc = convertToHexString(tag->epc->data, tag->epc->size);
    char* crc = uint16_to_hex_string(tag->epc->crc);
    char* pc = uint16_to_hex_string(tag->epc->pc);

    char* tid = tag->dump_finalized && tag->tid->size > 0 ?
                    convertToHexString(tag->tid->data, tag->tid->size) :
                    strdup("---");
    char* res = tag->dump_finalized && tag->reserved->size > 0 ?
                    convertToHexString(tag->reserved->data, tag->reserved->size) :
                    strdup("---");
    char* user = tag->dump_finalized && tag->user->size > 0 ?
                     convertToHexString(tag->user->data, tag->user->size) :
                     strdup("---");

    if(!epc || !crc || !pc || !tid || !res || !user) {
        free(epc);
        free(crc);
        free(pc);
        free(tid);
        free(res);
        free(user);
        return;
    }

    App->NumberOfEpcsToRead = wrapper->tag_count;
    App->CurEpcIndex = (uint32_t)zero_index + 1U;
    App->DeepReadDone = tag->dump_finalized;
    App->DeepReading = false;

    with_view_model(
        App->ViewRead,
        UHFReaderConfigModel * model,
        {
            model->IsReading = false;
            model->IsDumping = false;
            model->NumEpcsRead = (uint32_t)wrapper->tag_count;
            model->CurEpcIndex = (uint32_t)zero_index + 1U;
            model->Rssi = tag->epc->rssi;
            model->SingleTidAvailable = tag->tid && tag->tid->size > 0;
            model->SingleResAvailable = tag->reserved && tag->reserved->size > 0;
            model->SingleUserAvailable = tag->user && tag->user->size > 0;
            furi_string_set_str(model->EpcValue, epc);
            furi_string_set_str(model->Crc, crc);
            furi_string_set_str(model->Pc, pc);
        },
        true);

    /*
     * ViewEpc and ViewBankMem are lazy since alpha33. The worker wrapper is
     * authoritative; optional presentation views may legitimately be NULL.
     */
    if(App->ViewEpc) {
        with_view_model(
            App->ViewEpc,
            UHFRFIDTagModel * model,
            {
                furi_string_set_str(model->Epc, epc);
                furi_string_set_str(model->Tid, tid);
                furi_string_set_str(model->Reserved, res);
                furi_string_set_str(model->User, user);
                furi_string_set_str(model->Crc, crc);
                furi_string_set_str(model->Pc, pc);
                model->Rssi = tag->epc->rssi;
                model->DeepReadDone = tag->dump_finalized;
                model->IsDeepReading = false;
                model->TidBankRead = tag->dump_finalized && tag->tid_status != UHFTagBankUnknown;
                model->UserBankRead = tag->dump_finalized && tag->user_status != UHFTagBankUnknown;
                model->ResBankRead = tag->dump_finalized &&
                                     tag->reserved_status != UHFTagBankUnknown;
            },
            true);
    }

    if(App->ViewBankMem) {
        with_view_model(
            App->ViewBankMem,
            UHFRFIDTagModel * model,
            {
                furi_string_set_str(model->Epc, epc);
                furi_string_set_str(model->Tid, tid);
                furi_string_set_str(model->Reserved, res);
                furi_string_set_str(model->User, user);
                furi_string_set_str(model->Crc, crc);
                furi_string_set_str(model->Pc, pc);
                model->Rssi = tag->epc->rssi;
                model->DeepReadDone = tag->dump_finalized;
                model->TidBankRead = tag->dump_finalized && tag->tid_status != UHFTagBankUnknown;
                model->UserBankRead = tag->dump_finalized && tag->user_status != UHFTagBankUnknown;
                model->ResBankRead = tag->dump_finalized &&
                                     tag->reserved_status != UHFTagBankUnknown;
                model->CurEpcIndex = (uint32_t)zero_index + 1U;
                model->VScrollLine = 0;
            },
            true);
    }

    UHF_T(
        "UI",
        "LOAD WRAPPER idx=%u epc_view=%p bank_view=%p heap=%lu",
        (unsigned int)zero_index,
        (void*)App->ViewEpc,
        (void*)App->ViewBankMem,
        (unsigned long)memmgr_get_free_heap());

    free(epc);
    free(crc);
    free(pc);
    free(tid);
    free(res);
    free(user);
}

static void uhf_reader_finalize_scan(UHFReaderApp* App) {
    if(!App || !App->YRM100XWorker) return;
    UHF_I(
        "UI",
        "finalize BEGIN wrapper=%u epc=%u full=%u",
        (unsigned int)(App->YRM100XWorker->uhf_tag_wrapper ?
                           App->YRM100XWorker->uhf_tag_wrapper->tag_count :
                           0U),
        (unsigned int)App->YRM100XWorker->FullMultiEpcCount,
        (unsigned int)App->YRM100XWorker->FullMultiDumpCount);

    App->IsReading = false;
    App->MultiDumpInProgress = false;
    App->DeepReading = false;

    // Catch a terminal FULL/PARTIAL dump that beat the GUI service by a few ms.
    if(App->FullMultiReadMode || App->FullSingleReadMode || App->ForeverReadMode) {
        UHFTagWrapper* wrapper = App->YRM100XWorker->uhf_tag_wrapper;
        for(size_t i = 0; wrapper && i < wrapper->tag_count; i++) {
            UHFTag* tag = wrapper->tags[i];
            if(tag && tag->dump_finalized && !tag->auto_dump_saved) {
                uhf_auto_dump_save_tag(App, tag);
            }
        }
        if(App->ForeverReadMode) read_trim_forever_history(wrapper);
    }

    UHFTagWrapper* wrapper = App->YRM100XWorker->uhf_tag_wrapper;
    size_t count = wrapper ? wrapper->tag_count : 0;

    with_view_model(
        App->ViewRead,
        UHFReaderConfigModel * model,
        {
            model->IsReading = false;
            model->IsDumping = false;
            model->NumEpcsRead = (uint32_t)count;
            model->ForeverTotalEpcs =
                App->ForeverReadMode ? (uint32_t)App->YRM100XWorker->FullMultiEpcCount : 0U;
        },
        true);

    if(count > 0) {
        uhf_reader_load_wrapper_tag(App, 0);

        if(App->ForeverReadMode) {
            App->NumberOfEpcsToRead = count;

            with_view_model(
                App->ViewRead,
                UHFReaderConfigModel * model,
                {
                    model->NumEpcsRead = (uint32_t)count;
                    model->CurEpcIndex = 1U;
                    model->ForeverTotalEpcs = (uint32_t)App->YRM100XWorker->FullMultiEpcCount;
                },
                true);
        }
    } else {
        App->NumberOfEpcsToRead = 0;
        App->CurEpcIndex = 0;
        App->DeepReadDone = false;

        with_view_model(
            App->ViewRead,
            UHFReaderConfigModel * model,
            {
                furi_string_set_str(model->EpcValue, "No tags found");
                model->CurEpcIndex = 0;
                model->Rssi = 0;
            },
            true);
    }
}

/**
 * @brief      Read Draw Callback.
 * @details    This function is called when the user selects read on the main submenu.
 * @param      canvas    The canvas - Canvas object for drawing the screen.
 * @param      model  The view model - model for the view with variables required for drawing.
*/
void uhf_reader_view_read_draw_callback(Canvas* canvas, void* model) {
    UHFReaderConfigModel* MyModel = (UHFReaderConfigModel*)model;
    char XStr[32];

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontPrimary);
    if(MyModel->IsForeverMode) {
        if(MyModel->ForeverWaiting && MyModel->ForeverWaitSeconds > 0U) {
            char countdown[4];
            snprintf(
                countdown, sizeof(countdown), "%u", (unsigned int)MyModel->ForeverWaitSeconds);
            canvas_draw_str(canvas, 2, 11, countdown);
        }
        /* Two character widths keep the title clear of the Prev button. */
        canvas_draw_str(canvas, 45, 11, "Forever");
    } else if(MyModel->IsFullMultiMode) {
        canvas_draw_str(canvas, 31, 11, "Full Multi");
    } else if(MyModel->IsFullSingleMode) {
        canvas_draw_str(canvas, 27, 11, "Full Single");
    } else {
        canvas_draw_str(canvas, 44, 11, "Scan");
    }
    canvas_set_font(canvas, FontSecondary);

    // Row 1: Full modes have E/F/P, Multi has counters, Single has none.
    const uint32_t epc_total = MyModel->IsForeverMode ? MyModel->ForeverTotalEpcs :
                                                        MyModel->NumEpcsRead;
    snprintf(XStr, sizeof(XStr), "%lu", (unsigned long)epc_total);
    if(MyModel->IsForeverMode || MyModel->IsFullMultiMode || MyModel->IsFullSingleMode) {
        canvas_draw_str(canvas, 2, 22, "E:");
        canvas_draw_str(canvas, 13, 22, XStr);

        canvas_draw_str(canvas, 39, 22, "F:");
        snprintf(XStr, sizeof(XStr), "%lu", (unsigned long)MyModel->FullMultiDumped);
        canvas_draw_str(canvas, 50, 22, XStr);

        canvas_draw_str(canvas, 75, 22, "P:");
        snprintf(XStr, sizeof(XStr), "%lu", (unsigned long)MyModel->FullMultiPartial);
        canvas_draw_str(canvas, 86, 22, XStr);
    } else if(!MyModel->IsSingleMode) {
        canvas_draw_str(canvas, 4, 22, "# EPCs:");
        canvas_draw_str(canvas, 45, 22, XStr);
        snprintf(XStr, sizeof(XStr), "%lu", (unsigned long)MyModel->CurEpcIndex);
        canvas_draw_str(canvas, 70, 22, "Cur Tag:");
        canvas_draw_str(canvas, 115, 22, XStr);
    }

    // Live RSSI meter — proximity/aiming feedback for the currently shown tag
    // (the payoff of Read (Single); also handy in Read (Multi)).
    if(MyModel->NumEpcsRead > 0) {
        snprintf(XStr, sizeof(XStr), "R:%d", (int)MyModel->Rssi);
        canvas_draw_str(canvas, 92, 11, XStr);
    }

    const char* EpcStr = furi_string_get_cstr(MyModel->EpcValue);
    size_t EpcLen = strlen(EpcStr);

    if(MyModel->IsSingleMode && MyModel->NumEpcsRead > 0) {
        const char* tail = EpcLen > 5 ? EpcStr + EpcLen - 5 : EpcStr;

        char EpcSummary[24];
        snprintf(EpcSummary, sizeof(EpcSummary), "EPC: ...%s", tail);
        canvas_draw_str(canvas, 1, 25, EpcSummary);

        char BanksLine1[32];
        snprintf(
            BanksLine1,
            sizeof(BanksLine1),
            "TID: %s   Res: %s",
            MyModel->SingleTidAvailable ? "Yes" : "No",
            MyModel->SingleResAvailable ? "Yes" : "No");
        canvas_draw_str(canvas, 1, 37, BanksLine1);

        char BanksLine2[20];
        snprintf(
            BanksLine2,
            sizeof(BanksLine2),
            "User: %s",
            MyModel->SingleUserAvailable ? "Yes" : "No");
        canvas_draw_str(canvas, 1, 48, BanksLine2);
    } else {
        // Multi and Full modes keep the full EPC wrapped over two lines.
        const size_t CharsPerLine = 20;

        char Line1[21];
        memset(Line1, 0, sizeof(Line1));
        size_t Line1Len = EpcLen < CharsPerLine ? EpcLen : CharsPerLine;
        memcpy(Line1, EpcStr, Line1Len);
        canvas_draw_str(canvas, 0, 33, Line1);

        if(EpcLen > CharsPerLine) {
            char Line2[21];
            memset(Line2, 0, sizeof(Line2));
            size_t Line2Len = (EpcLen - CharsPerLine) < CharsPerLine ? (EpcLen - CharsPerLine) :
                                                                       CharsPerLine;
            memcpy(Line2, EpcStr + CharsPerLine, Line2Len);
            canvas_draw_str(canvas, 0, 44, Line2);
        }
    }

    if(MyModel->IsDumping) {
        snprintf(
            XStr,
            sizeof(XStr),
            "Full dump: %lu/%lu",
            (unsigned long)MyModel->DumpCurrent,
            (unsigned long)MyModel->DumpTotal);
        canvas_draw_str(canvas, 18, 56, XStr);
    } else if(!MyModel->IsReading) {
        /*
         * Prev/Next only make sense when at least two tags exist.
         *
         * Consequences:
         * - Read (Single): never shown;
         * - Read Full (Single): never shown;
         * - Read (Multi): hidden for 0/1 tag, shown from 2 tags;
         * - Read Full (Multi): hidden for 0/1 tag, shown from 2 tags.
         */
        if(MyModel->NumEpcsRead > 1) {
            elements_button_up(canvas, "Prev");
            elements_button_down(canvas, "Next");
        }

        elements_button_left(canvas, "Save");
        elements_button_center(canvas, "Start");
        elements_button_right(canvas, "More");

        if(MyModel->IsForeverMode && MyModel->NumEpcsRead > 1U && MyModel->CurEpcIndex > 0U) {
            snprintf(
                XStr,
                sizeof(XStr),
                "%lu/%lu",
                (unsigned long)MyModel->CurEpcIndex,
                (unsigned long)MyModel->NumEpcsRead);
            canvas_draw_str_aligned(canvas, 126, 50, AlignRight, AlignBottom, XStr);
        }
    } else {
        elements_button_center(canvas, "Stop");
    }
}

/**
 * @brief      Callback when the user exits the read screen.
 * @details    This function is called when the user exits the read screen.
 * @param      context  The context - not used
 * @return     the view id of the next view.
*/
uint32_t uhf_reader_navigation_read_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewRead;
}

static void uhf_reader_fullmulti_gui_service(UHFReaderApp* App, UHFTagWrapper* wrapper) {
    if(!App || !wrapper || !App->YRM100XWorker) return;
    if(!App->IsReading ||
       (!App->FullMultiReadMode && !App->FullSingleReadMode && !App->ForeverReadMode))
        return;

    /*
     * This function is called ONLY by the view-dispatcher custom callback,
     * therefore it is safe to touch notifications, Storage and Submenu.
     *
     * Alpha9 did these operations directly from FuriTimer callback. The
     * second auto-save could block while mutating SubmenuSaved from the
     * timer thread; RFID worker kept scanning, but sounds/UI/saves appeared
     * frozen. That exact sequence is visible in the alpha9 trace.
     */

    size_t epc_count = App->YRM100XWorker->FullMultiEpcCount;
    while(App->LastFullMultiEpcBeepCount < epc_count) {
        App->LastFullMultiEpcBeepCount++;
        UHF_I(
            "UI",
            "BEEP EPC %u/%u",
            (unsigned int)App->LastFullMultiEpcBeepCount,
            (unsigned int)epc_count);
        uhf_notify_epc(App);
    }

    size_t full_count = App->YRM100XWorker->FullMultiDumpCount;
    while(App->LastFullMultiDumpBeepCount < full_count) {
        App->LastFullMultiDumpBeepCount++;
        UHF_I(
            "UI",
            "BEEP FULL %u/%u",
            (unsigned int)App->LastFullMultiDumpBeepCount,
            (unsigned int)full_count);
        uhf_notify_success(App);
    }

    size_t partial_count = App->YRM100XWorker->FullMultiPartialCount;
    while(App->LastFullMultiPartialBeepCount < partial_count) {
        App->LastFullMultiPartialBeepCount++;
        UHF_W(
            "UI",
            "BEEP PARTIAL %u/%u",
            (unsigned int)App->LastFullMultiPartialBeepCount,
            (unsigned int)partial_count);
        uhf_notify_error(App);
    }

    uint32_t forever_ack = 0U;

    // Save terminal captures on GUI thread, one file per tag, while scanning.
    for(size_t i = 0; i < wrapper->tag_count; i++) {
        UHFTag* tag = wrapper->tags[i];
        if(!tag || !tag->dump_finalized || tag->auto_dump_saved) continue;

        UHF_I(
            "UI",
            "AUTOSAVE GUI BEGIN tag=%u full=%d tid=%u user=%u rsv=%u",
            (unsigned int)(i + 1U),
            tag->full_dump_complete ? 1 : 0,
            (unsigned int)tag->tid->size,
            (unsigned int)tag->user->size,
            (unsigned int)tag->reserved->size);
        uhf_debug_flush();

        bool ok = uhf_auto_dump_save_tag(App, tag);

        if(ok && App->ForeverReadMode) {
            forever_ack = App->YRM100XWorker->ForeverCaptureSerial;
        }

        UHF_I("UI", "AUTOSAVE GUI END tag=%u ok=%d", (unsigned int)(i + 1U), ok ? 1 : 0);
        uhf_debug_flush();
    }

    if(forever_ack) {
        /*
         * The worker is waiting for this ACK, so the GUI can safely discard
         * the oldest immutable captures before allowing the next cycle.
         */
        read_trim_forever_history(wrapper);

        App->YRM100XWorker->ForeverSavedSerial = forever_ack;
        UHF_I(
            "FOREVER",
            "GUI SAVE ACK serial=%lu history=%u",
            (unsigned long)forever_ack,
            (unsigned int)wrapper->tag_count);
        uhf_debug_flush();
    }
}

/**
 * @brief      Callback for the timer used for the read screen
 * @details    This function is called for timer events.
 * @param      context  The UHFReaderApp - Used to change app variables.
*/
void uhf_reader_view_read_timer_callback(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    if(!App || !App->ViewDispatcher) return;

    /*
     * FuriTimer callback is not the GUI thread.
     * Absolutely no Storage, notification or Submenu operations here.
     * Coalesce events so a slow SD write cannot fill the dispatcher queue.
     */
    if(!App->FullMultiUiEventPending) {
        App->FullMultiUiEventPending = true;
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFReaderEventIdRedrawScreen);
    }
}
/**
 * @brief      Callback for the saved text input screen.
 * @details    This function saves the current tag selected with all its info
 * @param      context  The UHFReaderApp - Used to change app variables.
*/
void uhf_reader_save_text_updated(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;
    bool Redraw = true;

    //Allocating FuriStrings to store each of the values associated with each UHF Tag
    FuriString* Tid = furi_string_alloc();
    FuriString* Mem = furi_string_alloc();
    FuriString* Res = furi_string_alloc();
    FuriString* Pc = furi_string_alloc();
    FuriString* Crc = furi_string_alloc();

    //Set the current EPC to save for the app based on the model's value
    with_view_model(
        App->ViewRead,
        UHFReaderConfigModel * model,
        {
            /*
             * Both FuriString objects keep their own ownership.
             * Never replace model->EpcName with App->EpcName.
            */
            furi_string_set_str(model->EpcName, App->TempSaveBuffer);
            App->EpcToSave = (char*)furi_string_get_cstr(model->EpcValue);
        },
        Redraw);

    //Set the tid, User and Reserved memory values if any tags were read
    with_view_model(
        App->ViewEpc,
        UHFRFIDTagModel * model,
        {
            if(App->NumberOfEpcsToRead > 0) {
                furi_string_set(Tid, model->Tid);
                furi_string_set(Mem, model->User);
                furi_string_set(Res, model->Reserved);
                furi_string_set(Pc, model->Pc);
                furi_string_set(Crc, model->Crc);
            } else {
                furi_string_free(Tid);
                furi_string_free(Mem);
                furi_string_free(Res);
                furi_string_free(Pc);
                furi_string_free(Crc);
                return;
            }
        },
        Redraw);

    //Write the tag through the shared writer (single source of truth for format)
    save_uhf_tag_to_file(
        App,
        App->TempSaveBuffer,
        App->EpcToSave,
        furi_string_get_cstr(Tid),
        furi_string_get_cstr(Res),
        furi_string_get_cstr(Mem),
        furi_string_get_cstr(Pc),
        furi_string_get_cstr(Crc));

    //Freeing all the FuriStrings used
    furi_string_free(Res);
    furi_string_free(Tid);
    furi_string_free(Mem);
    furi_string_free(Pc);
    furi_string_free(Crc);
    dolphin_deed(DolphinDeedRfidSave);
    view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewRead);
}

/**
 * @brief      Callback for read screen input.
 * @details    This function is called when the user presses a button while on the read screen.
 * @param      event    The event - InputEvent object.
 * @param      context  The context - UHFReaderApp object.
 * @return     true if the event was handled, false otherwise.
*/
bool uhf_reader_view_read_input_callback(InputEvent* event, void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    /*
     * While RF work is active, physical Back is a Stop control, not
     * navigation. Consume every phase of the key event and route the short
     * release through the exact same GUI-thread stop path as center OK.
     */
    if(event->key == InputKeyBack && App->IsReading) {
        if(event->type == InputTypeShort) {
            UHF_I("UI", "BACK DURING READ -> STOP");
            uhf_debug_flush();
            view_dispatcher_send_custom_event(App->ViewDispatcher, UHFReaderEventIdOkPressed);
        }
        return true;
    }

    // Left = Save
    if(event->key == InputKeyLeft && !App->IsReading && event->type == InputTypeShort) {
        if(App->NumberOfEpcsToRead == 0) return true;

        /*
         * uhf_reader_save_text_updated() consumes the EPC detail model.
         * Allocate/populate it only when Save is actually requested.
         */
        uhf_reader_ensure_epc_views(App);
        if(App->CurEpcIndex > 0U) {
            uhf_reader_load_wrapper_tag(App, (size_t)(App->CurEpcIndex - 1U));
        }

        //Setting the text input header
        text_input_set_header_text(App->SaveInput, "Save EPC");
        bool Redraw = false;
        with_view_model(
            App->ViewRead,
            UHFReaderConfigModel * model,
            {
                //Prefill a default name of Tag_<last 8 EPC chars>
                const char* EpcStr = furi_string_get_cstr(model->EpcValue);
                size_t Len = strlen(EpcStr);
                const char* Tail = Len > 8 ? EpcStr + (Len - 8) : EpcStr;
                snprintf(App->TempSaveBuffer, App->TempBufferSaveSize, "Tag_%s", Tail);
            },
            Redraw);

        //Set the text input result callback function
        bool ClearPreviousText = false;
        text_input_set_result_callback(
            App->SaveInput,
            uhf_reader_save_text_updated,
            App,
            App->TempSaveBuffer,
            App->TempBufferSaveSize,
            ClearPreviousText);
        view_set_previous_callback(
            text_input_get_view(App->SaveInput), uhf_reader_navigation_read_callback);
        view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewSaveInput);
        return true;
    }
    //Handles all short input types
    if(event->type == InputTypeShort) {
        //YRM100X: navigate to the previous collected EPC in the multi-tag list
        if(event->key == InputKeyUp && !App->IsReading) {
            UHF_I(
                "UI",
                "Prev pressed count=%u cur=%u",
                (unsigned int)App->NumberOfEpcsToRead,
                (unsigned int)App->CurEpcIndex);
            UHFTagWrapper* wrapper = App->YRM100XWorker->uhf_tag_wrapper;
            uint32_t idx = App->CurEpcIndex;

            if(wrapper && App->NumberOfEpcsToRead > 1 && idx > 1 && idx <= wrapper->tag_count) {
                dolphin_deed(DolphinDeedNfcRead);
                uhf_reader_load_wrapper_tag(App, (size_t)(idx - 2U));
            }
            return true;
        }
        //YRM100X: navigate to the next collected EPC in the multi-tag list
        else if(event->key == InputKeyDown && !App->IsReading) {
            UHF_I(
                "UI",
                "Next pressed count=%u cur=%u",
                (unsigned int)App->NumberOfEpcsToRead,
                (unsigned int)App->CurEpcIndex);
            UHFTagWrapper* wrapper = App->YRM100XWorker->uhf_tag_wrapper;
            uint32_t idx = App->CurEpcIndex;

            if(wrapper && App->NumberOfEpcsToRead > 1 && idx >= 1 && idx < wrapper->tag_count) {
                uhf_reader_load_wrapper_tag(App, (size_t)idx);
            }
            return true;
        }

        //If the right button is pressed, navigate to the EPC dump screen
        else if(event->key == InputKeyRight && !App->IsReading && App->NumberOfEpcsToRead > 0) {
            UHF_I(
                "UI",
                "More pressed count=%u cur=%u",
                (unsigned int)App->NumberOfEpcsToRead,
                (unsigned int)App->CurEpcIndex);

            uhf_reader_ensure_epc_views(App);

            /*
             * Newly allocated detail views start empty. Populate them from the
             * current authoritative wrapper tag before opening More.
             */
            if(App->CurEpcIndex > 0U) {
                uhf_reader_load_wrapper_tag(App, (size_t)(App->CurEpcIndex - 1U));
            }

            // Sync deep-read state into EPC dump model before switching
            with_view_model(
                App->ViewEpc,
                UHFRFIDTagModel * model,
                {
                    model->IsDeepReading = App->DeepReading;
                    model->DeepReadDone = App->DeepReadDone;
                },
                false);
            view_set_previous_callback(App->ViewEpc, uhf_reader_navigation_read_callback);
            view_dispatcher_switch_to_view(App->ViewDispatcher, UHFReaderViewEpcDump);
            return true;
        }

    } else if(event->type == InputTypePress) {
        //Handles the start button being pressed
        if(event->key == InputKeyOk) {
            view_dispatcher_send_custom_event(App->ViewDispatcher, UHFReaderEventIdOkPressed);

            return true;
        }
    }

    return false;
}

/**
 * @brief      Callback when the user exits the read screen.
 * @details    This function is called when the user exits the read screen.
 * @param      context  The context - not used
 * @return     the view id of the next view.
*/
uint32_t uhf_reader_navigation_read_submenu_callback(void* context) {
    UNUSED(context);
    return UHFReaderViewSubmenu;
}

/**
 * @brief      Callback when the user exits the read screen.
 * @details    This function is called when the user exits the read screen.
 * @param      context  The context - UHFReaderApp object.
*/
static void
    uhf_reader_reset_scan_session(UHFReaderApp* App, const char* reason, bool reset_display) {
    if(!App) return;

    if(App->YRM100XWorker) {
        UHFTagWrapper* wrapper = App->YRM100XWorker->uhf_tag_wrapper;
        if(wrapper) uhf_tag_wrapper_reset_list(wrapper);

        App->YRM100XWorker->FullMultiEpcCount = 0;
        App->YRM100XWorker->FullMultiDumpCount = 0;
        App->YRM100XWorker->FullMultiPartialCount = 0;
        App->YRM100XWorker->ForeverCaptureSerial = 0;
        App->YRM100XWorker->ForeverSavedSerial = 0;
        App->YRM100XWorker->ForeverWaiting = false;
        App->YRM100XWorker->ForeverWaitRemainingSeconds = 0;
        App->YRM100XWorker->DumpCurrent = 0;
        App->YRM100XWorker->DumpTotal = 0;
    }

    App->NumberOfEpcsToRead = 0;
    App->NumberOfTidsToRead = 0;
    App->NumberOfResToRead = 0;
    App->NumberOfMemToRead = 0;
    App->CurEpcIndex = 0;
    App->CurTidIndex = 0;
    App->CurResIndex = 0;
    App->CurMemIndex = 0;

    App->LastMultiBeepCount = 0;
    App->LastFullMultiEpcBeepCount = 0;
    App->LastFullMultiDumpBeepCount = 0;
    App->LastFullMultiPartialBeepCount = 0;
    App->FullMultiUiEventPending = false;

    if(reset_display && App->ViewRead) {
        with_view_model(
            App->ViewRead,
            UHFReaderConfigModel * model,
            {
                model->CurEpcIndex = 0;
                model->NumEpcsRead = 0;
                model->FullMultiDumped = 0;
                model->FullMultiPartial = 0;
                model->DumpCurrent = 0;
                model->DumpTotal = 0;
                model->IsDumping = false;
                model->IsReading = false;
                model->ForeverWaiting = false;
                model->ForeverWaitSeconds = 0;
                model->ForeverTotalEpcs = 0;
                model->ScrollOffset = 0;
                model->SingleTidAvailable = false;
                model->SingleResAvailable = false;
                model->SingleUserAvailable = false;
                furi_string_set_str(model->EpcValue, "Press Start");
                furi_string_set_str(model->Pc, "XXXX");
                furi_string_set_str(model->Crc, "XXXX");
            },
            true);
    }

    if(reset_display && App->ViewEpc) {
        with_view_model(
            App->ViewEpc,
            UHFRFIDTagModel * epc_model,
            {
                furi_string_set_str(epc_model->Epc, "No Tags read");
                furi_string_set_str(epc_model->Tid, "---");
                furi_string_set_str(epc_model->Reserved, "---");
                furi_string_set_str(epc_model->User, "---");
                furi_string_set_str(epc_model->Crc, "----");
                furi_string_set_str(epc_model->Pc, "----");
                epc_model->CurEpcIndex = 0;
                epc_model->DeepReadDone = false;
                epc_model->IsDeepReading = false;
            },
            false);
    }

    if(reset_display && App->ViewBankMem) {
        with_view_model(
            App->ViewBankMem,
            UHFRFIDTagModel * bm_model,
            {
                furi_string_set_str(bm_model->Tid, "");
                furi_string_set_str(bm_model->Reserved, "");
                furi_string_set_str(bm_model->User, "");
                bm_model->CurEpcIndex = 0;
                bm_model->VScrollLine = 0;
                bm_model->CurrentBank = 0;
            },
            false);
    }

    UHF_I(
        "UI",
        "SCAN SESSION RESET reason=%s wrapper=0 epc=0 full=0 partial=0",
        reason ? reason : "?");
    uhf_debug_flush();
}

void uhf_reader_view_read_exit_callback(void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    /*
     * Physical Back is routed to Stop by the input callback in every active
     * read mode. This exit callback remains a second safety net.
     *
     * Stop/join the worker first so it cannot mutate the published slot, then
     * make one final GUI-thread save pass BEFORE destroying the timer/view.
     * This preserves a just-terminalized FT/PT even if Back lands between
     * worker finalization and the next 100 ms GUI service tick.
     */
    if(App->YRM100XWorker) {
        uhf_worker_stop(App->YRM100XWorker);

        if(App->ForeverReadMode && App->YRM100XWorker->uhf_tag_wrapper) {
            bool was_reading = App->IsReading;
            App->IsReading = true;
            uhf_reader_fullmulti_gui_service(App, App->YRM100XWorker->uhf_tag_wrapper);
            App->IsReading = was_reading;
        }
    }

    App->IsReading = false;

    if(App->Timer) {
        furi_timer_stop(App->Timer);
        furi_timer_free(App->Timer);
        App->Timer = NULL;
    }
    App->FullMultiUiEventPending = false;
}

/**
 * @brief      Read enter callback function.
 * @details    This function is called when the view transitions to the read screen.
 * @param      context  The context - UHFReaderApp object.
*/
void uhf_reader_view_read_enter_callback(void* context) {
    //Grab the period for the timer
    uint32_t Period = furi_ms_to_ticks(100);
    UHFReaderApp* App = (UHFReaderApp*)context;
    dolphin_deed(DolphinDeedNfcRead);

    App->IsReading = false;
    App->MultiDumpInProgress = false;

    // A new visit to Read is a new scan session.
    if(App->YRM100XWorker) uhf_worker_stop(App->YRM100XWorker);
    uhf_reader_reset_scan_session(App, "view_enter", true);

    // Start timer only after old worker/UI counters are clean.
    furi_assert(App->Timer == NULL);
    App->Timer =
        furi_timer_alloc(uhf_reader_view_read_timer_callback, FuriTimerTypePeriodic, context);
    furi_timer_start(App->Timer, Period);

    //Setting default reading states
    with_view_model(
        App->ViewRead,
        UHFReaderConfigModel * model,
        {
            model->IsReading = false;
            model->IsDumping = false;
            model->DumpCurrent = 0;
            model->DumpTotal = 0;
            model->IsFullMultiMode = App->FullMultiReadMode;
            model->IsFullSingleMode = App->FullSingleReadMode;
            model->IsForeverMode = App->ForeverReadMode;
            model->ForeverTotalEpcs = 0;
            model->ForeverWaiting = false;
            model->ForeverWaitSeconds = 0;
            model->IsSingleMode = App->SingleReadMode;
            model->SingleTidAvailable = false;
            model->SingleResAvailable = false;
            model->SingleUserAvailable = false;
            model->FullMultiDumped = 0;
            model->FullMultiPartial = 0;
        },
        true);
}

/**
 * @brief      Callback for the YRM100 worker
 * @details    This function is called when there is a worker event
 * @param      event    The worker event  - UHFWorkerEvent event.
 * @param     ctx  The context - UHFReaderApp object.
*/
void uhf_read_tag_worker_callback(UHFWorkerEvent event, void* ctx) {
    UHFReaderApp* App = (UHFReaderApp*)ctx;
    UHF_I(
        "UI",
        "worker callback event=%d reading=%d single=%d fullsingle=%d fullmulti=%d forever=%d epc=%u full=%u partial=%u",
        (int)event,
        App->IsReading ? 1 : 0,
        App->SingleReadMode ? 1 : 0,
        App->FullSingleReadMode ? 1 : 0,
        App->FullMultiReadMode ? 1 : 0,
        App->ForeverReadMode ? 1 : 0,
        (unsigned int)(App->YRM100XWorker ? App->YRM100XWorker->FullMultiEpcCount : 0U),
        (unsigned int)(App->YRM100XWorker ? App->YRM100XWorker->FullMultiDumpCount : 0U),
        (unsigned int)(App->YRM100XWorker ? App->YRM100XWorker->FullMultiPartialCount : 0U));
    uhf_debug_flush();

    // A stop request means the GUI-side stop handler (or view teardown) owns the UI
    // update AND the thread join. Posting a view event here would block on a full
    // dispatcher queue while that handler joins this worker → deadlock. Stay silent
    // once a stop is pending (ADR-0002).
    if(uhf_worker_stop_requested(App->YRM100XWorker)) {
        return;
    }

    // NOTE: do NOT call with_view_model() here — this function runs on the
    // worker OS thread, not the GUI thread. Triggering a ViewPort redraw from
    // the worker thread causes the ViewPort lockup warning. Scroll animation is
    // driven by the periodic timer on the GUI thread instead.

    if(event == UHFWorkerEventSuccess || event == UHFWorkerEventNoTagDetected) {
        notification_message(App->Notifications, &uhf_sequence_blink_stop);

        if(App->SingleReadMode) {
            if(event == UHFWorkerEventSuccess) {
                uhf_notify_success(App);
                dolphin_deed(DolphinDeedNfcReadSuccess);
            }
            view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventWorkerExit);
        } else if(App->FullSingleReadMode) {
            // GUI-thread WorkerExit will emit EPC + FULL/PARTIAL sound and auto-save.
            view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventWorkerExit);
        } else if(App->FullMultiReadMode) {
            // Full(Multi) already completed/saved tags during the live loop.
            view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventWorkerExit);
        } else if(App->ForeverReadMode) {
            /*
             * A Forever worker has no natural success path. If it exits here
             * because of an unexpected fatal condition, finalize the last slot.
             */
            view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventWorkerExit);
        } else {
            // Fast Multi optionally performs a full batch AFTER Stop.
            view_dispatcher_send_custom_event(
                App->ViewDispatcher, UHFCustomEventMultiInventoryDone);
        }
    } else if(event == UHFWorkerEventAborted || event == UHFWorkerEventFail) {
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        if(event == UHFWorkerEventFail) {
            uhf_notify_error(App);
            UHF_E("MEM", "READ worker failed; terminating busy UI state");
        }
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventWorkerExitAborted);
    }
}

/**
 * @brief      Worker callback for a deep-read of a single selected tag.
 * @details    On success it triggers navigation to the tag actions menu; any other
 *             outcome (abort/fail) returns to the read view.
 * @param      event  The worker event.
 * @param      ctx    The context - UHFReaderApp object.
*/
void uhf_deep_read_worker_callback(UHFWorkerEvent event, void* ctx) {
    UHFReaderApp* App = (UHFReaderApp*)ctx;
    // Treat both explicit success and timer-expired abort as "done" —
    // navigate to the banks screen with whatever data was captured.
    if(event == UHFWorkerEventSuccess ||
       (event == UHFWorkerEventAborted && App->DeepReadTimerExpired)) {
        if(event == UHFWorkerEventSuccess) {
            uhf_notify_success(App);
            dolphin_deed(DolphinDeedNfcReadSuccess);
        }
        App->DeepReadTimerExpired = false;
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventDeepReadDone);
    } else {
        // User pressed Back — abort without navigating to banks
        notification_message(App->Notifications, &uhf_sequence_blink_stop);
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventDeepReadAborted);
    }
}

void uhf_multi_full_dump_worker_callback(UHFWorkerEvent event, void* ctx) {
    UHFReaderApp* App = (UHFReaderApp*)ctx;

    if(event == UHFWorkerEventAborted && uhf_worker_stop_requested(App->YRM100XWorker)) {
        return;
    }

    view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventMultiFullDumpDone);
}

/**
 * @brief      Callback for custom read events.
 * @details    This function is called when a custom event is sent to the view dispatcher.
 * @param      event    The event id - UHFReaderAppEventId value.
 * @param      context  The context - UHFReaderApp object.
 * @return     true if the event was handled, false otherwise.
*/
bool uhf_reader_view_read_custom_event_callback(uint32_t event, void* context) {
    UHFReaderApp* App = (UHFReaderApp*)context;

    switch(event) {
    //Redraw the screen. The read view's 300ms periodic timer drives this; while a scan
    //is active we snapshot the shared tag wrapper here — on the GUI thread — for the
    //live # EPCs / current-tag / RSSI display. The worker never posts view events, so
    //a stop-time join can never deadlock against it (ADR-0002).
    case UHFReaderEventIdRedrawScreen: {
        UHFTagWrapper* wrapper = App->YRM100XWorker->uhf_tag_wrapper;
        size_t count = wrapper ? wrapper->tag_count : 0U;

        /*
         * All Full(Multi)/Full(Single) sounds + automatic file/menu writes are serviced
         * here on the GUI dispatcher thread.
         */
        if(wrapper) {
            uhf_reader_fullmulti_gui_service(App, wrapper);
            count = wrapper->tag_count;
        }

        if(App->FullMultiReadMode || App->FullSingleReadMode || App->ForeverReadMode) {
            with_view_model(
                App->ViewRead,
                UHFReaderConfigModel * _model,
                {
                    _model->IsFullMultiMode = App->FullMultiReadMode;
                    _model->IsFullSingleMode = App->FullSingleReadMode;
                    _model->IsForeverMode = App->ForeverReadMode;
                    _model->ForeverWaiting = App->ForeverReadMode &&
                                             App->YRM100XWorker->ForeverWaiting;
                    _model->ForeverWaitSeconds = App->YRM100XWorker->ForeverWaitRemainingSeconds;
                    _model->ForeverTotalEpcs = (uint32_t)App->YRM100XWorker->FullMultiEpcCount;
                    _model->FullMultiDumped = (uint32_t)App->YRM100XWorker->FullMultiDumpCount;
                    _model->FullMultiPartial = (uint32_t)App->YRM100XWorker->FullMultiPartialCount;
                },
                true);
        }

        if(App->MultiDumpInProgress) {
            with_view_model(
                App->ViewRead,
                UHFReaderConfigModel * _model,
                {
                    _model->IsDumping = true;
                    _model->DumpCurrent = (uint32_t)App->YRM100XWorker->DumpCurrent;
                    _model->DumpTotal = (uint32_t)App->YRM100XWorker->DumpTotal;
                },
                true);
            App->FullMultiUiEventPending = false;
            return true;
        }
        if(App->IsReading && count > 0) {
            bool first_visible_tag = App->NumberOfEpcsToRead == 0U;

            App->NumberOfEpcsToRead = count;

            // Most-recently discovered tag (Forever keeps a bounded history).
            UHFTag* latest = wrapper->tags[count - 1];

            if(App->ForeverReadMode && latest->epc->size == 0) {
                App->FullMultiUiEventPending = false;
                return true;
            }

            if(first_visible_tag) {
                UHF_I(
                    "UI",
                    "REDRAW FIRST TAG count=%u epc_view=%p bank_view=%p "
                    "heap=%lu",
                    (unsigned int)count,
                    (void*)App->ViewEpc,
                    (void*)App->ViewBankMem,
                    (unsigned long)memmgr_get_free_heap());
                uhf_debug_flush();
            }

            char TempEpc[(EPC_MAX_BANK_SIZE * 2U) + 1U];
            char TempCrc[5];
            char TempPc[5];

            read_hex_to_buf(TempEpc, sizeof(TempEpc), latest->epc->data, latest->epc->size);
            read_u16_to_buf(TempCrc, sizeof(TempCrc), latest->epc->crc);
            read_u16_to_buf(TempPc, sizeof(TempPc), latest->epc->pc);

            with_view_model(
                App->ViewRead,
                UHFReaderConfigModel * _model,
                {
                    furi_string_set_str(_model->EpcValue, TempEpc);
                    _model->NumEpcsRead = (uint32_t)count;
                    _model->CurEpcIndex = (uint32_t)count;
                    furi_string_set_str(_model->Crc, TempCrc);
                    furi_string_set_str(_model->Pc, TempPc);
                    _model->Rssi = latest->epc->rssi;
                },
                true);

            /*
             * ROOT CAUSE OF alpha35 furi_check_failed:
             * ViewEpc is lazy and is NULL until More/Save creates it.
             */
            if(App->ViewEpc) {
                with_view_model(
                    App->ViewEpc,
                    UHFRFIDTagModel * _model,
                    {
                        furi_string_set_str(_model->Epc, TempEpc);
                        furi_string_set_str(_model->Crc, TempCrc);
                        furi_string_set_str(_model->Pc, TempPc);
                        _model->Rssi = latest->epc->rssi;
                    },
                    false);
            }
        } else {
            with_view_model(
                App->ViewRead, UHFReaderConfigModel * _model, { UNUSED(_model); }, true);
        }

        App->FullMultiUiEventPending = false;
        return true;
    }

    case UHFCustomEventMultiInventoryDone: {
        App->IsReading = false;
        uhf_worker_stop(App->YRM100XWorker);

        size_t count = App->YRM100XWorker->uhf_tag_wrapper->tag_count;
        if(count > 0 && App->SettingMultiFullIndex == 1) {
            App->MultiDumpInProgress = true;
            App->YRM100XWorker->DumpCurrent = 0;
            App->YRM100XWorker->DumpTotal = count;

            with_view_model(
                App->ViewRead,
                UHFReaderConfigModel * _model,
                {
                    _model->IsReading = false;
                    _model->IsDumping = true;
                    _model->DumpCurrent = 0;
                    _model->DumpTotal = (uint32_t)count;
                },
                true);

            uhf_worker_start(
                App->YRM100XWorker,
                UHFWorkerStateDeepReadAll,
                uhf_multi_full_dump_worker_callback,
                App);
            return true;
        }

        if(count > 0 && App->SettingAutoSaveMultiIndex == 1) {
            uhf_auto_dump_save_all(App);
        }

        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventWorkerExit);
        return true;
    }

    case UHFCustomEventMultiFullDumpDone: {
        uhf_worker_stop(App->YRM100XWorker);
        App->MultiDumpInProgress = false;

        with_view_model(
            App->ViewRead,
            UHFReaderConfigModel * _model,
            {
                _model->IsDumping = false;
                _model->DumpCurrent = (uint32_t)App->YRM100XWorker->DumpCurrent;
                _model->DumpTotal = (uint32_t)App->YRM100XWorker->DumpTotal;
            },
            true);

        if(App->SettingAutoSaveMultiIndex == 1) {
            uhf_auto_dump_save_all(App);
        }

        uhf_notify_success(App);
        view_dispatcher_send_custom_event(App->ViewDispatcher, UHFCustomEventWorkerExit);
        return true;
    }

    //Handles the worker exiting after a multi-tag read
    case UHFCustomEventWorkerExit: {
        // Natural one-shot/list-full completion.
        uhf_worker_stop(App->YRM100XWorker);

        /*
         * Read Full (Single) may finish between 100 ms timer ticks.
         * Service its two-stage sound + automatic .uhf/Saved write here on
         * the GUI thread so a completed one-shot can never exit unsaved.
         */
        if(App->FullSingleReadMode && App->YRM100XWorker->uhf_tag_wrapper) {
            uhf_reader_fullmulti_gui_service(App, App->YRM100XWorker->uhf_tag_wrapper);
        }

        uhf_reader_finalize_scan(App);
        return true;
    }

    //Handles the worker exiting after user pressed Stop
    case UHFCustomEventWorkerExitAborted: {
        bool Redraw = true;
        App->IsReading = false;
        // Worker has already delivered its terminal event and exited — joining here
        // is safe (no more view events will be posted) and readies it for restart.
        uhf_worker_stop(App->YRM100XWorker);
        with_view_model(
            App->ViewRead, UHFReaderConfigModel * _model, { _model->IsReading = false; }, Redraw);
        return true;
    }

    //Deep-read of the selected multi-tag finished: populate banks and open tag actions
    case UHFCustomEventDeepReadDone: {
        // This event is now handled by the EPC dump view's custom callback.
        // It should not fire while the Read Menu is active.
        return false;
    }

    //Deep-read was aborted (back pressed / fail): stay on the read view
    case UHFCustomEventDeepReadAborted: {
        // This event is now handled by the EPC dump view's custom callback.
        return false;
    }

    //The ok button was pressed
    case UHFReaderEventIdOkPressed:

        //Check if the app is reading
        if(App->IsReading) {
            UHF_I(
                "UI",
                "STOP PRESSED wrapper=%u epc=%u full=%u",
                (unsigned int)(App->YRM100XWorker->uhf_tag_wrapper ?
                                   App->YRM100XWorker->uhf_tag_wrapper->tag_count :
                                   0U),
                (unsigned int)App->YRM100XWorker->FullMultiEpcCount,
                (unsigned int)App->YRM100XWorker->FullMultiDumpCount);
            uhf_debug_flush();

            App->IsReading = false;
            notification_message(App->Notifications, &uhf_sequence_blink_stop);

            uint32_t stop_tick = furi_get_tick();
            UHF_I("UI", "calling worker_stop");
            uhf_debug_flush();
            uhf_worker_stop(App->YRM100XWorker);
            UHF_I(
                "UI",
                "worker_stop returned elapsed_ticks=%lu",
                (unsigned long)(furi_get_tick() - stop_tick));
            uhf_debug_flush();

            if(App->SingleReadMode || App->FullSingleReadMode || App->FullMultiReadMode ||
               App->ForeverReadMode) {
                /*
                 * Finalize immediately on the GUI thread.
                 * This guarantees Prev/Next/Save/More become usable as soon as
                 * Stop returns, even when the dispatcher queue is not enabled.
                 */
                uhf_reader_finalize_scan(App);
            } else {
                view_dispatcher_send_custom_event(
                    App->ViewDispatcher, UHFCustomEventMultiInventoryDone);
            }
        } else if(App->MultiDumpInProgress) {
            // Ignore center presses while the full batch is being read.
            return true;
        } else {
            //Check if the reader is connected before sending a read command
            if(uhf_reader_require_antenna(App, UHFReaderViewRead)) {
                UHF_I(
                    "UI",
                    "START PRESSED single=%d fullsingle=%d fullmulti=%d forever=%d normalmulti=%d",
                    App->SingleReadMode ? 1 : 0,
                    App->FullSingleReadMode ? 1 : 0,
                    App->FullMultiReadMode ? 1 : 0,
                    App->ForeverReadMode ? 1 : 0,
                    (!App->SingleReadMode && !App->FullSingleReadMode && !App->FullMultiReadMode &&
                     !App->ForeverReadMode) ?
                        1 :
                        0);
                uhf_debug_flush();
                App->IsReading = false;
                // Starting a new scan — reset deep-read state
                App->DeepReadDone = false;
                App->DeepReadTimerExpired = false;
                App->MultiDumpInProgress = false;
                App->LastMultiBeepCount = 0;
                App->LastFullMultiEpcBeepCount = 0;
                App->LastFullMultiDumpBeepCount = 0;
                App->LastFullMultiPartialBeepCount = 0;
                App->FullMultiUiEventPending = false;

                /*
                 * Stop/reset before IsReading=true. This prevents a timer tick
                 * from replaying previous worker EPC/FULL/PARTIAL counters.
                 */
                uhf_worker_stop(App->YRM100XWorker);
                uhf_reader_reset_scan_session(App, "start_pressed", true);

                App->IsReading = true;
                with_view_model(
                    App->ViewRead,
                    UHFReaderConfigModel * model,
                    {
                        model->IsReading = true;
                        furi_string_set_str(model->EpcValue, "Wait RFID Tag...");
                    },
                    true);

                UHF_I(
                    "UI",
                    "WAIT RFID TAG mode single=%d fullsingle=%d fullmulti=%d forever=%d",
                    App->SingleReadMode ? 1 : 0,
                    App->FullSingleReadMode ? 1 : 0,
                    App->FullMultiReadMode ? 1 : 0,
                    App->ForeverReadMode ? 1 : 0);
                uhf_debug_flush();

                notification_message(App->Notifications, &uhf_sequence_blink_start_cyan);

                App->YRM100XWorker->ReadProfile = App->SettingTagProfileIndex;

                UHF_I(
                    "UI",
                    "Read Tag Profile=%s(%u)",
                    App->SettingTagProfileNames[App->SettingTagProfileIndex],
                    (unsigned int)App->SettingTagProfileIndex);
                uhf_debug_flush();

                UHFWorkerState read_state = UHFWorkerStateDetectMultiple;
                if(App->SingleReadMode) {
                    read_state = UHFWorkerStateDetectSingle;
                } else if(App->FullSingleReadMode) {
                    read_state = UHFWorkerStateDetectSingleFull;
                    App->YRM100XWorker->FullDumpMaxAttempts = App->SettingFullDumpAttempts;

                    UHF_I(
                        "UI",
                        "Read Full(Single) configured attempts=%u",
                        (unsigned int)App->SettingFullDumpAttempts);
                    uhf_debug_flush();
                } else if(App->FullMultiReadMode) {
                    read_state = UHFWorkerStateDetectMultipleFull;
                    App->YRM100XWorker->FullDumpMaxAttempts = App->SettingFullDumpAttempts;

                    UHF_I(
                        "UI",
                        "Read Full(Multi) configured attempts=%u",
                        (unsigned int)App->SettingFullDumpAttempts);
                    uhf_debug_flush();
                } else if(App->ForeverReadMode) {
                    read_state = UHFWorkerStateDetectForever;
                    App->YRM100XWorker->FullDumpMaxAttempts = App->SettingFullDumpAttempts;
                    App->YRM100XWorker->ForeverDelaySeconds = App->SettingForeverDelaySeconds;

                    UHF_I(
                        "UI",
                        "Read Forever configured attempts=%u delay=%us",
                        (unsigned int)App->SettingFullDumpAttempts,
                        (unsigned int)App->SettingForeverDelaySeconds);
                    uhf_debug_flush();
                }

                uhf_worker_start(
                    App->YRM100XWorker, read_state, uhf_read_tag_worker_callback, App);
            }
        }
        return true;
    default:
        return false;
    }
}

/**
 * @brief      Allocates the saved input view.
 * @details    This function allocates all variables for the saved input view.
 * @param      context  The context - UHFReaderApp object.
*/
void saved_input_alloc(UHFReaderApp* App) {
    //Allocate a new text input component
    App->SaveInput = text_input_alloc();
    view_dispatcher_add_view(
        App->ViewDispatcher, UHFReaderViewSaveInput, text_input_get_view(App->SaveInput));

    //Setting the max size of the buffer
    App->TempBufferSaveSize = 150;
    App->TempSaveBuffer = (char*)malloc(App->TempBufferSaveSize);
}

/**
 * @brief      Allocates the read view.
 * @details    This function allocates all variables for the read view.
 * @param      context  The context - UHFReaderApp object.
*/
void view_read_alloc(UHFReaderApp* App) {
    //Allocating the view and setting all callback functions
    saved_input_alloc(App);
    App->ViewRead = view_alloc();
    view_set_draw_callback(App->ViewRead, uhf_reader_view_read_draw_callback);
    view_set_input_callback(App->ViewRead, uhf_reader_view_read_input_callback);
    view_set_previous_callback(App->ViewRead, uhf_reader_navigation_read_submenu_callback);
    view_set_enter_callback(App->ViewRead, uhf_reader_view_read_enter_callback);
    view_set_exit_callback(App->ViewRead, uhf_reader_view_read_exit_callback);
    view_set_context(App->ViewRead, App);
    view_set_custom_callback(App->ViewRead, uhf_reader_view_read_custom_event_callback);

    //Allocating the view model
    view_allocate_model(App->ViewRead, ViewModelTypeLockFree, sizeof(UHFReaderConfigModel));
    UHFReaderConfigModel* Model = view_get_model(App->ViewRead);
    FuriString* EpcValueDefault = furi_string_alloc();
    furi_string_set_str(EpcValueDefault, "Press Start");

    //Setting default values for the view model
    Model->Setting1Index = App->Setting1Index;
    Model->Setting2Power = App->Setting2PowerStr;
    Model->SettingReadAp = App->DefaultAccessPwdStr;
    Model->Setting1Value = furi_string_alloc_set(App->Setting1Names[App->Setting1Index]);
    Model->Pc = furi_string_alloc_set("XXXX");
    Model->Crc = furi_string_alloc_set("XXXX");
    Model->EpcName = furi_string_alloc_set("Enter name");
    Model->ScrollOffset = 0;
    Model->ScrollingText = "Press Start";
    Model->EpcValue = EpcValueDefault;
    Model->CurEpcIndex = 1;
    Model->NumEpcsRead = 0;
    Model->IsReading = false;
    Model->IsFullMultiMode = false;
    Model->IsFullSingleMode = false;
    Model->IsForeverMode = false;
    Model->ForeverTotalEpcs = 0;
    Model->ForeverWaiting = false;
    Model->ForeverWaitSeconds = 0;
    Model->IsSingleMode = false;
    Model->SingleTidAvailable = false;
    Model->SingleResAvailable = false;
    Model->SingleUserAvailable = false;
    Model->FullMultiDumped = 0;
    Model->FullMultiPartial = 0;
    view_dispatcher_add_view(App->ViewDispatcher, UHFReaderViewRead, App->ViewRead);
}

/**
 * @brief      Frees the read view.
 * @details    This function frees all variables for the read view.
 * @param      context  The context - UHFReaderApp object.
*/
void view_read_free(UHFReaderApp* App) {
    view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewSaveInput);
    text_input_free(App->SaveInput);
    App->SaveInput = NULL;

    free(App->TempSaveBuffer);
    App->TempSaveBuffer = NULL;

    if(App->ViewRead) {
        UHFReaderConfigModel* model = view_get_model(App->ViewRead);
        if(model) {
            /*
             * OWNED by this view:
             * Setting1Value, Pc, Crc, EpcName, EpcValue.
             *
             * BORROWED from App and intentionally NOT freed here:
             * Setting2Power, SettingReadAp.
             */
            if(model->Setting1Value) {
                furi_string_free(model->Setting1Value);
                model->Setting1Value = NULL;
            }
            if(model->Pc) {
                furi_string_free(model->Pc);
                model->Pc = NULL;
            }
            if(model->Crc) {
                furi_string_free(model->Crc);
                model->Crc = NULL;
            }
            if(model->EpcName) {
                furi_string_free(model->EpcName);
                model->EpcName = NULL;
            }
            if(model->EpcValue) {
                furi_string_free(model->EpcValue);
                model->EpcValue = NULL;
            }
        }

        view_dispatcher_remove_view(App->ViewDispatcher, UHFReaderViewRead);
        view_free(App->ViewRead);
        App->ViewRead = NULL;
    }
}
